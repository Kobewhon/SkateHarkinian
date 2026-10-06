#include <spdlog/spdlog.h>
#include <sstream>
#include "NativeSkateRetarget.h"
#include "NativeSkateBoardPresentation.h"
#include "NativeSkateRetargetMath.h"
#include "NativeSkateContactMath.h"
#include "NativeSkateBoardModel.h"
#include <array>
#include <cstring>
#include <string>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
extern "C" {
#include "global.h"
}
namespace NativeSkateRetarget {
using namespace NativeSkateRetargetMath;
namespace {
constexpr float kRadians = 3.14159265358979323846f / 32768.f;
constexpr int kCount = 15;
// Public PlayerLimb enum, not guessed array positions. jointTable[limb] is
// rotation; jointTable[0] is the root translation in SkelAnime_DrawFlexLod.
const char* names[kCount] = { "HIPS",        "RIGHTUPLEG", "RIGHTLEG", "RIGHTFOOT",    "LEFTUPLEG",
                              "LEFTLEG",     "LEFTFOOT",   "SPINE3",   "HEAD",         "LEFTARM",
                              "LEFTFOREARM", "LEFTHAND",   "RIGHTARM", "RIGHTFOREARM", "RIGHTHAND" };
const int targets[kCount] = { PLAYER_LIMB_WAIST,      PLAYER_LIMB_R_THIGH,    PLAYER_LIMB_R_SHIN,    PLAYER_LIMB_R_FOOT,
                              PLAYER_LIMB_L_THIGH,    PLAYER_LIMB_L_SHIN,     PLAYER_LIMB_L_FOOT,    PLAYER_LIMB_UPPER,
                              PLAYER_LIMB_HEAD,       PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND,
                              PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM,  PLAYER_LIMB_R_HAND };
struct Profile {
    void** skeleton = nullptr;
    bool adult = true, ready = false;
    std::array<int, PLAYER_LIMB_MAX> parents{};
    std::array<V, PLAYER_LIMB_MAX> positions{};
    std::array<M, PLAYER_LIMB_MAX> local{}, global{};
    std::array<int, kCount> source{};
    std::array<M, kCount> calibration{}, sourceRest{};
    std::array<Vec3s, kCount> previous{};
    std::array<bool, kCount> priorValid{};
    float contactWeight[2] = {};
    uint64_t contactTick = 0;
    float chainLength = 0, nativeChainLength = 0;
    M restNativeHip;
    std::string error;
} profiles[2];
V nativeFeet[2]{}, linkFeet[2]{}, nativePelvis{}, linkPelvis{};
bool feetValid[2]{}, pelvisValid = false;
std::string failure;
bool applied = false;
BodyInfo bodyInfo;
ContactInfo contactInfo;
M Rotation(Vec3s a) {
    return Euler(a.x * kRadians, a.y * kRadians, a.z * kRadians);
}
s16 Angle(float a) {
    int32_t n = (int32_t)std::lrint(std::remainder(a, 6.283185307179586f) / kRadians);
    if (n >= 32768)
        n -= 65536;
    if (n < -32768)
        n += 65536;
    return (s16)n;
}
Vec3s Rotation(M a) {
    auto r = Euler(a);
    return { Angle(r.x), Angle(r.y), Angle(r.z) };
}
void Hierarchy(Player* p, Profile& profile, int index, int parent) {
    auto* limb = (LodLimb*)SEGMENTED_TO_VIRTUAL(p->skelAnime.skeleton[index]);
    int id = index + 1;
    profile.parents[id] = parent;
    profile.positions[id] = id == PLAYER_LIMB_ROOT
                                ? profile.positions[id]
                                : V{ (float)limb->jointPos.x, (float)limb->jointPos.y, (float)limb->jointPos.z };
    profile.global[id] = Mul(parent ? profile.global[parent] : M{}, profile.local[id]);
    if (limb->child != LIMB_DONE)
        Hierarchy(p, profile, limb->child, id);
    if (limb->sibling != LIMB_DONE)
        Hierarchy(p, profile, limb->sibling, parent);
}
bool Calibrate(Player* player, const Vec3s* neutral, const NativeSkateRuntime::SkaterPose& pose, Profile& p) {
    p = Profile{};
    p.skeleton = player->skelAnime.skeleton;
    p.adult = LINK_IS_ADULT;
    for (int i = 1; i < PLAYER_LIMB_MAX; ++i)
        p.local[i] = Rotation(neutral[i]);
    float rootScale = p.adult ? 1.f : .64f;
    p.positions[PLAYER_LIMB_ROOT] = { (float)neutral[0].x * rootScale, (float)neutral[0].y * rootScale,
                                      (float)neutral[0].z * rootScale };
    Hierarchy(player, p, 0, 0);
    // Calibrated anatomical rest: straight Link knee chains, with the actual
    // model's neutral foot basis. The stock wait pose already bends knees ~45deg;
    // treating that bend as a bind pose would add it to native knee flexion.
    for (auto side : { std::array<int, 3>{ PLAYER_LIMB_R_THIGH, PLAYER_LIMB_R_SHIN, PLAYER_LIMB_R_FOOT },
                       std::array<int, 3>{ PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN, PLAYER_LIMB_L_FOOT } }) {
        auto foot = p.global[side[2]];
        auto thigh = p.global[side[0]];
        thigh = Mul(Swing(Mul(thigh, V{ 1, 0, 0 }), V{ 0, -1, 0 }), thigh);
        p.local[side[0]] = Mul(Inverse(p.global[p.parents[side[0]]]), thigh);
        p.local[side[1]] = M{};
        p.local[side[2]] = Mul(Inverse(thigh), foot);
    }
    Hierarchy(player, p, 0, 0);
    p.chainLength = (Length(p.positions[PLAYER_LIMB_R_SHIN]) + Length(p.positions[PLAYER_LIMB_R_FOOT]) +
                     Length(p.positions[PLAYER_LIMB_L_SHIN]) + Length(p.positions[PLAYER_LIMB_L_FOOT])) *
                    .5f;
    for (int i = 0; i < kCount; ++i) {
        int found = -1;
        for (uint32_t j = 0; j < pose.count; ++j)
            if (std::strcmp(pose.joints[j].name, names[i]) == 0) {
                found = j;
                break;
            }
        if (found < 0) {
            p.error = std::string("Missing native bone ") + names[i];
            return false;
        }
        p.source[i] = found;
        M global, local;
        if (!Basis(pose.joints[found].restGlobal, global) ||
            !Basis(i == 0 ? pose.joints[found].restGlobal : pose.joints[found].restLocal, local)) {
            p.error = std::string("Invalid rest bone ") + names[i];
            return false;
        }
        p.sourceRest[i] = local;
        p.calibration[i] = Mul(Inverse(p.global[targets[i]]), global);
    }
    // Contact stance translations scale with actual leg proportions, separately
    // from the visual board multiplier. This avoids unreachable Child stance width.
    p.nativeChainLength = 0;
    for (int side = 0; side < 2; ++side) {
        int start = side ? 4 : 1;
        for (int segment = 0; segment < 2; ++segment) {
            const float* a = pose.joints[p.source[start + segment]].restGlobal;
            const float* b = pose.joints[p.source[start + segment + 1]].restGlobal;
            p.nativeChainLength += Length(V{ b[12] - a[12], b[13] - a[13], b[14] - a[14] }) * .5f;
        }
    }
    // The stock wait arms are bent/down, whereas RIG_TPOSE arms are straight.
    // Calibrate actual Link arm chains to the source rest directions once, so
    // native elbow/shoulder deltas do not add the wait pose's existing bend.
    for (auto side : { std::array<int, 3>{ 9, 10, 11 }, std::array<int, 3>{ 12, 13, 14 } }) {
        int arm = targets[side[0]], forearm = targets[side[1]], hand = targets[side[2]];
        const float* a = pose.joints[p.source[side[0]]].restGlobal;
        const float* f = pose.joints[p.source[side[1]]].restGlobal;
        V direction = { f[12] - a[12], f[13] - a[13], f[14] - a[14] };
        if (Length(direction) < .001f) {
            p.error = "Invalid arm rest chain";
            return false;
        }
        M correction = Swing(Mul(p.global[arm], p.positions[forearm]), direction);
        M armRest = Mul(correction, p.global[arm]), handRest = Mul(correction, p.global[hand]);
        p.local[arm] = Mul(Inverse(p.global[p.parents[arm]]), armRest);
        p.local[forearm] = M{};
        p.local[hand] = Mul(Inverse(armRest), handRest);
        Hierarchy(player, p, 0, 0);
    }
    Basis(pose.joints[p.source[0]].restGlobal, p.restNativeHip);
    p.ready = true;
    return true;
}
void Fk(Profile& p, const Vec3s* joints, std::array<M, PLAYER_LIMB_MAX>& r, std::array<V, PLAYER_LIMB_MAX>& t, int id) {
    int parent = p.parents[id];
    r[id] = Mul(parent ? r[parent] : M{}, Rotation(joints[id]));
    V offset = p.positions[id];
    if (id == PLAYER_LIMB_ROOT) {
        float s = p.adult ? 1.f : .64f;
        offset = { (float)joints[0].x * s, (float)joints[0].y * s, (float)joints[0].z * s };
    }
    t[id] = (parent ? t[parent] : V{}) + Mul(parent ? r[parent] : M{}, offset);
    for (int i = id + 1; i < PLAYER_LIMB_MAX; ++i)
        if (p.parents[i] == id)
            Fk(p, joints, r, t, i);
}
V World(V p, Player* player, const NativeSkateLink::Frame& frame) {
    return V{ frame.position[0], frame.position[1], frame.position[2] } +
           Mul(Euler(0, frame.yaw * kRadians, 0),
               V{ p.x * player->actor.scale.x, p.y * player->actor.scale.y, p.z * player->actor.scale.z });
}
bool HostPoint(const NativeSkateRuntime::PoseJoint& joint, const NativeSkateUnits::Anchor& anchor, V& p) {
    float h[3];
    if (!joint.valid || !anchor.Position(joint.world + 12, h))
        return false;
    p = { h[0], h[1], h[2] };
    return std::isfinite(Length(p)) && Length(p) < 1e7f;
}
} // namespace
void Reset() {
    contactInfo = {};
    for (auto& p : profiles)
        p = Profile{};
    applied = false;
    failure.clear();
    feetValid[0] = feetValid[1] = pelvisValid = false;
}
const BodyInfo& BodyDiagnostics() {
    return bodyInfo;
}
const ContactInfo& ContactDiagnostics() {
    return contactInfo;
}
void FrameStart() {
    bodyInfo = BodyInfo{};
    applied = false;
    feetValid[0] = feetValid[1] = pelvisValid = false;
}
namespace {
void GlobalPose(Profile& p, Vec3s* joints, const std::array<M, PLAYER_LIMB_MAX>& desired,
                const std::array<bool, PLAYER_LIMB_MAX>& mapped, std::array<M, PLAYER_LIMB_MAX>& global, int id) {
    int parent = p.parents[id];
    M parentRotation = parent ? global[parent] : M{};
    if (mapped[id])
        joints[id] = Rotation(Mul(Inverse(parentRotation), desired[id]));
    global[id] = Mul(parentRotation, Rotation(joints[id]));
    for (int i = id + 1; i < PLAYER_LIMB_MAX; ++i)
        if (p.parents[i] == id)
            GlobalPose(p, joints, desired, mapped, global, i);
}
} // namespace
bool Apply(Player* player, const NativeSkateLink::Frame& frame, Vec3s* joints,
           const NativeSkateRuntime::SkaterPose* pose, const NativeSkateUnits::Anchor& anchor, const float* diagnostics,
           const float* board, float pitch) {
    applied = false;
    feetValid[0] = feetValid[1] = pelvisValid = false;
    if (!pose || pose->version != 1 || !pose->count) {
        failure = "Final native pose unavailable";
        return false;
    }
    auto& p = profiles[frame.adult ? 0 : 1];
    if (!p.ready || p.skeleton != player->skelAnime.skeleton)
        if (!Calibrate(player, joints, *pose, p)) {
            failure = p.error;
            return false;
        }
    failure.clear();
    M yawInverse = Inverse(Euler(0, frame.yaw * kRadians, 0));
    M hip;
    const auto& sourceHip = pose->joints[p.source[0]];
    if (!sourceHip.valid || !Basis(sourceHip.world, hip)) {
        failure = "Invalid physical pelvis world basis";
        return false;
    }
    // Rest-calibrated BODY rotation belongs above both waist and upper-body
    // branches. This is a bone delta, NOT the raw Skate root/deck parent matrix.
    M body = Mul(hip, Inverse(p.restNativeHip));
    bool ragdoll = diagnostics && diagnostics[0] != 0;
    bool held = diagnostics && diagnostics[96] == 1.f;
    bool whole = held || ragdoll || std::abs(std::remainder(pitch, 6.2831853f)) > .05f || body.v[1][1] < .65f;
    bodyInfo.poseTick = pose->tick;
    bodyInfo.whole = whole;
    bodyInfo.ragdoll = ragdoll;
    std::array<M, PLAYER_LIMB_MAX> desired{}, rotations{};
    std::array<bool, PLAYER_LIMB_MAX> mapped{};
    desired[PLAYER_LIMB_ROOT] = Mul(Mul(yawInverse, body), p.global[PLAYER_LIMB_ROOT]);
    mapped[PLAYER_LIMB_ROOT] = true;
    // Ordinary air tricks now include the authored torso/head and arms. Reuse the
    // completed native arm animation; held-board/grab, ragdoll and whole-body
    // cases keep their accepted full-pose path. Sword is still overridden in Draw.
    const bool trickBody = diagnostics && !whole && diagnostics[7] >= 200.f && diagnostics[7] <= 202.f;
    auto maps = [&](int i) { return whole || i < 7 || trickBody; };
    for (int i = 0; i < kCount; ++i) {
        if (!maps(i))
            continue;
        const auto& j = pose->joints[p.source[i]];
        M current, rest;
        if (!j.valid || !Basis(j.world, current) || !Basis(j.restGlobal, rest)) {
            failure = std::string("Invalid native bone ") + names[i];
            if (p.priorValid[i])
                joints[targets[i]] = p.previous[i];
            continue;
        }
        desired[targets[i]] = Mul(Mul(Mul(yawInverse, current), Inverse(rest)), p.global[targets[i]]);
        mapped[targets[i]] = true;
    }
    GlobalPose(p, joints, desired, mapped, rotations, PLAYER_LIMB_ROOT);
    for (int i = 0; i < kCount; ++i)
        if (mapped[targets[i]]) {
            p.previous[i] = joints[targets[i]];
            p.priorValid[i] = true;
        }
    bool validRight = HostPoint(pose->joints[p.source[3]], anchor, nativeFeet[0]),
         validLeft = HostPoint(pose->joints[p.source[6]], anchor, nativeFeet[1]);
    bool validHip = HostPoint(sourceHip, anchor, nativePelvis);
    std::array<V, PLAYER_LIMB_MAX> t;
    Fk(p, joints, rotations, t, PLAYER_LIMB_ROOT);
    V offset{};
    bool correction = false;
    if (ragdoll && validHip) {
        // Solved physical pelvis owns ragdoll placement. Feet/board are not a
        // standing anchor in this state. Same final physical pose supplies arms/head.
        offset = nativePelvis - World(t[PLAYER_LIMB_WAIST], player, frame);
        correction = true;
    } else if (validRight && validLeft) {
        V right = World(t[PLAYER_LIMB_R_FOOT], player, frame), left = World(t[PLAYER_LIMB_L_FOOT], player, frame);
        float wr = .5f, wl = .5f;
        if (diagnostics && board && !whole && (diagnostics[7] == 100.f || diagnostics[7] == 103.f)) {
            // Native post-physics foot/deck membership owns support selection.
            // Choosing the lower foot incorrectly selected the pushing foot when it
            // reached the ground below the deck. Native order is LEFT, RIGHT.
            bool leftOnDeck = diagnostics[74] != 0, rightOnDeck = diagnostics[75] != 0;
            if (leftOnDeck != rightOnDeck) {
                wl = leftOnDeck ? 1.f : 0.f;
                wr = 1.f - wl;
            }
        }
        bodyInfo.leftSupport = wl;
        if (whole && validHip) {
            // Both position and rotation come from the SAME completed native pelvis
            // frame. Calibrate proportional foot reach relative to that frame instead
            // of capping an inverted rider at a grounded standing-root limit.
            V hipWorld = World(t[PLAYER_LIMB_WAIST], player, frame);
            V relativeNative = (nativeFeet[0] + nativeFeet[1]) * .5f - nativePelvis;
            V relativeLink = (right + left) * .5f - hipWorld;
            offset = nativePelvis - hipWorld + relativeNative - relativeLink;
        } else
            offset = (nativeFeet[0] - right) * wr + (nativeFeet[1] - left) * wl;
        correction = true;
    }
    if (correction) {
        // Safety validation only, not a pose clamp. The old 65%-leg-length cap
        // could truncate valid inverted proportional reach corrections.
        float max = (diagnostics ? std::max(1.f, diagnostics[20]) : 2.f) * anchor.unitsPerMeter +
                    p.chainLength * player->actor.scale.y;
        float length = Length(offset);
        if (!std::isfinite(length)) {
            failure = "Invalid local body correction";
            return false;
        }
        if (length > max) {
            failure = "Native rider local offset exceeds physical-body safety bound";
            return false;
        }
        offset = Mul(yawInverse, offset);
        float child = p.adult ? 1.f : .64f;
        auto bounded = [](s16 old, float delta) { return (s16)std::lrint(std::clamp(old + delta, -32767.f, 32767.f)); };
        joints[0].x = bounded(joints[0].x, offset.x / (player->actor.scale.x * child));
        joints[0].y = bounded(joints[0].y, offset.y / (player->actor.scale.y * child));
        joints[0].z = bounded(joints[0].z, offset.z / (player->actor.scale.z * child));
    }
    Fk(p, joints, rotations, t, PLAYER_LIMB_ROOT);
    for (int i = 1; i < PLAYER_LIMB_MAX; ++i) {
        V world = World(t[i], player, frame);
        bodyInfo.joints[i][0] = world.x;
        bodyInfo.joints[i][1] = world.y;
        bodyInfo.joints[i][2] = world.z;
        bodyInfo.valid[i] = true;
    }
    M riderWorld = Mul(Euler(0, frame.yaw * kRadians, 0), rotations[PLAYER_LIMB_ROOT]);
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            bodyInfo.pelvisFrame[column * 4 + row] = riderWorld.v[row][column];
    for (int a = 0; a < 3; ++a)
        bodyInfo.pelvisFrame[12 + a] = bodyInfo.joints[PLAYER_LIMB_WAIST][a];
    bodyInfo.pelvisFrame[15] = 1;
    // Proportion conversion for rider-relative objects, independent of board size.
    const float* rh = sourceHip.restGlobal;
    const float* rf = pose->joints[p.source[3]].restGlobal;
    const float* lf = pose->joints[p.source[6]].restGlobal;
    V restReach = { rh[12] - (rf[12] + lf[12]) * .5f, rh[13] - (rf[13] + lf[13]) * .5f,
                    rh[14] - (rf[14] + lf[14]) * .5f };
    float nativeReach = Length(restReach) * anchor.unitsPerMeter;
    bodyInfo.heldRelativeScale = nativeReach > .01f ? p.chainLength * player->actor.scale.y / nativeReach : 1.f;
    M linkDelta = Mul(riderWorld, Inverse(p.global[PLAYER_LIMB_ROOT]));
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) {
            bodyInfo.nativeBodyFrame[col * 4 + row] = body.v[row][col];
            bodyInfo.linkBodyFrame[col * 4 + row] = linkDelta.v[row][col];
        }
    for (int a = 0; a < 3; ++a) {
        bodyInfo.nativeBodyFrame[12 + a] = a == 0 ? nativePelvis.x : a == 1 ? nativePelvis.y : nativePelvis.z;
        bodyInfo.linkBodyFrame[12 + a] = bodyInfo.joints[PLAYER_LIMB_WAIST][a];
    }
    bodyInfo.nativeBodyFrame[15] = bodyInfo.linkBodyFrame[15] = 1;
    bodyInfo.localRoot[0] = joints[0].x;
    bodyInfo.localRoot[1] = joints[0].y;
    bodyInfo.localRoot[2] = joints[0].z;
    applied = true;
    return true;
}
bool CarryArm(Player* player, Vec3s* joints, const Vec3s* neutral, const NativeSkateRuntime::SkaterPose* pose,
              float weight) {
    if (!player || !pose || weight <= 0)
        return false;
    auto& p = profiles[LINK_IS_ADULT ? 0 : 1];
    if (!p.ready || p.skeleton != player->skelAnime.skeleton)
        if (!Calibrate(player, neutral, *pose, p))
            return false;
    const int arm = PLAYER_LIMB_R_SHOULDER, fore = PLAYER_LIMB_R_FOREARM, hand = PLAYER_LIMB_R_HAND;
    std::array<M, PLAYER_LIMB_MAX> r{}, rest{};
    std::array<V, PLAYER_LIMB_MAX> t{}, restPos{};
    Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    Fk(p, neutral, rest, restPos, PLAYER_LIMB_ROOT);
    const float upper = Length(p.positions[fore]), lower = Length(p.positions[hand]), length = upper + lower;
    const float side = restPos[arm].x >= restPos[PLAYER_LIMB_UPPER].x ? 1.f : -1.f;
    // Actual arm lengths define a relaxed, bent carrying arm beside the torso.
    V target = restPos[arm] + V{ side * .25f * length, -.75f * length, .12f * length };
    target = t[hand] * (1 - weight) + target * weight;
    auto solved = NativeSkateContactMath::Solve(t[arm], t[fore], target, upper, lower, V{ side, 0, .5f });
    M upperRot = Mul(Swing(t[fore] - t[arm], solved.knee - t[arm]), r[arm]);
    M lowerRot = Mul(Swing(t[hand] - t[fore], solved.ankle - solved.knee), r[fore]);
    joints[arm] = Rotation(Mul(Inverse(r[p.parents[arm]]), upperRot));
    joints[fore] = Rotation(Mul(Inverse(upperRot), lowerRot));
    // Keep the measured stock neutral wrist basis; do not inherit run-arm swing.
    joints[hand] = Rotation(Mul(Inverse(lowerRot), rest[hand]));
    return true;
}
// Retarget native hand positions through the same body/proportion frame as
// the held board. Only explicit native grab ownership may drive this solve.
void GrabHands(Player* player, const NativeSkateLink::Frame& frame, Vec3s* joints,
               const NativeSkateRuntime::SkaterPose* pose, const NativeSkateUnits::Anchor& anchor, const float* d) {
    if (!applied || !pose || !d || d[96] != 1.f || bodyInfo.ragdoll || bodyInfo.poseTick != pose->tick)
        return;
    auto& p = profiles[frame.adult ? 0 : 1];
    std::array<M, PLAYER_LIMB_MAX> r{};
    std::array<V, PLAYER_LIMB_MAX> t{};
    Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    M nativeBody, linkBody;
    if (!Basis(bodyInfo.nativeBodyFrame, nativeBody) || !Basis(bodyInfo.linkBodyFrame, linkBody))
        return;
    M yawInverse = Inverse(Euler(0, frame.yaw * kRadians, 0));
    V pelvis{ bodyInfo.joints[PLAYER_LIMB_WAIST][0], bodyInfo.joints[PLAYER_LIMB_WAIST][1],
              bodyInfo.joints[PLAYER_LIMB_WAIST][2] };
    for (auto chain : { std::array<int, 3>{ 9, 10, 11 }, std::array<int, 3>{ 12, 13, 14 } }) {
        V source;
        if (!HostPoint(pose->joints[p.source[chain[2]]], anchor, source))
            continue;
        V relative = Mul(Inverse(nativeBody), source - nativePelvis) * bodyInfo.heldRelativeScale;
        V world = pelvis + Mul(linkBody, relative);
        V local = Mul(yawInverse, world - V{ frame.position[0], frame.position[1], frame.position[2] });
        local = { local.x / player->actor.scale.x, local.y / player->actor.scale.y, local.z / player->actor.scale.z };
        int arm = targets[chain[0]], fore = targets[chain[1]], hand = targets[chain[2]];
        auto solve = NativeSkateContactMath::Solve(t[arm], t[fore], local, Length(p.positions[fore]),
                                                   Length(p.positions[hand]), t[fore] - t[arm]);
        M upper = Mul(Swing(t[fore] - t[arm], solve.knee - t[arm]), r[arm]);
        M lower = Mul(Swing(t[hand] - t[fore], solve.ankle - solve.knee), r[fore]);
        M wrist = r[hand];
        joints[arm] = Rotation(Mul(Inverse(r[p.parents[arm]]), upper));
        joints[fore] = Rotation(Mul(Inverse(upper), lower));
        joints[hand] = Rotation(Mul(Inverse(lower), wrist));
        Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    }
}
void HandplantHand(Player* player, const NativeSkateLink::Frame& frame, Vec3s* joints,
                   const NativeSkateRuntime::SkaterPose* pose, const float target[3], float weight) {
    if (!applied || !pose)
        return;
    auto& p = profiles[frame.adult ? 0 : 1];
    std::array<M, PLAYER_LIMB_MAX> r{};
    std::array<V, PLAYER_LIMB_MAX> t{};
    Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    // Link has different proportions from the native rider. Translate the
    // complete presentation frame to keep the real hand anchor reachable; the
    // board receives this SAME offset, retaining the foot/deck relationship.
    V shift = (V{ target[0], target[1], target[2] } - World(t[PLAYER_LIMB_R_HAND], player, frame)) *
              std::clamp(weight, 0.f, 1.f);
    if (!std::isfinite(Length(shift)) || Length(shift) > 100)
        return;
    M invYaw = Inverse(Euler(0, frame.yaw * kRadians, 0));
    V delta = Mul(invYaw, shift);
    float rootScale = p.adult ? 1.f : .64f;
    joints[0].x += (s16)std::lround(delta.x / (player->actor.scale.x * rootScale));
    joints[0].y += (s16)std::lround(delta.y / (player->actor.scale.y * rootScale));
    joints[0].z += (s16)std::lround(delta.z / (player->actor.scale.z * rootScale));
    Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    for (int i = 1; i < PLAYER_LIMB_MAX; ++i) {
        V w = World(t[i], player, frame);
        bodyInfo.joints[i][0] = w.x;
        bodyInfo.joints[i][1] = w.y;
        bodyInfo.joints[i][2] = w.z;
    }
    for (int i = 0; i < 3; ++i) {
        bodyInfo.handplantOffset[i] = i == 0 ? shift.x : i == 1 ? shift.y : shift.z;
        bodyInfo.pelvisFrame[12 + i] += bodyInfo.handplantOffset[i];
        bodyInfo.linkBodyFrame[12 + i] += bodyInfo.handplantOffset[i];
    }
    V local =
        Mul(invYaw, V{ target[0] - frame.position[0], target[1] - frame.position[1], target[2] - frame.position[2] });
    local = { local.x / player->actor.scale.x, local.y / player->actor.scale.y, local.z / player->actor.scale.z };
    int arm = PLAYER_LIMB_R_SHOULDER, fore = PLAYER_LIMB_R_FOREARM, hand = PLAYER_LIMB_R_HAND;
    auto solve = NativeSkateContactMath::Solve(t[arm], t[fore], local, Length(p.positions[fore]),
                                               Length(p.positions[hand]), t[fore] - t[arm]);
    M upper = Mul(Swing(t[fore] - t[arm], solve.knee - t[arm]), r[arm]);
    M lower = Mul(Swing(t[hand] - t[fore], solve.ankle - solve.knee), r[fore]);
    joints[arm] = Rotation(Mul(Inverse(r[p.parents[arm]]), upper));
    joints[fore] = Rotation(Mul(Inverse(upper), lower));
    joints[hand] = Rotation(Mul(Inverse(lower), r[hand]));
}
// Two-handed biped lifting pose; stock sword still has final priority in Draw.
void PropArms(Player* player, Vec3s* joints, const Vec3s* neutral, const NativeSkateRuntime::SkaterPose* pose) {
    if (!player || !pose)
        return;
    auto& p = profiles[LINK_IS_ADULT ? 0 : 1];
    if (!p.ready || p.skeleton != player->skelAnime.skeleton)
        if (!Calibrate(player, neutral, *pose, p))
            return;
    std::array<M, PLAYER_LIMB_MAX> r{};
    std::array<V, PLAYER_LIMB_MAX> t{};
    Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    for (auto chain : { std::array<int, 3>{ PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND },
                        std::array<int, 3>{ PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_HAND } }) {
        int arm = chain[0], fore = chain[1], hand = chain[2];
        float length = Length(p.positions[fore]) + Length(p.positions[hand]);
        float side = t[arm].x >= t[PLAYER_LIMB_UPPER].x ? 1.f : -1.f;
        V target = t[arm] + V{ side * .12f * length, -.25f * length, .82f * length };
        auto solve = NativeSkateContactMath::Solve(t[arm], t[fore], target, Length(p.positions[fore]),
                                                   Length(p.positions[hand]), V{ side, -1, 0 });
        M upper = Mul(Swing(t[fore] - t[arm], solve.knee - t[arm]), r[arm]);
        M lower = Mul(Swing(t[hand] - t[fore], solve.ankle - solve.knee), r[fore]);
        joints[arm] = Rotation(Mul(Inverse(r[p.parents[arm]]), upper));
        joints[fore] = Rotation(Mul(Inverse(upper), lower));
        Fk(p, joints, r, t, PLAYER_LIMB_ROOT);
    }
}
void Contact(Player* player, const NativeSkateLink::Frame& frame, Vec3s* joints,
             const NativeSkateRuntime::SkaterPose* pose, const NativeSkateUnits::Anchor& anchor, const float* d,
             const float* board) {
    contactInfo = {};
    if (!applied || !pose || !d || !board || bodyInfo.ragdoll || bodyInfo.whole || d[96] == 1.f || d[7] >= 500.f)
        return;
    auto& p = profiles[frame.adult ? 0 : 1];
    M deck;
    if (!p.ready || !Basis(board, deck))
        return;
    V center;
    float host[3];
    if (!anchor.Position(board + 12, host))
        return;
    center = { host[0], host[1], host[2] };
    M yaw = Euler(0, frame.yaw * kRadians, 0), invYaw = Inverse(yaw);
    V up = Mul(deck, V{ 0, 1, 0 });
    float scale = NativeSkateBoardModel::VisualScale(
                      frame.adult, CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScale"), 1),
                      CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleAdult"), 1),
                      CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleChild"), .8f))
                      .final;
    uint64_t dt = p.contactTick && pose->tick >= p.contactTick ? pose->tick - p.contactTick : 0;
    std::array<M, PLAYER_LIMB_MAX> rotations;
    std::array<V, PLAYER_LIMB_MAX> positions;
    Fk(p, joints, rotations, positions, PLAYER_LIMB_ROOT);
    const int thighs[] = { PLAYER_LIMB_R_THIGH, PLAYER_LIMB_L_THIGH },
              shins[] = { PLAYER_LIMB_R_SHIN, PLAYER_LIMB_L_SHIN }, feet[] = { PLAYER_LIMB_R_FOOT, PLAYER_LIMB_L_FOOT },
              sources[] = { 3, 6 };
    V targets[2], soles[2];
    float sum = 0, shift = 0;
    V shiftVector{};
    for (int side = 0; side < 2; ++side) {
        const auto& source = pose->joints[p.source[sources[side]]];
        V native;
        if (!HostPoint(source, anchor, native))
            return;
        V local = Mul(Inverse(deck), native - center) * (1 / anchor.unitsPerMeter);
        float requested = (d[side == 0 ? 75 : 74] != 0) ? std::clamp(d[side == 0 ? 2 : 1], 0.f, 1.f) : 0.f;
        if (!p.contactTick)
            p.contactWeight[side] = requested;
        else if (dt)
            p.contactWeight[side] = NativeSkateContactMath::Weight(p.contactWeight[side], requested, dt);
        float weight = p.contactWeight[side];
        float proportion = p.nativeChainLength > .001f
                               ? p.chainLength * player->actor.scale.y / (p.nativeChainLength * anchor.unitsPerMeter)
                               : 1.f;
        V point = { local.x * proportion * scale, NativeSkateContactMath::DeckTopMeters(local.z * proportion) * scale,
                    local.z * proportion * scale };
        targets[side] = center + Mul(deck, point * anchor.unitsPerMeter);
        soles[side] =
            World(positions[feet[side]] + Mul(rotations[feet[side]], NativeSkateContactMath::Sole(frame.adult, side)),
                  player, frame);
        if (requested == 0)
            targets[side] = soles[side];
        shift += Dot(targets[side] - soles[side], up) * weight;
        shiftVector = shiftVector + (targets[side] - soles[side]) * weight;
        sum += requested > 0 ? weight : 0;
        std::copy_n(source.world, 16, contactInfo.nativeFeet[side]);
        anchor.Position(contactInfo.nativeFeet[side] + 12, contactInfo.nativeFeet[side] + 12);
        for (int i = 0; i < 3; ++i) {
            contactInfo.nativeLocal[side][i] = i == 0 ? local.x : i == 1 ? local.y : local.z;
            contactInfo.targets[side][i] = i == 0 ? targets[side].x : i == 1 ? targets[side].y : targets[side].z;
        }
        contactInfo.weight[side] = weight;
        contactInfo.nativeClearance[side] = local.y - NativeSkateContactMath::DeckTopMeters(local.z);
    }
    p.contactTick = pose->tick;
    if (sum > .001f) {
        float influence = std::max(d[75] > 0 ? p.contactWeight[0] : 0.f, d[74] > 0 ? p.contactWeight[1] : 0.f);
        V tangent = shiftVector * (1 / sum) - up * (shift / sum);
        float tangentLimit = p.chainLength * player->actor.scale.y * .75f;
        float tangentLength = Length(tangent);
        if (tangentLength > tangentLimit)
            tangent = tangent * (tangentLimit / tangentLength);
        tangent = tangent * influence;
        float maximum = p.chainLength * player->actor.scale.y * .5f * influence;
        float low = -maximum, high = maximum;
        bool feasible = true;
        for (int side = 0; side < 2; ++side) {
            float weight = p.contactWeight[side];
            if (d[side == 0 ? 75 : 74] == 0 || weight < .001f)
                continue;
            int foot = feet[side];
            V normal = Mul(invYaw, up), restNormal = Mul(Inverse(p.global[foot]), V{ 0, 1, 0 });
            M rotation = Mul(Swing(Mul(rotations[foot], restNormal),
                                   Unit(Mul(rotations[foot], restNormal) * (1 - weight) + normal * weight)),
                             rotations[foot]);
            V ankle = soles[side] + (targets[side] - soles[side]) * weight -
                      Mul(yaw, Mul(rotation, NativeSkateContactMath::Sole(frame.adult, side)) * player->actor.scale.y);
            float reach =
                (Length(p.positions[shins[side]]) + Length(p.positions[feet[side]])) * player->actor.scale.y - .01f;
            feasible = NativeSkateContactMath::ReachInterval(World(positions[thighs[side]], player, frame) + tangent,
                                                             ankle, up, reach, low, high) &&
                       feasible;
        }
        float correction = std::clamp(shift / sum * influence, -maximum, maximum);
        if (feasible && low <= high)
            correction = std::clamp(correction, low, high);
        V local = Mul(invYaw, up * correction + tangent);
        float child = frame.adult ? 1.f : .64f;
        auto coordinate = [](float x) { return (s16)std::lrint(std::clamp(x, -32767.f, 32767.f)); };
        joints[0].x = coordinate(joints[0].x + local.x / (player->actor.scale.x * child));
        joints[0].y = coordinate(joints[0].y + local.y / (player->actor.scale.y * child));
        joints[0].z = coordinate(joints[0].z + local.z / (player->actor.scale.z * child));
        Fk(p, joints, rotations, positions, PLAYER_LIMB_ROOT);
    }
    for (int side = 0; side < 2; ++side) {
        float weight = p.contactWeight[side];
        if (weight < .001f || d[side == 0 ? 75 : 74] == 0)
            continue;
        int thigh = thighs[side], shin = shins[side], foot = feet[side];
        V normal = Mul(invYaw, up);
        V restNormal = Mul(Inverse(p.global[foot]), V{ 0, 1, 0 });
        M footRotation = Mul(Swing(Mul(rotations[foot], restNormal),
                                   Unit(Mul(rotations[foot], restNormal) * (1 - weight) + normal * weight)),
                             rotations[foot]);
        V targetWorld = soles[side] + (targets[side] - soles[side]) * weight;
        V local = Mul(invYaw, targetWorld - V{ frame.position[0], frame.position[1], frame.position[2] });
        local = { local.x / player->actor.scale.x, local.y / player->actor.scale.y, local.z / player->actor.scale.z };
        V ankle = local - Mul(footRotation, NativeSkateContactMath::Sole(frame.adult, side));
        auto leg = NativeSkateContactMath::Solve(positions[thigh], positions[shin], ankle, Length(p.positions[shin]),
                                                 Length(p.positions[foot]), Mul(invYaw, Mul(deck, V{ 0, 0, 1 })));
        M upper = Mul(Swing(positions[shin] - positions[thigh], leg.knee - positions[thigh]), rotations[thigh]);
        M lower = Mul(Swing(positions[foot] - positions[shin], leg.ankle - leg.knee), rotations[shin]);
        joints[thigh] = Rotation(Mul(Inverse(rotations[p.parents[thigh]]), upper));
        joints[shin] = Rotation(Mul(Inverse(upper), lower));
        joints[foot] = Rotation(Mul(Inverse(lower), footRotation));
        Fk(p, joints, rotations, positions, PLAYER_LIMB_ROOT);
    }
    contactInfo.valid = true;
    contactInfo.tick = pose->tick;
    contactInfo.visualScale = scale;
    std::copy_n(pose->joints[p.source[0]].world, 16, contactInfo.nativePelvis);
    anchor.Position(contactInfo.nativePelvis + 12, contactInfo.nativePelvis + 12);
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            contactInfo.linkRoot[col * 4 + row] = yaw.v[row][col];
    contactInfo.linkRoot[15] = 1;
    std::copy_n(frame.position, 3, contactInfo.linkRoot + 12);
    std::copy_n(board, 16, contactInfo.board);
    contactInfo.board[12] = center.x;
    contactInfo.board[13] = center.y;
    contactInfo.board[14] = center.z;
    for (int side = 0; side < 2; ++side) {
        V sole =
            World(positions[feet[side]] + Mul(rotations[feet[side]], NativeSkateContactMath::Sole(frame.adult, side)),
                  player, frame);
        contactInfo.error[side] = Length(sole - targets[side]);
        contactInfo.clearance[side] = Dot(sole - targets[side], up);
        for (int i = 0; i < 3; ++i)
            contactInfo.linkSoles[side][i] = i == 0 ? sole.x : i == 1 ? sole.y : sole.z;
    }
}
bool HeldBoard(const NativeSkateRuntime::SkaterPose* pose, const NativeSkateUnits::Anchor& anchor, const float* d,
               float out[16]) {
    if (!pose || !d || d[96] != 1.f || !applied || bodyInfo.poseTick != pose->tick ||
        !bodyInfo.valid[PLAYER_LIMB_WAIST])
        return false;
    float point[3];
    if (!anchor.Position(d + 92, point))
        return false;
    return NativeSkateBoardPresentation::Convert(bodyInfo.nativeBodyFrame, bodyInfo.linkBodyFrame, d + 80, point,
                                                 bodyInfo.joints[PLAYER_LIMB_WAIST], bodyInfo.heldRelativeScale, out);
}
void Capture(s32 limb) {
    if (contactInfo.valid && (limb == PLAYER_LIMB_WAIST || limb == PLAYER_LIMB_R_FOOT || limb == PLAYER_LIMB_L_FOOT)) {
        float* matrix = limb == PLAYER_LIMB_WAIST ? contactInfo.linkPelvis
                                                  : contactInfo.linkFeet[limb == PLAYER_LIMB_R_FOOT ? 0 : 1];
        Vec3f origin = {}, o;
        Matrix_MultVec3f(&origin, &o);
        matrix[12] = o.x;
        matrix[13] = o.y;
        matrix[14] = o.z;
        matrix[15] = 1;
        for (int col = 0; col < 3; ++col) {
            Vec3f axis = {}, end;
            if (col == 0)
                axis.x = 1;
            else if (col == 1)
                axis.y = 1;
            else
                axis.z = 1;
            Matrix_MultVec3f(&axis, &end);
            V v = Unit(V{ end.x - o.x, end.y - o.y, end.z - o.z });
            matrix[col * 4] = v.x;
            matrix[col * 4 + 1] = v.y;
            matrix[col * 4 + 2] = v.z;
        }
    }

    if (contactInfo.valid && (limb == PLAYER_LIMB_R_FOOT || limb == PLAYER_LIMB_L_FOOT)) {
        int side = limb == PLAYER_LIMB_R_FOOT ? 0 : 1;
        auto sole = NativeSkateContactMath::Sole(LINK_IS_ADULT, side);
        Vec3f local = { sole.x, sole.y, sole.z }, world;
        Matrix_MultVec3f(&local, &world);
        for (int i = 0; i < 3; ++i)
            contactInfo.linkSoles[side][i] = i == 0 ? world.x : i == 1 ? world.y : world.z;
        V delta = { world.x - contactInfo.targets[side][0], world.y - contactInfo.targets[side][1],
                    world.z - contactInfo.targets[side][2] };
        contactInfo.error[side] = Length(delta);
        contactInfo.clearance[side] = Dot(delta, V{ contactInfo.board[4], contactInfo.board[5], contactInfo.board[6] });
        M boardBasis;
        if (Basis(contactInfo.board, boardBasis)) {
            V relative = Mul(Inverse(boardBasis), V{ world.x - contactInfo.board[12], world.y - contactInfo.board[13],
                                                     world.z - contactInfo.board[14] });
            contactInfo.linkLocal[side][0] = relative.x;
            contactInfo.linkLocal[side][1] = relative.y;
            contactInfo.linkLocal[side][2] = relative.z;
        }
    }

    if (limb == PLAYER_LIMB_ROOT) {
        Vec3f origin = {}, o;
        Matrix_MultVec3f(&origin, &o);
        M rendered;
        for (int col = 0; col < 3; ++col) {
            Vec3f axis = {}, end;
            if (col == 0)
                axis.x = 1;
            else if (col == 1)
                axis.y = 1;
            else
                axis.z = 1;
            Matrix_MultVec3f(&axis, &end);
            V v = Unit(V{ end.x - o.x, end.y - o.y, end.z - o.z });
            for (int row = 0; row < 3; ++row)
                rendered.v[row][col] = row == 0 ? v.x : row == 1 ? v.y : v.z;
        }
        auto& p = profiles[LINK_IS_ADULT ? 0 : 1];
        if (p.ready) {
            M delta = Mul(rendered, Inverse(p.global[PLAYER_LIMB_ROOT]));
            for (int col = 0; col < 3; ++col)
                for (int row = 0; row < 3; ++row)
                    bodyInfo.linkBodyFrame[col * 4 + row] = delta.v[row][col];
        }
    }
    Vec3f zero = {}, out;
    Matrix_MultVec3f(&zero, &out);
    if (limb > 0 && limb < PLAYER_LIMB_MAX) {
        bodyInfo.joints[limb][0] = out.x;
        bodyInfo.joints[limb][1] = out.y;
        bodyInfo.joints[limb][2] = out.z;
        bodyInfo.valid[limb] = true;
        Vec3f axis = { 0, 1, 0 }, end;
        Matrix_MultVec3f(&axis, &end);
        float dx = end.x - out.x, dy = end.y - out.y, dz = end.z - out.z;
        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len > .00001f) {
            bodyInfo.up[limb][0] = dx / len;
            bodyInfo.up[limb][1] = dy / len;
            bodyInfo.up[limb][2] = dz / len;
        }
    }
    if (limb == PLAYER_LIMB_R_FOOT) {
        linkFeet[0] = { out.x, out.y, out.z };
        feetValid[0] = true;
    }
    if (limb == PLAYER_LIMB_L_FOOT) {
        linkFeet[1] = { out.x, out.y, out.z };
        feetValid[1] = true;
    }
    if (limb == PLAYER_LIMB_WAIST) {
        linkPelvis = { out.x, out.y, out.z };
        pelvisValid = true;
    }
}
namespace {
void Line(PlayState* play, V a, V b, Color_RGBA8 color) {
    V delta = b - a;
    if (Length(delta) < .01f || Length(delta) > 300)
        return;
    V side = Unit(Cross(delta, V{ 0, 1, 0 }));
    if (Length(side) < .5f)
        side = V{ 1, 0, 0 };
    side = side * .35f;
    V points[4] = { side, side * -1, delta - side, delta + side };
    auto* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
    if (!v)
        return;
    for (int i = 0; i < 4; ++i) {
        v[i] = {};
        v[i].v.ob[0] = (s16)std::lrint(points[i].x * 100);
        v[i].v.ob[1] = (s16)std::lrint(points[i].y * 100);
        v[i].v.ob[2] = (s16)std::lrint(points[i].z * 100);
    }
    Matrix_Push();
    Matrix_Translate(a.x, a.y, a.z, MTXMODE_NEW);
    Matrix_Scale(.01f, .01f, .01f, MTXMODE_APPLY);
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, color.r, color.g, color.b, color.a);
    gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    Matrix_Pop();
}
} // namespace
void Debug(PlayState* play, const NativeSkateRuntime::SkaterPose* pose, const NativeSkateUnits::Anchor& anchor,
           bool active, const char* state, const char* error) {
    if (contactInfo.valid && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.LinkContactDebug"), 0)) {
        static uint64_t last = ~uint64_t(0);
        if (last != contactInfo.tick) {
            last = contactInfo.tick;
            auto matrixText = [](const float* m) {
                std::ostringstream text;
                for (int i = 0; i < 16; ++i) {
                    if (i)
                        text << ',';
                    text << m[i];
                }
                return text.str();
            };
            SPDLOG_INFO("[NativeLinkContact] tick={} age={} board={} nativePelvis={} linkRoot={} linkPelvis={}",
                        contactInfo.tick, LINK_IS_ADULT ? "ADULT" : "CHILD", matrixText(contactInfo.board),
                        matrixText(contactInfo.nativePelvis), matrixText(contactInfo.linkRoot),
                        matrixText(contactInfo.linkPelvis));
            for (int foot = 0; foot < 2; ++foot)
                SPDLOG_INFO(
                    "[NativeLinkContact] side={} nativeFoot={} nativeBoardLocal={},{},{} linkFoot={} sole={},{},{} target={},{},{} nativeJointClearance={} soleClearance={} error={} contact={} linkBoardLocalHost={},{},{}",
                    foot, matrixText(contactInfo.nativeFeet[foot]), contactInfo.nativeLocal[foot][0],
                    contactInfo.nativeLocal[foot][1], contactInfo.nativeLocal[foot][2],
                    matrixText(contactInfo.linkFeet[foot]), contactInfo.linkSoles[foot][0],
                    contactInfo.linkSoles[foot][1], contactInfo.linkSoles[foot][2], contactInfo.targets[foot][0],
                    contactInfo.targets[foot][1], contactInfo.targets[foot][2], contactInfo.nativeClearance[foot],
                    contactInfo.clearance[foot], contactInfo.error[foot], contactInfo.weight[foot],
                    contactInfo.linkLocal[foot][0], contactInfo.linkLocal[foot][1], contactInfo.linkLocal[foot][2]);
        }
    }

    if (!play)
        return;
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.LinkContactDebug"), 0) && contactInfo.valid) {
        M basis;
        if (Basis(contactInfo.board, basis)) {
            V center = { contactInfo.board[12], contactInfo.board[13], contactInfo.board[14] };
            float size = NativeSkateUnits::kOotUnitsPerMeter * contactInfo.visualScale;
            V corners[4];
            for (int i = 0; i < 4; ++i)
                corners[i] = center + Mul(basis, V{ (i == 0 || i == 3 ? -.1f : .1f) * size, .01f * size,
                                                    (i < 2 ? -.4f : .4f) * size });
            for (int i = 0; i < 4; ++i)
                Line(play, corners[i], corners[(i + 1) % 4], { 220, 220, 60, 220 });
        }
        for (int side = 0; side < 2; ++side) {
            V a = { contactInfo.linkSoles[side][0], contactInfo.linkSoles[side][1], contactInfo.linkSoles[side][2] },
              b = { contactInfo.targets[side][0], contactInfo.targets[side][1], contactInfo.targets[side][2] };
            Line(play, a, b, { 255, 80, 50, 255 });
            Line(play, b + V{ -.5f, 0, 0 }, b + V{ .5f, 0, 0 }, { 40, 255, 90, 255 });
            Line(play, b + V{ 0, -.5f, 0 }, b + V{ 0, .5f, 0 }, { 40, 255, 90, 255 });
        }
        GraphicsContext* __gfxCtx = play->state.gfxCtx;
        Gfx* opa = POLY_OPA_DISP;
        Gfx* gfx = Graph_GfxPlusOne(opa);
        gSPDisplayList(OVERLAY_DISP++, gfx);
        GfxPrint printer;
        GfxPrint_Init(&printer);
        GfxPrint_Open(&printer, gfx);
        GfxPrint_SetColor(&printer, 240, 255, 220, 255);
        GfxPrint_SetPos(&printer, 1, 4);
        GfxPrint_Printf(&printer, "CONTACT R/L:%.2f %.2f %s", contactInfo.weight[0], contactInfo.weight[1],
                        LINK_IS_ADULT ? "ADULT" : "CHILD");
        GfxPrint_SetPos(&printer, 1, 5);
        GfxPrint_Printf(&printer, "SOLE ERROR R/L:%.2f %.2f", contactInfo.error[0], contactInfo.error[1]);
        GfxPrint_SetPos(&printer, 1, 6);
        GfxPrint_Printf(&printer, "DECK CLEARANCE R/L:%.2f %.2f", contactInfo.clearance[0], contactInfo.clearance[1]);
        gfx = GfxPrint_Close(&printer);
        GfxPrint_Destroy(&printer);
        gSPEndDisplayList(gfx++);
        Graph_BranchDlist(opa, gfx);
        POLY_OPA_DISP = gfx;
    }
    const bool skeleton = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugSkeleton"), 0) != 0;
    if (skeleton && pose) {
        for (uint32_t i = 0; i < pose->count; ++i) {
            int parent = pose->joints[i].parent;
            if (parent < 0 || parent >= (int)pose->count)
                continue;
            V a, b;
            if (HostPoint(pose->joints[i], anchor, a) && HostPoint(pose->joints[parent], anchor, b))
                Line(play, a, b, { 40, 230, 255, 200 });
        }
        if (active && applied)
            for (int i = 0; i < 2; ++i)
                if (feetValid[i])
                    Line(play, nativeFeet[i], linkFeet[i], { 255, 220, 50, 255 });
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugHeldBoard"), 0) ||
        CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugBody"), 0) ||
        CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugAnimation"), 0))
        return;
    if (!CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Debug"), 0) && !skeleton)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    bool valid = active && applied;
    GfxPrint_SetPos(&p, 1, 17);
    GfxPrint_Printf(&p, "POSE:%s JOINTS:%u LOWER:%s %s", valid ? "NATIVE" : "OOT", pose ? pose->count : 0,
                    valid ? "ON" : "OFF", LINK_IS_ADULT ? "ADULT" : "CHILD");
    if (valid && feetValid[0] && feetValid[1]) {
        GfxPrint_SetPos(&p, 1, 18);
        GfxPrint_Printf(&p, "FOOT ERROR L/R:%.2f %.2f PELVIS:%.2f", Length(linkFeet[1] - nativeFeet[1]),
                        Length(linkFeet[0] - nativeFeet[0]), pelvisValid ? Length(linkPelvis - nativePelvis) : 0);
        GfxPrint_SetPos(&p, 1, 19);
        GfxPrint_Printf(&p, "NATIVE L:%.1f %.1f %.1f R:%.1f %.1f %.1f", nativeFeet[1].x, nativeFeet[1].y,
                        nativeFeet[1].z, nativeFeet[0].x, nativeFeet[0].y, nativeFeet[0].z);
        GfxPrint_SetPos(&p, 1, 20);
        GfxPrint_Printf(&p, "LINK L:%.1f %.1f %.1f R:%.1f %.1f %.1f", linkFeet[1].x, linkFeet[1].y, linkFeet[1].z,
                        linkFeet[0].x, linkFeet[0].y, linkFeet[0].z);
    } else {
        GfxPrint_SetPos(&p, 1, 18);
        GfxPrint_Printf(&p, "FOOT/PELVIS ERROR:N/A %.20s", state ? state : "");
    }
    const char* why = !failure.empty() ? failure.c_str() : error;
    if (why && *why) {
        GfxPrint_SetColor(&p, 255, 100, 80, 255);
        GfxPrint_SetPos(&p, 1, 21);
        GfxPrint_Printf(&p, "POSE:%.55s", why);
    }
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateRetarget
