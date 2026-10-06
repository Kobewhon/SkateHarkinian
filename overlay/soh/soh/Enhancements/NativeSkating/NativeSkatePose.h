#pragma once
#include <cstdint>
namespace NativeSkateRuntime {
constexpr uint32_t kMaxPoseJoints = 256, kPoseVersion = 1;
struct PoseJoint {
    uint32_t id;
    int32_t parent;
    char name[64];
    float local[16], restLocal[16], global[16], restGlobal[16], world[16];
    uint32_t valid;
};
struct SkaterPose {
    uint32_t version, count;
    uint64_t tick;
    float root[16];
    PoseJoint joints[kMaxPoseJoints];
};
static_assert(sizeof(PoseJoint) == 396 && sizeof(SkaterPose) == 101456);
} // namespace NativeSkateRuntime
