#include "NativeSkateHud.h"
#include "NativeSkateObjectDropper.h"
#include "NativeSkateObjectRegistry.h"
#include "NativeSkateObjectEditor.h"
#include "NativeSkateInput.h"
#include "NativeSkateUnits.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "../SkateHarkinian/VhsPickup.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <memory>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/DisplayList.h>
#include <cstdio>
extern "C" {
void FrameInterpolation_RecordOpenChild(const void*, int);
void FrameInterpolation_RecordCloseChild();
#include "global.h"
float OTRGetDimensionFromRightEdge(float);
float OTRGetDimensionFromLeftEdge(float);
}
namespace NativeSkateObjectDropper {
using namespace NativeSkateObjects;
constexpr const char* kEnabled = "gEnhancements.NativeSkate.ObjectDropper.Enabled";
struct Shape {
    Mesh mesh;
    std::vector<Vec3s> vertices;
    std::vector<CollisionPoly> polygons;
    SurfaceType surface{};
    CamData camera{};
    CollisionHeader header{};
};
static std::array<Shape, kTypeCount> shapes;
static Registry objects;
static bool vertDirty = true;
static std::vector<NativeSkateVert::Ramp> vertRamps;
static std::array<std::unique_ptr<DynaPolyActor>, kMaxObjects> actors;
static PlayState* bound = nullptr;
static int scene = -1, room = -1;
static bool editing = false, valid = false, releaseGate = false, manualHeightActive = false;
static Player* frozen = nullptr;
static ActorFunc stockUpdate = nullptr;
static Pose lastRenderablePreview;
static bool haveRenderablePreview = false;
static Pose preview;
static Type selected = Type::Marker;
static Vec3f root;
static double lastTime = 0, noticeUntil = 0;
static uint16_t previous = 0;
static uint16_t previousSecondary = 0;
static float deleteTime = 0;
static bool deleteEligible = false, deleteFired = false;
constexpr float kRecallHoldSeconds = .5f;
static uint32_t lastFrame = UINT32_MAX;
static const char* notice = "";
static std::function<void(Actor*)> changed;
using NativeSkateObjectEditor::Stage;
static NativeSkateObjectEditor::MovementBasis movementBasis;
static NativeSkateObjectEditor::StickOwnership stickOwnership;
static double uiChanged = 0;
static Stage uiStage = Stage::Browser;
static Stage stage = Stage::Browser;
static Category category = Category::Ramps;
static int item = 0;
static Id carried = 0;
static bool propGrabWasHeld = false, propGrabConsumed = false;
static Id picked = 0;
static bool markerKnown = false;
static NativeSkateObjectEditor::Camera editCamera;
static s16 cameraId = CAM_ID_NONE, parentCameraId = CAM_ID_MAIN;
static Camera* ownedCamera = nullptr;
static double dynamicsTime = 0, dynamicsAccumulator = 0;
static Vec3f previousPlayer{};
static bool previousPlayerValid = false;
static void SetCollision(Id id, bool enabled) {
    int i = objects.Find(id);
    if (i < 0 || !actors[i] || bound != gPlayState)
        return;
    int bg = objects.slots[i]->collisionHandle;
    if (enabled)
        func_8003EC50(bound, &bound->colCtx.dyna, bg);
    else
        func_8003EBF8(bound, &bound->colCtx.dyna, bg);
}
static void CancelPick() {
    if (picked)
        SetCollision(picked, true);
    picked = 0;
}
static void RestoreCamera() {
    if (cameraId != CAM_ID_NONE && bound == gPlayState && Play_GetCamera(bound, cameraId) == ownedCamera) {
        if (GET_ACTIVE_CAM(bound) == ownedCamera)
            Play_ChangeCameraStatus(bound, parentCameraId, CAM_STAT_ACTIVE);
        Play_ClearCamera(bound, cameraId);
    }
    cameraId = CAM_ID_NONE;
    ownedCamera = nullptr;
}

static float Tuning(const char* key, float fallback, float low, float high) {
    float v = CVarGetFloat(key, fallback);
    return std::isfinite(v) ? std::clamp(v, low, high) : fallback;
}
static void Feedback(const char* message) {
    notice = message;
    noticeUntil = NativeSkateInput::Now() + 1.5;
}
static void BodyUpdate(Actor*, PlayState*) {
}
static void FrozenUpdate(Actor*, PlayState* play) {
    GameInteractor_ExecuteOnPlayerUpdate();
}
static bool Safe(PlayState* play) {
    return play && GET_PLAYER(play) && play->sceneNum >= 0 && !play->pauseCtx.state && !play->gameOverCtx.state &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && !play->msgCtx.msgMode && !Player_InCsMode(play) &&
           gSaveContext.health > 0;
}
bool Active() {
    return editing;
}
void Exit(const char* reason) {
    CancelPick();
    RestoreCamera();
    if (frozen && bound == gPlayState && GET_PLAYER(bound) == frozen && frozen->actor.update == FrozenUpdate)
        frozen->actor.update = stockUpdate;
    frozen = nullptr;
    stockUpdate = nullptr;
    if (editing)
        SPDLOG_INFO("[ObjectDropper] DROPPER_EXIT scene={} room={} reason={}", scene, room, reason);
    editing = false;
    valid = false;
    releaseGate = false;
    CVarSetInteger(kEnabled, 0);
    NativeSkateInput::Reset(NativeSkateInput::Now());
}
static void Remove(Id id, const char* reason, bool discard = false) {
    vertDirty = true;
    if (carried == id)
        carried = 0;
    int slot = objects.Find(id);
    if (slot < 0)
        return;
    auto& entry = *objects.slots[slot];
    if (actors[slot]) {
        if (!discard && bound == gPlayState && entry.collisionHandle >= 0 &&
            bound->colCtx.dyna.bgActors[entry.collisionHandle].actor == &actors[slot]->actor)
            DynaPoly_DeleteBgActor(bound, &bound->colCtx.dyna, entry.collisionHandle);
        if (changed)
            changed(&actors[slot]->actor);
        actors[slot].reset();
    }
    SPDLOG_INFO(
        "[ObjectDropper] OBJECT_REMOVE id={} type={} scene={} room={} pos={} {} {} yaw={} collision={} reason={}", id,
        Definitions()[(int)entry.type].id, scene, room, entry.pose.x, entry.pose.y, entry.pose.z, entry.pose.yaw,
        entry.collisionHandle, reason);
    objects.Remove(id);
}
void Clear(const char* reason, bool sceneReplaced) {
    if (sceneReplaced) {
        picked = 0;
        cameraId = CAM_ID_NONE;
        ownedCamera = nullptr;
        frozen = nullptr;
        stockUpdate = nullptr;
    }
    Exit(reason);
    for (auto& e : objects.slots)
        if (e)
            Remove(e->id, reason, sceneReplaced);
    objects.Clear();
    carried = 0;
    propGrabWasHeld = propGrabConsumed = false;
    markerKnown = false;
    previousPlayerValid = false;
    dynamicsTime = 0;
    dynamicsAccumulator = 0;
    bound = nullptr;
    scene = room = -1;
    lastFrame = UINT32_MAX;
    SPDLOG_INFO("[ObjectDropper] OBJECT_CLEAR reason={}", reason);
}
static void ReleaseProp();
static void Enter(PlayState* play, bool nativeOwned) {
    ReleaseProp();
    haveRenderablePreview = false;
    manualHeightActive = false;
    stickOwnership = {};
    movementBasis = {};
    uiChanged = NativeSkateInput::Now();
    uiStage = Stage::Browser;
    stage = Stage::Browser;
    category = Category::Ramps;
    item = 0;
    picked = 0;
    previousSecondary = NativeSkateInput::Current().buttons;
    deleteTime = 0;
    deleteEligible = deleteFired = false;
    auto* player = GET_PLAYER(play);
    editing = true;
    previous = play->state.input[0].cur.button;
    root = player->actor.world.pos;
    float yaw = player->actor.shape.rot.y * 3.14159265359f / 32768;
    float distance = Tuning(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.Distance"), 110.f, 60.f, 300.f);
    preview = { root.x + std::sin(yaw) * distance, root.y, root.z + std::cos(yaw) * distance, yaw };
    lastTime = NativeSkateInput::Now();
    editCamera = { yaw, .45f, 240.f };
    parentCameraId = play->activeCamera;
    cameraId = Play_CreateSubCamera(play);
    if (cameraId != CAM_ID_NONE) {
        ownedCamera = Play_GetCamera(play, cameraId);
        Camera_RequestSetting(ownedCamera, CAM_SET_FREE0);
        Play_ChangeCameraStatus(play, parentCameraId, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, cameraId, CAM_STAT_ACTIVE);
    } else {
        Feedback("CAMERA UNAVAILABLE");
        Exit("CAMERA_CAPACITY");
        return;
    }
    if (!nativeOwned) {
        frozen = player;
        stockUpdate = player->actor.update;
        player->actor.update = FrozenUpdate;
    }
    SPDLOG_INFO("[ObjectDropper] DROPPER_ENTER scene={} room={} nativeOwned={} OBJECT_PREVIEW type={}", scene, room,
                nativeOwned, Definitions()[(int)selected].id);
}
static bool PropReady(Type type);
// The same read-only validator controls preview tint and confirmation. Air height is unrestricted.
static bool Validate(PlayState* play) {
    if (!Safe(play) || bound != play || scene != play->sceneNum || room != play->roomCtx.curRoom.num || !Valid(preview))
        return false;
    const auto& d = Definitions()[(int)selected];
    if (d.resource && (selected == Type::Vhs ? !SkateHarkinian_VhsObjectReady() : !PropReady(selected)))
        return false;
    if (picked) {
        if (objects.Find(picked) < 0)
            return false;
    } else {
        if (objects.Count() >= kMaxObjects)
            return false;
        if (d.collision == Collision::StaticSolid) {
            auto& dyn = play->colCtx.dyna;
            int polys = shapes[(int)selected].header.numPolygons, vertices = shapes[(int)selected].header.numVertices;
            bool freeSlot = false;
            for (int i = 0; i < BG_ACTOR_MAX; ++i) {
                if (!(dyn.bgActorFlags[i] & 1))
                    freeSlot = true;
                if ((dyn.bgActorFlags[i] & 1) && !(dyn.bgActorFlags[i] & 2) && dyn.bgActors[i].colHeader) {
                    polys += dyn.bgActors[i].colHeader->numPolygons;
                    vertices += dyn.bgActors[i].colHeader->numVertices;
                }
            }
            if (!freeSlot || polys > dyn.polyListMax || vertices > dyn.vtxListMax)
                return false;
        }
    }
    float dx = preview.x - root.x, dz = preview.z - root.z;
    if (std::hypot(dx, dz) < std::hypot(d.width, d.length) / 2 + 20)
        return false;
    Vec3f a{ root.x, root.y + 30, root.z }, b{ preview.x, preview.y + d.height / 2, preview.z }, hit;
    CollisionPoly* poly = nullptr;
    int bgId;
    return !BgCheck_EntityLineTest1(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bgId);
}
static void SyncBody(int i) {
    if (objects.slots[i]) {
        auto profile = Definitions()[(int)objects.slots[i]->type].profile;
        if (profile == Profile::Quarter || profile == Profile::Mini || profile == Profile::MiniReverse)
            vertDirty = true;
    }
    if (!actors[i] || !objects.slots[i])
        return;
    auto& e = *objects.slots[i];
    actors[i]->actor.world.pos = { e.pose.x, e.pose.y, e.pose.z };
    auto angle = [](float x) { return (s16)std::lround(x * 32768 / 3.14159265359f); };
    actors[i]->actor.shape.rot = { angle(e.pose.pitch), angle(e.pose.yaw), angle(e.pose.roll) };
    actors[i]->actor.world.rot = actors[i]->actor.shape.rot;
}
static bool PropReady(Type type);
static bool Place(PlayState* play) {
    auto& d = Definitions()[(int)selected];
    valid = Validate(play);
    if (!valid) {
        Feedback("INVALID PLACEMENT");
        return false;
    }
    if (picked) {
        int i = objects.Find(picked);
        if (i < 0) {
            picked = 0;
            return false;
        }
        auto& e = *objects.slots[i];
        e.pose = preview;
        e.vx = e.vy = e.vz = 0;
        e.asleep = false;
        SyncBody(i);
        SetCollision(picked, true);
        picked = 0;
        Feedback("OBJECT PLACED");
        return true;
    }
    int slot = objects.Add(selected, preview);
    if (slot < 0) {
        Feedback("OBJECT LIMIT REACHED");
        return false;
    }
    auto& entry = *objects.slots[slot];
    if (d.collision == Collision::StaticSolid) {
        auto actor = std::make_unique<DynaPolyActor>();
        DynaPolyActor_Init(actor.get(), 0);
        actor->actor.id = -1;
        actor->actor.update = BodyUpdate;
        actor->actor.category = ACTORCAT_BG;
        actor->actor.room = room;
        actor->actor.scale = { 1, 1, 1 };
        actor->actor.world.pos = { preview.x, preview.y, preview.z };
        actor->actor.shape.rot.y = (s16)std::lround(preview.yaw * 32768 / 3.14159265359f);
        actor->actor.world.rot = actor->actor.shape.rot;
        int bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &actor->actor, &shapes[(int)selected].header);
        if (bgId < 0 || bgId >= BG_ACTOR_MAX) {
            objects.Remove(entry.id);
            Feedback("COLLISION SLOTS FULL");
            return false;
        }
        actor->bgId = bgId;
        entry.collisionHandle = bgId;
        actors[slot] = std::move(actor);
        SyncBody(slot);
        if (changed)
            changed(&actors[slot]->actor);
    }
    SPDLOG_INFO("[ObjectDropper] OBJECT_PLACE id={} type={} scene={} room={} pos={} {} {} yaw={} collision={}",
                entry.id, d.id, scene, room, preview.x, preview.y, preview.z, preview.yaw, entry.collisionHandle);
    Feedback("OBJECT PLACED");
    return true;
}
static float FloorAt(PlayState* play, Vec3f position, Actor* ignore) {
    CollisionPoly* poly = nullptr;
    int bg;
    float floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, ignore, &position);
    return std::isfinite(floor) && floor > BGCHECK_Y_MIN && poly && poly->normal.y > 1000 ? floor : BGCHECK_Y_MIN;
}
static void Snap(PlayState* play) {
    Vec3f probe{ preview.x, std::max(root.y, preview.y) + 200, preview.z };
    float floor = FloorAt(play, probe, GET_PLAYER(play) ? &GET_PLAYER(play)->actor : nullptr);
    if (floor > BGCHECK_Y_MIN) {
        float bottom = 0;
        for (auto v : shapes[(int)selected].mesh.vertices)
            bottom = std::min(bottom, NativeSkateObjectEditor::Rotate(v, preview).y);
        preview.y = floor - bottom;
    } else
        Feedback("NO FLOOR");
}
struct EntryList {
    std::array<int, kMaxObjects> data{};
    int count = 0;
    void push_back(int x) {
        data[count++] = x;
    }
    bool empty() const {
        return count == 0;
    }
    int size() const {
        return count;
    }
    int operator[](int i) const {
        return data[i];
    }
};
static EntryList Entries() {
    EntryList out;
    if (category == Category::Placed) {
        for (int i = 0; i < kMaxObjects; ++i)
            if (objects.slots[i])
                out.push_back(i);
    } else
        for (int i = 0; i < kTypeCount; ++i)
            if (Definitions()[i].category == category)
                out.push_back(i);
    return out;
}
static void ChangeCategory(int direction) {
    int c = (int)category;
    do {
        c = (c + direction + 7) % 7;
    } while (c == (int)Category::Debug &&
             !(CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0) &&
               CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.DebugLibrary"), 0)));
    category = (Category)c;
    item = 0;
}
static void Choose(PlayState* play) {
    auto list = Entries();
    if (list.empty()) {
        Feedback("NO OBJECTS");
        return;
    }
    item = std::clamp(item, 0, (int)list.size() - 1);
    CancelPick();
    if (category == Category::Placed) {
        auto& e = *objects.slots[list[item]];
        selected = e.type;
        preview = e.pose;
        manualHeightActive = true;
        picked = e.id;
        SetCollision(picked, false);
    } else {
        manualHeightActive = false;
        selected = (Type)list[item];
        float yaw = GET_PLAYER(play)->actor.shape.rot.y * 3.14159265359f / 32768;
        float distance =
            std::max(Tuning(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.Distance"), 110.f, 60.f, 300.f),
                     std::hypot(Definitions()[(int)selected].width, Definitions()[(int)selected].length) / 2 + 21);
        preview = { root.x + std::sin(yaw) * distance, root.y, root.z + std::cos(yaw) * distance, yaw };
        Snap(play);
    }
    stage = Stage::Manipulating;
    Feedback("POSITION OBJECT");
}
static bool Attach(Id id) {
    int i = objects.Find(id);
    if (i < 0 || !markerKnown) {
        Feedback("SET SESSION MARKER FIRST");
        return false;
    }
    objects.slots[i]->attached = true;
    objects.slots[i]->markerPose = id == picked ? preview : objects.slots[i]->pose;
    Feedback("ATTACHED TO MARKER");
    SPDLOG_INFO("[ObjectDropper] OBJECT_MARKER_ATTACH id={}", id);
    return true;
}
void SessionMarkerSet(bool interact) {
    markerKnown = true;
    for (auto& e : objects.slots)
        if (e && e->attached)
            e->markerPose = e->pose;
    if (interact && bound == gPlayState) {
        Id nearest = 0;
        float best = 10000;
        auto* p = GET_PLAYER(bound);
        if (p)
            for (auto& e : objects.slots)
                if (e) {
                    float dx = e->pose.x - p->actor.world.pos.x, dz = e->pose.z - p->actor.world.pos.z;
                    float d = dx * dx + dz * dz;
                    if (d < best && std::abs(e->pose.y - p->actor.world.pos.y) < 80) {
                        best = d;
                        nearest = e->id;
                    }
                }
        if (nearest)
            Attach(nearest);
    }
}
static void ReleaseProp();
void RestoreMarkerObjects() {
    for (int i = 0; i < kMaxObjects; ++i)
        if (objects.slots[i] && objects.slots[i]->attached) {
            if (objects.slots[i]->id == carried)
                ReleaseProp();
            auto& e = *objects.slots[i];
            e.pose = e.markerPose;
            e.vx = e.vy = e.vz = 0;
            e.asleep = false;
            SyncBody(i);
            SPDLOG_INFO("[ObjectDropper] OBJECT_MARKER_RESTORE id={}", e.id);
        }
}
const std::vector<NativeSkateVert::Ramp>& VertRamps() {
    if (!vertDirty)
        return vertRamps;
    vertDirty = false;
    vertRamps.clear();
    for (const auto& e : objects.slots)
        if (e && e->id != picked) {
            const auto& d = Definitions()[(int)e->type];
            if (d.profile != Profile::Quarter && d.profile != Profile::Mini && d.profile != Profile::MiniReverse)
                continue;
            NativeSkateVert::Ramp ramp;
            ramp.id = e->id;
            ramp.width = d.width;
            ramp.right = NativeSkateObjectEditor::Rotate({ d.profile == Profile::Mini ? 1.f : -1.f, 0, 0 }, e->pose);
            ramp.lipTangent = NativeSkateObjectEditor::Rotate({ 0, 1, 0 }, e->pose);
            int count = d.profile == Profile::Quarter ? ((int)e->type >= 23 ? 13 : 9) : 13;
            ramp.count = count;
            for (int j = 0; j < count; ++j) {
                int source = d.profile == Profile::Mini ? count - 1 - j : j;
                auto v = shapes[(int)e->type].mesh.vertices[source * 2];
                v.x = 0;
                auto q = NativeSkateObjectEditor::Rotate(v, e->pose);
                ramp.points[j] = { q.x + e->pose.x, q.y + e->pose.y, q.z + e->pose.z };
            }
            vertRamps.push_back(ramp);
        }
    return vertRamps;
}
bool CarryingProp() {
    return carried != 0;
}
bool PropInputOwned() {
    return propGrabConsumed;
}
static void ReleaseProp() {
    int i = objects.Find(carried);
    if (i >= 0) {
        auto& e = *objects.slots[i];
        e.vx = e.vy = e.vz = 0;
        e.asleep = false;
        SetCollision(carried, true);
        if (changed && actors[i])
            changed(&actors[i]->actor);
        SPDLOG_INFO("[ObjectDropper] PROP_RELEASE id={}", carried);
    }
    carried = 0;
}
// Called after native polling, before packet routing. No riding R1 interception.
bool BipedPropInput(PlayState* play, bool biped, bool swordDrawn) {
    bool held = play && ((NativeSkateInput::Current().buttons & 0x0200) || (play->state.input[0].cur.button & BTN_R));
    bool fresh = held && !propGrabWasHeld;
    propGrabWasHeld = held;
    bool context = biped && !swordDrawn && !editing && Safe(play) && play->activeCamera == CAM_ID_MAIN &&
                   play == bound && scene == play->sceneNum && room == play->roomCtx.curRoom.num;
    if (carried && (!context || !held))
        ReleaseProp();
    if (!held) {
        propGrabConsumed = false;
        return false;
    }
    if (!context)
        return propGrabConsumed;
    if (fresh && !carried) {
        auto* p = GET_PLAYER(play);
        float yaw = p->actor.shape.rot.y * 3.14159265359f / 32768;
        float best = 120;
        Id nearest = 0;
        for (const auto& e : objects.slots)
            if (e && Definitions()[(int)e->type].behavior == Behavior::Movable) {
                float dx = e->pose.x - p->actor.world.pos.x, dz = e->pose.z - p->actor.world.pos.z,
                      dist = std::hypot(dx, dz);
                if (dist < best && std::abs(e->pose.y - p->actor.world.pos.y) < 65 &&
                    (dist < 20 || (dx * std::sin(yaw) + dz * std::cos(yaw)) / dist > .35f)) {
                    best = dist;
                    nearest = e->id;
                }
            }
        if (nearest) {
            carried = nearest;
            propGrabConsumed = true;
            SetCollision(carried, false);
            int i = objects.Find(carried);
            auto& e = *objects.slots[i];
            e.vx = e.vy = e.vz = 0;
            e.asleep = true;
            if (changed && actors[i])
                changed(&actors[i]->actor);
            SPDLOG_INFO("[ObjectDropper] PROP_GRAB id={}", carried);
        }
    }
    return propGrabConsumed;
}
static void Dynamics(PlayState* play, float dt) {
    auto* p = GET_PLAYER(play);
    Vec3f motion{};
    if (previousPlayerValid) {
        motion = { p->actor.world.pos.x - previousPlayer.x, 0, p->actor.world.pos.z - previousPlayer.z };
    }
    previousPlayer = p->actor.world.pos;
    previousPlayerValid = true;
    if (carried) {
        int i = objects.Find(carried);
        if (i >= 0) {
            auto& e = *objects.slots[i];
            const auto& d = Definitions()[(int)e.type];
            float yaw = p->actor.shape.rot.y * 3.14159265359f / 32768;
            Pose local = e.pose;
            local.yaw = 0;
            float back = 0, bottom = 0;
            for (auto v : shapes[(int)e.type].mesh.vertices) {
                auto q = NativeSkateObjectEditor::Rotate(v, local);
                back = std::min(back, q.z);
                bottom = std::min(bottom, q.y);
            }
            float radius = 22.f - back;
            Pose next = e.pose;
            next.x = p->actor.world.pos.x + std::sin(yaw) * radius;
            next.z = p->actor.world.pos.z + std::cos(yaw) * radius;
            next.y = p->actor.world.pos.y + 30.f - bottom;
            next.yaw = yaw;
            Vec3f from{ p->actor.world.pos.x, next.y + d.height / 2, p->actor.world.pos.z },
                to{ next.x, next.y + d.height / 2, next.z }, hit;
            CollisionPoly* poly = nullptr;
            int bg;
            bool blocked = BgCheck_EntityLineTest2(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true,
                                                   &bg, actors[i] ? &actors[i]->actor : nullptr);
            if (Valid(next) && !blocked) {
                e.pose = next;
                SyncBody(i);
            }
        } else
            carried = 0;
    }
    dynamicsAccumulator = std::min(dynamicsAccumulator + dt, .0666667);
    bool moved = std::hypot(motion.x, motion.z) > 0.05f && std::hypot(motion.x, motion.z) < 50;
    while (dynamicsAccumulator + 1e-8 >= 1. / 60) {
        dynamicsAccumulator -= 1. / 60;
        for (int i = 0; i < kMaxObjects; ++i)
            if (objects.slots[i] && Definitions()[(int)objects.slots[i]->type].behavior == Behavior::Movable &&
                objects.slots[i]->id != picked && objects.slots[i]->id != carried) {
                auto& e = *objects.slots[i];
                auto& d = Definitions()[(int)e.type];
                if (!editing && moved && e.asleep &&
                    std::hypot(e.pose.x - p->actor.world.pos.x, e.pose.z - p->actor.world.pos.z) <
                        std::hypot(d.width, d.length) / 2 + 22 &&
                    std::abs(e.pose.y - p->actor.world.pos.y) < d.height + 25) {
                    float scale = 60 / std::max(1.f, std::hypot(motion.x, motion.z));
                    e.vx = std::clamp(motion.x * scale, -60.f, 60.f);
                    e.vz = std::clamp(motion.z * scale, -60.f, 60.f);
                    e.asleep = false;
                }
                if (e.asleep)
                    continue;
                Pose next = e.pose;
                next.x += e.vx / 60;
                next.z += e.vz / 60;
                Vec3f from{ e.pose.x, e.pose.y + d.height / 2, e.pose.z }, to{ next.x, next.y + d.height / 2, next.z },
                    hit;
                CollisionPoly* poly = nullptr;
                int bg;
                if ((e.vx || e.vz) &&
                    BgCheck_EntityLineTest2(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bg,
                                            actors[i] ? &actors[i]->actor : nullptr)) {
                    next.x = e.pose.x;
                    next.z = e.pose.z;
                    e.vx = e.vz = 0;
                }
                e.vy = std::max(-120.f, e.vy - 180.f / 60);
                next.y += e.vy / 60;
                float base = BGCHECK_Y_MIN;
                for (auto v : shapes[(int)e.type].mesh.vertices) {
                    auto q = NativeSkateObjectEditor::Rotate(v, next);
                    Vec3f probe{ next.x + q.x, e.pose.y + q.y + 2, next.z + q.z };
                    float floor = FloorAt(play, probe, actors[i] ? &actors[i]->actor : nullptr);
                    if (floor > BGCHECK_Y_MIN && floor - q.y <= e.pose.y + 2)
                        base = std::max(base, floor - q.y);
                }
                if (base > BGCHECK_Y_MIN && next.y <= base) {
                    next.y = base;
                    e.vy = 0;
                    e.vx *= .9f;
                    e.vz *= .9f;
                    e.asleep = std::hypot(e.vx, e.vz) < .05f;
                }
                if (Valid(next)) {
                    e.pose = next;
                    SyncBody(i);
                } else {
                    e.vx = e.vy = e.vz = 0;
                    e.asleep = true;
                }
            }
        moved = false;
    }
}
static void UpdateCamera(PlayState* play, float dt, const Input& input, bool readInput = true) {
    if (!ownedCamera || cameraId == CAM_ID_NONE || !Valid(preview))
        return;
    if (readInput)
        editCamera.Read(stickOwnership.BlocksCamera() ? 0.f : input.cur.right_stick_x / 80.f,
                        stickOwnership.BlocksCamera() ? 0.f : input.cur.right_stick_y / 80.f,
                        stage == Stage::Manipulating
                            ? ((input.cur.button & BTN_DRIGHT ? 1 : 0) - (input.cur.button & BTN_DLEFT ? 1 : 0))
                            : 0,
                        dt);
    Vec3f at{ preview.x, preview.y + Definitions()[(int)selected].height / 2, preview.z };
    auto point = editCamera.Eye({ at.x, at.y, at.z });
    Vec3f eye{ point.x, point.y, point.z }, hit;
    CollisionPoly* poly = nullptr;
    int bg;
    int i = objects.Find(picked);
    if (BgCheck_EntityLineTest2(&play->colCtx, &at, &eye, &hit, &poly, true, true, true, true, &bg,
                                i >= 0 && actors[i] ? &actors[i]->actor : nullptr)) {
        Vec3f delta{ hit.x - at.x, hit.y - at.y, hit.z - at.z };
        float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (length > 16) {
            float factor = (length - 8) / length;
            eye = { at.x + delta.x * factor, at.y + delta.y * factor, at.z + delta.z * factor };
        } else {
            auto previousEye = ownedCamera->eye;
            float dx = previousEye.x - at.x, dy = previousEye.y - at.y, dz = previousEye.z - at.z;
            if (std::isfinite(dx) && std::isfinite(dy) && std::isfinite(dz) && dx * dx + dy * dy + dz * dz > 256)
                eye = previousEye;
            else {
                float ex = eye.x - at.x, ey = eye.y - at.y, ez = eye.z - at.z;
                float distance = std::max(1.f, std::sqrt(ex * ex + ey * ey + ez * ez));
                eye = { at.x + ex * 24 / distance, at.y + ey * 24 / distance, at.z + ez * 24 / distance };
            }
        }
    }
    Play_CameraSetAtEye(play, cameraId, &at, &eye);
    Play_CameraSetFov(play, cameraId, 60.f);
}
bool Update(PlayState* play, bool nativeOwned) {
    if (!play)
        return false;
    if (bound && (bound != play || scene != play->sceneNum)) {
        Clear("OBJECT_SCENE_CLEANUP", true);
    } else if (bound && room != play->roomCtx.curRoom.num)
        Clear("OBJECT_ROOM_CLEANUP");
    if (!nativeOwned) {
        if (carried)
            ReleaseProp();
        propGrabConsumed = false;
    }
    double now = NativeSkateInput::Now();
    float dt = (float)std::clamp(now - dynamicsTime, 0., .05);
    dynamicsTime = now;
    if (play->pauseCtx.state) {
        lastTime = now;
        return editing;
    }
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->gameOverCtx.state || gSaveContext.health <= 0) {
        if (editing || objects.Count())
            Clear("OBJECT_TEARDOWN_CLEANUP");
        return false;
    }
    if (!Safe(play)) {
        if (editing)
            Exit("WORLD_INTERACTION");
        return false;
    }
    if (!bound) {
        bound = play;
        scene = play->sceneNum;
        room = play->roomCtx.curRoom.num;
        dt = 0;
    }
    Dynamics(play, dt);
    if (releaseGate && nativeOwned) {
        auto raw = NativeSkateInput::Current();
        if (raw.buttons || play->state.input[0].cur.button || raw.triggers[0] > 20 || raw.triggers[1] > 20)
            return true;
        releaseGate = false;
    }
    if (!CVarGetInteger(kEnabled, 0)) {
        if (editing) {
            Exit("MENU_EXIT");
            releaseGate = nativeOwned;
            return nativeOwned;
        }
        return false;
    }
    if (!editing)
        Enter(play, nativeOwned);
    if (!editing)
        return false;
    if (lastFrame == play->gameplayFrames)
        return editing;
    lastFrame = play->gameplayFrames;
    auto& input = play->state.input[0];
    uint16_t held = input.cur.button, pressed = (held & ~previous) | input.press.button;
    previous = held;
    auto secondary = NativeSkateInput::Current();
    uint16_t secondaryFresh = secondary.buttons & ~previousSecondary;
    previousSecondary = secondary.buttons;
    if (!(secondary.buttons & NativeSkateObjectEditor::kRecallAction)) {
        deleteEligible = deleteFired = false;
        deleteTime = 0;
    }
    dt = (float)std::clamp(now - lastTime, 0., .05);
    lastTime = now;
    auto* player = GET_PLAYER(play);
    player->actor.world.pos = player->actor.prevPos = player->actor.home.pos = root;
    player->actor.velocity = { 0, 0, 0 };
    player->actor.speedXZ = player->linearVelocity = 0;
    const auto rotationTriggers = NativeSkateInput::ReadDropperTriggers();
    const bool l2RotateActive = stage == Stage::Manipulating && rotationTriggers.leftHeld;
    const bool r2RotateActive = stage == Stage::Manipulating && rotationTriggers.rightHeld;
    bool modifier = l2RotateActive || r2RotateActive;
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.InputDebug"), 0)) {
        static int leftBucket = -1, rightBucket = -1, percent = -1;
        static bool leftHeld = false, rightHeld = false;
        int l = rotationTriggers.leftRaw / 1024, r = rotationTriggers.rightRaw / 1024;
        if (l != leftBucket || r != rightBucket || percent != rotationTriggers.thresholdPercent ||
            leftHeld != rotationTriggers.leftHeld || rightHeld != rotationTriggers.rightHeld) {
            SPDLOG_INFO(
                "[DropperInput] L2 raw={} R2 raw={} engineThresholdPercent={} L2 logicalHeld={} R2 logicalHeld={} rotation={} {}",
                rotationTriggers.leftRaw, rotationTriggers.rightRaw, rotationTriggers.thresholdPercent,
                rotationTriggers.leftHeld, rotationTriggers.rightHeld, l2RotateActive, r2RotateActive);
            leftBucket = l;
            rightBucket = r;
            percent = rotationTriggers.thresholdPercent;
            leftHeld = rotationTriggers.leftHeld;
            rightHeld = rotationTriggers.rightHeld;
        }
    }
    stickOwnership.Read(modifier, input.cur.right_stick_x / 80.f, input.cur.right_stick_y / 80.f);
    UpdateCamera(play, dt, input);
    // Stock C-button mappings can be generated by the same right stick used by this editor.
    if (std::hypot(input.cur.right_stick_x / 80.f, input.cur.right_stick_y / 80.f) > .15f)
        pressed &= ~(BTN_CLEFT | BTN_CUP | BTN_CRIGHT | BTN_CDOWN);
    if (pressed & BTN_B) {
        if (stage == Stage::Manipulating) {
            CancelPick();
            stage = Stage::Browser;
        } else if (stage == Stage::Browser)
            stage = Stage::Root;
        else {
            Exit("BROWSER_BACK");
            releaseGate = nativeOwned;
            input.cur.button = input.press.button = input.rel.button = 0;
            return true;
        }
    } else if (stage != Stage::Manipulating) {
        if (stage == Stage::Root) {
            if (pressed & BTN_DUP)
                ChangeCategory(-1);
            if (pressed & BTN_DDOWN)
                ChangeCategory(1);
            if (pressed & BTN_A)
                stage = Stage::Browser;
            if (pressed & BTN_CLEFT) {
                Remove(objects.Last(), "OBJECT_UNDO");
                Feedback("UNDO");
            }
            if (pressed & BTN_CRIGHT) {
                for (auto& e : objects.slots)
                    if (e)
                        Remove(e->id, "OBJECT_CLEAR");
                Feedback("OBJECTS CLEARED");
            }
        } else {
            if (pressed & BTN_DLEFT)
                ChangeCategory(-1);
            if (pressed & BTN_DRIGHT)
                ChangeCategory(1);
            auto list = Entries();
            if (!list.empty()) {
                if (pressed & BTN_DUP)
                    item = (item + (int)list.size() - 1) % list.size();
                if (pressed & BTN_DDOWN)
                    item = (item + 1) % list.size();
            }
            if (category == Category::Placed && (secondaryFresh & NativeSkateObjectEditor::kDeleteAction) &&
                !list.empty()) {
                item = std::clamp(item, 0, (int)list.size() - 1);
                Id id = objects.slots[list[item]]->id;
                Remove(id, "OBJECT_BROWSER_DELETE");
                auto remaining = Entries();
                item = remaining.empty() ? 0 : std::min(item, remaining.size() - 1);
                Feedback(remaining.empty() ? "No Placed Objects" : "OBJECT DELETED");
            } else if (pressed & BTN_A)
                Choose(play);
            if ((pressed & BTN_L) && objects.Last()) {
                int i = objects.Find(objects.Last());
                selected = objects.slots[i]->type;
                preview = objects.slots[i]->pose;
                manualHeightActive = true;
                stage = Stage::Manipulating;
            }
        }
    } else {
        if (ownedCamera)
            movementBasis.Read({ ownedCamera->eye.x, ownedCamera->eye.y, ownedCamera->eye.z },
                               { ownedCamera->at.x, ownedCamera->at.y, ownedCamera->at.z });
        const bool pitchModifier = r2RotateActive, yawModifier = l2RotateActive;
        float speed = NativeSkateObjectEditor::kTranslationMultiplier *
                      Tuning(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.MoveSpeed"),
                             NativeSkateObjectEditor::kMoveSpeed, 20.f, 200.f);
        float x = input.cur.stick_x / 80.f, y = input.cur.stick_y / 80.f;
        // Y-X-Z: positive local X pitch lowers +Z front, so stick-up uses negative pitch.
        if (pitchModifier)
            NativeSkateObjectEditor::Turn(preview, -input.cur.right_stick_y / 80.f, 1, dt);
        if (yawModifier)
            NativeSkateObjectEditor::Turn(preview, input.cur.right_stick_x / 80.f, 0, dt);
        movementBasis.Move(preview, x, y, speed, dt);
        if (held & (BTN_DUP | BTN_DDOWN))
            manualHeightActive = true;
        if (held & BTN_DUP)
            preview.y += speed * dt;
        if (held & BTN_DDOWN)
            preview.y -= speed * dt;
        preview.y = std::clamp(preview.y, -27000.f, 27000.f);
        if (((pressed & BTN_CLEFT) && !manualHeightActive && !modifier) ||
            (secondaryFresh & NativeSkateObjectEditor::kSnapAction)) {
            preview.pitch = preview.roll = 0;
            Snap(play);
            manualHeightActive = false;
        }
        if (pressed & BTN_L) {
            CancelPick();
            manualHeightActive = true;
            Feedback("DUPLICATE CANDIDATE");
        }
        bool recall = (secondary.buttons & NativeSkateObjectEditor::kRecallAction) != 0;
        if (!recall) {
            deleteTime = 0;
            deleteEligible = deleteFired = false;
        } else {
            if (secondaryFresh & NativeSkateObjectEditor::kRecallAction)
                deleteEligible = picked != 0;
            if (deleteEligible && !deleteFired)
                deleteTime += dt;
        }
        bool deleteCommand =
            (pressed & BTN_CUP) || (recall && deleteEligible && !deleteFired && deleteTime >= kRecallHoldSeconds);
        if (deleteCommand) {
            deleteFired = true;
            if (picked) {
                Id id = picked;
                picked = 0;
                Remove(id, "OBJECT_DELETE");
                stage = Stage::Browser;
            } else
                Feedback("SELECT A PLACED OBJECT");
        }
        if (pressed & BTN_CRIGHT) {
            if (picked)
                Attach(picked);
            else
                Feedback("PLACE OBJECT BEFORE ATTACH");
        }
        if (pressed & BTN_A) {
            valid = Validate(play);
            if (Place(play))
                stage = Stage::Browser;
        }
    }
    if (stage == Stage::Manipulating) {
        valid = Validate(play);
        if (Valid(preview)) {
            lastRenderablePreview = preview;
            haveRenderablePreview = true;
        }
    }
    if (stage != uiStage) {
        uiStage = stage;
        uiChanged = now;
    }
    UpdateCamera(play, dt, input, false);
    input.cur.button = input.press.button = input.rel.button = 0;
    input.cur.stick_x = input.cur.stick_y = input.cur.right_stick_x = input.cur.right_stick_y = input.rel.stick_x =
        input.rel.stick_y = 0;
    return true;
}
static std::array<std::shared_ptr<Ship::IResource>, kTypeCount> propResources;
static std::array<bool, kTypeCount> propAttempted{};
static bool PropReady(Type type) {
    int i = (int)type;
    const auto& d = Definitions()[i];
    if (!d.resource)
        return false;
    if (!propAttempted[i]) {
        propAttempted[i] = true;
        propResources[i] = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(d.resource, true);
        if (!std::dynamic_pointer_cast<Fast::DisplayList>(propResources[i])) {
            propResources[i].reset();
            SPDLOG_ERROR("[SkateProps] Missing display list {}", d.resource);
        }
    }
    return propResources[i] != nullptr;
}
static void DrawMesh(PlayState* play, const Shape& shape, bool ghost, int type) {
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    // Preserve the textured opaque pass below the red translucent geometry overlay.
    bool red = ghost && !valid;
    if (red && Definitions()[type].resource && type != (int)Type::Vhs)
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_DECAL, G_RM_AA_ZB_XLU_DECAL2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0,
                    ghost       ? (valid ? 60 : 255)
                    : type == 0 ? 60
                                : 160,
                    red     ? 25
                    : ghost ? 190
                            : 160,
                    red     ? 20
                    : ghost ? 255
                            : 80,
                    red     ? 145
                    : ghost ? 85
                            : 255);
    // Batch collision hull overlay, including curved props exceeding the 32-vertex cache.
    for (size_t first = 0; first < shape.mesh.triangles.size(); first += 10) {
        size_t count = std::min(size_t(10), shape.mesh.triangles.size() - first);
        auto* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, count * 3 * sizeof(Vtx));
        if (!v)
            return;
        for (size_t j = 0; j < count; ++j)
            for (int k = 0; k < 3; ++k) {
                auto p = shape.vertices[shape.mesh.triangles[first + j][k]];
                v[j * 3 + k] = {};
                v[j * 3 + k].v.ob[0] = p.x;
                v[j * 3 + k].v.ob[1] = p.y;
                v[j * 3 + k].v.ob[2] = p.z;
                v[j * 3 + k].v.cn[3] = 255;
            }
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, (int)count * 3, 0);
        for (int j = 0; j < count; ++j)
            gSP1Triangle(POLY_XLU_DISP++, j * 3, j * 3 + 1, j * 3 + 2, 0);
    }
}
static void DrawProp(PlayState* play, Type type, bool ghost) {
    if (!PropReady(type)) {
        if (ghost)
            DrawMesh(play, shapes[(int)type], true, (int)type);
        return;
    }
    int i = (int)type;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    auto dl = std::dynamic_pointer_cast<Fast::DisplayList>(propResources[i]);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl->GetPointer());
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (ghost)
        DrawMesh(play, shapes[i], true, i);
}
static void DrawObject(PlayState* play, Type type, Pose pose, bool ghost, Id id) {
    ::FrameInterpolation_RecordOpenChild(&objects, (int)id);
    Matrix_Push();
    Matrix_Translate(pose.x, pose.y, pose.z, MTXMODE_NEW);
    Matrix_RotateY(pose.yaw, MTXMODE_APPLY);
    Matrix_RotateX(pose.pitch, MTXMODE_APPLY);
    Matrix_RotateZ(pose.roll, MTXMODE_APPLY);
    if (type == Type::Vhs) {
        if (SkateHarkinian_VhsObjectReady()) {
            float scale = Definitions()[(int)type].scale;
            Matrix_Translate(0, Definitions()[(int)type].visualYOffset, 0, MTXMODE_APPLY);
            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
            SkateHarkinian_DrawVhsObject(play);
        }
        if (ghost) {
            float inverse = 1 / Definitions()[(int)type].scale;
            Matrix_Scale(inverse, inverse, inverse, MTXMODE_APPLY);
            DrawMesh(play, shapes[(int)type], true, (int)type);
        }
    } else if (Definitions()[(int)type].resource)
        DrawProp(play, type, ghost);
    else
        DrawMesh(play, shapes[(int)type], ghost, (int)type);
    Matrix_Pop();
    ::FrameInterpolation_RecordCloseChild();
}
static void EditorText(GfxPrint* p, int x, int y, const char* text) {
    GfxPrint_SetPosPx(p, 0, y);
    for (const char* c = text; *c; ++c) {
        Gfx* start = p->dList;
        GfxPrint_Printf(p, "%c", *c);
        for (Gfx* q = start; q + 2 < p->dList; ++q) {
            if ((q->words.w0 >> 24) == G_TEXRECT) {
                const auto a = q->words.w0, b = q->words.w1, st = (q + 1)->words.w1, dt = (q + 2)->words.w1;
                int xl = ((b >> 12) & 4095) + x * 4, yl = b & 4095, xh = ((a >> 12) & 4095) + x * 4, yh = a & 4095;
                Gfx* out = q;
                gSPWideTextureRectangle(out++, xl, yl, xh, yh, (b >> 24) & 7, st >> 16, st & 65535, dt >> 16,
                                        dt & 65535);
                q += 2;
            }
        }
    }
}
void Draw(PlayState* play) {
    if (!bound || bound != play || scene != play->sceneNum || room != play->roomCtx.curRoom.num)
        return;
    for (auto& e : objects.slots)
        if (e && e->id != picked)
            DrawObject(play, e->type, e->pose, false, e->id);
    if (!editing || !Safe(play))
        return;
    if (stage == Stage::Manipulating) {
        if (!Valid(preview) && !haveRenderablePreview)
            return;
        DrawObject(play, selected, Valid(preview) ? preview : lastRenderablePreview, true, 0);
    }
    if (!CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.ShowHUD"), 1))
        return;
    {
        GraphicsContext* __gfxCtx = play->state.gfxCtx;
        Gfx* opa = POLY_OPA_DISP;
        Gfx* gfx = Graph_GfxPlusOne(opa);
        gSPDisplayList(OVERLAY_DISP++, gfx);
        GfxPrint p;
        GfxPrint_Init(&p);
        GfxPrint_Open(&p, gfx);
        constexpr int kEditorHudBottom = 186, kEditorHudLeft = 16, kEditorLineHeight = 9;
        const int left = (int)OTRGetDimensionFromLeftEdge((float)kEditorHudLeft);
        float fade = (float)std::clamp((NativeSkateInput::Now() - uiChanged) / .15, 0., 1.);
        auto line = [&](int row, const char* text, bool accent = false) {
            int y = kEditorHudBottom + row * kEditorLineHeight;
            if (NativeSkateHud::FontReady()) {
                NativeSkateHud::Overlay(text, 16, float(y), accent ? 8.5f : 7.5f, accent ? 90 : 235, accent ? 215 : 235,
                                        accent ? 245 : 235, (int)(235 * fade));
                return;
            }
            GfxPrint_SetColor(&p, 0, 0, 0, (u8)(180 * fade));
            EditorText(&p, left + 1, y + 1, text);
            GfxPrint_SetColor(&p, accent ? 90 : 235, accent ? 215 : 235, accent ? 245 : 235, (u8)(235 * fade));
            EditorText(&p, left, y, text);
        };
        if (stage == Stage::Root) {
            line(-3, "Object Dropper", true);
            line(-2, NativeSkateObjectEditor::Name(category));
            line(-1, "Category / Browse / Back");
            line(0, "Undo / Clear");
        } else if (stage == Stage::Browser) {
            line(-3, NativeSkateObjectEditor::Name(category), true);
            auto list = Entries();
            if (list.empty())
                line(-2, category == Category::Placed ? "No Placed Objects" : "No objects yet");
            else {
                int index = std::clamp(item, 0, (int)list.size() - 1);
                const char* name = category == Category::Placed
                                       ? Definitions()[(int)objects.slots[list[index]]->type].name
                                       : Definitions()[list[index]].name;
                line(-2, name);
                char count[24];
                std::snprintf(count, sizeof(count), category == Category::Placed ? "%d/%d  X/Square Delete" : "%d / %d",
                              index + 1, (int)list.size());
                line(-1, count);
            }
            line(0, "Prev / Select / Next / Back");
        } else {
            line(-3, Definitions()[(int)selected].name, true);
            line(-2, "RT+RS: Tilt / LT+RS: Turn");
            line(-1, picked ? "Drop / Copy / Delete / Back" : "Drop / Duplicate / Back");
            if (picked) {
                int i = objects.Find(picked);
                line(0, i >= 0 && objects.slots[i]->attached ? "MARKER ATTACHED" : "Attach to Marker");
            }
        }
        if (NativeSkateInput::Now() < noticeUntil)
            line(-4, notice, true);
        gfx = GfxPrint_Close(&p);
        GfxPrint_Destroy(&p);
        gSPEndDisplayList(gfx++);
        Graph_BranchDlist(opa, gfx);
        POLY_OPA_DISP = gfx;
    }
}
uint64_t GrindOwner(const Actor* actor) {
    for (int i = 0; i < kMaxObjects; ++i)
        if (actors[i] && &actors[i]->actor == actor && objects.slots[i])
            return objects.slots[i]->id;
    return 0;
}
bool LocalGrinds(Actor* actor, std::vector<std::vector<std::array<float, 3>>>& paths) {
    for (int i = 0; i < kMaxObjects; ++i)
        if (actors[i] && &actors[i]->actor == actor && objects.slots[i]) {
            for (const auto& path : GrindPaths(objects.slots[i]->type)) {
                std::vector<std::array<float, 3>> points;
                for (auto p : path)
                    points.push_back({ p.x / NativeSkateUnits::kOotUnitsPerMeter,
                                       p.y / NativeSkateUnits::kOotUnitsPerMeter,
                                       p.z / NativeSkateUnits::kOotUnitsPerMeter });
                paths.push_back(std::move(points));
            }
            return true;
        }
    return false;
}
bool RidingSurface(const Actor* actor) {
    for (int i = 0; i < kMaxObjects; ++i)
        if (actors[i] && &actors[i]->actor == actor && objects.slots[i]) {
            const auto& d = Definitions()[(int)objects.slots[i]->type];
            return d.behavior == Behavior::SkateSurface ||
                   (d.behavior == Behavior::Movable &&
                    (d.profile == Profile::Kicker || d.profile == Profile::Bank || d.profile == Profile::Pad));
        }
    return false;
}
int AudioMaterial(Actor* actor) {
    for (int i = 0; i < kMaxObjects; ++i)
        if (actors[i] && &actors[i]->actor == actor && objects.slots[i])
            return (int)Definitions()[(int)objects.slots[i]->type].audio;
    return 0;
}
void Initialize(std::function<void(Actor*)> notify) {
    changed = std::move(notify);
    CVarSetInteger(kEnabled, 0);
    for (int i = 0; i < kTypeCount; ++i) {
        auto& s = shapes[i];
        s.mesh = Geometry((Type)i);
        for (auto v : s.mesh.vertices)
            s.vertices.push_back({ (s16)v.x, (s16)v.y, (s16)v.z });
        for (auto& t : s.mesh.triangles) {
            auto a = s.mesh.vertices[t[0]], b = s.mesh.vertices[t[1]], c = s.mesh.vertices[t[2]];
            float x = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
                  y = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
                  z = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x), len = std::sqrt(x * x + y * y + z * z);
            CollisionPoly poly{};
            for (int n = 0; n < 3; ++n)
                poly.vtxData[n] = t[n];
            poly.normal = { (s16)std::lround(x / len * 32767), (s16)std::lround(y / len * 32767),
                            (s16)std::lround(z / len * 32767) };
            poly.dist = (s16)std::lround(-(x * a.x + y * a.y + z * a.z) / len);
            s.polygons.push_back(poly);
        }
        auto& h = s.header;
        h.minBounds = { 32767, 32767, 32767 };
        h.maxBounds = { -32767, -32767, -32767 };
        for (auto v : s.vertices) {
            h.minBounds.x = std::min(h.minBounds.x, v.x);
            h.minBounds.y = std::min(h.minBounds.y, v.y);
            h.minBounds.z = std::min(h.minBounds.z, v.z);
            h.maxBounds.x = std::max(h.maxBounds.x, v.x);
            h.maxBounds.y = std::max(h.maxBounds.y, v.y);
            h.maxBounds.z = std::max(h.maxBounds.z, v.z);
        }
        h.numVertices = (u16)s.vertices.size();
        h.vtxList = s.vertices.data();
        h.numPolygons = (u16)s.polygons.size();
        h.polyList = s.polygons.data();
        h.surfaceTypeList = &s.surface;
        h.cameraDataList = &s.camera;
        h.cameraDataListLen = 1;
    }
}
} // namespace NativeSkateObjectDropper

extern "C" int NativeSkateDropperBlocksStockCamera(PlayState* play) {
    using namespace NativeSkateObjectDropper;
    // Stock camera runs independently of Player/editor update. Never give it editor stick samples.
    return editing && bound == play && ownedCamera != nullptr;
}
