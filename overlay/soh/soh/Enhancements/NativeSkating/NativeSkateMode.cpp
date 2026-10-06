#include "NativeSkateLegacyPop.h"
extern "C" {
struct Player;
void Player_PlayVoiceSfx(Player*, unsigned short);
}
#include "NativeSkateMode.h"
#include "NativeSkateAudio.h"
#include "NativeSkateActors.h"
#include "NativeSkateOwnership.h"
#include "NativeSkateInput.h"
#include "NativeSkateBuild.h"
#include "NativeSkateContextInput.h"
#include "NativeSkateSessionMarker.h"
#include "NativeSkateUtilityInput.h"
#include "NativeSkateProxy.h"
#include "NativeSkateLink.h"
#include "NativeSkateRetarget.h"
#include "NativeSkateBoardPresentation.h"
#include "NativeSkateRetargetMath.h"
#include "NativeSkatePresentationMath.h"
#include "NativeSkateProxyGeometry.h"
#include "NativeSkateRuntime.h"
#include "NativeSkateHud.h"
#include "NativeSkateObjectDropper.h"
#include "NativeSkateUnits.h"
#include "NativeSkateWorld.h"
#include "World/NativeSkateDynamic.h"
#include "World/NativeSkateGrindCompiler.h"
#include "World/NativeSkateGrindDebug.h"
#include "NativeSkateCache.h"
#include "NativeSkatePerf.h"
#include "World/NativeSkateSceneJobs.h"
#include <tuple>
#include <cstring>
#include "World/NativeSkateGeometry.h"
#include "World/NativeSkateSceneProfile.h"
#include "World/NativeSkateWorldDebug.h"
#include <SDL2/SDL.h>
#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <memory>
#include <string>
#include <deque>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <chrono>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
extern "C" {
float OTRGetDimensionFromLeftEdge(float v);
float OTRGetDimensionFromRightEdge(float v);
#include "global.h"
#include "z64.h"
extern PlayState* gPlayState;
s32 Player_UpdateNativeWorldInteractions(PlayState*, Player*, s32);
s32 Player_NativeCombatUpdate(PlayState*, Player*, f32);
void Player_NativeRegisterColliders(PlayState*, Player*);
void Player_NativeHandoff(PlayState*, Player*);
s32 Player_NativeWater(PlayState*, Player*);
s32 Player_NativeDryFloor(PlayState*, Player*);
s32 Player_NativeSwordDrawn(Player*);
s32 Player_NativeSwordUpdate(PlayState*, Player*, s32, s32, s32);
}

namespace {
NativeSkateSceneJobs::Compiler gGrindJob;
NativeSkateHud::State gHud;
NativeSkateVert::Controller gVert;
int gScoreRoom = -1;
std::optional<std::chrono::steady_clock::time_point> gReadyInputEdge;
NativeSkateGrindCompiler::Result gGrinds;
bool gGrindsReady = false, gGrindsUploaded = false, gGrindsEnabled = false;
double gGrindUploadMs = 0;
using GrindCacheKey = std::tuple<int, const void*, uint64_t>;
GrindCacheKey gGrindKey{};
NativeSkateCache::Bounded<GrindCacheKey, NativeSkateGrindCompiler::Result> gGrindCache;
size_t GrindBytes(const NativeSkateGrindCompiler::Result& r) {
    size_t n = sizeof(r) + r.edges.capacity() * sizeof(NativeSkateGrindCompiler::Edge) +
               r.splines.capacity() * sizeof(NativeSkateGrindCompiler::Spline);
    for (const auto& s : r.splines)
        n += s.points.capacity() * sizeof(NativeSkateGrindCompiler::Point);
    return n;
}
void BeginGrindCompile(int scene, const void* header, const std::vector<float>& triangles) {
    gVert.Reset();
    gGrindJob.Cancel();
    gGrindsReady = false;
    gGrindsUploaded = false;
    gGrinds = {};
    std::string error;
    auto overrides = NativeSkateGrindCompiler::LoadOverrides(
        NativeSkateRuntime::Session::ExecutableDirectory() + "\\native-skate-surfaces.json", scene, error);
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&](uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            hash ^= (value >> (i * 8)) & 255;
            hash *= 1099511628211ull;
        }
    };
    mix(2); // Compiler version. Cache identity includes geometry and parsed overrides.
    for (float f : triangles) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        mix(bits);
    }
    for (const auto& entry : overrides) {
        mix(entry.first);
        mix(entry.second.accept);
        mix(entry.second.reject);
        mix(entry.second.split);
        mix(entry.second.category);
        mix(entry.second.joinGroup);
    }
    gGrindKey = std::make_tuple(scene, header, hash);
    auto cached = gGrindCache.Find(gGrindKey);
    if (cached) {
        gGrinds = *cached;
        gGrindsReady = true;
        return;
    }
    gGrindJob.Submit(triangles, overrides);
    if (!error.empty())
        SPDLOG_ERROR("[NativeSkateGrinds] override rejected: {}", error);
}
struct PreparedScene {
    std::vector<float> flat;
    NativeSkateGeometry::Result adapted;
};
using PreparedKey = std::tuple<int, const void*, uint64_t, bool, int>;
NativeSkateCache::Bounded<PreparedKey, PreparedScene> gPreparedCache;
// Runtime data is supplied by the player and kept next to this executable.
const std::string& DataRoot() {
    static const std::string root = NativeSkateRuntime::Session::ExecutableDirectory() + "/skate-data/assets";
    return root;
}
bool DeveloperDiagnostics() {
    return CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0) != 0;
}

// Reserved provider outside OoT Dyna slot IDs (1..50).
constexpr uint32_t kKokiriWalkwayProvider = 4096;
static bool gWalkwayUploaded = false, gWalkwayEnabled = false;
struct State {
    bool enabled = false, requested = false, failed = false, wasF8 = false, f8Pressed = false, hostOwned = false;
    uint64_t updateTicks = 0, drawTicks = 0;
    bool r2Down = false;
    uint64_t r2PressTick = 0, r2PressCount = 0;
    bool prewarmReported = false;
    bool interactionSuspended = false;
    int sceneId = -1, interaction = 0;
    Player* player = nullptr;
    PlayState* play = nullptr;
    CollisionHeader* collision = nullptr;
    ActorFunc originalDraw = nullptr, originalUpdate = nullptr;
    float originalGravity = 0;
    std::string status = "DISABLED", error;
    NativeSkateRuntime::Session runtime;
    NativeSkateRuntime::Snapshot snapshot{};
    NativeSkateDynamic::World dynamic;
    NativeSkateActors::World actors;
    uint64_t lastActorBail = 0;
    NativeSkateWorld::Snapshot world;
    NativeSkateUnits::Anchor anchor;
    NativeSkateGeometry::Result adapted;
    NativeSkateSceneProfile::Profile profile;
    bool worldEnabled = false;
    float spatialScale = 1;
    int riderBodyProfile = -1;
    float actorPosition[3] = {}, boardPosition[3] = {};
    uint64_t updates = 0;
    uint64_t pushEvents = 0, applications = 0;
    bool previousNewPush = false, previousX = false;
    uint64_t hostUpdateBase = 0;
    uint64_t pushNumber = 0, pushEndTick = 0, pushApplicationStart = 0;
    float pushBefore = 0, pushPeak = 0;
    float lastPushDelta = 0, applicationBase = 0;
    std::string stopReason = "NONE";
    std::deque<std::string> trace;
    std::deque<std::string> landingRows;
    int landingAfter = 0;
    std::ofstream capture;
};
State g;
using OwnershipReason = NativeSkateOwnership::Reason;
using OwnershipMode = NativeSkateOwnership::Mode;
OwnershipMode gOwnershipMode = OwnershipMode::DISABLED;
OwnershipReason gOwnershipReason = OwnershipReason::STARTUP;
uint64_t gPlayerGeneration = 0, gBoundPlayerGeneration = 0, gSceneGeneration = 0, gBoundSceneGeneration = 0;
bool gRecoveryQueued = false, gWasF9 = false, gFullReload = false, gRecoveryBiped = false, gSessionInvalid = false;
double gFailureWindow = 0;
unsigned gFailureCount = 0;
unsigned gRecoveryAttempts = 0;
double gRecoveryStarted = 0, gRecoveryRetry = 0, gRecoveryTotal = 0, gRecoveryMax = 0;
uint64_t gRecoverySamples = 0;
std::string gRecoveryNotice;
double gRecoveryNoticeUntil = 0;
struct OwnershipRecord {
    double time;
    uint64_t tick, generation, sceneGeneration;
    const void* player;
    int scene, room, cs, transition;
    uint32_t flags;
    bool requested, owned, valid;
    float playerPos[3], nativePos[3], error;
    const void* contact;
};
struct BoardTransitionRecord {
    uint64_t tick;
    double time;
    uint32_t before, after;
    uint16_t buttons;
    uint8_t r2;
    bool edge, ok, requested, actual;
    float native[28];
};
std::array<BoardTransitionRecord, 480> gBoardTransitionRing{};
size_t gBoardTransitionHead = 0, gBoardTransitionCount = 0;
uint16_t gBoardTransitionButtons = 0;
void CaptureBoardTransition(const NativeSkateRuntime::Packet& packet, uint32_t before, bool ok) {
    auto& r = gBoardTransitionRing[gBoardTransitionHead];
    r = {};
    r.tick = g.snapshot.tick;
    r.time = NativeSkateInput::Now();
    r.before = before;
    r.after = g.snapshot.stateId;
    r.buttons = packet.buttons;
    r.r2 = packet.triggers[1];
    r.edge = (packet.buttons & 0x8000) && !(gBoardTransitionButtons & 0x8000);
    r.ok = ok;
    r.requested = g.requested;
    r.actual = g.hostOwned;
    std::copy_n(g.runtime.RiderDiagnostics() + 200, 28, r.native);
    gBoardTransitionButtons = packet.buttons;
    gBoardTransitionHead = (gBoardTransitionHead + 1) % 480;
    gBoardTransitionCount = std::min(size_t(480), gBoardTransitionCount + 1);
}
std::array<OwnershipRecord, 480> gOwnershipRing{};
size_t gOwnershipHead = 0, gOwnershipCount = 0;
double gOwnershipSample = 0;
void OwnershipTransition(OwnershipMode mode, OwnershipReason reason) {
    SPDLOG_INFO(
        "[NativeOwnership] {} -> {} reason={} requested={} enabled={} actual={} runtime={} PlayerGeneration={} SceneGeneration={}",
        NativeSkateOwnership::Name(gOwnershipMode), NativeSkateOwnership::Name(mode),
        NativeSkateOwnership::Name(reason), g.requested, CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Enabled"), 0),
        g.hostOwned, g.runtime.StateName(), gPlayerGeneration, gSceneGeneration);
    gOwnershipMode = mode;
    gOwnershipReason = reason;
}
void QueueRecovery(OwnershipReason reason);
void MonitorOwnership();

bool gMarkerCameraPending = false;
void CaptureMarkerCamera(PlayState* play, Player* player, NativeSkateSessionMarker::CameraPose& saved) {
    saved = {};
    if (!play || !player || NativeSkateObjectDropper::Active() || play->pauseCtx.state || play->msgCtx.msgMode ||
        Player_InCsMode(play) || play->activeCamera != CAM_ID_MAIN)
        return;
    auto* c = GET_ACTIVE_CAM(play);
    if (!c)
        return;
    float root[] = { player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z },
          at[] = { c->at.x, c->at.y, c->at.z }, eye[] = { c->eye.x, c->eye.y, c->eye.z };
    if (saved.Capture(root, at, eye)) {
        saved.fov = c->fov;
        saved.roll = c->roll;
        saved.manual = play->manualCamera;
        saved.zoomSetting = CVarGetInteger(CVAR_SETTING("FreeLook.MaxCameraDistance"), 0);
    }
}
NativeSkateSessionMarker::Saved gMarker;
NativeSkateUtility::Input gUtility;
NativeSkateUtility::Input::Commands gUtilityCommands;
bool gUtilityObjectInteract = false;
// Dry-land gameplay anchor is deliberately independent of marker/recovery/entry.
NativeSkateLegacyPop::State gLegacyPop;
bool gLegacyTriangleOwned = false, gLegacyAuthoredFs360 = false;
NativeSkateSessionMarker::Saved gDryLand;
NativeSkateContextInput::DryGate gDryGate;
NativeSkateContextInput::Action gSwordInput;
bool gSwordRequest = false, gLogicalAttack = false, gWaterPending = false;
bool gLogicalTap = false;
uint64_t gWaterTick = 0;
Vec3f gDryHost = {};
struct DrySample {
    NativeSkateSessionMarker::Saved saved;
    Vec3f position{};
    double time = 0;
    float clearance = 0, speed = 0;
};
std::array<DrySample, 32> gDryHistory{};
size_t gDryHistoryHead = 0, gDryHistoryCount = 0;
double gDryNextSample = 0, gDryHysteresisUntil = 0, gWaterLastReturn = -100;
Vec3f gWaterLastPosition{};
int gWaterRepeatCount = 0;
constexpr float kWaterRecoveryClearance = 120.f;
// Use current room WaterBox geometry, not texture guesses. High dry platforms
// above water remain valid; shoreline points need board-sized inland clearance.
float WaterClearance(PlayState* play, Vec3f position) {
    float clearance = 100000.f;
    auto* header = play->colCtx.colHeader;
    if (!header)
        return 0;
    for (unsigned i = 0; i < header->numWaterBoxes; ++i) {
        const auto& box = header->waterBoxes[i];
        unsigned room = (box.properties >> 13) & 0x3f;
        if ((room != (unsigned)play->roomCtx.curRoom.num && room != 0x3f) || (box.properties & 0x80000) ||
            position.y > box.ySurface + 20)
            continue;
        float dx = std::max({ float(box.xMin) - position.x, 0.f, position.x - float(box.xMin + box.xLength) });
        float dz = std::max({ float(box.zMin) - position.z, 0.f, position.z - float(box.zMin + box.zLength) });
        clearance = std::min(clearance, std::hypot(dx, dz));
    }
    // Query current engine water semantics too (including Zora's Domain's
    // special box) at a bounded footprint ring; no WaterBox pointers retained.
    for (int i = 0; i < 16; ++i) {
        float a = i * 6.28318530718f / 16;
        float y = position.y;
        WaterBox* box = nullptr;
        if (WaterBox_GetSurface1(play, &play->colCtx, position.x + std::cos(a) * kWaterRecoveryClearance,
                                 position.z + std::sin(a) * kWaterRecoveryClearance, &y, &box) &&
            y >= position.y - 20)
            clearance = 0;
    }
    return clearance;
}
void ClearDryHistory() {
    gDryHistoryHead = gDryHistoryCount = 0;
    gDryNextSample = gDryHysteresisUntil = 0;
    gWaterLastReturn = -100;
    gWaterRepeatCount = 0;
}

bool gMarkerLoaded = false;
std::string gMarkerNotice;
uint64_t gMarkerNoticeEnd = 0;
void MarkerNotice(const char* text) {
    gMarkerNotice = text;
    gMarkerNoticeEnd = g.snapshot.tick + 120;
    SPDLOG_INFO("[NativeSkate] {}", text);
}
uint32_t gStartupResetCount = 0;
bool gActivationTracePending = false;
double gExitPromptStarted = 0;
bool gPromptWasActive = false;
float gBoardVisual[16] = {}, gBoardBlendFrom[16] = {};
bool gHaveVisualBoard = false, gWasCarry = false;
NativeSkateBoardPresentation::Mode gBoardMode = NativeSkateBoardPresentation::Mode::PHYSICS_BOARD;
uint64_t gBoardBlendTick = 0;
bool gBoardBlendLocal = false;
float gPreviousBoardFrame[16] = {};

float gCameraForward[3] = { 0, 0, 1 };
float gMoveDot = 1;
bool gMoveDotValid = false, gWasReversed = false;
std::string gPreviousState;
void CaptureCamera(PlayState* play) {
    if (!play)
        return;
    Camera* camera = GET_ACTIVE_CAM(play);
    if (!camera)
        return;
    const float x = camera->at.x - camera->eye.x, z = camera->at.z - camera->eye.z;
    const float length = std::sqrt(x * x + z * z);
    if (std::isfinite(length) && length > .001f) {
        gCameraForward[0] = x / length;
        gCameraForward[1] = 0;
        gCameraForward[2] = z / length;
    }
}
constexpr const char* kDebugCvar = CVAR_ENHANCEMENT("NativeSkate.Debug");
constexpr const char* kEnabledCvar = CVAR_ENHANCEMENT("NativeSkate.Enabled");
void OnPlayerUpdate();
void AnimationTrace(const NativeSkateRuntime::Packet&);
void NativePlayerDraw(Actor* actor, PlayState* play);
NativeSkateLink::Frame LinkFrame() {
    const float heading =
        NativeSkateBoardPresentation::Biped(g.snapshot.stateId) ? g.runtime.BipedDiagnostics()[3] : g.snapshot.heading;
    return NativeSkateLink::Build(g.actorPosition, heading, g.spatialScale, LINK_IS_ADULT,
                                  g.player ? g.player->actor.scale.y : .01f);
}
bool ProxyRider() {
    return CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ProxyRider"), 0) != 0;
}
void NativePlayerUpdate(Actor* actor, PlayState* play) {
    // The active proxy has no OoT character locomotion/animation update.
    // Rust advances once through the same adapter tick as before.
    if (g.hostOwned && g.player && actor == &g.player->actor && play == gPlayState)
        OnPlayerUpdate();
}
bool OwnsCurrentPlayer() {
    return g.hostOwned && g.player && g.play && g.play == gPlayState && g.sceneId == g.play->sceneNum &&
           GET_PLAYER(g.play) == g.player;
}

void RestorePlayer(OwnershipReason reason = OwnershipReason::UNKNOWN_BUG, bool preservePosition = false) {
    // Pointer/generation validity, not scene number, determines whether the saved
    // callbacks can be restored. Destroy hook runs before the actor is freed.
    if (g.hostOwned && g.player && gPlayState && GET_PLAYER(gPlayState) == g.player) {
        // Keep the last authoritative position; never rewind to entry.
        if (!preservePosition && g.anchor.valid)
            g.anchor.Position(g.snapshot.trajectory, g.actorPosition);
        const auto frame = LinkFrame();
        // OoT uses a foot-origin actor when control returns, not native COM.
        if (!preservePosition)
            g.player->actor.world.pos = (!ProxyRider() && frame.valid)
                                            ? Vec3f{ frame.position[0], frame.position[1], frame.position[2] }
                                            : Vec3f{ g.actorPosition[0], g.actorPosition[1], g.actorPosition[2] };
        g.player->actor.prevPos = g.player->actor.world.pos;
        g.player->actor.home.pos = g.player->actor.world.pos;
        if (reason == OwnershipReason::USER_F8_DISABLE || reason == OwnershipReason::CONSOLE_DISABLE)
            Player_NativeHandoff(gPlayState, g.player);
        g.player->actor.draw = g.originalDraw;
        g.player->actor.update = g.originalUpdate;
        g.player->actor.gravity = g.originalGravity;
        g.player->actor.velocity = { 0, 0, 0 };
        g.player->actor.speedXZ = 0;
        g.player->linearVelocity = 0;
        g.player->pushedSpeed = 0;
        g.player->yaw = g.player->actor.shape.rot.y;
        g.player->actor.world.rot.y = g.player->actor.shape.rot.y;
        SPDLOG_INFO("[NativeSkate] ownership -> OOT; restored update/draw/gravity at current native position");
    }
    if (g.hostOwned)
        OwnershipTransition(g.requested ? OwnershipMode::SUSPENDED : OwnershipMode::DISABLED, reason);
    NativeSkateLink::End();
    gHaveVisualBoard = false;
    gBoardBlendTick = 0;
    g.hostOwned = false;
    g.originalDraw = nullptr;
    g.originalUpdate = nullptr;
}
void Stop(const char* why, OwnershipReason reason = OwnershipReason::UNKNOWN_BUG, bool preservePosition = false) {
    gLegacyPop.Reset();
    gLegacyTriangleOwned = false;
    gLegacyAuthoredFs360 = false;
    gVert.Reset();
    NativeSkateAudio::Stop();
    gScoreRoom = -1;
    gDryLand.valid = false;
    gDryGate.Reset();
    ClearDryHistory();
    gSwordInput.Reset();
    gWaterPending = false;
    gSwordRequest = false;
    gLogicalTap = false;
    g.actors.Clear(g.runtime);
    gGrindJob.Cancel();
    gGrindsReady = false;
    gGrindsUploaded = false;
    gGrinds = {};
    g.actors.Clear(g.runtime);
    g.dynamic.Clear();
    if (reason == OwnershipReason::USER_F8_DISABLE || reason == OwnershipReason::CONSOLE_DISABLE)
        gHud.line.Exit(gHud.tick);
    if (g.runtime.Ready())
        g.runtime.ClearScore();
    gHud.Clear();
    auto disableBegin = std::chrono::steady_clock::now();
    if (g.capture.is_open())
        g.capture.close();
    if (g.runtime.Ready())
        g.runtime.Suspend();
    RestorePlayer(reason, preservePosition);
    g.interactionSuspended = false;
    g.interaction = 0;
    g.anchor.valid = false;
    g.enabled = false;
    g.failed = false;
    g.error.clear();
    g.status = g.runtime.Ready() ? "READY" : "DISABLED";
    SPDLOG_INFO("[NativeSkate] DISABLE_ms={}",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - disableBegin).count());
    if (why)
        SPDLOG_INFO("[NativeSkate] {}", why);
}
bool UpdateKokiriWalkway(Player* player, bool fresh = false) {
    if (g.world.kokiriWalkway.empty())
        return true;
    // Player world position is the authoritative feet/root placement. The
    // low slab starts at Y85; re-enable before an elevated board can land on it.
    const bool enabled = player->actor.world.pos.y >= 65.f;
    if (!fresh && gWalkwayUploaded && enabled == gWalkwayEnabled)
        return true;
    const std::array<float, 16> identity = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    if (!g.runtime.Dynamic(kKokiriWalkwayProvider, &g.world.kokiriWalkway, identity.data(), enabled))
        return false;
    gWalkwayUploaded = true;
    gWalkwayEnabled = enabled;
    if (DeveloperDiagnostics())
        SPDLOG_INFO("[KokiriWalkway] contact={} rootY={} triangles={}", enabled, player->actor.world.pos.y,
                    g.world.kokiriWalkway.size() / 9);
    return true;
}
bool Start(Player* player, PlayState* play) {
    if (!g.runtime.Ready()) {
        g.status = g.runtime.StateName();
        if (g.runtime.State() == NativeSkateRuntime::WarmState::FAILED) {
            g.error = g.runtime.Error();
            g.failed = true;
        }
        return false;
    }
    auto activationBegin = std::chrono::steady_clock::now();
    NativeSkatePerf::Scope activationTiming(NativeSkatePerf::Activation);
    g.status = "INITIALIZING";
    g.error.clear();
    if (!player || !play) {
        g.status = "WAITING_FOR_PLAYER";
        return false;
    }
    int reload = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ReloadSceneData"), 0);
    static int lastReload = 0;
    if (reload != lastReload) {
        lastReload = reload;
        g.dynamic.ClearResources();
        gGrindJob.Cancel();
        gGrindCache.Clear();
        gPreparedCache.Clear();
        NativeSkateWorld::ClearCache();
        NativeSkateSceneProfile::Invalidate();
        NativeSkateGrindCompiler::InvalidateOverrides();
    }
    if (!NativeSkateWorld::Capture(play, g.world)) {
        g.status = "WAITING_FOR_WORLD";
        g.error = g.world.error;
        return false;
    }
    g.profile = NativeSkateSceneProfile::Load(play->sceneNum, NativeSkateRuntime::Session::ExecutableDirectory());
    g.worldEnabled = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.World.Enabled"), 0) != 0;
    const auto preparedKey = std::make_tuple((int)play->sceneNum, (const void*)play->colCtx.colHeader, g.world.identity,
                                             g.worldEnabled, reload);
    if (const auto* hit = gPreparedCache.Find(preparedKey)) {
        g.adapted = hit->adapted;
        g.world.triangles = hit->flat;
    } else {
        g.adapted =
            NativeSkateGeometry::Adapt(g.world.triangles, { g.profile.stairSmoothing, g.profile.microTolerance },
                                       g.worldEnabled && g.profile.collision);
        if (g.worldEnabled && g.profile.collision)
            NativeSkateSceneProfile::Apply(g.profile, g.adapted);
        g.world.triangles = NativeSkateGeometry::Flatten(g.adapted);
        // Preserve Phase6 floating-point conversion exactly, once per prepared cache entry.
        for (size_t i = 0; i < g.world.triangles.size(); ++i) {
            float origin = g.profile.anchor[i % 3] / NativeSkateUnits::kOotUnitsPerMeter;
            g.world.triangles[i] = origin + (g.world.triangles[i] - origin) * 1.f;
        }
        gPreparedCache.Put(preparedKey, { g.world.triangles, g.adapted },
                           sizeof(PreparedScene) + g.world.triangles.capacity() * sizeof(float) +
                               g.adapted.triangles.capacity() * sizeof(NativeSkateGeometry::Triangle));
    }
    if (!g.profile.error.empty())
        SPDLOG_ERROR("[NativeSkateWorld] profile rejected: {}", g.profile.error);
    float requestedScale = CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.World.Scale"), g.profile.worldScale);
    if (!std::isfinite(requestedScale) || requestedScale < 1 || requestedScale > 1.33f)
        requestedScale = 1;
    g.spatialScale = 1; // CLEAN-2H: coherent OoT world; no experimental distance scaling.
    CVarSetFloat(CVAR_ENHANCEMENT("NativeSkate.World.Scale"), 1.f);
    NativeSkatePerf::metrics[NativeSkatePerf::StaticPrep].Add(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - activationBegin).count());
    SPDLOG_INFO("[NativeSkateWorld] STATIC_COLLISION_PREP={}ms triangles={}",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - activationBegin).count(),
                g.world.triangles.size() / 9);

    float spawn[3] = { (float)player->actor.world.pos.x, (float)player->actor.world.pos.y,
                       (float)player->actor.world.pos.z };
    float rustSpawn[3];
    if (!NativeSkateUnits::Anchor::ToRust(spawn, rustSpawn)) {
        g.error = "Invalid OoT spawn position";
        g.failed = true;
        g.status = "FALLBACK";
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        float origin = g.profile.anchor[i] / NativeSkateUnits::kOotUnitsPerMeter;
        rustSpawn[i] = origin + (rustSpawn[i] - origin) * g.spatialScale;
    }
    const float heading = (float)player->actor.shape.rot.y * (3.14159265358979323846f / 32768.0f);
    if (DeveloperDiagnostics() && CVarGetInteger(kDebugCvar, 0) != 0) {
        if (g.capture.is_open())
            g.capture.close();
        const std::string path = NativeSkateRuntime::Session::ExecutableDirectory() + "\\native-push-capture.bin";
        g.capture.open(path, std::ios::binary | std::ios::trunc);
        const uint32_t magic = 0x3150534e, count = (uint32_t)(g.world.triangles.size() / 9);
        g.capture.write((const char*)&magic, 4);
        g.capture.write((const char*)&count, 4);
        g.capture.write((const char*)rustSpawn, sizeof(rustSpawn));
        g.capture.write((const char*)&heading, 4);
        g.capture.write((const char*)g.world.triangles.data(), g.world.triangles.size() * sizeof(float));
        g.capture.flush();
        SPDLOG_INFO("[NativePush] replay capture={} open={} collisionTriangles={}", path, g.capture.is_open(), count);
    }
    if (!g.runtime.Activate(g.world.triangles, rustSpawn, heading, g.snapshot)) {
        g.error = g.runtime.Error();
        g.failed = true;
        g.status = "FALLBACK";
        SPDLOG_ERROR("[NativeSkate] INIT FAILED: {}", g.error);
        return false;
    }
    gWalkwayUploaded = false;
    if (!UpdateKokiriWalkway(player, true)) {
        g.error = g.runtime.Error();
        return false;
    }
    BeginGrindCompile(play->sceneNum, play->colCtx.colHeader, g.world.triangles);
    g.runtime.ClearScore();
    gHud.Clear();
    g.dynamic.Clear();
    if (!g.dynamic.Update(play, g.runtime)) {
        g.error = g.runtime.Error();
        g.failed = true;
        return false;
    }
    const int profile = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.RiderBodyProfile"), LINK_IS_ADULT ? 1 : 2);
    g.riderBodyProfile = profile;
    const float riderHeight =
        profile == 0 ? 0.f
                     : (profile == 2 ? NativeSkateRiderScale::kChildHeight : NativeSkateRiderScale::kAdultHeight) /
                           NativeSkateUnits::kOotUnitsPerMeter;
    if (!g.runtime.SetRiderHeight(riderHeight)) {
        g.error = g.runtime.Error();
        g.failed = true;
        g.status = "FALLBACK";
        g.runtime.Suspend();
        return false;
    }
    NativeSkateInput::Reset(NativeSkateInput::Now());
    const auto initial = NativeSkateInput::Current();
    if (g.capture.is_open()) {
        const uint32_t count = 1;
        g.capture.write((const char*)&count, 4);
        g.capture.write((const char*)&initial, sizeof(initial));
    }
    gCameraForward[0] = std::sin(heading);
    gCameraForward[1] = 0;
    gCameraForward[2] = std::cos(heading);
    CaptureCamera(play);
    if (!g.runtime.StepCamera(std::vector<NativeSkateRuntime::Packet>{ initial }, gCameraForward, g.snapshot)) {
        g.error = g.runtime.Error();
        g.failed = true;
        g.status = "FALLBACK";
        g.runtime.Suspend();
        SPDLOG_ERROR("[NativeSkate] FIRST TICK FAILED: {}", g.error);
        return false;
    }
    // Collision was built in world space. Never subtract initial COM height
    // from deck/rider outputs: anchor the shared coordinate frame to spawn.
    g.anchor.Set(rustSpawn, spawn);
    g.anchor.unitsPerMeter = NativeSkateUnits::kOotUnitsPerMeter / g.spatialScale;
    if (!g.anchor.valid) {
        g.error = "COM calibration failed";
        g.failed = true;
        g.status = "FALLBACK";
        g.runtime.Suspend();
        return false;
    }
    g.player = player;
    g.play = play;
    g.sceneId = play->sceneNum;
    g.interactionSuspended = false;
    g.collision = play->colCtx.colHeader;
    if (!g.anchor.Position(g.snapshot.trajectory, g.actorPosition) ||
        !g.anchor.Position(&g.snapshot.board[12], g.boardPosition)) {
        g.error = "Initial Rust transforms failed calibration";
        g.failed = true;
        g.status = "FALLBACK";
        g.runtime.Suspend();
        RestorePlayer(OwnershipReason::RUNTIME_FAILURE, true);
        return false;
    }
    if (!g.requested) {
        g.runtime.Suspend();
        g.status = "READY";
        g.enabled = false;
        return true;
    } // F9 OFF prepares, never acquires Player.
    g.originalDraw = player->actor.draw;
    g.originalUpdate = player->actor.update;
    g.originalGravity = player->actor.gravity;
    g.hostOwned = true;
    gBoundPlayerGeneration = gPlayerGeneration;
    gBoundSceneGeneration = gSceneGeneration;
    g.lastActorBail = 0;
    gDryLand.valid = false;
    gDryGate.Reset();
    gSwordInput.Reset();
    gSwordRequest = false;
    gWaterPending = false;
    gLogicalTap = false;
    OwnershipTransition(OwnershipMode::ACTIVE, gRecoveryQueued ? gOwnershipReason : OwnershipReason::RESUME);
    if (gReadyInputEdge) {
        SPDLOG_INFO(
            "[NativeSkatePerf] F8_EDGE_TO_OWNERSHIP={}ms",
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - *gReadyInputEdge).count());
        gReadyInputEdge.reset();
    }
    NativeSkateLink::Begin(play, player);
    player->actor.draw = ProxyRider() ? nullptr : NativePlayerDraw;
    player->actor.update = NativePlayerUpdate;
    g.status = "ACTIVE";
    g.enabled = true;
    g.updates = 0;
    g.pushEvents = 0;
    g.applications = 0;
    g.previousNewPush = false;
    g.previousX = false;
    g.applicationBase = g.runtime.PushDiagnostics()[1];
    g.lastPushDelta = 0;
    g.stopReason = "NONE";
    g.trace.clear();
    g.hostUpdateBase = g.updateTicks;
    g.pushNumber = 0;
    g.pushEndTick = 0;
    g.landingRows.clear();
    g.landingAfter = 0;
    gMoveDot = 1;
    gWasReversed = false;
    gMoveDotValid = false;
    gPreviousState.clear();
    SPDLOG_INFO("[NativeSkate] READY_TO_ACTIVE_ms={} {}",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - activationBegin).count(),
                g.runtime.Profile());
    SPDLOG_INFO("[NativeSkate] ownership -> RUST; OoT player update suspended");
    SPDLOG_INFO("[NativeSkate] SESSION ACTIVE ABI=4 dll={} data={} triangles={}", g.runtime.DllPath(), DataRoot(),
                g.world.accepted);
    return true;
}
void SetRequested(bool requested) {
    NativeSkateObjectDropper::Exit("F8_MODE_CHANGE");
    g.requested = requested;
    if (!requested) {
        gRecoveryQueued = false;
        gRecoveryAttempts = 0;
        Stop("activation requested OFF",
             g.f8Pressed ? OwnershipReason::USER_F8_DISABLE : OwnershipReason::CONSOLE_DISABLE, g.failed);
        OwnershipTransition(OwnershipMode::DISABLED,
                            g.f8Pressed ? OwnershipReason::USER_F8_DISABLE : OwnershipReason::CONSOLE_DISABLE);
        return;
    }
    if (gSessionInvalid || g.failed || g.runtime.State() == NativeSkateRuntime::WarmState::FAILED)
        QueueRecovery(OwnershipReason::SELF_HEAL);
    g.failed = false;
    g.error.clear();
    g.enabled = true;
    g.status = "WAITING_FOR_PLAYER";
    SPDLOG_INFO("[NativeSkate] activation requested ON");
}
bool gPaused = false;
void UpdatePause(PlayState* play) {
    const bool paused = play && play->pauseCtx.state != 0;
    if (!OwnsCurrentPlayer()) {
        gPaused = false;
        return;
    }
    if (paused == gPaused)
        return;
    gPaused = paused;
    if (paused)
        NativeSkateAudio::Stop();
    if (paused)
        NativeSkateInput::Pause(NativeSkateInput::Now());
    else {
        NativeSkateInput::Resume(NativeSkateInput::Now());
    }
    SPDLOG_INFO("[NativeSkate] PAUSE {} tick={} rider={} {} {} player={} {} {}; session preserved",
                paused ? "ENTER" : "EXIT", g.snapshot.tick, g.snapshot.trajectory[0], g.snapshot.trajectory[1],
                g.snapshot.trajectory[2], g.player->actor.world.pos.x, g.player->actor.world.pos.y,
                g.player->actor.world.pos.z);
}
void DumpOwnership() {
    for (size_t n = 0; n < gBoardTransitionCount; ++n) {
        const auto& r = gBoardTransitionRing[(gBoardTransitionHead + 480 - gBoardTransitionCount + n) % 480];
        SPDLOG_WARN(
            "[NativeBoardTransition] time={} tick={} state={}->{} buttons={} TriangleHeld={} TriangleEdge={} R2={} stepOK={} requested={} actual={} NewToggle={} Toggle={} NewDismount={} Dismount={} Mount={} MountRaw={} OBDismount={} LeftGrab={} RightGrab={} FS360={} Grabbing={} Catching={} boardState={} hand={} revertFrames={} revertRequest={} MGMount={} MGMountRaw={} MGDismount={} MGGrabbing={} MGCatching={}",
            r.time, r.tick, r.before, r.after, r.buttons, (r.buttons & 0x8000) != 0, r.edge, r.r2, r.ok, r.requested,
            r.actual, r.native[0], r.native[1], r.native[2], r.native[3], r.native[4], r.native[5], r.native[6],
            r.native[7], r.native[8], r.native[9], r.native[10], r.native[11], r.native[12], r.native[13], r.native[14],
            r.native[15], r.native[20], r.native[21], r.native[22], r.native[26], r.native[27]);
    }

    for (size_t n = 0; n < gOwnershipCount; ++n) {
        const auto& r = gOwnershipRing[(gOwnershipHead + 480 - gOwnershipCount + n) % 480];
        SPDLOG_WARN(
            "[NativeOwnershipRing] time={} tick={} scene={}/{} Player={} gen={}/{} requested={} actual={} valid={} player={} {} {} native={} {} {} error={} cs={} flags={} transition={} contact={}",
            r.time, r.tick, r.scene, r.room, r.player, r.generation, r.sceneGeneration, r.requested, r.owned, r.valid,
            r.playerPos[0], r.playerPos[1], r.playerPos[2], r.nativePos[0], r.nativePos[1], r.nativePos[2], r.error,
            r.cs, r.flags, r.transition, r.contact);
    }
}
void QueueRecovery(OwnershipReason reason) {
    NativeSkateObjectDropper::Exit("RECOVERY_QUEUED");
    if (gRecoveryQueued && reason != OwnershipReason::F9_RECOVERY)
        return;
    if (reason == OwnershipReason::F9_RECOVERY) {
        gFailureCount = 0;
        gFailureWindow = NativeSkateInput::Now();
    }
    if (reason == OwnershipReason::RUNTIME_FAILURE) {
        double now = NativeSkateInput::Now();
        if (now - gFailureWindow > 10) {
            gFailureWindow = now;
            gFailureCount = 0;
        }
        if (++gFailureCount > 3) {
            RestorePlayer(reason, true);
            gRecoveryQueued = false;
            g.failed = true;
            g.enabled = false;
            g.status = "FAILED";
            gRecoveryNotice = "NATIVE SKATE RELOAD FAILED";
            gRecoveryNoticeUntil = now + 4;
            OwnershipTransition(OwnershipMode::FAILED, reason);
            return;
        }
    }
    gRecoveryBiped = g.requested && NativeSkateBoardPresentation::Biped(g.snapshot.stateId);
    if (reason == OwnershipReason::F9_RECOVERY || gOwnershipMode != OwnershipMode::RECOVERING) {
        gRecoveryAttempts = 0;
        gRecoveryStarted = NativeSkateInput::Now();
    }
    if (reason != OwnershipReason::F9_RECOVERY || CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.OwnershipDebug"), 0))
        DumpOwnership();
    RestorePlayer(reason, true);
    gRecoveryQueued = true;
    gRecoveryRetry = 0;
    g.failed = false;
    g.enabled = g.requested;
    g.status = "RECOVERING";
    gRecoveryNotice = "NATIVE SKATE RELOADING...";
    gRecoveryNoticeUntil = NativeSkateInput::Now() + 60;
    OwnershipTransition(OwnershipMode::RECOVERING, reason);
}
bool UnsafeInteraction(PlayState* play, Player* player) {
    return !play || !player || play->transitionTrigger != TRANS_TRIGGER_OFF || play->gameOverCtx.state ||
           play->pauseCtx.state || play->msgCtx.msgMode || Player_InCsMode(play) || play->csCtx.state != CS_STATE_IDLE;
}
void RecoverAtCurrentPlayer() {
    PlayState* play = gPlayState;
    Player* player = play ? GET_PLAYER(play) : nullptr;
    if (!gRecoveryQueued || UnsafeInteraction(play, player) || NativeSkateInput::Now() < gRecoveryRetry)
        return;
    NativeSkateObjectDropper::Clear("OBJECT_F9_CLEANUP");
    // This is the one recovery path for F9, self-heal and unhealthy F8 ON.
    // Immutable process assets remain resident unless native reinitialization fails.
    gGrindJob.Cancel();
    gGrindsReady = gGrindsUploaded = false;
    gGrinds = {};
    g.actors.Clear(g.runtime);
    g.dynamic.Clear();
    gHud.Clear();
    g.lastActorBail = 0;
    if (g.runtime.State() == NativeSkateRuntime::WarmState::PREWARMING)
        return;
    if (gFullReload || g.runtime.State() == NativeSkateRuntime::WarmState::FAILED) {
        g.runtime.Stop();
        gFullReload = false;
        g.runtime.Prewarm(DataRoot());
        return;
    }
    if (!g.runtime.Ready()) {
        g.runtime.Prewarm(DataRoot());
        return;
    }
    const Vec3f current = player->actor.world.pos;
    const s16 yaw = player->actor.shape.rot.y;
    g.failed = false;
    g.enabled = true;
    g.anchor.valid = false;
    g.interactionSuspended = false;
    g.player = nullptr;
    g.play = nullptr;
    bool ok = Start(player, play);
    if (ok && g.requested && gRecoveryBiped) {
        NativeSkateRuntime::Marker stable;
        if (g.runtime.CaptureMarker(stable)) {
            stable.onboard = 0;
            ok = g.runtime.LoadMarker(stable, g.snapshot);
        } else {
            stable.version = 1;
            stable.onboard = 0;
            stable.frame[0] = stable.frame[5] = stable.frame[10] = stable.frame[15] = 1;
            const float angle = yaw * (3.14159265358979323846f / 32768.f);
            stable.frame[0] = stable.frame[10] = std::cos(angle);
            stable.frame[2] = -std::sin(angle);
            stable.frame[8] = std::sin(angle);
            stable.frame[12] = current.x / NativeSkateUnits::kOotUnitsPerMeter;
            stable.frame[13] = current.y / NativeSkateUnits::kOotUnitsPerMeter;
            stable.frame[14] = current.z / NativeSkateUnits::kOotUnitsPerMeter;
            ok = g.runtime.LoadMarker(stable, g.snapshot);
        }
        if (!ok)
            g.failed = true;
        else {
            g.runtime.ClearScore();
            NativeSkateInput::Reset(NativeSkateInput::Now());
        }
    }
    if (ok) {
        if (!g.requested) {
            Stop("F9 OFF remains ready", gOwnershipReason, true);
            player->actor.world.pos = current;
            player->actor.prevPos = current;
            player->actor.shape.rot.y = yaw;
        }
        gRecoveryQueued = false;
        gRecoveryAttempts = 0;
        g.failed = false;
        gSessionInvalid = false;
        const double ms = (NativeSkateInput::Now() - gRecoveryStarted) * 1000;
        gRecoveryTotal += ms;
        gRecoveryMax = std::max(gRecoveryMax, ms);
        ++gRecoverySamples;
        gRecoveryNotice = "NATIVE SKATE READY";
        gRecoveryNoticeUntil = NativeSkateInput::Now() + 2;
        OwnershipTransition(g.requested ? OwnershipMode::ACTIVE : OwnershipMode::DISABLED, gOwnershipReason);
        SPDLOG_INFO("[NativeRecovery] complete ms={} avg={} max={} samples={} requested={} player={} {} {}", ms,
                    gRecoveryTotal / gRecoverySamples, gRecoveryMax, gRecoverySamples, g.requested, current.x,
                    current.y, current.z);
    } else if (g.failed) {
        gSessionInvalid = true;
        RestorePlayer(OwnershipReason::RUNTIME_FAILURE, true);
        gFullReload = true;
        if (++gRecoveryAttempts >= 2) {
            gRecoveryQueued = false;
            g.enabled = false;
            g.failed = true;
            g.status = "FAILED";
            gRecoveryNotice = "NATIVE SKATE RELOAD FAILED";
            gRecoveryNoticeUntil = NativeSkateInput::Now() + 4;
            OwnershipTransition(OwnershipMode::FAILED, OwnershipReason::RUNTIME_FAILURE);
        } else
            gRecoveryRetry = NativeSkateInput::Now() + .25;
    }
}
void MonitorOwnership() {
    PlayState* play = gPlayState;
    Player* player = play ? GET_PLAYER(play) : nullptr;
    if (!player)
        return;
    if (g.requested && gSaveContext.health == 0) {
        CVarSetInteger(kEnabledCvar, 0);
        g.requested = false;
        gRecoveryQueued = false;
        Stop("stock death ownership policy", OwnershipReason::DEATH, true);
        return;
    }
    const double now = NativeSkateInput::Now();
    float error = 0;
    auto frame = LinkFrame();
    if (g.hostOwned && frame.valid)
        error = std::hypot(
            std::hypot(player->actor.world.pos.x - frame.position[0], player->actor.world.pos.z - frame.position[2]),
            player->actor.world.pos.y - frame.position[1]);
    if (now - gOwnershipSample >= 1. / 60.) {
        gOwnershipSample = now;
        auto& r = gOwnershipRing[gOwnershipHead];
        r = {};
        r.time = now;
        r.tick = g.snapshot.tick;
        r.player = player;
        r.generation = gPlayerGeneration;
        r.sceneGeneration = gSceneGeneration;
        r.scene = play->sceneNum;
        r.room = play->roomCtx.curRoom.num;
        r.cs = player->csAction;
        r.flags = player->stateFlags1;
        r.transition = play->transitionTrigger;
        r.requested = g.requested;
        r.owned = OwnsCurrentPlayer();
        r.valid = g.runtime.Ready() && !gSessionInvalid;
        r.error = error;
        r.contact = player->cylinder.base.oc;
        for (int a = 0; a < 3; ++a) {
            r.playerPos[a] = (&player->actor.world.pos.x)[a];
            r.nativePos[a] = g.actorPosition[a];
        }
        gOwnershipHead = (gOwnershipHead + 1) % 480;
        gOwnershipCount = std::min(size_t(480), gOwnershipCount + 1);
    }
    if (gRecoveryQueued) {
        RecoverAtCurrentPlayer();
        return;
    }
    NativeSkateOwnership::Observation o;
    o.requested = g.requested;
    o.owned = g.hostOwned;
    o.valid = g.runtime.Ready() && !gSessionInvalid;
    o.unsafe = UnsafeInteraction(play, player);
    o.paused = play->pauseCtx.state != 0;
    o.positionError = error;
    o.callbacks = g.enabled && g.status == "ACTIVE" && player->actor.update == NativePlayerUpdate &&
                  player->actor.draw == (ProxyRider() ? nullptr : NativePlayerDraw);
    o.replaced = g.hostOwned && (g.player != player || g.play != play || g.sceneId != play->sceneNum ||
                                 g.collision != play->colCtx.colHeader || gBoundPlayerGeneration != gPlayerGeneration ||
                                 gBoundSceneGeneration != gSceneGeneration);
    const auto action = NativeSkateOwnership::Decide(o);
    if (action == NativeSkateOwnership::Action::RECOVER) {
        // Prewarm is a valid not-ready state, and FAILED waits for deliberate F9/F8 retry.
        if (gOwnershipMode == OwnershipMode::FAILED || (!g.runtime.Ready() && !g.failed && !g.hostOwned))
            return;
        SPDLOG_WARN("[NativeOwnership] self heal error={} callbacks={} replaced={} cs={} transition={}", error,
                    o.callbacks, o.replaced, player->csAction, play->transitionTrigger);
        QueueRecovery(o.replaced ? OwnershipReason::PLAYER_REPLACED : OwnershipReason::SELF_HEAL);
        return;
    }
    if (action == NativeSkateOwnership::Action::SUSPEND) {
        g.runtime.Suspend();
        RestorePlayer(play->transitionTrigger != TRANS_TRIGGER_OFF ? OwnershipReason::SCENE_TRANSITION
                      : play->msgCtx.msgMode                       ? OwnershipReason::TEMP_SUSPEND_DIALOGUE
                                                                   : OwnershipReason::TEMP_SUSPEND_CUTSCENE,
                      true);
        g.anchor.valid = false;
        g.interactionSuspended = true;
        g.status = "SUSPENDED";
    }
    if (action == NativeSkateOwnership::Action::ACTIVATE && !g.enabled)
        QueueRecovery(OwnershipReason::SELF_HEAL);
    if (action == NativeSkateOwnership::Action::DISABLE)
        Stop("ownership policy OFF", OwnershipReason::CONSOLE_DISABLE, true);
}
// Loading is a lifecycle boundary, not a runtime fault. Run while the old
// PlayState/collision context is still alive, then sever all scene references.
void SuspendForLoading(bool destroyed = false) {
    NativeSkateObjectDropper::Clear("OBJECT_LOADING_CLEANUP", destroyed);
    NativeSkateAudio::Stop();
    gLegacyPop.Reset();
    gLegacyTriangleOwned = false;
    gLegacyAuthoredFs360 = false;
    gVert.Reset();
    gMarker.valid = false;
    gMarkerCameraPending = false;
    gUtilityCommands = {};
    gUtility = {};
    gDryLand.valid = false;
    gDryGate.Reset();
    ClearDryHistory();
    gWaterPending = false;
    gSwordInput.Reset();
    gSwordRequest = false;
    gLogicalTap = false;
    gGrindJob.Cancel();
    gGrindsReady = false;
    gGrindsUploaded = false;
    gGrinds = {};
    g.actors.Clear(g.runtime);
    g.dynamic.Clear();
    gWalkwayUploaded = false;
    if (g.runtime.Ready()) {
        g.runtime.ClearScore();
        g.runtime.Suspend();
    }
    gHud.Clear();
    NativeSkateHud::ClearOverlays();
    RestorePlayer(OwnershipReason::SCENE_TRANSITION, true);
    g.anchor.valid = false;
    g.interactionSuspended = true;
    g.status = "LOADING";
    gRecoveryQueued = false;
    NativeSkateInput::Reset(NativeSkateInput::Now());
    if (destroyed) {
        g.player = nullptr;
        g.play = nullptr;
        g.collision = nullptr;
        g.sceneId = -1;
    }
}
void OnFrame() {
    NativeSkatePerf::enabled =
        DeveloperDiagnostics() && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Performance"), 0) != 0;
    UpdatePause(gPlayState);

    if (gMarker.valid && gPlayState &&
        (gMarker.scene != gPlayState->sceneNum || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF)) {
        gMarker.valid = false;
        MarkerNotice("SESSION MARKER CLEARED");
    }
    if (OwnsCurrentPlayer() && g.status == "ACTIVE" && !gPaused && gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
        if (!g.actors.Update(gPlayState, g.runtime, g.snapshot.tick))
            SPDLOG_ERROR("[NativeActors] update failed: {}", g.runtime.Error());
    }
    ++g.updateTicks;
    if (g.updateTicks % 300 == 0 && NativeSkatePerf::enabled && !gPaused) {
        for (int i = 0; i < NativeSkatePerf::Count; ++i) {
            const auto& m = NativeSkatePerf::metrics[i];
            SPDLOG_INFO("[NativeSkatePerf] {} avg={}ms max={}ms samples={}",
                        NativeSkatePerf::Name((NativeSkatePerf::Stage)i), m.Average(), m.max, m.count);
        }
        std::array<uint64_t, 16> stats{};
        g.runtime.LifecycleStats(stats);
        SPDLOG_INFO(
            "[NativeSkateCache] static={} {}B prepared={} {}B grinds={} {}B dynamic={} localResources={} {}B dynamicSplines={} activeGrinds={} nativeWorlds={} {}B hits={} misses={} events={} overflow={} job={} discarded={}",
            NativeSkateWorld::CacheEntries(), NativeSkateWorld::CacheBytes(), gPreparedCache.Count(),
            gPreparedCache.Bytes(), gGrindCache.Count(), gGrindCache.Bytes(), g.dynamic.MeshCount(),
            g.dynamic.ResourceCount(), g.dynamic.ResourceBytes(), g.dynamic.GrindCount(), stats[8], stats[2], stats[3],
            stats[4], stats[5], stats[0], stats[1], gGrindJob.State(), gGrindJob.Discarded());
        SPDLOG_INFO("[NativeSkateCache] nativeGrindProviders={} {}B hits={} misses={}", stats[12], stats[13], stats[14],
                    stats[15]);
    }
    if (!g.failed)
        g.runtime.Prewarm(DataRoot());
    if (g.runtime.State() != NativeSkateRuntime::WarmState::PREWARMING && !g.prewarmReported &&
        g.runtime.State() != NativeSkateRuntime::WarmState::UNINITIALIZED) {
        g.prewarmReported = true;
        NativeSkatePerf::metrics[NativeSkatePerf::Prewarm].Add(g.runtime.PrewarmMilliseconds());
        SPDLOG_INFO("[NativeSkate] {} runtime={}", g.runtime.Profile(), g.runtime.StateName());
        if (g.runtime.State() == NativeSkateRuntime::WarmState::FAILED) {
            g.failed = true;
            g.error = g.runtime.Error();
            SPDLOG_ERROR("[NativeSkate] PREWARM FAILED: {}", g.error);
        }
    }
    if (!g.requested)
        g.status = g.runtime.StateName();

    // Fast3dGui feeds ImGui keyboard state from Win32/DXGI OR SDL.
    // SDL_GetKeyboardState is not populated by the DXGI window backend.
    const bool f8 = ImGui::GetCurrentContext() && ImGui::IsKeyDown(ImGuiKey_F8);
    g.f8Pressed = f8 && !g.wasF8;
    if (g.f8Pressed) {
        const int before = CVarGetInteger(kEnabledCvar, 0);
        const bool requested = before == 0;
        CVarSetInteger(kEnabledCvar, requested ? 1 : 0);
        gActivationTracePending = requested;
        if (requested && g.runtime.Ready())
            gReadyInputEdge = std::chrono::steady_clock::now();
        else
            gReadyInputEdge.reset();
        SPDLOG_INFO(
            "[NativeSkate] F8_EDGE ENABLED_BEFORE={} ENABLED_AFTER_SET={} RUNTIME_STATE={} OWNERSHIP_BEFORE={} STARTUP_RESET_COUNT={}",
            before, CVarGetInteger(kEnabledCvar, 0), g.runtime.StateName(), OwnsCurrentPlayer(), gStartupResetCount);
        SPDLOG_INFO("[NativeSkate] F8 toggle -> {}", requested ? "ON" : "OFF");
    }
    g.wasF8 = f8;
    const bool f9 = ImGui::GetCurrentContext() && ImGui::IsKeyDown(ImGuiKey_F9);
    if (f9 && !gWasF9)
        QueueRecovery(OwnershipReason::F9_RECOVERY);
    gWasF9 = f9;
    const bool requested = CVarGetInteger(kEnabledCvar, 0) != 0;
    if (requested != g.requested)
        SetRequested(requested);
    if (gPlayState && gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        if (g.status != "LOADING")
            SuspendForLoading();
        return;
    }
    MonitorOwnership();
}
void Fail(const std::string& error) {
    if (g.capture.is_open()) {
        g.capture.flush();
        g.capture.close();
    }
    g.stopReason = error.find("Nonfinite") != std::string::npos || error.find("Non-finite") != std::string::npos ||
                           error.find("Host frame diagnostic") != std::string::npos
                       ? "INVALID_SNAPSHOT"
                       : "RUNTIME_ERROR";
    for (const auto& row : g.trace)
        SPDLOG_ERROR("[NativePush] STOP CAPTURE {}", row);
    g.error = error;
    gSessionInvalid = true;
    g.failed = true;
    g.status = "FAILED";
    OwnershipTransition(OwnershipMode::FAILED, OwnershipReason::RUNTIME_FAILURE);
    QueueRecovery(OwnershipReason::RUNTIME_FAILURE);
    SPDLOG_ERROR("[NativeSkate] LIVE RUNTIME FAILED: {}", error);
}
void NativePlayerDraw(Actor* actor, PlayState* play) {
    if (OwnsCurrentPlayer() && actor == &g.player->actor && g.status == "ACTIVE" && !ProxyRider())
        NativeSkateLink::Draw(play, g.player, LinkFrame(), g.runtime.FinalPose(), g.anchor, g.snapshot.state,
                              g.runtime.RiderDiagnostics(), g.snapshot.board, g.snapshot.bodyPitch);
}
void Apply(Player* player) {
    if (!player || !g.enabled || !OwnsCurrentPlayer() || g.status != "ACTIVE")
        return;
    if (!g.anchor.Position(g.snapshot.trajectory, g.actorPosition) ||
        !g.anchor.Position(&g.snapshot.board[12], g.boardPosition)) {
        Fail("Nonfinite or invalid native position");
        return;
    }
    // Host actor is only the camera/lifecycle anchor. OoT movement is discarded;
    // the independent native proxy and board use Rust outputs below.
    // Stock world actors expect a feet-origin Player, not physical COM.
    // Keep the Rust COM anchor unchanged; publish the accepted Link host frame.
    const auto hostFrame = LinkFrame();
    player->actor.world.pos = (!ProxyRider() && hostFrame.valid)
                                  ? Vec3f{ hostFrame.position[0], hostFrame.position[1], hostFrame.position[2] }
                                  : Vec3f{ g.actorPosition[0], g.actorPosition[1], g.actorPosition[2] };
    player->actor.velocity = { 0, 0, 0 };
    player->actor.speedXZ = 0;
    player->actor.gravity = 0;
    player->actor.shape.rot.y =
        (s16)std::lrintf((NativeSkateBoardPresentation::Biped(g.snapshot.stateId) ? g.runtime.BipedDiagnostics()[3]
                                                                                  : g.snapshot.heading) *
                         (32768.0f / 3.14159265358979323846f));
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
    // Stock Player_UpdateCommon copies home.pos into prevPos. While bypassed,
    // keep this history current so releasing ownership cannot rewind collision.
    player->actor.home.pos = player->actor.world.pos;
    player->actor.draw = ProxyRider() ? nullptr : NativePlayerDraw;
}
void OnPlayerUpdate() {
    if (gPlayState && gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        if (g.status != "LOADING")
            SuspendForLoading();
        return;
    }
    if (gUtilityCommands.toggle) {
        gUtilityCommands.toggle = false;
        gUtilityCommands.set = gUtilityCommands.load = false;
        gSwordInput.Reset();
        gSwordRequest = false;
        gLogicalTap = false;
        if (NativeSkateObjectDropper::Active())
            NativeSkateObjectDropper::Exit("UTILITY_TOGGLE");
        else if (OwnsCurrentPlayer() && !gRecoveryQueued)
            CVarSetInteger(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.Enabled"), 1);
    }
    if (gVert.Active() && (NativeSkateObjectDropper::Active() ||
                           CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.Enabled"), 0))) {
        float frame[16]{}, anchor[3]{};
        std::copy_n(g.snapshot.board, 16, frame);
        float velocity[3]{ gVert.velocity.x / g.anchor.unitsPerMeter, gVert.velocity.y / g.anchor.unitsPerMeter,
                           gVert.velocity.z / g.anchor.unitsPerMeter };
        if (!g.runtime.VertStep(frame, velocity, anchor, 0, g.snapshot)) {
            QueueRecovery(OwnershipReason::RUNTIME_FAILURE);
            return;
        }
        gVert.Reset();
    }
    if (NativeSkateObjectDropper::Update(gPlayState, OwnsCurrentPlayer())) {
        NativeSkateAudio::Stop();
        gSwordInput.Reset();
        gSwordRequest = false;
        gLogicalTap = false;
        NativeSkateInput::Reset(NativeSkateInput::Now());
        return;
    }
    const bool requested = CVarGetInteger(kEnabledCvar, 0) != 0;
    if (requested != g.requested)
        SetRequested(requested);
    if (!g.enabled || gRecoveryQueued)
        return;
    PlayState* play = gPlayState;
    Player* player = (play ? GET_PLAYER(play) : nullptr);
    if (!player || !play) {
        g.status = "WAITING_FOR_PLAYER";
        return;
    }
    UpdatePause(play);
    if (play->pauseCtx.state != 0)
        return;
    if (g.interactionSuspended) {
        if (play->sceneNum == g.sceneId &&
            (play->transitionTrigger != TRANS_TRIGGER_OFF || Player_InCsMode(play) ||
             (player->stateFlags1 & (PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_CUTSCENE))))
            return;
        if (Player_InCsMode(play) || play->transitionTrigger != TRANS_TRIGGER_OFF)
            return;
        g.interactionSuspended = false;
        g.failed = false;
        g.error.clear();
        g.status = "INITIALIZING";
    }
    if (g.sceneId != play->sceneNum || g.play != play || g.player != player || g.collision != play->colCtx.colHeader ||
        gBoundPlayerGeneration != gPlayerGeneration || gBoundSceneGeneration != gSceneGeneration) {
        if (gMarker.valid && (gMarker.scene != play->sceneNum || g.play != play)) {
            gMarker.valid = false;
            MarkerNotice("SESSION MARKER CLEARED");
        }
        gGrindJob.Cancel();
        gGrindsReady = false;
        gGrindsUploaded = false;
        gGrinds = {};
        g.actors.Clear(g.runtime);
        g.dynamic.Clear();
        gVert.Reset();
        if (g.runtime.Ready()) {
            g.runtime.ClearScore();
            g.runtime.Suspend();
        }
        gHud.Clear();
        RestorePlayer(OwnershipReason::SCENE_TRANSITION, true);
        g.anchor.valid = false;
        g.player = player;
        g.play = play;
        g.sceneId = play->sceneNum;
        g.interactionSuspended = false;
        g.collision = play->colCtx.colHeader;
        g.status = "INITIALIZING";
    }
    if (!g.hostOwned) {
        if (g.failed) {
            g.status = "FALLBACK";
            return;
        }
        // Leave stock spawn/door/cutscene actions running until they finish;
        // freezing their nonzero csAction would permanently veto scene exits.
        if (Player_InCsMode(play) || play->transitionTrigger != TRANS_TRIGGER_OFF || play->msgCtx.msgMode) {
            g.status = "WAITING_FOR_WORLD_INTERACTION";
            return;
        }
        Start(player, play);
        if (g.status != "ACTIVE") {
            if (g.failed) {
                gSessionInvalid = true;
                QueueRecovery(OwnershipReason::RUNTIME_FAILURE);
            }
            return;
        }
        if (gActivationTracePending) {
            SPDLOG_INFO("[NativeSkate] F8 OWNERSHIP_AFTER=RUST ENABLED={} RUNTIME_STATE={} STARTUP_RESET_COUNT={}",
                        CVarGetInteger(kEnabledCvar, 0), g.runtime.StateName(), gStartupResetCount);
            gActivationTracePending = false;
        }
    }
    std::array<float, 4> modifiers{};
    const char* modifierNames[] = { CVAR_ENHANCEMENT("NativeSkate.Cheats.OllieHeightMultiplier"),
                                    CVAR_ENHANCEMENT("NativeSkate.Cheats.BoardSpeedMultiplier"),
                                    CVAR_ENHANCEMENT("NativeSkate.Cheats.PushAccelerationMultiplier"),
                                    CVAR_ENHANCEMENT("NativeSkate.Cheats.AirControlMultiplier") };
    for (int i = 0; i < 4; ++i) {
        float value = CVarGetFloat(modifierNames[i], 1.f);
        modifiers[i] = std::isfinite(value) ? std::clamp(value, i == 3 ? 0.f : .5f, i == 3 ? 2.f : 3.f) : 1.f;
    }
    if (!g.runtime.Modifiers(modifiers, CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Cheats.NoBail"), 0) != 0)) {
        QueueRecovery(OwnershipReason::RUNTIME_FAILURE);
        return;
    }
    if (gScoreRoom != -1 && gScoreRoom != play->roomCtx.curRoom.num) {
        g.runtime.ClearScore();
        gHud.Clear();
    }
    gScoreRoom = play->roomCtx.curRoom.num;
    if (Player_InCsMode(play) || play->transitionTrigger != TRANS_TRIGGER_OFF || play->msgCtx.msgMode) {
        const Vec3f current = player->actor.world.pos;
        g.runtime.Suspend();
        RestorePlayer(play->transitionTrigger != TRANS_TRIGGER_OFF ? OwnershipReason::SCENE_TRANSITION
                      : play->msgCtx.msgMode                       ? OwnershipReason::TEMP_SUSPEND_DIALOGUE
                                                                   : OwnershipReason::TEMP_SUSPEND_CUTSCENE,
                      true);
        player->actor.world.pos = current;
        player->actor.prevPos = current;
        g.anchor.valid = false;
        g.interactionSuspended = true;
        g.status = "WORLD_INTERACTION";
        SPDLOG_INFO(
            "[NativeSkate] stock interaction yield csAction={} flags={} transition={} position={} {} {}; native assets resident",
            player->csAction, player->stateFlags1, play->transitionTrigger, current.x, current.y, current.z);
        return;
    }
    const int bodyProfile = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.RiderBodyProfile"), LINK_IS_ADULT ? 1 : 2);
    if (bodyProfile != g.riderBodyProfile) {
        float h =
            bodyProfile == 0 ? 0.f : (bodyProfile == 2 ? 43.343237f : 62.165557f) / NativeSkateUnits::kOotUnitsPerMeter;
        if (!g.runtime.SetRiderHeight(h)) {
            Fail(g.runtime.Error());
            return;
        }
        g.riderBodyProfile = bodyProfile;
    }
    if (!gGrindsReady && gGrindJob.Poll(gGrinds)) {
        NativeSkatePerf::metrics[NativeSkatePerf::Extract].Add(gGrinds.extractionMs);
        NativeSkatePerf::metrics[NativeSkatePerf::Classify].Add(gGrinds.classificationMs);
        NativeSkatePerf::metrics[NativeSkatePerf::Weld].Add(gGrinds.weldMs);
        gGrindsReady = gGrinds.error.empty();
        if (gGrindsReady)
            gGrindCache.Put(gGrindKey, gGrinds, GrindBytes(gGrinds));
        SPDLOG_INFO(
            "[NativeSkateGrinds] scene={} triangles={} edges={} seams={} candidates={} splines={} length={}m extraction={}ms classification={}ms weld={}ms",
            g.sceneId, gGrinds.triangles, gGrinds.edges.size(), gGrinds.seams, gGrinds.candidates,
            gGrinds.splines.size(), gGrinds.length, gGrinds.extractionMs, gGrinds.classificationMs, gGrinds.weldMs);
    }
    const bool grindEnabled = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Grinds.Enabled"), 1) != 0;
    if (gGrindsReady && (!gGrindsUploaded || grindEnabled != gGrindsEnabled)) {
        auto begin = std::chrono::steady_clock::now();
        std::vector<float> points;
        std::vector<uint32_t> offsets{ 0 };
        if (grindEnabled)
            for (const auto& line : gGrinds.splines) {
                for (const auto& p : line.points)
                    for (float f : p)
                        points.push_back(f);
                offsets.push_back((uint32_t)(points.size() / 3));
            }
        if (!g.runtime.Grinds(points, offsets)) {
            SPDLOG_ERROR("[NativeSkateGrinds] upload failed; ordinary skating retained: {}", g.runtime.Error());
        } else {
            gGrindsUploaded = true;
            gGrindsEnabled = grindEnabled;
            gGrindUploadMs =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            NativeSkatePerf::metrics[NativeSkatePerf::Upload].Add(gGrindUploadMs);
            SPDLOG_INFO("[NativeSkateGrinds] native upload={}ms surfaces={} enabled={}", gGrindUploadMs,
                        offsets.size() - 1, grindEnabled);
        }
    }
    if (gSaveContext.health == 0) {
        CVarSetInteger(kEnabledCvar, 0);
        g.requested = false;
        Stop("stock Player death", OwnershipReason::DEATH, true);
        return;
    }
    Actor* hit = player->cylinder.base.oc;
    float impact = 0;
    if (hit && (hit->category == ACTORCAT_ENEMY || hit->category == ACTORCAT_BOSS)) {
        float x = hit->world.pos.x - player->actor.world.pos.x, z = hit->world.pos.z - player->actor.world.pos.z,
              length = std::hypot(x, z);
        if (length > .001f)
            impact = (g.snapshot.trajectoryVelocity[0] * x + g.snapshot.trajectoryVelocity[2] * z) / length;
    }
    float threshold = NativeSkatePresentationMath::Bound(
        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.ActorImpactBailSpeed"), 2.f), 2.f, .5f, 10.f);
    int combat = Player_NativeCombatUpdate(play, player, impact >= threshold ? impact : 0);
    if (combat && gSaveContext.health == 0) {
        CVarSetInteger(kEnabledCvar, 0);
        g.requested = false;
        Stop("stock damage fatal", OwnershipReason::DEATH, true);
        return;
    }
    if (combat && NativeSkateActors::Onboard(g.snapshot.stateId) &&
        (g.lastActorBail == 0 || g.snapshot.tick >= g.lastActorBail + 30)) {
        bool accepted = !CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Cheats.NoBail"), 0) &&
                        g.runtime.ActorBail(g.snapshot) && g.snapshot.stateId == 300;
        g.lastActorBail = g.snapshot.tick;
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ActorCollisionDebug"), 0))
            SPDLOG_INFO("[NativeActors] damage={} bodyImpact={} relativeSpeed={} BAIL_REQUESTED=1 BAIL_ACCEPTED={}",
                        bool(combat & 1), bool(combat & 2), impact, accepted);
    }
    CaptureCamera(play);
    NativeSkateInput::Poll();
    const bool propInput = NativeSkateObjectDropper::BipedPropInput(
        play, NativeSkateBoardPresentation::Biped(g.snapshot.stateId), Player_NativeSwordDrawn(player));
    if (propInput) {
        play->state.input[0].cur.button &= ~BTN_R;
        play->state.input[0].press.button &= ~BTN_R;
    }
    // Publish this frame's settled/carry pose and collision ownership before
    // the native step. Grab/release must not leave a one-frame stale collider.
    if (!g.dynamic.Update(play, g.runtime) || !UpdateKokiriWalkway(player)) {
        Fail(g.runtime.Error());
        return;
    }
    if (gUtility.Mask() & NativeSkateUtility::Circle) {
        gSwordInput.Reset();
        gSwordRequest = false;
        gLogicalTap = false;
    }
    gLogicalAttack = !(gUtility.Mask() & NativeSkateUtility::Circle) && (play->state.input[0].cur.button & BTN_B) != 0;
    gLogicalTap = gLogicalTap || (!(gUtility.Mask() & NativeSkateUtility::Circle) && !gLogicalAttack &&
                                  (play->state.input[0].press.button & BTN_B) != 0);
    if (!NativeSkateInput::AdvanceTo(NativeSkateInput::Now(), [](const std::vector<NativeSkateRuntime::Packet>&
                                                                     samples) {
            if (gMarkerLoaded)
                return true;
            static std::vector<NativeSkateRuntime::Packet> gameplay;
            gameplay.clear();
            gameplay.reserve(512);
            for (const auto& raw : samples) {
                if (gWaterPending) {
                    gameplay.push_back({});
                    continue;
                }
                const auto actions = gUtilityCommands;
                gUtilityCommands.set = gUtilityCommands.load = false;
                if (actions.set && !gVert.Active()) {
                    const bool attachObject = gUtilityObjectInteract;
                    gUtilityObjectInteract = false;
                    NativeSkateRuntime::Marker captured;
                    if (g.runtime.CaptureMarker(captured)) {
                        gMarker.valid = true;
                        gMarker.scene = g.sceneId;
                        gMarker.room = g.play->roomCtx.curRoom.num;
                        gMarker.native = captured;
                        CaptureMarkerCamera(g.play, g.player, gMarker.camera);
                        NativeSkateObjectDropper::SessionMarkerSet(attachObject);
                        MarkerNotice("SESSION MARKER SET");
                    } else
                        MarkerNotice("CANNOT SET MARKER HERE");
                }
                if (actions.load) {
                    gVert.Reset();
                    if (!gMarker.valid || gMarker.scene != g.sceneId)
                        MarkerNotice("NO SESSION MARKER");
                    else if (gMarker.room != g.play->roomCtx.curRoom.num)
                        MarkerNotice("MARKER ROOM NOT LOADED");
                    else {
                        NativeSkateObjectDropper::RestoreMarkerObjects();
                        if (!g.dynamic.Update(g.play, g.runtime))
                            return false;
                        if (!g.runtime.LoadMarker(gMarker.native, g.snapshot))
                            return false;
                        g.runtime.ClearScore();
                        gHud.Clear();
                        NativeSkateAudio::Stop();
                        gLegacyPop.Reset();
                        gLegacyTriangleOwned = false;
                        gLegacyAuthoredFs360 = false;
                        gMarkerCameraPending = gMarker.camera.valid;
                        gMarkerLoaded = true;
                        MarkerNotice("SESSION MARKER LOADED");
                        return true;
                    }
                }
                auto packet = raw;
                const bool legacyEnabled =
                    CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Cheats.LegacyFs360PopGlitch"), 0) != 0;
                const bool legacyAir = g.snapshot.stateId >= 200 && g.snapshot.stateId <= 202 && !gVert.Active() &&
                                       !gWaterPending && Player_NativeWater(g.play, g.player) < 0;
                auto triggerState = NativeSkateInput::ReadDropperTriggers();
                uint8_t triggerMask = (triggerState.leftHeld ? 1 : 0) | (triggerState.rightHeld ? 2 : 0);
                bool landing = false;
                double touchdown = 100.;
                if (legacyEnabled && legacyAir && gLegacyPop.triggerSeen && (raw.buttons & 0x8000) &&
                    !gLegacyPop.previousTriangle && g.snapshot.trajectoryVelocity[1] < -.1f) {
                    float from[3] = { g.snapshot.board[12], g.snapshot.board[13] + .03f, g.snapshot.board[14] },
                          to[3] = { from[0], from[1] - 1.5f, from[2] }, hit[8]{};
                    if (g.runtime.Probe(from, to, hit) && hit[0] > 0 && hit[5] >= .85f) {
                        float gap = g.snapshot.board[13] - hit[2] - .08f;
                        touchdown = std::max(0.f, gap) / (-g.snapshot.trajectoryVelocity[1]);
                        landing = gap >= -.03f && gap <= 1.f;
                    }
                }
                bool triangle = (raw.buttons & 0x8000) != 0;
                if (!triangle)
                    gLegacyTriangleOwned = false;
                bool superPop =
                    gLegacyPop.Read(legacyEnabled, gSceneGeneration, legacyAir && gLegacyAuthoredFs360, legacyAir,
                                    triggerMask, triangle, landing, touchdown, g.snapshot.tick / 60.);
                if (superPop) {
                    if (!g.runtime.LegacyPop(g.snapshot))
                        return false;
                    gLegacyTriangleOwned = true;
                    SPDLOG_INFO("[NativeSkate] Legacy FS360 pop generation={} tick={}", gLegacyPop.generation,
                                g.snapshot.tick);
                }
                if (gLegacyTriangleOwned)
                    packet.buttons &= static_cast<uint16_t>(~0x8000);
                packet.buttons &= static_cast<uint16_t>(~0x0100);
                if (NativeSkateObjectDropper::PropInputOwned())
                    packet.buttons &= ~0x0200;
                if (gUtility.Mask() & NativeSkateUtility::Up)
                    packet.buttons &= ~1;
                if (gUtility.Mask() & NativeSkateUtility::Down)
                    packet.buttons &= ~2;
                // Brake is still the existing native B-button intent; only its
                // contextual input arbitration changes, never its force/physics.
                packet.buttons &= static_cast<uint16_t>(~0x2000);
                gameplay.push_back(packet);
            }
            const bool biped = NativeSkateBoardPresentation::Biped(g.snapshot.stateId);
            const bool swordSafe = !gWaterPending && (biped || std::strcmp(g.snapshot.state, "PhysicsGround") == 0);
            if (gLogicalTap) {
                auto tap =
                    gSwordInput.Read(true, !biped, Player_NativeSwordDrawn(g.player), swordSafe, g.snapshot.tick);
                gSwordRequest = gSwordRequest || tap.attack;
                gLogicalTap = false;
            }
            const auto circle =
                gSwordInput.Read(gLogicalAttack, !biped, Player_NativeSwordDrawn(g.player), swordSafe, g.snapshot.tick);
            gSwordRequest = gSwordRequest || (!gWaterPending && circle.attack);
            for (auto& packet : gameplay)
                if (!gWaterPending && circle.brake)
                    packet.buttons |= 0x2000;
            const auto before = g.snapshot;
            if (g.capture.is_open()) {
                const uint32_t count = (uint32_t)samples.size();
                g.capture.write((const char*)&count, 4);
                g.capture.write((const char*)samples.data(), samples.size() * sizeof(samples[0]));
                g.capture.flush();
            }
            float hostBoard[3];
            g.anchor.Position(g.snapshot.board + 12, hostBoard);
            NativeSkateVert::Point v{ g.snapshot.trajectoryVelocity[0] * g.anchor.unitsPerMeter,
                                      g.snapshot.trajectoryVelocity[1] * g.anchor.unitsPerMeter,
                                      g.snapshot.trajectoryVelocity[2] * g.anchor.unitsPerMeter };
            bool vertWasActive = gVert.Active();
            const auto& vertRamps = NativeSkateObjectDropper::VertRamps();
            bool vertOwnerLost = vertWasActive && std::none_of(vertRamps.begin(), vertRamps.end(),
                                                               [](const auto& r) { return r.id == gVert.owner; });
            const float* vertDiag = g.runtime.RiderDiagnostics();
            bool eligible = (vertWasActive && (g.snapshot.stateId == 1000 || g.snapshot.stateId == 1001)) ||
                            NativeSkateVert::CanAcquire(g.snapshot.stateId, vertDiag[96] == 1.f,
                                                        vertDiag[74] != 0.f || vertDiag[75] != 0.f);
            // Special contact cannot swallow dismount or an authored trick request.
            // Return all coupled bodies to native ownership, then forward the SAME
            // packets; no synthesized pop/grace or forced airborne repeated impulse.
            const bool vertReleaseInput = std::any_of(gameplay.begin(), gameplay.end(), [](const auto& p) {
                return (p.buttons & 0x8000) != 0 || std::abs((int)p.right[0]) > 8000 ||
                       std::abs((int)p.right[1]) > 8000;
            });
            bool vertNow = gVert.Update(vertRamps, { hostBoard[0], hostBoard[1], hostBoard[2] }, v, eligible,
                                        (gameplay.back().buttons & 0x0200) != 0, 1.f / 60.f, vertReleaseInput,
                                        float(gameplay.back().left[0]) / 32767.f);
            float vertFrame[16], vertVelocity[3], plantAnchor[3];
            gVert.Frame(1 / g.anchor.unitsPerMeter, vertFrame);
            for (int i = 0; i < 3; ++i) {
                vertFrame[12 + i] += g.anchor.rust[i] - g.anchor.oot[i] / g.anchor.unitsPerMeter;
                plantAnchor[i] = g.anchor.rust[i] + ((&gVert.anchor.x)[i] - g.anchor.oot[i]) / g.anchor.unitsPerMeter;
                vertVelocity[i] = (&gVert.velocity.x)[i] / g.anchor.unitsPerMeter;
            }
            if (vertWasActive && !gVert.Handplant() && gHud.line.inCombo && gHud.history.size() &&
                gHud.history.back() == "HANDPLANT")
                gHud.line.Commit(500, g.snapshot.tick);
            if (vertNow && !vertWasActive) {
                gHud.line.Commit(gHud.combo, g.snapshot.tick);
            }
            if (gVert.award) {
                gHud.line.Start(g.snapshot.tick);
                gHud.line.Observe(500);
                gHud.current = 500;
                gHud.history.push_back("HANDPLANT");
                if (gHud.history.size() > 4)
                    gHud.history.pop_front();
                gHud.historyTick = g.snapshot.tick;
            }
            bool ok = true;
            if (vertNow)
                ok = g.runtime.VertStep(vertFrame, vertVelocity, plantAnchor, gVert.Handplant() ? 2 : 1, g.snapshot,
                                        gVert.Weight());
            else if (vertWasActive) {
                if (vertOwnerLost)
                    std::copy_n(g.snapshot.board, 16, vertFrame);
                ok = g.runtime.VertStep(vertFrame, vertVelocity, plantAnchor, 0, g.snapshot);
                if (ok)
                    ok = g.runtime.StepCamera(gameplay, gCameraForward, g.snapshot);
                if (gHud.line.inCombo)
                    gHud.line.Commit(500, g.snapshot.tick);
            } else
                ok = g.runtime.StepCamera(gameplay, gCameraForward, g.snapshot);
            if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0)) {
                static uint64_t lastOwner = 0;
                static int lastPhase = -1;
                static unsigned lastState = 0;
                if (lastOwner != gVert.owner || lastPhase != (int)gVert.phase || lastState != g.snapshot.stateId) {
                    std::array<uint64_t, 6> worker{};
                    g.runtime.WorkerProgress(worker);
                    SPDLOG_INFO(
                        "[NativeSkate] CONTACT hostFrame={} nativeTick={} before={} state={} vertOwner={} phase={} releaseInput={} buttons={} velocity={},{},{} workerSubmitted={} completed={} command={} workerTick={} heartbeatMs={}",
                        g.updateTicks, g.snapshot.tick, before.state, g.snapshot.state, gVert.owner, (int)gVert.phase,
                        vertReleaseInput, gameplay.back().buttons, g.snapshot.trajectoryVelocity[0],
                        g.snapshot.trajectoryVelocity[1], g.snapshot.trajectoryVelocity[2], worker[0], worker[1],
                        worker[2], worker[3], worker[5]);
                    lastOwner = gVert.owner;
                    lastPhase = (int)gVert.phase;
                    lastState = g.snapshot.stateId;
                }
            }
            if (ok && NativeSkatePresentationMath::JumpVoice(before.stateId, g.snapshot.stateId,
                                                             g.snapshot.trajectoryVelocity[1])) {
                Player_PlayVoiceSfx(g.player, NA_SE_VO_LI_AUTO_JUMP);
            }
            if (ok && NativeSkateActors::Onboard(g.snapshot.stateId) &&
                g.actors.Impact(
                    g.runtime, before.trajectoryVelocity,
                    NativeSkatePresentationMath::Bound(
                        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.ActorImpactBailSpeed"), 2.f), 2.f, .5f, 10.f)) &&
                (g.lastActorBail == 0 || g.snapshot.tick >= g.lastActorBail + 30)) {
                bool accepted = !CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Cheats.NoBail"), 0) &&
                                g.runtime.ActorBail(g.snapshot) && g.snapshot.stateId == 300;
                g.lastActorBail = g.snapshot.tick;
                if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ActorCollisionDebug"), 0))
                    SPDLOG_INFO("[NativeActors] native enemy contact onset BAIL_REQUESTED=1 BAIL_ACCEPTED={}",
                                accepted);
            }
            if (ok) {
                std::vector<NativeSkateRuntime::Event> events;
                bool read = false;
                {
                    NativeSkatePerf::Scope timing(NativeSkatePerf::EventDrain);
                    read = g.runtime.ReadEvents(events);
                }
                if (read) {
                    NativeSkatePerf::Scope timing(NativeSkatePerf::HudUpdate);
                    for (const auto& e : events) {
                        if (e.phase == 1 && std::strcmp(e.name, "ID_TRICK_FLIP_FS_360_POP_SHUVIT") == 0) {
                            gLegacyAuthoredFs360 = true;
                            gLegacyPop.previousTrick = false;
                        } else if ((e.phase == 1 && std::strncmp(e.name, "ID_TRICK_FLIP_", 14) == 0) || e.phase == 4 ||
                                   e.phase == 5 || e.phase == 7) {
                            gLegacyAuthoredFs360 = false;
                            gLegacyPop.eligible = false;
                        }
                        gHud.Apply(e);
                        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ScoreDebug"), 0) &&
                            (e.phase == 1 || e.phase == 4 || e.phase == 5 || e.phase == 7))
                            SPDLOG_INFO("[NativeSkate score] {} combo={} line={} multiplier={} timerSeconds={}",
                                        gHud.line.transition, gHud.combo, gHud.line.score, gHud.multiplier,
                                        std::max(0.0, gHud.line.deadline - NativeSkateHud::Line::Seconds(gHud.tick)));
                    }
                    bool lineWasActive = gHud.line.active;
                    gHud.Advance(g.snapshot.tick, gVert.Active() || g.snapshot.stateId == 200 ||
                                                      g.snapshot.stateId == 201 || g.snapshot.stateId == 202 ||
                                                      std::strstr(g.snapshot.state, "Grind") ||
                                                      std::strstr(g.snapshot.state, "Manual"));
                    if (lineWasActive && !gHud.line.active &&
                        CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ScoreDebug"), 0))
                        SPDLOG_INFO("[NativeSkate score] LINE_COMPLETE line={} banked={}", gHud.line.score,
                                    gHud.line.total);
                }
            }
            gPreviousState = before.state;
            const auto* b = g.runtime.BipedDiagnostics();
            static int lastHorizontalSign = 0;
            const int horizontalSign =
                std::string(g.snapshot.state) == "BipedGround" && std::abs(b[14]) > .2f ? (b[14] > 0 ? 1 : -1) : 0;
            if (horizontalSign && horizontalSign != lastHorizontalSign && CVarGetInteger(kDebugCvar, 0))
                SPDLOG_INFO(
                    "[NativeSkate] BIPED LX physical={} host={} rust={} cameraRight={},{} desired={},{} actualVelocity={},{}",
                    b[14], b[14], b[18], b[19], b[20], b[5], b[6], b[9], b[10]);
            lastHorizontalSign = horizontalSign;
            const float desiredLength = std::hypot(b[5], b[6]), actualLength = std::hypot(b[9], b[10]);
            gMoveDotValid =
                ok && std::string(g.snapshot.state) == "BipedGround" && desiredLength > .2f && actualLength > .2f;
            gMoveDot = gMoveDotValid ? (b[5] * b[9] + b[6] * b[10]) / (desiredLength * actualLength) : 1;
            const bool reversed = gMoveDotValid && gMoveDot < -.5f;
            if (reversed && !gWasReversed && CVarGetInteger(kDebugCvar, 0))
                SPDLOG_WARN(
                    "[NativeSkate] BIPED CONTROL ORIENTATION FLIPPED tick={} transition={}->{} cam={} body={} boardHeading={} rawStick={},{} world={},{} velocity={},{} dot={} flags2476={} category={}",
                    g.snapshot.tick, before.state, g.snapshot.state, b[2], b[3], b[4], samples.back().left[0] / 32767.f,
                    samples.back().left[1] / 32767.f, b[5], b[6], b[9], b[10], gMoveDot, b[13], b[12]);
            gWasReversed = reversed;
            const auto* d = g.runtime.PushDiagnostics();
            if (ok && CVarGetInteger(kDebugCvar, 0) != 0) {
                auto c = NativeSkateWorldDebug::Measure(g.adapted, g.snapshot, g.anchor, d);
                std::ostringstream landing;
                landing << "tick=" << g.snapshot.tick << " state=" << g.snapshot.state
                        << " boardY=" << g.snapshot.board[13] << " COMY=" << g.snapshot.trajectory[1]
                        << " groundHostY=" << c.ground << " floorValid=" << c.floorValid
                        << " verticalVelocity=" << g.snapshot.trajectoryVelocity[1] << " contactCount=" << d[9]
                        << " wheelCount=" << d[13] << " normal=" << c.normal[0] << "," << c.normal[1] << ","
                        << c.normal[2] << " clearanceHost=" << c.clearance << " catch=NOT_EXPORTED landing="
                        << (before.stateId != g.snapshot.stateId ? "STATE_TRANSITION" : "NONE");
                const bool landed = std::string(g.snapshot.state) == "PhysicsGround" &&
                                    std::string(before.state).find("Air") != std::string::npos;
                if (landed) {
                    for (const auto& r : g.landingRows)
                        SPDLOG_INFO("[NativeLanding] BEFORE {}", r);
                    g.landingAfter = 11;
                }
                if (g.landingAfter > 0) {
                    SPDLOG_INFO("[NativeLanding] AFTER {}", landing.str());
                    --g.landingAfter;
                }
                g.landingRows.push_back(landing.str());
                if (g.landingRows.size() > 10)
                    g.landingRows.pop_front();
            }
            const bool newPush = d[3] != 0;
            if (newPush && !g.previousNewPush)
                ++g.pushEvents;
            g.previousNewPush = newPush;
            if (ok)
                NativeSkateAudio::Update(g.play, g.player, g.snapshot, newPush, gWaterPending,
                                         g.runtime.RiderDiagnostics()[40] > 0);
            const uint64_t applications = (uint64_t)std::max(0.0f, d[1] - g.applicationBase);
            const bool fired = applications > g.applications;
            g.applications = applications;
            const auto speed = [](const float* v) { return std::sqrt(v[0] * v[0] + v[2] * v[2]); };
            const float previous = speed(before.trajectoryVelocity),
                        current = ok ? speed(g.snapshot.trajectoryVelocity) : previous;
            const auto& packet = samples.back();
            const bool x = (packet.buttons & 0x1000) != 0, edge = x && !g.previousX;
            g.previousX = x;
            if (edge) {
                ++g.pushNumber;
                g.pushBefore = previous;
                g.pushPeak = previous;
                g.pushApplicationStart = g.applications - (fired ? 1 : 0);
                g.pushEndTick = g.updates + 72;
            }
            if (g.pushEndTick && DeveloperDiagnostics()) {
                g.pushPeak = std::max(g.pushPeak, current);
                g.lastPushDelta = current - g.pushBefore;
                if (!ok || g.updates >= g.pushEndTick) {
                    SPDLOG_INFO(
                        "[NativePush] PUSH TABLE press={} before={} after={} peak={} delta={} applications={} state={} ok={}",
                        g.pushNumber, g.pushBefore, current, g.pushPeak, g.lastPushDelta,
                        g.applications - g.pushApplicationStart, ok ? g.snapshot.state : "FAILED", ok);
                    g.pushEndTick = 0;
                }
            }
            if (DeveloperDiagnostics()) {
                std::ostringstream row;
                row << "tick=" << before.tick << " time=" << NativeSkateInput::Now() << " dt=" << d[0]
                    << " samples=" << samples.size() << " Xdown=" << x << " Xedge=" << edge
                    << " bits=" << packet.buttons << " pushing=" << d[2] << " NewPush=" << newPush
                    << " pushDV=" << d[17] << " continue=" << d[18] << " forceXYZ=" << d[14] << "," << d[15] << ","
                    << d[16] << " target=" << d[5] << " signedSpeed=" << d[6] << " mass=" << d[7] << " forceDt=" << d[8]
                    << " applications=" << applications << " fired=" << fired << " RustCOMspeed=" << previous << "->"
                    << current << " RustDeckSpeed=" << speed(before.velocity) << "->"
                    << (ok ? speed(g.snapshot.velocity) : speed(before.velocity)) << " hostAnchorSpeed=" << previous
                    << "->" << current << " hostPropulsionApplications=0"
                    << " state=" << before.state << "->" << (ok ? g.snapshot.state : "FAILED") << " stateId=" << d[12]
                    << " contacts=" << d[9] << " wheelContacts=" << d[13] << " ragdoll=" << d[10] << " brake=" << d[11]
                    << " velocity=" << g.snapshot.trajectoryVelocity[0] << "," << g.snapshot.trajectoryVelocity[1]
                    << "," << g.snapshot.trajectoryVelocity[2] << " trajectory=" << g.snapshot.trajectory[0] << ","
                    << g.snapshot.trajectory[1] << "," << g.snapshot.trajectory[2]
                    << " normal=" << g.snapshot.groundNormal[0] << "," << g.snapshot.groundNormal[1] << ","
                    << g.snapshot.groundNormal[2] << " ok=" << ok;
                g.trace.push_back(row.str());
                if (g.trace.size() > 180)
                    g.trace.pop_front();
                const bool stopped = ok && previous > 1.0f && current < previous * 0.25f;
                if (stopped) {
                    g.stopReason = d[10] != 0                             ? "WIPEOUT"
                                   : before.stateId != g.snapshot.stateId ? "STATE_TRANSITION"
                                   : d[11] > 0                            ? "BRAKE"
                                                                          : "UNKNOWN";
                    for (const auto& r : g.trace)
                        SPDLOG_WARN("[NativePush] STOP CAPTURE reason={} {}", g.stopReason, r);
                }
                if (CVarGetInteger(kDebugCvar, 0) != 0 &&
                    (edge || newPush || fired || !ok || before.stateId != g.snapshot.stateId))
                    SPDLOG_INFO("[NativePush] {}", row.str());
            }
            CaptureBoardTransition(packet, before.stateId, ok);
            if (!ok) {
                g.error = g.runtime.Error();
                return false;
            }
            bool r2Now = packet.triggers[1] != 0;
            if (r2Now && !g.r2Down) {
                g.r2PressTick = g.snapshot.tick;
                ++g.r2PressCount;
            }
            g.r2Down = r2Now;
            if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.AnimationTrace"), 0))
                AnimationTrace(packet);
            ++g.updates;
            return true;
        })) {
        Fail(g.error.empty() ? g.runtime.Error() : g.error);
        return;
    }
    const bool markerLoadedThisFrame = gMarkerLoaded;
    if (gMarkerLoaded) {
        gMarkerLoaded = false;
        NativeSkateInput::Reset(NativeSkateInput::Now());
        gHaveVisualBoard = false;
        gBoardBlendTick = 0;
        gBoardBlendLocal = false;
        NativeSkateLink::Begin(play, player);
        g.applicationBase = g.runtime.PushDiagnostics()[1];
        g.previousNewPush = false;
        g.previousX = false;
        SPDLOG_INFO("[NativeSkate] marker native reset scene={} room={} state={} position={} {} {}", gMarker.scene,
                    gMarker.room, g.snapshot.state, g.snapshot.trajectory[0], g.snapshot.trajectory[1],
                    g.snapshot.trajectory[2]);
    }
    Apply(player);
    const bool swordSafe = !gWaterPending && (NativeSkateBoardPresentation::Biped(g.snapshot.stateId) ||
                                              std::strcmp(g.snapshot.state, "PhysicsGround") == 0);
    NativeSkateLink::Sword(Player_NativeSwordUpdate(play, player, gSwordRequest,
                                                    NativeSkateBoardPresentation::Biped(g.snapshot.stateId),
                                                    swordSafe) != 0);
    gSwordRequest = false;
    Player_NativeRegisterColliders(play, player);
    if (markerLoadedThisFrame)
        player->actor.prevPos = player->actor.world.pos; // Marker teleport is not traversal.
    const std::string nativeState = g.snapshot.state;
    const bool grounded =
        nativeState == "PhysicsGround" || nativeState == "BipedGround" || nativeState == "GroundAnimation";
    if (gDryLand.valid && (gDryLand.scene != play->sceneNum || gDryLand.room != play->roomCtx.curRoom.num))
        gDryLand.valid = false;
    if (gWaterPending) {
        if (g.snapshot.tick - gWaterTick >= 18) {
            // Recheck the actual saved dry floor before a gameplay reset.
            const auto current = player->actor.world.pos;
            player->actor.world.pos = gDryHost;
            const bool valid = gDryLand.valid && Player_NativeDryFloor(play, player) &&
                               WaterClearance(play, gDryHost) >= kWaterRecoveryClearance;
            player->actor.world.pos = current;
            if (valid && g.runtime.WaterReturn(gDryLand.native, g.snapshot)) {
                gVert.Reset();
                Apply(player);
                player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
                NativeSkateInput::Reset(NativeSkateInput::Now());
                gSwordInput.Reset();
                gSwordRequest = false;
                gHaveVisualBoard = false;
                gBoardBlendTick = 0;
                gBoardBlendLocal = false;
                NativeSkateLink::Begin(play, player);
                Player_NativeRegisterColliders(play, player);
                gDryHysteresisUntil = NativeSkateInput::Now() + 2.;
                gWaterLastReturn = NativeSkateInput::Now();
                gWaterLastPosition = player->actor.world.pos;
                MarkerNotice("RETURNED TO DRY LAND");
                SPDLOG_INFO("[NativeWater] bail return tick={} sessionReady={} recoveryRequested=0", g.snapshot.tick,
                            g.runtime.Ready());
            } else {
                gDryLand.valid = false;
                SPDLOG_WARN("[NativeWater] saved dry anchor no longer valid; no unsafe teleport");
            }
            gWaterPending = false;
            gDryGate.Reset();
        }
        NativeSkateLink::UpdateVisual(play, player, g.snapshot.state,
                                      std::hypot(g.snapshot.trajectoryVelocity[0], g.snapshot.trajectoryVelocity[2]),
                                      g.snapshot.tick);
        return; // Underwater ragdoll is gameplay, not an OoT void/respawn.
    }
    if (Player_NativeWater(play, player) > 0 && gDryLand.valid) {
        const double now = NativeSkateInput::Now();
        if (now - gWaterLastReturn < 5.)
            ++gWaterRepeatCount;
        else
            gWaterRepeatCount = 0;
        // Newest stable same-room sample wins. Repeated fast bails exclude the
        // last return neighborhood rather than repeatedly returning to it.
        for (size_t n = 0; n < gDryHistoryCount; ++n) {
            const auto& sample = gDryHistory[(gDryHistoryHead + 31 - n) % 32];
            if (!sample.saved.valid || sample.saved.scene != play->sceneNum ||
                sample.saved.room != play->roomCtx.curRoom.num || sample.clearance < kWaterRecoveryClearance)
                continue;
            if (gWaterRepeatCount && std::hypot(sample.position.x - gWaterLastPosition.x,
                                                sample.position.z - gWaterLastPosition.z) < kWaterRecoveryClearance)
                continue;
            Vec3f current = player->actor.world.pos;
            player->actor.world.pos = sample.position;
            bool safe =
                Player_NativeDryFloor(play, player) && WaterClearance(play, sample.position) >= kWaterRecoveryClearance;
            player->actor.world.pos = current;
            if (safe) {
                gDryLand = sample.saved;
                gDryHost = sample.position;
                break;
            }
        }
        if (NativeSkateActors::Onboard(g.snapshot.stateId) && g.snapshot.stateId != 300) {
            if (!g.runtime.ActorBail(g.snapshot)) {
                SPDLOG_WARN("[NativeWater] native bail rejected: {}", g.runtime.Error());
                return;
            }
        }
        NativeSkateAudio::Stop();
        gWaterPending = true;
        gWaterTick = g.snapshot.tick;
        gDryGate.Reset();
        gSwordInput.Reset();
        SPDLOG_INFO(
            "[NativeWater] deep-water entry native bail state={} tick={} anchorScene={} room={} recoveryRequested=0",
            g.snapshot.state, g.snapshot.tick, gDryLand.scene, gDryLand.room);
        return;
    }
    const double dryNow = NativeSkateInput::Now();
    const float clearance = WaterClearance(play, player->actor.world.pos);
    if (gDryGate.Read(grounded && dryNow >= gDryHysteresisUntil && clearance >= kWaterRecoveryClearance &&
                          Player_NativeDryFloor(play, player) && play->transitionTrigger == TRANS_TRIGGER_OFF,
                      g.snapshot.tick) &&
        dryNow >= gDryNextSample) {
        NativeSkateRuntime::Marker dry;
        if (g.runtime.CaptureMarker(dry)) {
            gDryLand.valid = true;
            gDryLand.scene = play->sceneNum;
            gDryLand.room = play->roomCtx.curRoom.num;
            gDryLand.native = dry;
            gDryHost = player->actor.world.pos;
            // Stationary frames must not evict older, spatially distinct inland anchors.
            const auto& last = gDryHistory[(gDryHistoryHead + 31) % 32];
            if (!gDryHistoryCount || last.saved.room != gDryLand.room ||
                std::hypot(last.position.x - gDryHost.x, last.position.z - gDryHost.z) >= 30.f) {
                gDryHistory[gDryHistoryHead] = { gDryLand, gDryHost, dryNow, clearance,
                                                 std::hypot(g.snapshot.trajectoryVelocity[0],
                                                            g.snapshot.trajectoryVelocity[2]) };
                gDryHistoryHead = (gDryHistoryHead + 1) % 32;
                gDryHistoryCount = std::min(size_t(32), gDryHistoryCount + 1);
            }
            gDryNextSample = dryNow + .25;
        }
    }
    g.interaction = Player_UpdateNativeWorldInteractions(play, player, grounded);
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugTransitions"), 0)) {
        const auto* floor = player->actor.floorPoly;
        const unsigned exit =
            floor ? SurfaceType_GetSceneExitIndex(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) : 0;
        SPDLOG_INFO(
            "[NativeTraversal] tick={} scene={} room={} native={} {} {} player={} {} {} prev={} {} {} floor={} bg={} exit={} door={} csAction={} flags={} grounded={} result={} transition={} entrance={}",
            g.snapshot.tick, play->sceneNum, play->roomCtx.curRoom.num, g.actorPosition[0], g.actorPosition[1],
            g.actorPosition[2], player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z,
            player->actor.prevPos.x, player->actor.prevPos.y, player->actor.prevPos.z, player->actor.floorHeight,
            player->actor.floorBgId, exit, player->doorType, player->csAction, player->stateFlags1, grounded,
            g.interaction, play->transitionTrigger, play->nextEntranceIndex);
    }
    if (g.interaction || play->transitionTrigger != TRANS_TRIGGER_OFF) {
        // Stock doorway/cutscene owns traversal AFTER skating is suspended.
        // Retain its chosen actor position; do not restore an old scene anchor.
        const Vec3f stockPosition = player->actor.world.pos;
        g.runtime.Suspend();
        RestorePlayer(play->transitionTrigger != TRANS_TRIGGER_OFF ? OwnershipReason::SCENE_TRANSITION
                                                                   : OwnershipReason::TEMP_SUSPEND_CUTSCENE,
                      true);
        player->actor.world.pos = stockPosition;
        player->actor.prevPos = stockPosition;
        g.anchor.valid = false;
        g.interactionSuspended = true;
        g.status = "WORLD_INTERACTION";
        SPDLOG_INFO("[NativeSkate] stock world interaction={} scene={} entrance={}; Rust scene session suspended",
                    g.interaction, play->sceneNum, play->nextEntranceIndex);
        return;
    }
    NativeSkateLink::UpdateVisual(play, player, g.snapshot.state,
                                  std::hypot(g.snapshot.trajectoryVelocity[0], g.snapshot.trajectoryVelocity[2]),
                                  g.snapshot.tick);
}

void BodyDebug(PlayState* play) {
    const float* d = g.runtime.RiderDiagnostics();
    const auto* b = g.runtime.FinalPose();
    const auto& l = NativeSkateRetarget::BodyDiagnostics();
    auto frame = LinkFrame();
    float root[3] = {}, com[3] = {}, top[3] = {}, deck[3] = {};
    g.anchor.Position(g.snapshot.root + 12, root);
    g.anchor.Position(g.snapshot.trajectory, com);
    g.anchor.Position(g.snapshot.board + 12, deck);
    float ntop[3] = { g.snapshot.trajectory[0], d[22], g.snapshot.trajectory[2] };
    g.anchor.Position(ntop, top);
    float ceiling = INFINITY, nativeFloor = -INFINITY;
    int ceilingId = -1;
    float x = g.snapshot.trajectory[0], z = g.snapshot.trajectory[2];
    for (size_t i = 0; i + 8 < g.world.triangles.size(); i += 9) {
        auto t = &g.world.triangles[i];
        float den = (t[5] - t[8]) * (t[0] - t[6]) + (t[6] - t[3]) * (t[2] - t[8]);
        if (std::abs(den) < 1e-8f)
            continue;
        float a = ((t[5] - t[8]) * (x - t[6]) + (t[6] - t[3]) * (z - t[8])) / den,
              c = ((t[8] - t[2]) * (x - t[6]) + (t[0] - t[6]) * (z - t[8])) / den;
        float y = a * t[1] + c * t[4] + (1 - a - c) * t[7];
        if (a >= 0 && c >= 0 && a + c <= 1 && den > 0 && y <= g.snapshot.trajectory[1])
            nativeFloor = std::max(nativeFloor, y);
        if (a >= 0 && c >= 0 && a + c <= 1 && den < 0 && y > g.snapshot.trajectory[1] && y < ceiling) {
            ceiling = y;
            ceilingId = (int)i / 9;
        }
    }
    float cy[3] = { x, ceiling, z };
    if (ceilingId >= 0)
        g.anchor.Position(cy, cy);
    float dx = g.player->actor.world.pos.x - frame.position[0], dy = g.player->actor.world.pos.y - frame.position[1],
          dz = g.player->actor.world.pos.z - frame.position[2];
    float hostError = std::sqrt(dx * dx + dy * dy + dz * dz);
    bool sideContact = d[27] > 0 && d[29] <= .5f;
    const char* owner = hostError > .1f ? "HOST_CORRECTION" : sideContact ? "NATIVE_RIDER" : "NONE/UNKNOWN";
    float ground = g.player->actor.floorHeight;
    if (std::isfinite(nativeFloor)) {
        float floorPoint[3] = { x, nativeFloor, z };
        g.anchor.Position(floorPoint, floorPoint);
        ground = floorPoint[1];
    }
    float nativeL[3] = {}, nativeR[3] = {}, pelvis[3] = {};
    float pelvisLocal = 0;
    if (b) {
        for (uint32_t j = 0; j < b->count; ++j) {
            const auto& q = b->joints[j];
            if (!strcmp(q.name, "LEFTFOOT"))
                g.anchor.Position(q.world + 12, nativeL);
            if (!strcmp(q.name, "RIGHTFOOT"))
                g.anchor.Position(q.world + 12, nativeR);
            if (!strcmp(q.name, "HIPS")) {
                g.anchor.Position(q.world + 12, pelvis);
                pelvisLocal = q.local[13];
            }
        }
    }
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    int row = 1;
#define BODY_LINE(...)             \
    GfxPrint_SetPos(&p, 1, row++); \
    GfxPrint_Printf(&p, __VA_ARGS__)
    BODY_LINE("NATIVE ACTIVE %s AGE:%s", g.snapshot.state, frame.adult ? "ADULT" : "CHILD");
    BODY_LINE("COM Y:%.2f ACTOR Y:%.2f", com[1], g.player->actor.world.pos.y);
    BODY_LINE("ANIM ROOT Y:%.2f LINK BASE Y:%.2f", root[1], frame.position[1]);
    BODY_LINE("POSE ROOT LOCAL:%.0f PELVIS:%.3fm", l.localRoot[1], pelvisLocal);
    BODY_LINE("NATIVE FEET Y L/R:%.2f %.2f", nativeL[1], nativeR[1]);
    BODY_LINE("LINK FEET Y L/R:%.2f %.2f", l.joints[PLAYER_LIMB_L_FOOT][1], l.joints[PLAYER_LIMB_R_FOOT][1]);
    BODY_LINE("NATIVE/LINK PELVIS Y:%.2f %.2f", pelvis[1], l.joints[PLAYER_LIMB_WAIST][1]);
    BODY_LINE("DECK Y:%.2f FLOOR Y:%.2f", deck[1], ground);
    BODY_LINE("PUSH:%d SUPPORT LEFT:%.2f", (int)d[10], l.leftSupport);
    BODY_LINE("BODY PITCH:%.3f YAW:%.3f", d[5], d[6]);
    BODY_LINE("WHOLE:%d RAGDOLL:%d", l.whole, l.ragdoll);
    BODY_LINE("NATIVE BODY H/R:%.1f %.1f", d[20] * g.anchor.unitsPerMeter, d[21] * g.anchor.unitsPerMeter);
    BODY_LINE("WALL BODY SCALE:%.3f TARGET:%.1f", d[35], d[36] * NativeSkateUnits::kOotUnitsPerMeter);
    BODY_LINE("BODY TOP/BOTTOM:%.2f %.2f", top[1], top[1] - (d[22] - d[23]) * g.anchor.unitsPerMeter);
    BODY_LINE("VISIBLE HEIGHT:%.2f",
              frame.adult ? NativeSkateRiderScale::kAdultHeight : NativeSkateRiderScale::kChildHeight);
    BODY_LINE("OOT CYLINDER H/R:%d %d INACTIVE", g.player->cylinder.dim.height, g.player->cylinder.dim.radius);
    BODY_LINE("CEILING TRI:%d Y:%.2f", ceilingId, ceilingId < 0 ? 0 : cy[1]);
    BODY_LINE("OPENING:%.2f BLOCK:%s", ceilingId < 0 ? 0 : cy[1] - ground, owner);
    BODY_LINE("CONTACT PART:%d N:%.2f %.2f %.2f", (int)d[27] - 1, d[28], d[29], d[30]);
    BODY_LINE("HOST POST STEP ERROR:%.4f", hostError);
    BODY_LINE("UP Y ROOT/PELVIS/CHEST:%.2f %.2f %.2f", l.up[PLAYER_LIMB_ROOT][1], l.up[PLAYER_LIMB_WAIST][1],
              l.up[PLAYER_LIMB_UPPER][1]);
    BODY_LINE("BOARD UP:%.2f %.2f %.2f", g.snapshot.board[4], g.snapshot.board[5], g.snapshot.board[6]);
    BODY_LINE("BOARD/RIDER PARENTS:WORLD/WORLD");
    BODY_LINE("RAG HEAD/CHEST Y:%.1f %.1f", l.joints[PLAYER_LIMB_HEAD][1], l.joints[PLAYER_LIMB_UPPER][1]);
    BODY_LINE("RAG HANDS Y:%.1f %.1f", l.joints[PLAYER_LIMB_L_HAND][1], l.joints[PLAYER_LIMB_R_HAND][1]);
#undef BODY_LINE
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.BodyTrace"), 0)) {
        static uint64_t last = 0;
        if (last != g.snapshot.tick) {
            last = g.snapshot.tick;
            SPDLOG_INFO(
                "[NativeSkateBody] tick={} state={} COM_Y={} animationRootY={} pelvisLocal={} nativeFeetY={},{} LinkRootY={} localRootY={} LinkPelvisY={} LinkFeetY={},{} deckY={} floorY={} push={} supportLeft={} ragdoll={} pitch={} yaw={} ceilingTri={} opening={} contactPart={} normal={},{},{} owner={} hostError={}",
                last, g.snapshot.state, com[1], root[1], pelvisLocal, nativeL[1], nativeR[1], frame.position[1],
                l.localRoot[1], l.joints[PLAYER_LIMB_WAIST][1], l.joints[PLAYER_LIMB_L_FOOT][1],
                l.joints[PLAYER_LIMB_R_FOOT][1], deck[1], ground, d[10], l.leftSupport, d[0], d[5], d[6], ceilingId,
                ceilingId < 0 ? 0 : cy[1] - ground, (int)d[27] - 1, d[28], d[29], d[30], owner, hostError);
        }
    }
}

struct Relationship {
    float native[12] = {}, link[12] = {}, nativePosition[3] = {};
    bool valid = false;
};
Relationship RiderRelationships() {
    using namespace NativeSkateRetargetMath;
    Relationship result;
    const auto* pose = g.runtime.FinalPose();
    if (!pose)
        return result;
    const NativeSkateRuntime::PoseJoint* hip = nullptr;
    for (uint32_t i = 0; i < pose->count; ++i)
        if (!strcmp(pose->joints[i].name, "HIPS")) {
            hip = &pose->joints[i];
            break;
        }
    if (!hip)
        return result;
    M h, rest, board, link;
    const auto& info = NativeSkateRetarget::BodyDiagnostics();
    if (!Basis(hip->world, h) || !Basis(hip->restGlobal, rest) || !Basis(g.snapshot.board, board) ||
        !Basis(info.pelvisFrame, link) || !g.anchor.Position(hip->world + 12, result.nativePosition))
        return result;
    M body = Mul(h, Inverse(rest));
    V bp = { g.boardPosition[0], g.boardPosition[1], g.boardPosition[2] },
      np = { result.nativePosition[0], result.nativePosition[1], result.nativePosition[2] },
      lp = { info.pelvisFrame[12], info.pelvisFrame[13], info.pelvisFrame[14] };
    V rn = Mul(Inverse(body), bp - np), rl = Mul(Inverse(link), bp - lp);
    M nr = Mul(Inverse(body), board), lr = Mul(Inverse(link), board);
    result.native[0] = rn.x;
    result.native[1] = rn.y;
    result.native[2] = rn.z;
    result.link[0] = rl.x;
    result.link[1] = rl.y;
    result.link[2] = rl.z;
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b) {
            result.native[3 + a * 3 + b] = nr.v[a][b];
            result.link[3 + a * 3 + b] = lr.v[a][b];
        }
    result.valid = true;
    return result;
}
void AnimationTrace(const NativeSkateRuntime::Packet& packet) {
    const auto* pose = g.runtime.FinalPose();
    if (!pose)
        return;
    float com[3] = {};
    if (!g.anchor.Position(g.snapshot.trajectory, com) || !g.anchor.Position(g.snapshot.board + 12, g.boardPosition))
        return;
    const auto frame =
        NativeSkateLink::Build(com, g.snapshot.heading, g.spatialScale, LINK_IS_ADULT, g.player->actor.scale.y);
    const float* d = g.runtime.RiderDiagnostics();
    if (std::strcmp(g.snapshot.state, "BipedGround"))
        NativeSkateLink::TraceNativePose(g.player, frame, pose, g.anchor, d, g.snapshot.board, g.snapshot.bodyPitch);
    const auto& l = NativeSkateRetarget::BodyDiagnostics();
    const auto relations = RiderRelationships();
    std::ostringstream row;
    row << "tick=" << g.snapshot.tick << " state=" << g.snapshot.state
        << " poseSource=" << (std::strcmp(g.snapshot.state, "BipedGround") ? "NATIVE_FINAL" : "OOT_BIPED")
        << " rootSource=NATIVE_FINAL_POSE_LOCAL_TO_COM bodySource=CALIBRATED_HIPS boardSource="
        << (d[40] > 0 ? "CARRY_PRESENTATION" : "NATIVE_DECK") << " pushLifecycle=" << d[10] << " pushIntention=" << d[8]
        << " pushFoot=" << (l.leftSupport > .5f ? "RIGHT" : "LEFT") << " pitch=" << d[5] << " yaw=" << d[6]
        << " r2Sample=" << (int)packet.triggers[1] << " throwIntent=" << d[45] << " held=" << d[40]
        << " free=" << d[41];
    auto vec = [&](const char* name, const float* v, int n) {
        row << " " << name << "=";
        for (int a = 0; a < n; ++a) {
            if (a)
                row << ",";
            row << v[a];
        }
    };
    float root[3] = {}, body[3] = {};
    g.anchor.Position(g.snapshot.root + 12, root);
    g.anchor.Position(d + 60, body);
    vec("COM", com, 3);
    vec("nativeAnimationRoot", root, 3);
    vec("nativePhysicalRoot", body, 3);
    vec("LinkActorPlanned", frame.position, 3);
    vec("boardBasisWorld", g.snapshot.board, 12);
    vec("boardCenter", g.boardPosition, 3);
    for (uint32_t i = 0; i < pose->count; ++i)
        if (!strcmp(pose->joints[i].name, "HIPS") || !strcmp(pose->joints[i].name, "LEFTFOOT") ||
            !strcmp(pose->joints[i].name, "RIGHTFOOT")) {
            float v[3];
            if (g.anchor.Position(pose->joints[i].world + 12, v))
                vec(pose->joints[i].name, v, 3);
        }
    vec("LinkPelvisFK", l.joints[PLAYER_LIMB_WAIST], 3);
    vec("LinkLeftFootFK", l.joints[PLAYER_LIMB_L_FOOT], 3);
    vec("LinkRightFootFK", l.joints[PLAYER_LIMB_R_FOOT], 3);
    vec("LinkLocalRoot", l.localRoot, 3);
    if (relations.valid) {
        vec("BOARD_RELATIVE_TO_NATIVE_RIDER", relations.native, 12);
        vec("BOARD_RELATIVE_TO_LINK", relations.link, 12);
    }
    SPDLOG_INFO("[NativeAnimationTick] {}", row.str());
}
void AnimationDebug(PlayState* play) {
    const auto& l = NativeSkateRetarget::BodyDiagnostics();
    const auto r = RiderRelationships();
    const float* d = g.runtime.RiderDiagnostics();
    const auto input = NativeSkateInput::Current();
    const auto physical = NativeSkateInput::GetDiagnostics();
    auto frame = LinkFrame();
    const auto distance = [](const float* a, const float* b) {
        float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
        return std::sqrt(x * x + y * y + z * z);
    };
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    int row = 1;
#define ANIM_LINE(...)             \
    GfxPrint_SetPos(&p, 1, row++); \
    GfxPrint_Printf(&p, __VA_ARGS__)
    ANIM_LINE("NATIVE ACTIVE %s", g.snapshot.state);
    ANIM_LINE("POSE:%s", strcmp(g.snapshot.state, "BipedGround") ? "NATIVE_FINAL" : "OOT_BIPED");
    ANIM_LINE("RUNTIME:%s", g.runtime.StateName());
    ANIM_LINE("BODY:CALIBRATED NATIVE HIPS");
    ANIM_LINE("RIDER:%.1f %.1f %.1f", r.nativePosition[0], r.nativePosition[1], r.nativePosition[2]);
    ANIM_LINE("LINK PELVIS:%.1f %.1f %.1f", l.joints[PLAYER_LIMB_WAIST][0], l.joints[PLAYER_LIMB_WAIST][1],
              l.joints[PLAYER_LIMB_WAIST][2]);
    ANIM_LINE("ACTOR:%.1f %.1f %.1f", g.player->actor.world.pos.x, g.player->actor.world.pos.y,
              g.player->actor.world.pos.z);
    ANIM_LINE("BOARD:%.1f %.1f %.1f", g.boardPosition[0], g.boardPosition[1], g.boardPosition[2]);
    ANIM_LINE("VIS BOARD:%.1f %.1f %.1f", gBoardVisual[12], gBoardVisual[13], gBoardVisual[14]);
    ANIM_LINE("PHYS-RIDER:%.1f VIS-RIDER:%.1f", distance(g.boardPosition, r.nativePosition),
              distance(gBoardVisual + 12, l.joints[PLAYER_LIMB_WAIST]));
    ANIM_LINE("VIS HAND L/R:%.1f %.1f", distance(gBoardVisual + 12, l.joints[PLAYER_LIMB_L_HAND]),
              distance(gBoardVisual + 12, l.joints[PLAYER_LIMB_R_HAND]));
    ANIM_LINE("HELD SCALE:%.3f GRAB:%d", l.heldRelativeScale, (int)d[96]);
    ANIM_LINE("FOOT-BOARD L/R:%.2f %.2f", distance(l.joints[PLAYER_LIMB_L_FOOT], g.boardPosition),
              distance(l.joints[PLAYER_LIMB_R_FOOT], g.boardPosition));
    ANIM_LINE("BODY PITCH/YAW:%.3f %.3f", d[5], d[6]);
    ANIM_LINE("PUSH LIFECYCLE:%d INTENT:%.2f", (int)d[10], d[8]);
    ANIM_LINE("SUPPORT L:%.2f DECK L/R:%d/%d", l.leftSupport, (int)d[74], (int)d[75]);
    ANIM_LINE("R2 RAW:%d NORM:%.3f EDGE:%d", physical.r2Raw, input.triggers[1] / 255.f,
              (int)(g.r2PressTick == g.snapshot.tick));
    ANIM_LINE("THROW INTENT:%d COUNTDOWN:%d", (int)d[45], (int)d[44]);
    ANIM_LINE("BOARD:%s STATE:%d", d[40] > 0 ? "HELD" : d[41] > 0 ? "FREE" : "OTHER", (int)d[43]);
    ANIM_LINE("RENDER:%s", NativeSkateBoardPresentation::Name(gBoardMode));
    ANIM_LINE("ENTRANCE INDEX:%d DOOR TYPE:%d", play->nextEntranceIndex, g.player->doorType);
    ANIM_LINE("SCENE:%d TRANSITION:%s", play->sceneNum,
              play->transitionTrigger == TRANS_TRIGGER_OFF ? "OFF" : "PENDING");
    ANIM_LINE("PARENTS:RIDER WORLD / BOARD WORLD");
    ANIM_LINE("TRACE:%s (NATIVE TICKS)",
              CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.AnimationTrace"), 0) ? "ON" : "OFF");
#undef ANIM_LINE
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}

void HeldBoardDebug(PlayState* play) {
    const auto& b = NativeSkateRetarget::BodyDiagnostics();
    const float* d = g.runtime.RiderDiagnostics();
    auto r = RiderRelationships();
    auto distance = [](const float* a, const float* c) {
        float x = a[0] - c[0], y = a[1] - c[1], z = a[2] - c[2];
        return std::sqrt(x * x + y * y + z * z);
    };
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    int row = 1;
#define HELD_LINE(...)             \
    GfxPrint_SetPos(&p, 1, row++); \
    GfxPrint_Printf(&p, __VA_ARGS__)
    HELD_LINE("NATIVE %s / %s", g.runtime.StateName(), g.snapshot.state);
    HELD_LINE("BOARD MODE:%s", NativeSkateBoardPresentation::Name(gBoardMode));
    HELD_LINE("PHYSICAL:%.1f %.1f %.1f", g.boardPosition[0], g.boardPosition[1], g.boardPosition[2]);
    HELD_LINE("VISIBLE:%.1f %.1f %.1f", gBoardVisual[12], gBoardVisual[13], gBoardVisual[14]);
    HELD_LINE("NATIVE RIDER:%.1f %.1f %.1f", r.nativePosition[0], r.nativePosition[1], r.nativePosition[2]);
    for (auto i :
         { PLAYER_LIMB_WAIST, PLAYER_LIMB_L_HAND, PLAYER_LIMB_R_HAND, PLAYER_LIMB_L_FOOT, PLAYER_LIMB_R_FOOT }) {
        const char* name = i == PLAYER_LIMB_WAIST    ? "PELVIS"
                           : i == PLAYER_LIMB_L_HAND ? "L HAND"
                           : i == PLAYER_LIMB_R_HAND ? "R HAND"
                           : i == PLAYER_LIMB_L_FOOT ? "L FOOT"
                                                     : "R FOOT";
        HELD_LINE("%s:%.1f %.1f %.1f", name, b.joints[i][0], b.joints[i][1], b.joints[i][2]);
    }
    HELD_LINE("VIS-RIDER:%.1f PHYS-RIDER:%.1f", distance(gBoardVisual + 12, b.joints[PLAYER_LIMB_WAIST]),
              distance(g.boardPosition, r.nativePosition));
    HELD_LINE("VIS-HAND L/R:%.1f %.1f", distance(gBoardVisual + 12, b.joints[PLAYER_LIMB_L_HAND]),
              distance(gBoardVisual + 12, b.joints[PLAYER_LIMB_R_HAND]));
    HELD_LINE("VIS-FOOT L/R:%.1f %.1f", distance(gBoardVisual + 12, b.joints[PLAYER_LIMB_L_FOOT]),
              distance(gBoardVisual + 12, b.joints[PLAYER_LIMB_R_FOOT]));
    HELD_LINE("GRAB:%d BODY PITCH/YAW:%.2f %.2f", (int)d[96], g.snapshot.bodyPitch, g.snapshot.bodyYaw);
    HELD_LINE("RIDER-REL SCALE:%.3f", b.heldRelativeScale);
    HELD_LINE("SOURCE:FINAL SKATEBOARD_ROOT");
    HELD_LINE("PHYSICS BOARD:UNCHANGED");
    HELD_LINE("RIDER/BOARD:WORLD SIBLINGS");
#undef HELD_LINE
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}

void DrawMarkerNotice(PlayState* play) {
    if (gPaused || gMarkerNotice.empty() || g.snapshot.tick >= gMarkerNoticeEnd)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 0, 0, 0, 200);
    GfxPrint_SetPosPx(&p, 41, 177);
    GfxPrint_Printf(&p, "%s", gMarkerNotice.c_str());
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    GfxPrint_SetPosPx(&p, 40, 176);
    GfxPrint_Printf(&p, "%s", gMarkerNotice.c_str());
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
// Use stock font/HD-font setup, expanding only its rectangles to signed wide
// coordinates. The normal 12-bit N64 rectangles cannot anchor left on ultrawide.
void PromptText(GfxPrint* p, int x, int y, const char* text) {
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
void DrawTogglePrompt(PlayState* play, bool active) {
    const double now = NativeSkateInput::Now();
    if (active && !gPromptWasActive)
        gExitPromptStarted = now;
    gPromptWasActive = active;
    if (!CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.TogglePrompt"), 1))
        return;
    Player* player = GET_PLAYER(play);
    if (!player || play->pauseCtx.state || play->gameOverCtx.state || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->msgCtx.msgMode || Player_InCsMode(play) || GameInteractor_NoUIActive() ||
        (player->stateFlags1 &
         (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_CUTSCENE)) ||
        play->interfaceCtx.healthAlpha == 0)
        return;
    const double elapsed = now - gExitPromptStarted;
    if (active && elapsed >= 1.5)
        return;
    const int alpha = active ? int(255 * std::min(1.0, (1.5 - elapsed) / .35)) : 255;
    const int x =
        int(OTRGetDimensionFromLeftEdge(16.f)) + std::clamp(CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0), 0, 40);
    const int y = SCREEN_HEIGHT - 22 - std::clamp(CVarGetInteger(CVAR_COSMETIC("HUD.Margin.B"), 0), 0, 40);
    if (NativeSkateHud::FontReady()) {
        NativeSkateHud::Overlay(active ? "Press F8 to exit Skate Mode" : "Press F8 to enter Skate Mode",
                                float(16 + std::clamp(CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0), 0, 40)),
                                float(y), 8.5f, 245, 240, 220, alpha);
        return;
    }
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    const char* text = active ? "Press F8 to exit Skate Mode" : "Press F8 to enter Skate Mode";
    GfxPrint_SetColor(&p, 0, 0, 0, alpha);
    PromptText(&p, x + 1, y + 1, text);
    GfxPrint_SetColor(&p, 245, 240, 220, alpha);
    PromptText(&p, x, y, text);
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
void DrawRecoveryNotice(PlayState* play) {
    if (gRecoveryNotice.empty() || NativeSkateInput::Now() > gRecoveryNoticeUntil || play->pauseCtx.state ||
        play->msgCtx.msgMode || play->gameOverCtx.state || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->interfaceCtx.healthAlpha == 0)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    const int x = int(OTRGetDimensionFromLeftEdge(16.f));
    GfxPrint_SetColor(&p, 0, 0, 0, 220);
    PromptText(&p, x + 1, SCREEN_HEIGHT - 37, gRecoveryNotice.c_str());
    GfxPrint_SetColor(&p, 245, 240, 220, 255);
    PromptText(&p, x, SCREEN_HEIGHT - 38, gRecoveryNotice.c_str());
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
void DrawOwnershipDebug(PlayState* play) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.OwnershipDebug"), 0) || !GET_PLAYER(play))
        return;
    const auto& r = gOwnershipRing[(gOwnershipHead + 479) % 480];
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    GfxPrint_SetPos(&p, 1, 1);
    GfxPrint_Printf(&p, "OWNER:%s REQUEST:%d CVAR:%d ACTUAL:%d", NativeSkateOwnership::Name(gOwnershipMode),
                    g.requested, CVarGetInteger(kEnabledCvar, 0), OwnsCurrentPlayer());
    GfxPrint_SetPos(&p, 1, 2);
    GfxPrint_Printf(&p, "RUNTIME:%s VALID:%d", g.runtime.StateName(), g.runtime.Ready());
    GfxPrint_SetPos(&p, 1, 3);
    GfxPrint_Printf(&p, "SCENE/ROOM:%d/%d GEN:%llu/%llu", r.scene, r.room, gPlayerGeneration, gSceneGeneration);
    GfxPrint_SetPos(&p, 1, 4);
    GfxPrint_Printf(&p, "PLAYER:%p ERROR:%.2f", r.player, r.error);
    GfxPrint_SetPos(&p, 1, 5);
    GfxPrint_Printf(&p, "PLAYER:%.1f %.1f %.1f", r.playerPos[0], r.playerPos[1], r.playerPos[2]);
    GfxPrint_SetPos(&p, 1, 6);
    GfxPrint_Printf(&p, "NATIVE:%.1f %.1f %.1f", r.nativePos[0], r.nativePos[1], r.nativePos[2]);
    GfxPrint_SetPos(&p, 1, 7);
    GfxPrint_Printf(&p, "CS:%d TRANS:%d CONTACT:%p", r.cs, r.transition, r.contact);
    GfxPrint_SetPos(&p, 1, 8);
    GfxPrint_Printf(&p, "REASON:%s", NativeSkateOwnership::Name(gOwnershipReason));
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
void DrawUtilityOverlay(PlayState* play) {
    if (!gUtility.held || !OwnsCurrentPlayer() || NativeSkateObjectDropper::Active() || play->pauseCtx.state ||
        play->msgCtx.msgMode || Player_InCsMode(play) || gRecoveryQueued)
        return;
    constexpr int kUtilityTop = 80;
    constexpr float kUtilityRightPadding = 160.f;
    const int x = int(OTRGetDimensionFromRightEdge(SCREEN_WIDTH - kUtilityRightPadding));
    if (NativeSkateHud::FontReady()) {
        const char* lines[] = { "Skate Options", "D-Up: Load Marker", "D-Down: Place Marker", "B: Object Dropper" };
        for (int i = 0; i < 4; ++i)
            NativeSkateHud::Overlay(lines[i], 16, float(kUtilityTop + i * 12), i ? 8.f : 10.f, i ? 235 : 100, 245, 255,
                                    255, true);
        return;
    }
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    const char* lines[] = { "Skate Options", "D-Up: Load Marker", "D-Down: Place Marker", "B: Object Dropper" };
    for (int i = 0; i < 4; ++i) {
        int y = kUtilityTop + i * 12;
        GfxPrint_SetColor(&p, 0, 0, 0, 210);
        PromptText(&p, x + 1, y + 1, lines[i]);
        GfxPrint_SetColor(&p, i ? 235 : 100, 245, 255, 255);
        PromptText(&p, x, y, lines[i]);
    }
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
void OnDraw() {
    NativeSkateHud::ClearOverlays();
    ++g.drawTicks;
    if (g.drawTicks == 1)
        SPDLOG_INFO("[NativeSkate] OnPlayDrawEnd callback EXECUTED");
    PlayState* play = gPlayState;
    if (!play)
        return;
    const bool heldDebug =
        DeveloperDiagnostics() && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugHeldBoard"), 0) != 0;
    const bool animationDebug =
        DeveloperDiagnostics() && !heldDebug && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugAnimation"), 0) != 0;
    const bool bodyDebug = DeveloperDiagnostics() && !heldDebug && !animationDebug &&
                           CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugBody"), 0) != 0;
    const bool debug =
        DeveloperDiagnostics() && CVarGetInteger(kDebugCvar, 0) != 0 && !heldDebug && !bodyDebug && !animationDebug;
    const bool ownership = OwnsCurrentPlayer() && g.status == "ACTIVE" && g.requested;
    if (ownership && DeveloperDiagnostics()) {
        g.dynamic.Draw(play, g.runtime);
        NativeSkateGrindDebug::Draw(play, gGrinds, gGrindsReady);
    }
    float boardDraw[16] = {};
    bool drawBoard = true;
    if (ownership) {
        bool carry = NativeSkateBoardPresentation::Carry(g.snapshot.stateId, g.runtime.RiderDiagnostics()[40] > 0) &&
                     CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.BipedCarryBoard"), 1) != 0 && !ProxyRider();
        auto boardMode = carry ? NativeSkateBoardPresentation::Mode::CARRIED_BOARD
                               : (g.snapshot.stateId >= 200 && g.snapshot.stateId <= 202
                                      ? NativeSkateBoardPresentation::Mode::AIR_BOARD
                                      : NativeSkateBoardPresentation::Mode::PHYSICS_BOARD);
        if (carry)
            drawBoard = NativeSkateLink::CarryMatrix(LinkFrame(), g.spatialScale, boardDraw);
        else if (!ProxyRider() && g.runtime.RiderDiagnostics()[96] == 1.f &&
                 NativeSkateRetarget::HeldBoard(g.runtime.FinalPose(), g.anchor, g.runtime.RiderDiagnostics(),
                                                boardDraw)) {
            drawBoard = true;
            boardMode = NativeSkateBoardPresentation::Mode::HELD_BOARD;
        } else {
            drawBoard = NativeSkateProxyGeometry::BoardRotation(g.snapshot.board, boardDraw);
            for (int i = 0; i < 3; ++i)
                boardDraw[12 + i] = g.boardPosition[i];
        }
        if (drawBoard) {
            if (gVert.Handplant()) {
                const auto& body = NativeSkateRetarget::BodyDiagnostics();
                if (body.poseTick == g.snapshot.tick)
                    for (int i = 0; i < 3; ++i)
                        boardDraw[12 + i] += body.handplantOffset[i];
            }
            float riderFrame[16] = {};
            auto frame = LinkFrame();
            const auto& body = NativeSkateRetarget::BodyDiagnostics();
            if (!carry && body.poseTick == g.snapshot.tick && body.valid[PLAYER_LIMB_WAIST]) {
                std::copy_n(body.linkBodyFrame, 16, riderFrame);
                for (int a = 0; a < 3; ++a)
                    riderFrame[12 + a] = body.joints[PLAYER_LIMB_WAIST][a];
            } else {
                float y = frame.yaw * (3.14159265358979323846f / 32768.f);
                riderFrame[0] = riderFrame[10] = std::cos(y);
                riderFrame[2] = -std::sin(y);
                riderFrame[8] = std::sin(y);
                riderFrame[5] = riderFrame[15] = 1;
                for (int a = 0; a < 3; ++a)
                    riderFrame[12 + a] = frame.position[a];
            }
            if (gHaveVisualBoard && boardMode != gBoardMode) {
                // Native authored airborne/catch motion must not be delayed by a
                // second visual blend. Only board-handling ownership needs one.
                const bool handling = boardMode == NativeSkateBoardPresentation::Mode::CARRIED_BOARD ||
                                      gBoardMode == NativeSkateBoardPresentation::Mode::CARRIED_BOARD;
                gBoardBlendLocal = handling && NativeSkateBoardPresentation::Relative(gPreviousBoardFrame, gBoardVisual,
                                                                                      gBoardBlendFrom);
                gBoardBlendTick = handling ? g.snapshot.tick : 0;
            }
            float duration = NativeSkatePresentationMath::Bound(
                CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.PresentationBlendTime"), .2f), .2f, .1f, .35f);
            float elapsed = g.snapshot.tick >= gBoardBlendTick ? (g.snapshot.tick - gBoardBlendTick) / 60.f : duration;
            float movingStart[16] = {};
            if (gHaveVisualBoard && gBoardBlendLocal && gBoardBlendTick && elapsed < duration &&
                NativeSkateBoardPresentation::World(riderFrame, gBoardBlendFrom, movingStart)) {
                float t = NativeSkatePresentationMath::Smooth(elapsed / duration);
                NativeSkatePresentationMath::Blend(movingStart, boardDraw, t, gBoardVisual);
                float reach = std::hypot(boardDraw[12] - movingStart[12], boardDraw[14] - movingStart[14]);
                gBoardVisual[13] += std::min(12.f, reach * .12f) * std::sin(t * 3.14159265359f);
            } else
                std::copy_n(boardDraw, 16, gBoardVisual);
            std::copy_n(riderFrame, 16, gPreviousBoardFrame);
            gHaveVisualBoard = true;
            gWasCarry = carry;
            gBoardMode = boardMode;
            if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugBoardFrames"), 0)) {
                static uint64_t loggedTick = ~uint64_t(0);
                if (loggedTick != g.snapshot.tick) {
                    loggedTick = g.snapshot.tick;
                    float relative[16] = {};
                    NativeSkateBoardPresentation::Relative(riderFrame, gBoardVisual, relative);
                    SPDLOG_INFO(
                        "[NativeBoardFrame] tick={} state={} mode={} physical={} {} {} visible={} {} {} rider={} {} {} relative={} {} {} blend={} pause={}",
                        g.snapshot.tick, g.snapshot.state, NativeSkateBoardPresentation::Name(boardMode),
                        g.boardPosition[0], g.boardPosition[1], g.boardPosition[2], gBoardVisual[12], gBoardVisual[13],
                        gBoardVisual[14], riderFrame[12], riderFrame[13], riderFrame[14], relative[12], relative[13],
                        relative[14], elapsed < duration, gPaused);
                    auto matrixText = [](const float* m) {
                        std::ostringstream text;
                        for (int i = 0; i < 16; ++i) {
                            if (i)
                                text << ',';
                            text << m[i];
                        }
                        return text.str();
                    };
                    float physical[16], physicalRelative[16] = {}, held[16], heldRelative[16] = {};
                    std::copy_n(g.snapshot.board, 16, physical);
                    for (int a = 0; a < 3; ++a)
                        physical[12 + a] = g.boardPosition[a];
                    NativeSkateBoardPresentation::Relative(body.nativeBodyFrame, physical, physicalRelative);
                    std::copy_n(g.runtime.RiderDiagnostics() + 80, 16, held);
                    g.anchor.Position(g.runtime.RiderDiagnostics() + 92, held + 12);
                    NativeSkateBoardPresentation::Relative(body.nativeBodyFrame, held, heldRelative);
                    const auto* pose = g.runtime.FinalPose();
                    SPDLOG_INFO(
                        "[NativeBoardMatrices] tick={} nativeRider={} animationToWorld={} physicalBoard={} heldBoard={} linkRider={} visibleBoard={} physicalRelative={} heldRelative={} visibleRelative={}",
                        g.snapshot.tick, matrixText(body.nativeBodyFrame),
                        pose ? matrixText(pose->root) : "UNAVAILABLE", matrixText(physical), matrixText(held),
                        matrixText(riderFrame), matrixText(gBoardVisual), matrixText(physicalRelative),
                        matrixText(heldRelative), matrixText(relative));
                }
            }
        }
    }
    if (ownership)
        NativeSkateProxy::Draw(play, g.snapshot, g.actorPosition, g.boardPosition, debug, (uint32_t)g.world.accepted,
                               NativeSkateInput::Retained(), g.error.c_str(), g.spatialScale,
                               g.runtime.PushDiagnostics(), g.runtime.BipedDiagnostics(), gPreviousState.c_str(),
                               gMoveDot, gMoveDotValid, ProxyRider(), gBoardVisual, drawBoard);
    if (ownership)
        NativeSkateWorldDebug::Draw(play, g.adapted, g.world, g.profile, g.snapshot, g.anchor, g.spatialScale,
                                    g.runtime.PushDiagnostics(), debug);
    if (ownership && debug) {
        float root[3] = {};
        g.anchor.Position(&g.snapshot.root[12], root);
        NativeSkateLink::Debug(play, g.player, LinkFrame(), root, ProxyRider(), gBoardVisual, gWasCarry,
                               g.spatialScale);
    }
    if (ownership && DeveloperDiagnostics())
        NativeSkateRetarget::Debug(play, g.runtime.FinalPose(), g.anchor,
                                   !ProxyRider() && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.LinkRetarget"), 1) &&
                                       std::string(g.snapshot.state) != "BipedGround",
                                   g.snapshot.state, g.runtime.PoseError().c_str());
    if (ownership && bodyDebug)
        BodyDebug(play);
    if (ownership && animationDebug)
        AnimationDebug(play);
    if (ownership && heldDebug)
        HeldBoardDebug(play);
    if (ownership)
        DrawMarkerNotice(play);
    DrawTogglePrompt(play, g.requested);
    NativeSkateObjectDropper::Draw(play);
    DrawRecoveryNotice(play);
    DrawUtilityOverlay(play);
    if (DeveloperDiagnostics())
        DrawOwnershipDebug(play);
    if (ownership && !NativeSkateObjectDropper::Active()) {
        NativeSkatePerf::Scope timing(NativeSkatePerf::HudDraw);
        NativeSkateHud::Draw(play, gHud);
    }
    if ((debug || animationDebug || bodyDebug || heldDebug) && !ownership)
        NativeSkateProxy::DrawModuleStatus(play, g.updateTicks, g.drawTicks, g.requested, g.enabled, g.wasF8,
                                           g.f8Pressed, ownership, g.status.c_str(), g.error.c_str());
    if (debug) {
        const auto input = NativeSkateInput::GetDiagnostics();
        NativeSkateProxy::DrawPushStatus(
            play, input.presses, g.pushEvents, g.applications, g.lastPushDelta, g.runtime.PushDiagnostics()[0],
            input.elapsed > 0 ? g.updates / input.elapsed : 0, input.elapsed > 0 ? input.samples / input.elapsed : 0,
            input.elapsed > 0 ? (g.updateTicks - g.hostUpdateBase) / input.elapsed : 0, input.xDown,
            g.stopReason.c_str());
    }
}
} // namespace
void RegisterNativeSkateMode() {
    static bool registered = false;
    if (registered)
        return;
    // ShipInit runs after config loading. This guard prevents config/UI re-init
    // from resetting an in-process toggle; only Enabled is process session state.
    CVarSetInteger(kEnabledCvar, 0);
    ++gStartupResetCount;
    SPDLOG_INFO("[NativeSkate] startup gameplay OFF; background prewarm retained STARTUP_RESET_COUNT={}",
                gStartupResetCount);
    SPDLOG_INFO("[SkateHarkinian] Playtest {} runtime ABI=4 props=50 audio=161 slots", SKATEHARKINIAN_BUILD_ID);
    NativeSkateInput::Initialize();
    NativeSkateObjectDropper::Initialize([](Actor* a) { g.dynamic.ActorInit(a); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(OnFrame);
    SPDLOG_INFO("[NativeSkate] OnGameFrameUpdate hook registered");
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdate);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([] { SuspendForLoading(true); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        ++gSceneGeneration;
        gMarker.valid = false;
        gMarkerCameraPending = false;
        g.interactionSuspended = false;
        g.enabled = g.requested;
        g.status = "INITIALIZING";
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>([](void* raw) {
        Actor* actor = (Actor*)raw;
        if (actor && actor->id == ACTOR_PLAYER) {
            NativeSkateObjectDropper::Clear("OBJECT_PLAYER_DESTROY");
            RestorePlayer(OwnershipReason::PLAYER_REPLACED, true);
            g.player = nullptr;
            g.play = nullptr;
            g.anchor.valid = false;
            ++gPlayerGeneration;
            gMarker.valid = false;
        }
        g.dynamic.ActorInit(actor);
        g.actors.ActorInit(actor, g.runtime);
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](void* raw) {
        Actor* actor = (Actor*)raw;
        if (actor && actor->id == ACTOR_PLAYER)
            ++gPlayerGeneration;
        g.dynamic.ActorInit(actor);
        g.actors.ActorInit(actor, g.runtime);
    });
    SPDLOG_INFO("[NativeSkate] OnPlayerUpdate hook registered");
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnDraw);
    SPDLOG_INFO("[NativeSkate] OnPlayDrawEnd hook registered");
    registered = true;
}
static RegisterShipInitFunc initFunc(RegisterNativeSkateMode);

// Read-only camera anchor adapter. The actor remains Rust COM; camera follows
// the actual Link presentation frame rather than adding OoT height atop COM.
extern "C" void NativeSkateCameraAnchor(Player* player, Vec3f* position) {
    if (!position || !player || !OwnsCurrentPlayer() || player != g.player || g.status != "ACTIVE" || ProxyRider())
        return;
    auto frame = LinkFrame();
    if (!frame.valid)
        return;
    float modelHeight = frame.adult ? NativeSkateRiderScale::kAdultHeight : NativeSkateRiderScale::kChildHeight;
    float offset = NativeSkatePresentationMath::Bound(
        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.CameraTargetOffset"), 0), 0, -20, 20);
    float chest = frame.position[1] + modelHeight * .72f * (player->actor.scale.y / .01f) + offset;
    position->y = chest - Player_GetHeight(player);
    player->actor.focus.pos = { frame.position[0], chest, frame.position[2] };
}

extern "C" void NativeSkateFilterHostInput(Input* input) {
    static_assert(BTN_L == NativeSkateUtility::L && BTN_B == NativeSkateUtility::Circle &&
                      BTN_DUP == NativeSkateUtility::Up && BTN_DDOWN == NativeSkateUtility::Down,
                  "utility logical masks");
    if (!input)
        return;
    const bool available = g.requested && OwnsCurrentPlayer() && !gRecoveryQueued && g.play->pauseCtx.state == 0 &&
                           g.play->transitionTrigger == TRANS_TRIGGER_OFF && !g.play->msgCtx.msgMode &&
                           !Player_InCsMode(g.play);
    auto commands =
        gUtility.Read(input->cur.button, input->press.button, available, NativeSkateObjectDropper::Active());
    if (!available) {
        gUtilityCommands = {};
        gUtilityObjectInteract = false;
        return;
    }
    gUtilityCommands.set |= commands.set;
    gUtilityCommands.load |= commands.load;
    gUtilityCommands.toggle |= commands.toggle;
    const bool editor = NativeSkateObjectDropper::Active();
    if (commands.set)
        gUtilityObjectInteract = NativeSkateBoardPresentation::Biped(g.snapshot.stateId) && (input->cur.button & BTN_R);
    const uint16_t mask = gUtility.HostMask(editor, commands.toggle) | (gUtilityObjectInteract ? BTN_R : 0);
    input->cur.button &= ~mask;
    input->press.button &= ~mask;
    input->rel.button &= ~mask;
}

extern "C" void NativeSkateMarkerCameraApply(Camera* camera) {
    if (!gMarkerCameraPending)
        return;
    if (!gMarker.valid || !gPlayState || gMarker.scene != gPlayState->sceneNum || !OwnsCurrentPlayer()) {
        gMarkerCameraPending = false;
        return;
    }
    auto* play = gPlayState;
    if (!camera || camera != GET_ACTIVE_CAM(play) || play->activeCamera != CAM_ID_MAIN ||
        NativeSkateObjectDropper::Active() || play->pauseCtx.state || play->msgCtx.msgMode || Player_InCsMode(play) ||
        play->transitionTrigger != TRANS_TRIGGER_OFF)
        return;
    auto* player = GET_PLAYER(play);
    float root[] = { player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z }, at[3], eye[3];
    const auto& saved = gMarker.camera;
    saved.Restore(root, at, eye);
    camera->at = { at[0], at[1], at[2] };
    camera->eye = camera->eyeNext = { eye[0], eye[1], eye[2] };
    camera->dist = saved.distance;
    camera->fov = saved.fov;
    camera->roll = saved.roll;
    play->camX = (s16)std::lround(saved.yaw * 32768.f / 3.14159265359f);
    play->camY = (s16)std::lround(saved.pitch * 32768.f / 3.14159265359f);
    play->manualCamera = saved.manual;
    if (saved.manual && CVarGetInteger(CVAR_SETTING("FreeLook.Enabled"), 0) && saved.zoomSetting > 0)
        CVarSetInteger(CVAR_SETTING("FreeLook.MaxCameraDistance"), saved.zoomSetting);
    gMarkerCameraPending = false;
    CaptureCamera(play);
}

extern "C" int NativeSkateBipedCameraEnabled(Camera* camera) {
    if (!camera || !OwnsCurrentPlayer() || gRecoveryQueued || gPaused || NativeSkateObjectDropper::Active())
        return 0;
    auto* play = g.play;
    if (!play || camera != GET_ACTIVE_CAM(play) || play->activeCamera != CAM_ID_MAIN ||
        camera->mode != CAM_MODE_NORMAL || play->pauseCtx.state || play->msgCtx.msgMode || Player_InCsMode(play) ||
        play->transitionTrigger != TRANS_TRIGGER_OFF)
        return 0;
    return NativeSkateBoardPresentation::Biped(g.snapshot.stateId);
}
extern "C" float NativeSkateBipedCameraTimeScale(Camera* camera) {
    static uint64_t lastGeneration = ~uint64_t(0);
    static double previous = 0;
    double now = NativeSkateInput::Now(), elapsed = now - previous;
    float dt = lastGeneration == gSceneGeneration && elapsed > 0 && elapsed < .1 ? (float)elapsed : 1.f / 20.f;
    previous = now;
    lastGeneration = gSceneGeneration;
    return dt * 20.f; // Match stock 20Hz orbit rate while independent of update cadence.
}

extern "C" uint64_t NativeSkateSceneGeneration() {
    return gSceneGeneration;
}

extern "C" int NativeSkateOwnsCurrentPlayer() {
    return OwnsCurrentPlayer();
}

extern "C" int NativeSkateHandplantTarget(float out[3]) {
    if (!gVert.Handplant() || !OwnsCurrentPlayer())
        return 0;
    out[0] = gVert.anchor.x;
    out[1] = gVert.anchor.y;
    out[2] = gVert.anchor.z;
    return 1;
}

extern "C" float NativeSkateHandplantWeight() {
    if (!gVert.Handplant())
        return 0;
    return gVert.Weight();
}
