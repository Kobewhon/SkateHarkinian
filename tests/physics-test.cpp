#include "NativeSkateUnits.h"
#include <array>
#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <chrono>
#include "NativeSkateObjectRegistry.h"
#include "NativeSkateObjectDropper.h"
#include "NativeSkateObjectEditor.h"
using s16 = int16_t;
using u16 = uint16_t;
using u32 = uint32_t;
struct Vec3f {
    float x = 0, y = 0, z = 0;
};
struct Vec3s {
    s16 x = 0, y = 0, z = 0;
};
struct PlayState;
struct Actor;
using ActorFunc = void (*)(Actor*, PlayState*);
struct Actor {
    int id = 0, category = 0, room = 0;
    Vec3f scale;
    struct {
        Vec3f pos;
        Vec3s rot;
    } world, home;
    struct {
        Vec3s rot;
    } shape;
    Vec3f velocity, prevPos;
    float speedXZ = 0;
    ActorFunc update = nullptr;
};
struct Player {
    Actor actor;
    float linearVelocity = 0;
};
struct DynaPolyActor {
    Actor actor;
    int bgId = -1;
};
struct CollisionPoly {
    uint16_t vtxData[3]{};
    Vec3s normal;
    int16_t dist = 0;
};
struct SurfaceType {};
struct CamData {};
struct CollisionHeader {
    Vec3s minBounds, maxBounds;
    u16 numVertices = 0;
    Vec3s* vtxList = nullptr;
    u16 numPolygons = 0;
    CollisionPoly* polyList = nullptr;
    SurfaceType* surfaceTypeList = nullptr;
    CamData* cameraDataList = nullptr;
    size_t cameraDataListLen = 0;
};
constexpr int BG_ACTOR_MAX = 50, ACTORCAT_BG = 1, TRANS_TRIGGER_OFF = 0;
constexpr float BGCHECK_Y_MIN = -32000;
constexpr uint16_t BTN_A = 0x8000, BTN_B = 0x4000, BTN_Z = 0x2000, BTN_L = 0x20, BTN_R = 0x10, BTN_DUP = 0x800,
                   BTN_DDOWN = 0x400, BTN_DLEFT = 0x200, BTN_DRIGHT = 0x100, BTN_CLEFT = 2, BTN_CRIGHT = 1, BTN_CUP = 8;
struct Dyna {
    struct Bg {
        Actor* actor = nullptr;
        CollisionHeader* colHeader = nullptr;
    } bgActors[50];
    uint16_t bgActorFlags[50]{};
    int polyListMax = 2000, vtxListMax = 2000;
};
struct CollisionContext {
    Dyna dyna;
};
struct Input {
    struct Data {
        uint16_t button = 0;
        int stick_x = 0, stick_y = 0, right_stick_x = 0, right_stick_y = 0;
    } cur, press, rel;
};
struct Camera {
    int status = 0;
    Vec3f at, eye;
};
struct PlayState {
    Camera cameras[4];
    int activeCamera = 0;
    Player player;
    int sceneNum = 1;
    struct {
        int state = 0;
    } pauseCtx, gameOverCtx;
    int transitionTrigger = 0;
    struct {
        int msgMode = 0;
    } msgCtx;
    struct {
        struct {
            int num = 0;
        } curRoom;
    } roomCtx;
    uint32_t gameplayFrames = 0;
    struct {
        Input input[1];
    } state;
    CollisionContext colCtx;
};
static PlayState* gPlayState = nullptr;
struct {
    int health = 48;
} gSaveContext;
#define GET_PLAYER(p) (&(p)->player)
#define CVAR_ENHANCEMENT(s) s
#define SPDLOG_INFO(...) ((void)0)
static bool enabled = false;
static double clockNow = 0;
static int deletes = 0, notifications = 0;
static bool wall = false, asset = true;
static int terrain = 0;
int CVarGetInteger(const char* name, int fallback) {
    return std::string(name).find("Enabled") != std::string::npos ? enabled : fallback;
}
void CVarSetInteger(const char*, int n) {
    enabled = n != 0;
}
float CVarGetFloat(const char*, float f) {
    return f;
}
namespace NativeSkateInput {
struct Packet {
    uint32_t buttons = 0;
    uint8_t triggers[2]{};
};
static Packet packet;
Packet Current() {
    return packet;
}
struct DropperTriggerState {
    int leftRaw = 0, rightRaw = 0, thresholdPercent = 25;
    bool leftHeld = false, rightHeld = false;
};
DropperTriggerState ReadDropperTriggers() {
    DropperTriggerState q;
    q.leftRaw = packet.triggers[0] * 32767 / 255;
    q.rightRaw = packet.triggers[1] * 32767 / 255;
    q.leftHeld = q.leftRaw > 32767 * .25f;
    q.rightHeld = q.rightRaw > 32767 * .25f;
    return q;
}
double Now() {
    return clockNow;
}
void Reset(double) {
}
} // namespace NativeSkateInput
bool Player_InCsMode(PlayState*) {
    return false;
}
void GameInteractor_ExecuteOnPlayerUpdate() {
}
void DynaPolyActor_Init(DynaPolyActor* a, int) {
    a->bgId = -1;
}
int DynaPoly_SetBgActor(PlayState*, Dyna* d, Actor* a, CollisionHeader* h) {
    for (int i = 0; i < 50; ++i)
        if (!(d->bgActorFlags[i] & 1)) {
            d->bgActorFlags[i] = 1;
            d->bgActors[i] = { a, h };
            return i;
        }
    return 50;
}
void DynaPoly_DeleteBgActor(PlayState*, Dyna* d, int i) {
    assert(i >= 0 && i < 50);
    assert(d->bgActors[i].actor);
    d->bgActors[i].actor = nullptr;
    d->bgActorFlags[i] |= 2;
    ++deletes;
}
bool BgCheck_EntityLineTest1(CollisionContext*, Vec3f*, Vec3f*, Vec3f*, CollisionPoly**, bool, bool, bool, bool, int*) {
    return wall;
}
float BgCheck_EntityRaycastFloor5(PlayState*, CollisionContext* c, CollisionPoly** p, int* bg, Actor* ignore,
                                  Vec3f* probe) {
    static CollisionPoly floor;
    floor.normal.y = 32767;
    *p = &floor;
    *bg = 50;
    float height = terrain == 1 ? (probe->x >= 0 ? 20.f : 0.f) : terrain == 2 ? probe->x * .1f : 0.f;
    for (int i = 0; i < 50; ++i) {
        auto& a = c->dyna.bgActors[i];
        if (!(c->dyna.bgActorFlags[i] & 1) || (c->dyna.bgActorFlags[i] & (2 | 4)) || a.actor == ignore || !a.actor ||
            !a.colHeader)
            continue;
        float minX = 1e6, maxX = -1e6, minZ = 1e6, maxZ = -1e6, top = 0;
        for (int v = 0; v < a.colHeader->numVertices; ++v) {
            auto q = a.colHeader->vtxList[v];
            minX = std::min(minX, float(q.x));
            maxX = std::max(maxX, float(q.x));
            minZ = std::min(minZ, float(q.z));
            maxZ = std::max(maxZ, float(q.z));
            top = std::max(top, float(q.y));
        }
        if (probe->x >= a.actor->world.pos.x + minX && probe->x <= a.actor->world.pos.x + maxX &&
            probe->z >= a.actor->world.pos.z + minZ && probe->z <= a.actor->world.pos.z + maxZ &&
            probe->y > a.actor->world.pos.y + top)
            height = std::max(height, a.actor->world.pos.y + top);
    }
    return height;
}
int SkateHarkinian_VhsObjectReady() {
    return asset;
}

constexpr int CAM_ID_NONE = -1, CAM_ID_MAIN = 0, CAM_STAT_WAIT = 1, CAM_STAT_ACTIVE = 2, CAM_SET_FREE0 = 0;
static int cameraCount = 0, cameraDeletes = 0;
#define GET_ACTIVE_CAM(p) (&(p)->cameras[(p)->activeCamera])
Camera* Play_GetCamera(PlayState* p, int id) {
    return id >= 0 && id < 4 ? &p->cameras[id] : nullptr;
}
int Play_CreateSubCamera(PlayState*) {
    ++cameraCount;
    return 1;
}
void Play_ClearCamera(PlayState* p, int) {
    ++cameraDeletes;
    p->activeCamera = 0;
}
void Play_ChangeCameraStatus(PlayState* p, int id, int status) {
    p->cameras[id].status = status;
    if (status == CAM_STAT_ACTIVE)
        p->activeCamera = id;
}
void Camera_RequestSetting(Camera*, int) {
}
void Play_CameraSetAtEye(PlayState* p, int id, Vec3f* at, Vec3f* eye) {
    p->cameras[id].at = *at;
    p->cameras[id].eye = *eye;
}
void Play_CameraSetFov(PlayState*, int, float) {
}
void func_8003EBF8(PlayState*, Dyna* d, int i) {
    d->bgActorFlags[i] |= 4;
}
void func_8003EC50(PlayState*, Dyna* d, int i) {
    d->bgActorFlags[i] &= ~4;
}
bool BgCheck_EntityLineTest2(CollisionContext* c, Vec3f* a, Vec3f* b, Vec3f* h, CollisionPoly** p, bool x, bool y,
                             bool z, bool w, int* bg, Actor*) {
    return BgCheck_EntityLineTest1(c, a, b, h, p, x, y, z, w, bg);
}
constexpr uint16_t BTN_CDOWN = 4;
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
                Feedback(remaining.empty() ? "NO PLACED OBJECTS" : "OBJECT DELETED");
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
static bool PropReady(Type) {
    return true;
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

using namespace NativeSkateObjectDropper;
static int fps = 60;
void Sweep(PlayState& p) {
    for (int i = 0; i < 50; ++i)
        if (p.colCtx.dyna.bgActorFlags[i] & 2) {
            p.colCtx.dyna.bgActorFlags[i] = 0;
            p.colCtx.dyna.bgActors[i] = {};
        }
}
void Frame(PlayState& p, uint16_t buttons = 0, bool own = true) {
    ++p.gameplayFrames;
    clockNow += 1. / fps;
    p.state.input[0].cur.button = buttons;
    p.state.input[0].press.button = 0;
    Update(&p, own);
    Sweep(p);
}
void Select(PlayState& p, Category c) {
    category = c;
    item = c == Category::Debug ? 1 : 0;
    stage = Stage::Browser;
    Frame(p);
    Frame(p, BTN_A);
    assert(stage == Stage::Manipulating);
}

int main() {
    PlayState p;
    gPlayState = &p;
    Initialize([](Actor*) { ++notifications; });
    Sweep(p);
    for (Type type :
         { Type::PhysicsKickerSmall, Type::PhysicsKickerMedium, Type::PhysicsBankSmall, Type::PhysicsManualLow }) {
        enabled = true;
        Frame(p);
        selected = type;
        preview = { 0, 180, 200, 0 };
        assert(Place(&p));
        Id id = objects.Last();
        Exit("PHYSICS");
        for (int i = 0; i < 600; ++i)
            Frame(p);
        auto& e = *objects.slots[objects.Find(id)];
        assert(std::abs(e.pose.y) < .01 && e.asleep);
        assert(GrindPaths(type).empty());
        p.player.actor.world.pos = { 0, 0, 110 };
        p.player.actor.shape.rot.y = 0;
        p.state.input[0].cur.button = BTN_R;
        assert(!BipedPropInput(&p, false, false));
        p.state.input[0].cur.button = 0;
        BipedPropInput(&p, false, false);
        p.state.input[0].cur.button = BTN_R;
        assert(!BipedPropInput(&p, true, true));
        p.state.input[0].cur.button = 0;
        BipedPropInput(&p, true, false);
        p.activeCamera = 1;
        p.state.input[0].cur.button = BTN_R;
        assert(!BipedPropInput(&p, true, false));
        p.state.input[0].cur.button = 0;
        BipedPropInput(&p, true, false);
        p.activeCamera = 0;
        p.pauseCtx.state = 1;
        p.state.input[0].cur.button = BTN_R;
        assert(!BipedPropInput(&p, true, false));
        p.state.input[0].cur.button = 0;
        BipedPropInput(&p, true, false);
        p.pauseCtx.state = 0;
        for (int cycle = 0; cycle < 100; ++cycle) {
            p.player.actor.world.pos = { e.pose.x, 0, e.pose.z - 80 };
            p.state.input[0].cur.button = BTN_R;
            assert(BipedPropInput(&p, true, false) && CarryingProp());
            assert(p.colCtx.dyna.bgActorFlags[e.collisionHandle] & 4);
            Pose before = e.pose;
            wall = true;
            Dynamics(&p, 1.f / 60);
            assert(e.pose.x == before.x && e.pose.y == before.y && e.pose.z == before.z);
            wall = false;
            NativeSkateInput::packet.buttons = 0x0200;
            p.state.input[0].cur.button = 0;
            assert(BipedPropInput(&p, true, false) && CarryingProp());
            float startX = e.pose.x;
            for (int i = 0; i < 60; ++i) {
                p.player.actor.world.pos.x += 1.f;
                assert(BipedPropInput(&p, true, false));
                Dynamics(&p, 1.f / 60);
                assert(Valid(e.pose) && e.asleep && e.vy == 0);
                int slot = objects.Find(id);
                assert(actors[slot]->actor.world.pos.x == e.pose.x && actors[slot]->actor.world.pos.y == e.pose.y &&
                       actors[slot]->actor.world.pos.z == e.pose.z);
            }
            assert(e.pose.x - startX > 50);
            NativeSkateInput::packet.buttons = 0;
            p.state.input[0].cur.button = 0;
            assert(!BipedPropInput(&p, true, false) && !CarryingProp());
            assert(!(p.colCtx.dyna.bgActorFlags[e.collisionHandle] & 4));
            for (int i = 0; i < 100; ++i)
                Dynamics(&p, 1.f / 60);
            assert(e.asleep && std::abs(e.pose.y) < .01);
        }
        p.player.actor.world.pos = { e.pose.x, 0, e.pose.z - 80 };
        p.state.input[0].cur.button = BTN_R;
        assert(BipedPropInput(&p, true, false));
        assert(PropInputOwned());
        assert(BipedPropInput(&p, false, false) && !CarryingProp() && PropInputOwned());
        p.state.input[0].cur.button = 0;
        assert(!BipedPropInput(&p, false, false) && !PropInputOwned());
        p.state.input[0].cur.button = BTN_R;
        assert(BipedPropInput(&p, true, false) && CarryingProp());
        Update(&p, false);
        assert(!CarryingProp() && !PropInputOwned() && !(p.colCtx.dyna.bgActorFlags[e.collisionHandle] & 4));
        p.state.input[0].cur.button = 0;
        BipedPropInput(&p, true, false);
        p.state.input[0].cur.button = BTN_R;
        assert(BipedPropInput(&p, true, false));
        Clear("F9");
        assert(!CarryingProp() && !objects.Count());
        Sweep(p);
        p.state.input[0].cur.button = 0;
    }
    // Twenty static/dynamic instances, sleeping bodies incur no collision transforms.
    enabled = true;
    Frame(p);
    for (int i = 0; i < 20; ++i) {
        selected = i % 2 ? Type::PhysicsKickerSmall : Type::ManualLow;
        preview = { float(i * 160), 100, 300, 0 };
        assert(Place(&p));
    }
    Exit("STRESS");
    for (int i = 0; i < 600; ++i)
        Dynamics(&p, 1.f / 60);
    for (const auto& e : objects.slots)
        if (e)
            assert(Definitions()[(int)e->type].behavior != Behavior::Movable || e->asleep);
    Clear("SCENE");
    assert(!objects.Count() && !CarryingProp());
    Sweep(p);
    enabled = true;
    Frame(p);
    for (int type : { 20, 21, 23, 24, 25, 26 }) {
        selected = (Type)type;
        preview = { float(type * 200), 40, 700, .785398f, .10f, 0 };
        assert(Place(&p));
    }
    Exit("VERT_METADATA");
    const auto& ramps = VertRamps();
    assert(ramps.size() == 6);
    for (const auto& r : ramps) {
        assert(r.id && r.count >= 9);
        auto c = NativeSkateVert::Along(r, 0);
        assert(c.normal.y > .95f);
        assert(c.length > 100);
        auto lip = NativeSkateVert::Along(r, c.length);
        assert(lip.tangent.y > .98f);
    }
    Clear("SCENE");
    assert(VertRamps().empty());
    Sweep(p);
    puts(
        "PASS 4 physics variants: gravity/settle/no grinds,400 carry-walk-release cycles, riding/sword exclusion, F9/scene cleanup,20 mixed props");
}
