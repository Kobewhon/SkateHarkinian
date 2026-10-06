#pragma once
#include <cstdint>
#include "NativeSkatePose.h"
#include "NativeSkateEvents.h"
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <atomic>

namespace NativeSkateRuntime {
constexpr uint32_t kAbi = 4;
struct Packet {
    uint16_t buttons;
    uint8_t triggers[2];
    int16_t left[2], right[2];
};
struct Snapshot {
    float root[16], board[16];
    uint32_t stateId;
    float velocity[3];
    uint64_t tick;
    char state[64];
    float coreIntents[7], bodyPitch, bodyYaw;
    float trajectory[3], trajectoryVelocity[3], groundNormal[3], heading;
};
static_assert(sizeof(Packet) == 12);
static_assert(sizeof(Snapshot) == 296);
struct Marker {
    uint32_t version = 0, onboard = 0, footForward = 0, reserved = 0;
    float frame[16] = {};
};
static_assert(sizeof(Marker) == 80);
enum class WarmState { UNINITIALIZED, PREWARMING, READY, ACTIVE, FAILED };
class Session {
  public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session();
    void Prewarm(const std::string& dataRoot);
    WarmState State() const {
        return mWarmState.load(std::memory_order_acquire);
    }
    const char* StateName() const;
    void Suspend();
    bool Activate(const std::vector<float>& triangles, const float spawn[3], float heading, Snapshot& output);
    std::string Profile() const;
    bool Start(const std::string& dataRoot, const std::vector<float>& triangles, const float spawn[3], float heading);
    bool Step(const std::vector<Packet>& samples, Snapshot& output);
    bool VertStep(const float frame[16], const float velocity[3], const float anchor[3], unsigned mode,
                  Snapshot& output, float handWeight = 1.f);
    bool StepCamera(const std::vector<Packet>& samples, const float forward[3], Snapshot& output);
    const SkaterPose* FinalPose() const {
        return mPoseValid ? &mPose : nullptr;
    }
    const std::string& PoseError() const {
        return mPoseError;
    }
    const float* RiderDiagnostics() const {
        return mRiderDiagnostics;
    }
    const float* BipedDiagnostics() const {
        return mBipedDiagnostics;
    }
    bool ReadEvents(std::vector<Event>& events);
    bool ClearScore();
    bool ActorBail(Snapshot& output);
    bool Modifiers(const std::array<float, 4>&, bool noBail);
    bool LegacyPop(Snapshot&);
    bool Probe(const float*, const float*, float*);
    bool ActorContacts(std::vector<std::array<float, 5>>& contacts);
    bool Contacts(std::vector<std::array<float, 7>>& contacts);
    bool WorkerProgress(std::array<uint64_t, 6>&) const;
    bool LifecycleStats(std::array<uint64_t, 16>& stats);
    double PrewarmMilliseconds() const {
        return Ready() ? mPrewarmMs : 0;
    }
    bool DynamicGrinds(uint32_t id, const std::vector<float>* points, const std::vector<uint32_t>* offsets,
                       const float frame[16], bool active);
    bool Grinds(const std::vector<float>& points, const std::vector<uint32_t>& offsets);
    bool Dynamic(uint32_t id, const std::vector<float>* local, const float frame[16], bool active,
                 bool ridingSurface = false);
    bool ReplaceWorld(const std::vector<float>& triangles, Snapshot& output);
    const float* PushDiagnostics() const {
        return mPushDiagnostics;
    }
    void Stop();
    bool SetRiderHeight(float height);
    bool CaptureMarker(Marker& marker);
    bool LoadMarker(const Marker& marker, Snapshot& output);
    bool WaterReturn(const Marker& marker, Snapshot& output);
    bool Ready() const {
        auto state = State();
        return (state == WarmState::READY || state == WarmState::ACTIVE) && mSession != nullptr;
    }
    const std::string& Error() const {
        return mError;
    }
    const std::string& DllPath() const {
        return mDllPath;
    }
    static std::string ExecutableDirectory();

  private:
    std::atomic<WarmState> mWarmState{ WarmState::UNINITIALIZED };
    std::thread mPrewarmThread;
    double mPrewarmMs = 0, mDllMs = 0, mAbiMs = 0, mPathMs = 0, mCreateMs = 0, mWorldMs = 0, mActivateMs = 0,
           mStepMs = 0, mPoseMs = 0;
    void DestroyLoaded();
    int (*mCaptureMarker)(void*, Marker*) = nullptr;
    int (*mLoadMarker)(void*, const Marker*, Snapshot*) = nullptr;
    int (*mWaterReturn)(void*, const Marker*, Snapshot*) = nullptr;
    int (*mActivate)(void*, const float*, float, Snapshot*) = nullptr;
    void* mModule = nullptr;
    void* mSession = nullptr;
    uint32_t (*mAbi)() = nullptr;
    const char* (*mLastError)() = nullptr;
    void* (*mCreate)(const char*, const float*, uint32_t, const float*, float) = nullptr;
    int (*mStepSamples)(void*, const Packet*, uint32_t, Snapshot*) = nullptr;
    int (*mReadPushDiagnostics)(void*, float*, uint32_t) = nullptr;
    float mPushDiagnostics[24] = {};
    float mBipedDiagnostics[24] = {};
    float mRiderDiagnostics[256] = {};
    int (*mSetRiderHeight)(void*, float) = nullptr;
    int (*mReadRider)(void*, float*, uint32_t) = nullptr;
    int (*mVertStep)(void*, const float*, const float*, const float*, uint32_t, float, Snapshot*) = nullptr;
    int (*mStepCamera)(void*, const Packet*, uint32_t, const float*, Snapshot*) = nullptr;
    int (*mReadBiped)(void*, float*, uint32_t) = nullptr;
    int (*mEvents)(void*, Event*, uint32_t, uint32_t) = nullptr;
    int (*mScoreReset)(void*) = nullptr;
    int (*mActorBail)(void*, Snapshot*) = nullptr;
    int (*mModifiers)(void*, const float*, uint32_t) = nullptr;
    int (*mLegacyPop)(void*, Snapshot*) = nullptr;
    int (*mProbe)(void*, const float*, const float*, float*) = nullptr;
    std::array<float, 4> mModifierValues{};
    bool mModifierValid = false, mModifierNoBail = false;
    int (*mActorContacts)(void*, float*, uint32_t) = nullptr;
    int (*mContacts)(void*, float*, uint32_t) = nullptr;
    int (*mLifecycleStats)(void*, uint64_t*, uint32_t) = nullptr;
    int (*mDynamicGrinds)(void*, uint32_t, const float*, const uint32_t*, uint32_t, uint32_t, const float*,
                          uint32_t) = nullptr;
    int (*mGrinds)(void*, const float*, const uint32_t*, uint32_t, uint32_t) = nullptr;
    int (*mDynamic)(void*, uint32_t, const float*, uint32_t, const float*, uint32_t) = nullptr;
    int (*mWorld)(void*, const float*, uint32_t, const float*, const uint32_t*, uint32_t, uint32_t,
                  Snapshot*) = nullptr;
    int (*mWorkerProgress)(void*, uint64_t*) = nullptr;
    int (*mDestroyChecked)(void*) = nullptr;
    void (*mDestroy)(void*) = nullptr;
    std::string mDllPath, mError;
    bool Validate(const Snapshot& snapshot);
    void ReadPose(uint64_t tick);
    int (*mReadPose)(void*, SkaterPose*, uint32_t) = nullptr;
    SkaterPose mPose{};
    bool mPoseValid = false;
    std::string mPoseError;
};
} // namespace NativeSkateRuntime
