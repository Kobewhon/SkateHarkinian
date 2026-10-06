#include "NativeSkateRuntime.h"
#include "NativeSkatePerf.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <sstream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

namespace NativeSkateRuntime {
namespace {
bool Finite(const float* values, size_t count) {
    return std::all_of(values, values + count, [](float v) { return std::isfinite(v); });
}
} // namespace
using Clock = std::chrono::steady_clock;
static double Milliseconds(Clock::time_point from) {
    return std::chrono::duration<double, std::milli>(Clock::now() - from).count();
}
Session::~Session() {
    Stop();
}
const char* Session::StateName() const {
    switch (State()) {
        case WarmState::UNINITIALIZED:
            return "UNINITIALIZED";
        case WarmState::PREWARMING:
            return "PREWARMING";
        case WarmState::READY:
            return "READY";
        case WarmState::ACTIVE:
            return "ACTIVE";
        default:
            return "FAILED";
    }
}
void Session::Prewarm(const std::string& root) {
    WarmState expected = WarmState::UNINITIALIZED;
    if (!mWarmState.compare_exchange_strong(expected, WarmState::PREWARMING))
        return;
    // No actors, CVars, SDL or graphics are accessed on this loader thread.
    // The DLL constructs/steps its own existing 16 MB physics worker.
    try {
        mPrewarmThread = std::thread([this, root] {
            auto begin = Clock::now();
            const float spawn[3] = { 0, 0, 0 };
            const std::vector<float> plane = { -100, 0, -100, 100,  0, 100, 100, 0, -100,
                                               -100, 0, -100, -100, 0, 100, 100, 0, 100 };
            bool ok = false;
            try {
                ok = Start(root, plane, spawn, 0);
            } catch (const std::exception& e) {
                mError = e.what();
                DestroyLoaded();
            } catch (...) {
                mError = "Background runtime prewarm exception";
                DestroyLoaded();
            }
            mPrewarmMs = Milliseconds(begin);
            mWarmState.store(ok ? WarmState::READY : WarmState::FAILED, std::memory_order_release);
        });
    } catch (const std::exception& e) {
        mError = e.what();
        mWarmState.store(WarmState::FAILED, std::memory_order_release);
    }
}
void Session::Suspend() {
    if (State() == WarmState::ACTIVE)
        mWarmState.store(WarmState::READY, std::memory_order_release);
}
std::string Session::Profile() const {
    if (State() == WarmState::PREWARMING)
        return "PREWARMING (background; OoT owns player)";
    std::ostringstream s;
    s << "NATIVE SKATE ACTIVATION PROFILE prewarm/background_ms=" << mPrewarmMs << " DLL_LOAD_ms=" << mDllMs
      << " ABI_ms=" << mAbiMs << " DATA_PATH_ms=" << mPathMs << " SESSION_CREATE_ms=" << mCreateMs
      << " WORLD_ms=" << mWorldMs << " RESEED_ms=" << mActivateMs << " NATIVE_STEP_ms=" << mStepMs
      << " POSE_READ_ms=" << mPoseMs;
    return s.str();
}
bool Session::CaptureMarker(Marker& marker) {
    if (!Ready() || !mCaptureMarker || !mCaptureMarker(mSession, &marker)) {
        mError = mLastError() ? mLastError() : "Native marker capture unavailable";
        return false;
    }
    return true;
}
bool Session::LoadMarker(const Marker& marker, Snapshot& output) {
    if (!Ready() || !mLoadMarker || !mLoadMarker(mSession, &marker, &output)) {
        mError = mLastError() ? mLastError() : "Native marker load unavailable";
        return false;
    }
    if (!Validate(output))
        return false;
    if (!mReadBiped(mSession, mBipedDiagnostics, 24) || !mReadPushDiagnostics(mSession, mPushDiagnostics, 24)) {
        mError = "Marker diagnostic refresh failed";
        return false;
    }
    ReadPose(output.tick);
    return true;
}
bool Session::WaterReturn(const Marker& marker, Snapshot& output) {
    if (!Ready() || !mWaterReturn || !mWaterReturn(mSession, &marker, &output)) {
        mError = mLastError() ? mLastError() : "Native water return unavailable";
        return false;
    }
    if (!Validate(output))
        return false;
    if (!mReadBiped(mSession, mBipedDiagnostics, 24) || !mReadPushDiagnostics(mSession, mPushDiagnostics, 24))
        return false;
    if (mReadRider)
        mReadRider(mSession, mRiderDiagnostics, 256);
    ReadPose(output.tick);
    return true;
}
bool Session::Activate(const std::vector<float>& triangles, const float spawn[3], float heading, Snapshot& output) {
    mModifierValid = false;
    NativeSkatePerf::Scope reseedTiming(NativeSkatePerf::SceneReseed);
    if (!Ready() || !mActivate) {
        mError = "Native runtime not prewarmed";
        return false;
    }
    auto time = Clock::now();
    if (!ReplaceWorld(triangles, output))
        return false;
    mWorldMs = Milliseconds(time);
    time = Clock::now();
    if (!mActivate(mSession, spawn, heading, &output)) {
        mError = mLastError() ? mLastError() : "Native reseed failed";
        return false;
    }
    mActivateMs = Milliseconds(time);
    if (!Validate(output))
        return false;
    ReadPose(output.tick);
    mWarmState.store(WarmState::ACTIVE, std::memory_order_release);
    return true;
}
std::string Session::ExecutableDirectory() {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    std::string value(path, n);
    auto slash = value.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : value.substr(0, slash);
#else
    return {};
#endif
}
bool Session::Start(const std::string& dataRoot, const std::vector<float>& triangles, const float spawn[3],
                    float heading) {
    DestroyLoaded();
    mError.clear();
    auto profileTime = Clock::now();
#ifdef _WIN32
    const auto exeDir = ExecutableDirectory();
    if (exeDir.empty()) {
        mError = "Could not resolve executable directory";
        return false;
    }
    mDllPath = exeDir + "\\skateharkinian_runtime.dll";
    const DWORD attrs = GetFileAttributesA(mDllPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        mError = "Runtime DLL missing: " + mDllPath;
        return false;
    }
    mModule = LoadLibraryExA(mDllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!mModule) {
        mError = "LoadLibraryExA failed: " + std::to_string(GetLastError()) + " path=" + mDllPath;
        return false;
    }
    mDllMs = Milliseconds(profileTime);
    profileTime = Clock::now();
    mCaptureMarker =
        reinterpret_cast<decltype(mCaptureMarker)>(GetProcAddress((HMODULE)mModule, "sh_skate_marker_capture"));
    mLoadMarker = reinterpret_cast<decltype(mLoadMarker)>(GetProcAddress((HMODULE)mModule, "sh_skate_marker_load"));
    mWaterReturn = reinterpret_cast<decltype(mWaterReturn)>(GetProcAddress((HMODULE)mModule, "sh_skate_water_return"));
    mActorContacts =
        reinterpret_cast<decltype(mActorContacts)>(GetProcAddress((HMODULE)mModule, "sh_skate_actor_contacts"));
    mActorBail = reinterpret_cast<decltype(mActorBail)>(GetProcAddress((HMODULE)mModule, "sh_skate_actor_bail"));
    mActivate = reinterpret_cast<decltype(mActivate)>(GetProcAddress((HMODULE)mModule, "sh_skate_activate"));
    mAbi = reinterpret_cast<decltype(mAbi)>(GetProcAddress((HMODULE)mModule, "sh_skate_abi"));
    mLastError = reinterpret_cast<decltype(mLastError)>(GetProcAddress((HMODULE)mModule, "sh_skate_error"));
    mCreate = reinterpret_cast<decltype(mCreate)>(GetProcAddress((HMODULE)mModule, "sh_skate_create"));
    mStepSamples = reinterpret_cast<decltype(mStepSamples)>(GetProcAddress((HMODULE)mModule, "sh_skate_step_samples"));
    mReadPushDiagnostics =
        reinterpret_cast<decltype(mReadPushDiagnostics)>(GetProcAddress((HMODULE)mModule, "sh_skate_push_diagnostics"));
    mLegacyPop = reinterpret_cast<decltype(mLegacyPop)>(GetProcAddress((HMODULE)mModule, "sh_skate_legacy_pop"));
    mProbe = reinterpret_cast<decltype(mProbe)>(GetProcAddress((HMODULE)mModule, "sh_skate_world_probe"));
    mModifiers = reinterpret_cast<decltype(mModifiers)>(GetProcAddress((HMODULE)mModule, "sh_skate_modifiers"));
    mVertStep = reinterpret_cast<decltype(mVertStep)>(GetProcAddress((HMODULE)mModule, "sh_skate_vert_step"));
    mStepCamera =
        reinterpret_cast<decltype(mStepCamera)>(GetProcAddress((HMODULE)mModule, "sh_skate_step_samples_camera"));
    mReadBiped = reinterpret_cast<decltype(mReadBiped)>(GetProcAddress((HMODULE)mModule, "sh_skate_biped_diagnostics"));
    mSetRiderHeight =
        reinterpret_cast<decltype(mSetRiderHeight)>(GetProcAddress((HMODULE)mModule, "sh_skate_rider_height"));
    mReadRider = reinterpret_cast<decltype(mReadRider)>(GetProcAddress((HMODULE)mModule, "sh_skate_rider_diagnostics"));
    mReadPose = reinterpret_cast<decltype(mReadPose)>(GetProcAddress((HMODULE)mModule, "sh_skate_read_pose"));
    mEvents = reinterpret_cast<decltype(mEvents)>(GetProcAddress((HMODULE)mModule, "sh_skate_events"));
    mLifecycleStats =
        reinterpret_cast<decltype(mLifecycleStats)>(GetProcAddress((HMODULE)mModule, "sh_skate_lifecycle_stats"));
    mContacts = reinterpret_cast<decltype(mContacts)>(GetProcAddress((HMODULE)mModule, "sh_skate_contacts"));
    mScoreReset = reinterpret_cast<decltype(mScoreReset)>(GetProcAddress((HMODULE)mModule, "sh_skate_score_reset"));
    mDynamicGrinds =
        reinterpret_cast<decltype(mDynamicGrinds)>(GetProcAddress((HMODULE)mModule, "sh_skate_dynamic_grinds"));
    mGrinds = reinterpret_cast<decltype(mGrinds)>(GetProcAddress((HMODULE)mModule, "sh_skate_grinds"));
    mDynamic = reinterpret_cast<decltype(mDynamic)>(GetProcAddress((HMODULE)mModule, "sh_skate_dynamic"));
    mWorld = reinterpret_cast<decltype(mWorld)>(GetProcAddress((HMODULE)mModule, "sh_skate_world"));
    mWorkerProgress =
        reinterpret_cast<decltype(mWorkerProgress)>(GetProcAddress((HMODULE)mModule, "sh_skate_worker_progress"));
    mDestroyChecked =
        reinterpret_cast<decltype(mDestroyChecked)>(GetProcAddress((HMODULE)mModule, "sh_skate_destroy_checked"));
    mDestroy = reinterpret_cast<decltype(mDestroy)>(GetProcAddress((HMODULE)mModule, "sh_skate_destroy"));
    if (!mLegacyPop || !mProbe || !mModifiers || !mVertStep || !mAbi || !mLastError || !mCreate || !mStepSamples ||
        !mWorld || !mDestroy || !mReadPushDiagnostics || !mStepCamera || !mReadBiped || !mReadRider ||
        !mSetRiderHeight || !mActivate || !mCaptureMarker || !mLoadMarker || !mWaterReturn || !mDynamic ||
        !mDynamicGrinds || !mGrinds || !mEvents || !mScoreReset || !mContacts) {
        mError = "Required ABI v4 gameplay/collision/grind/events/pose/water symbol missing";
        DestroyLoaded();
        return false;
    }
    if (mAbi() != kAbi) {
        mError = "ABI mismatch expected=4 actual=" + std::to_string(mAbi());
        DestroyLoaded();
        return false;
    }
    mAbiMs = Milliseconds(profileTime);
    profileTime = Clock::now();
    std::error_code ec;
    if (!std::filesystem::is_directory(dataRoot, ec) ||
        !std::filesystem::exists(std::filesystem::path(dataRoot) / "board.json", ec)) {
        mError = "Prepared runtime data missing: " + dataRoot;
        DestroyLoaded();
        return false;
    }
    mPathMs = Milliseconds(profileTime);
    profileTime = Clock::now();
    if (triangles.empty() || triangles.size() % 9 || triangles.size() / 9 > 1000000 ||
        !Finite(triangles.data(), triangles.size()) || !Finite(spawn, 3) || !std::isfinite(heading)) {
        mError = "Invalid session input/collision geometry";
        DestroyLoaded();
        return false;
    }
    mSession = mCreate(dataRoot.c_str(), triangles.data(), (uint32_t)(triangles.size() / 9), spawn, heading);
    mCreateMs = Milliseconds(profileTime);
    if (!mSession) {
        mError = mLastError() ? mLastError() : "Rust session creation failed without error";
        DestroyLoaded();
        return false;
    }
    return true;
#else
    (void)dataRoot;
    (void)triangles;
    (void)spawn;
    (void)heading;
    mError = "ABI v4 adapter supports Windows x64 only";
    return false;
#endif
}
bool Session::Validate(const Snapshot& s) {
    if (!Finite(s.root, 16)) {
        mError = "Nonfinite Rust root matrix";
        return false;
    }
    if (!Finite(s.board, 16)) {
        mError = "Nonfinite Rust board matrix";
        return false;
    }
    if (!Finite(s.trajectory, 3)) {
        mError = "Nonfinite Rust COM position";
        return false;
    }
    if (!Finite(s.trajectoryVelocity, 3)) {
        mError = "Nonfinite Rust COM velocity";
        return false;
    }
    if (!Finite(s.groundNormal, 3) || !std::isfinite(s.heading)) {
        mError = "Nonfinite Rust ground/heading output";
        return false;
    }
    if (!Finite(s.coreIntents, 7) || !std::isfinite(s.bodyPitch) || !std::isfinite(s.bodyYaw)) {
        mError = "Nonfinite Rust trick/body output";
        return false;
    }
    return true;
}
bool Session::Step(const std::vector<Packet>& samples, Snapshot& output) {
    if (!mSession || samples.empty() || samples.size() > 512) {
        mError = "Invalid sample batch/session";
        return false;
    }
    const bool ok = mStepSamples(mSession, samples.data(), (uint32_t)samples.size(), &output) != 0;
    if (!mReadPushDiagnostics(mSession, mPushDiagnostics, 24)) {
        mError = "Rust push diagnostic export failed";
        return false;
    }
    if (!ok) {
        mError = mLastError() ? mLastError() : "Rust tick failed";
        return false;
    }
    const bool valid = Validate(output);
    if (valid)
        ReadPose(output.tick);
    return valid;
}
bool Session::StepCamera(const std::vector<Packet>& samples, const float forward[3], Snapshot& output) {
    if (!mSession || samples.empty() || samples.size() > 512 || !Finite(forward, 3)) {
        mError = "Invalid camera sample batch/session";
        return false;
    }
    auto profileTime = Clock::now();
    if (!mStepCamera(mSession, samples.data(), (uint32_t)samples.size(), forward, &output)) {
        mError = mLastError() ? mLastError() : "Rust camera tick failed";
        return false;
    }
    mStepMs = Milliseconds(profileTime);
    if (!mReadPushDiagnostics(mSession, mPushDiagnostics, 24) || !mReadBiped(mSession, mBipedDiagnostics, 24)) {
        mError = "Runtime diagnostics failed";
        return false;
    }
    const bool valid = Validate(output);
    if (valid)
        ReadPose(output.tick);
    return valid;
}
bool Session::ReplaceWorld(const std::vector<float>& triangles, Snapshot& output) {
    if (!mSession || triangles.empty() || triangles.size() % 9 || triangles.size() / 9 > 1000000 ||
        !Finite(triangles.data(), triangles.size())) {
        mError = "Invalid replacement collision world";
        return false;
    }
    const uint32_t emptyOffsets[1] = { 0 };
    if (!mWorld(mSession, triangles.data(), (uint32_t)(triangles.size() / 9), nullptr, emptyOffsets, 0, 0, &output)) {
        mError = mLastError() ? mLastError() : "Rust collision update failed";
        return false;
    }
    const bool valid = Validate(output);
    if (valid)
        ReadPose(output.tick);
    return valid;
}
bool Session::SetRiderHeight(float height) {
    if (!mSession || !mSetRiderHeight || !mSetRiderHeight(mSession, height)) {
        mError = mLastError() ? mLastError() : "Rider clearance profile export unavailable";
        return false;
    }
    return true;
}
void Session::ReadPose(uint64_t tick) {
    auto profileTime = Clock::now();
    mPoseValid = false;
    float rider[256] = {};
    bool riderValid = mReadRider && mReadRider(mSession, rider, 256) && Finite(rider, 256);
    if (riderValid)
        std::copy_n(rider, 256, mRiderDiagnostics);
    if (!mReadPose) {
        mPoseError = "Final-pose export missing; root-only presentation";
        return;
    }
    if (!mReadPose(mSession, &mPose, sizeof(mPose))) {
        mPoseError = mLastError() ? mLastError() : "Final pose read failed";
        return;
    }
    if (mPose.version != kPoseVersion || mPose.count == 0 || mPose.count > kMaxPoseJoints || mPose.tick != tick ||
        !Finite(mPose.root, 16)) {
        mPoseError = "Invalid or stale final pose header";
        return;
    }
    mPoseError.clear();
    for (uint32_t i = 0; i < mPose.count; ++i) {
        auto& j = mPose.joints[i];
        j.name[63] = 0;
        if (j.id != i || j.parent >= (int32_t)mPose.count || j.parent == (int32_t)i) {
            mPoseError = "Invalid final-pose hierarchy";
            return;
        }
        if (!Finite(j.local, 16) || !Finite(j.restLocal, 16) || !Finite(j.global, 16) || !Finite(j.restGlobal, 16) ||
            !Finite(j.world, 16) || std::abs(j.world[12]) > 1e6f || std::abs(j.world[13]) > 1e6f ||
            std::abs(j.world[14]) > 1e6f) {
            j.valid = 0;
            mPoseError = std::string("Invalid pose bone: ") + j.name;
        }
    }
    mPoseMs = Milliseconds(profileTime);
    mPoseValid = true;
    if (!riderValid)
        mPoseError = "Invalid body diagnostics: retained prior values";
}
void Session::Stop() {
    if (mPrewarmThread.joinable())
        mPrewarmThread.join();
    DestroyLoaded();
    mWarmState.store(WarmState::UNINITIALIZED, std::memory_order_release);
}
void Session::DestroyLoaded() {
    // Never join a pathological native solver forever or unload DLL code
    // underneath an unfinished worker. Quarantine that module on failed stop.
    bool canUnload = mError.find("quarantined") == std::string::npos;
    if (mSession && mDestroyChecked)
        canUnload = mDestroyChecked(mSession) != 0;
    else if (mSession && mDestroy)
        mDestroy(mSession);
    mWorkerProgress = nullptr;
    mDestroyChecked = nullptr;
    mSession = nullptr;
    mAbi = nullptr;
    mLastError = nullptr;
    mCreate = nullptr;
    mStepSamples = nullptr;
    mWorld = nullptr;
    mDestroy = nullptr;
    mActorContacts = nullptr;
    mActorBail = nullptr;
    mReadRider = nullptr;
    mSetRiderHeight = nullptr;
    std::fill_n(mRiderDiagnostics, 256, 0.f);
    mReadPose = nullptr;
    mPoseValid = false;
    mPoseError.clear();
    mReadPushDiagnostics = nullptr;
    mStepCamera = nullptr;
    mReadBiped = nullptr;
#ifdef _WIN32
    if (mModule && canUnload)
        FreeLibrary((HMODULE)mModule);
#endif
    mModule = nullptr;
    mActivate = nullptr;
    mCaptureMarker = nullptr;
    mLoadMarker = nullptr;
    mWaterReturn = nullptr;
    mDynamic = nullptr;
    mDynamicGrinds = nullptr;
    mGrinds = nullptr;
    mEvents = nullptr;
    mScoreReset = nullptr;
    mContacts = nullptr;
    mLifecycleStats = nullptr;
}

} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::Dynamic(uint32_t id, const std::vector<float>* local, const float frame[16], bool active,
                      bool ridingSurface) {
    if (!mSession || !mDynamic || !Finite(frame, 16) ||
        (local && (local->empty() || local->size() % 9 || !Finite(local->data(), local->size())))) {
        mError = "Invalid/missing dynamic collision ABI";
        return false;
    }
    if (!mDynamic(mSession, id, local ? local->data() : nullptr, local ? (uint32_t)(local->size() / 9) : 0, frame,
                  active ? (ridingSurface ? 3 : 1) : 0)) {
        mError = mLastError() ? mLastError() : "Dynamic collision failed";
        return false;
    }
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::Grinds(const std::vector<float>& points, const std::vector<uint32_t>& offsets) {
    if (!mSession || !mGrinds || offsets.empty() || points.size() % 3 || !Finite(points.data(), points.size()) ||
        !mGrinds(mSession, points.empty() ? nullptr : points.data(), offsets.data(), (uint32_t)offsets.size() - 1,
                 (uint32_t)points.size() / 3)) {
        mError = mLastError() ? mLastError() : "Native grind upload unavailable";
        return false;
    }
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::DynamicGrinds(uint32_t id, const std::vector<float>* points, const std::vector<uint32_t>* offsets,
                            const float frame[16], bool active) {
    if (!mSession || !mDynamicGrinds || !Finite(frame, 16) ||
        !mDynamicGrinds(mSession, id, points && !points->empty() ? points->data() : nullptr,
                        offsets ? offsets->data() : nullptr, offsets ? (uint32_t)offsets->size() - 1 : 0,
                        points ? (uint32_t)points->size() / 3 : 0, frame, active ? 1 : 0)) {
        mError = mLastError() ? mLastError() : "Dynamic grind ABI failed";
        return false;
    }
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::ReadEvents(std::vector<Event>& events) {
    events.clear();
    if (!Ready() || !mEvents)
        return false;
    for (int batch = 0; batch < 8; ++batch) {
        Event buffer[512]{};
        int count = mEvents(mSession, buffer, 512, sizeof(Event));
        if (count < 0 || count > 512)
            return false;
        for (int i = 0; i < count; ++i) {
            auto& e = buffer[i];
            e.name[95] = 0;
            if (!std::isfinite(e.duration) || !std::isfinite(e.current) || !std::isfinite(e.combo) ||
                !std::isfinite(e.multiplier) || !std::isfinite(e.banked) || e.phase < 1 || e.phase > 7)
                return false;
            events.push_back(e);
        }
        if (count < 512)
            return true;
    }
    return true;
}
bool Session::ClearScore() {
    if (!Ready() || !mScoreReset || !mScoreReset(mSession))
        return false;
    std::vector<Event> discard;
    return ReadEvents(discard);
}
bool Session::Contacts(std::vector<std::array<float, 7>>& contacts) {
    contacts.clear();
    if (!Ready() || !mContacts)
        return false;
    std::array<float, 7> buffer[256];
    int n = mContacts(mSession, buffer[0].data(), 256);
    if (n < 0 || n > 256)
        return false;
    for (int i = 0; i < n; ++i) {
        if (!Finite(buffer[i].data(), 6))
            return false;
        contacts.push_back(buffer[i]);
    }
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::LifecycleStats(std::array<uint64_t, 16>& stats) {
    return Ready() && mLifecycleStats && mLifecycleStats(mSession, stats.data(), 16);
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::ActorBail(Snapshot& output) {
    if (!Ready() || !mActorBail)
        return false;
    if (!mActorBail(mSession, &output) || !Validate(output))
        return false;
    if (mReadRider)
        mReadRider(mSession, mRiderDiagnostics, 256);
    ReadPose(output.tick);
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::ActorContacts(std::vector<std::array<float, 5>>& contacts) {
    std::array<std::array<float, 5>, 256> buffer{};
    if (!Ready() || !mActorContacts)
        return false;
    int n = mActorContacts(mSession, buffer[0].data(), 256);
    if (n < 0 || n > 256)
        return false;
    contacts.assign(buffer.begin(), buffer.begin() + n);
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::VertStep(const float frame[16], const float velocity[3], const float anchor[3], unsigned mode,
                       Snapshot& output, float handWeight) {
    if (!Ready() || !mVertStep || !Finite(frame, 16) || !Finite(velocity, 3) || !Finite(anchor, 3))
        return false;
    if (!mVertStep(mSession, frame, velocity, anchor, mode, handWeight, &output) || !Validate(output)) {
        mError = mLastError() ? mLastError() : "Vert contact step failed";
        return false;
    }
    ReadPose(output.tick);
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::Modifiers(const std::array<float, 4>& values, bool noBail) {
    if (!Ready() || !mModifiers)
        return false;
    if (mModifierValid && mModifierValues == values && mModifierNoBail == noBail)
        return true;
    if (!mModifiers(mSession, values.data(), noBail ? 1 : 0)) {
        mError = mLastError() ? mLastError() : "Skate modifier update failed";
        return false;
    }
    mModifierValues = values;
    mModifierNoBail = noBail;
    mModifierValid = true;
    return true;
}
} // namespace NativeSkateRuntime

namespace NativeSkateRuntime {
bool Session::LegacyPop(Snapshot& output) {
    if (!Ready() || !mLegacyPop)
        return false;
    if (!mLegacyPop(mSession, &output) || !Validate(output)) {
        mError = mLastError() ? mLastError() : "Legacy pop failed";
        return false;
    }
    ReadPose(output.tick);
    return true;
}
bool Session::Probe(const float* from, const float* to, float* out) {
    return Ready() && mProbe && mProbe(mSession, from, to, out) != 0;
}
} // namespace NativeSkateRuntime

bool NativeSkateRuntime::Session::WorkerProgress(std::array<uint64_t, 6>& out) const {
    return mSession && mWorkerProgress && mWorkerProgress(mSession, out.data()) != 0;
}
