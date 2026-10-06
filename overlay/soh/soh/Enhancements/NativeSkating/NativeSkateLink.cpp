extern "C" int NativeSkateHandplantTarget(float*);
extern "C" float NativeSkateHandplantWeight();
#include "NativeSkateLink.h"
#include "NativeSkateBoardModel.h"
#include "NativeSkateRetargetMath.h"
#include "NativeSkateRetarget.h"
#include "NativeSkateBoardPresentation.h"
#include "NativeSkateObjectDropper.h"
#include "NativeSkatePresentationMath.h"
#include "NativeSkateProxyGeometry.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>
extern "C" {
#include "global.h"
#include "z64.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "assets/objects/object_link_boy/object_link_boy.h"
#include "assets/objects/object_link_child/object_link_child.h"
void Player_DrawGameplay(PlayState*, Player*, s32, Gfx*, OverrideLimbDrawOpa);
}
#include "NativeSkateJumpRender.h"
namespace NativeSkateLink {
namespace {
std::array<Vec3s, PLAYER_LIMB_BUF_COUNT> neutral{}, drawJoints{};
Player* owner = nullptr;
bool swordActive = false;
int age = -1;
std::array<Vec3s, PLAYER_LIMB_BUF_COUNT> bipedPose{}, lastPose{}, blendPose{};
float jumpTime = 0, landingTime = 1;
bool jumpAir = false;
int animation = 0;
float animationFrame = 0, blendTime = 1, poseTime = 1, visualDt = 0;
uint64_t previousTick = 0;
bool biped = false, havePose = false;
Vec3f head = {}, hand = {};
float handFrame[16] = {};
float carryWeight = 0;
uint64_t carryTick = 0;
bool headValid = false, handValid = false;
void BlendJoints(Vec3s* out, const Vec3s* a, float t) {
    for (int i = 0; i < PLAYER_LIMB_BUF_COUNT; ++i)
        for (int axis = 0; axis < 3; ++axis) {
            auto* dst = &out[i].x;
            const auto* src = &a[i].x;
            int delta = i ? static_cast<s16>(dst[axis] - src[axis]) : dst[axis] - src[axis];
            dst[axis] = static_cast<s16>(std::lrint(src[axis] + delta * t));
        }
}

Vec3f renderedRoot = {};
bool rendered = false;
bool renderJump = false, renderJumpCarry = false, renderJumpUpper = true;
NativeSkatePresentationMath::BipedJumpPose renderJumpPose{};
std::array<Vec3s, PLAYER_LIMB_MAX> finalJumpRot{};
std::array<Vec3f, PLAYER_LIMB_MAX> finalJumpPosition{};
bool jumpDiagnosticLogged = false;
void RootOnlyPost(PlayState* play, s32 limb, Gfx** dl, Vec3s* rot, void* data) {
    if (renderJump && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0) && limb > 0 &&
        limb < PLAYER_LIMB_MAX) {
        Vec3f zero = {};
        finalJumpRot[limb] = *rot;
        Matrix_MultVec3f(&zero, &finalJumpPosition[limb]);
    }

    if (limb == PLAYER_LIMB_HEAD || limb == PLAYER_LIMB_R_HAND) {
        Vec3f zero = {}, world;
        Matrix_MultVec3f(&zero, &world);
        if (limb == PLAYER_LIMB_HEAD) {
            head = world;
            headValid = true;
        } else {
            hand = world;
            Matrix_Get((MtxF*)handFrame);
            handValid = true;
        }
    }
    NativeSkateRetarget::Capture(limb);
    Player_PostLimbDrawGameplay(play, limb, dl, rot, data);
}
s32 RootOnlyOverride(PlayState* play, s32 limb, Gfx** dl, Vec3f* pos, Vec3s* rot, void* data) {
    // Stock equipment/head callbacks run first. Ground IK is excluded for the
    // authored native leg pose. Final jump corrections cannot be overwritten
    // by an earlier Player callback or an animation update.
    s32 result = false;
    if (limb != PLAYER_LIMB_L_THIGH && limb != PLAYER_LIMB_R_THIGH)
        result = Player_OverrideLimbDrawGameplayDefault(play, limb, dl, pos, rot, data);
    if (renderJump && (renderJumpUpper || limb < PLAYER_LIMB_UPPER))
        NativeSkateJumpRender::Apply(limb, *pos, *rot, renderJumpPose, renderJumpCarry);
    return result;
}
} // namespace
void Begin(PlayState* play, Player* player) {
    owner = player;
    age = gSaveContext.linkAge;
    rendered = false;
    havePose = false;
    headValid = handValid = false;
    previousTick = 0;
    jumpAir = false;
    jumpTime = 0;
    landingTime = 1;
    carryWeight = 0;
    carryTick = 0;
    animation = 0;
    animationFrame = 0;
    blendTime = poseTime = 1;
    NativeSkateRetarget::Reset();
    std::copy_n(player->skelAnime.jointTable, PLAYER_LIMB_BUF_COUNT, neutral.begin());
    // Read a stock OoT pose into adapter storage. Do not change the Player's
    // animation state or import any Skate skeleton data.
    AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)gPlayerAnim_link_normal_wait_free, 0,
                                  player->skelAnime.limbCount, neutral.data());
    bipedPose = neutral;
}
void End() {
    swordActive = false;
    headValid = handValid = false;
    owner = nullptr;
    age = -1;
    rendered = false;
    NativeSkateRetarget::Reset();
}
void Sword(bool active) {
    swordActive = active;
}
void UpdateVisual(PlayState* play, Player* player, const char* state, float speed, uint64_t tick) {
    if (owner != player)
        return;
    if (age != gSaveContext.linkAge)
        Begin(play, player);
    bool airborne = std::strcmp(state, "BipedAir") == 0;
    if (airborne && !jumpAir)
        jumpTime = 0;
    if (!airborne && jumpAir)
        landingTime = 0;
    jumpAir = airborne;
    if (!airborne)
        jumpDiagnosticLogged = false;
    visualDt = previousTick && tick >= previousTick ? std::min(.1f, (tick - previousTick) / 60.f) : 0;
    previousTick = tick;
    bool now = std::strcmp(state, "BipedGround") == 0 || std::strcmp(state, "BipedAir") == 0;
    if (now != biped) {
        biped = now;
        if (havePose)
            blendPose = lastPose;
        blendTime = 0;
    }
    blendTime += visualDt;
    poseTime += visualDt;
    if (jumpAir)
        jumpTime += visualDt;
    else
        landingTime += visualDt;
    if (!biped)
        return;
    int next = NativeSkatePresentationMath::BipedJumpAnimation(animation, speed, jumpAir);
    if (next != animation) {
        if (havePose)
            blendPose = lastPose;
        poseTime = 0;
        animation = next;
        animationFrame = 0;
    }
    auto* anim = (LinkAnimationHeader*)(animation == 0   ? gPlayerAnim_link_normal_wait_free
                                        : animation == 1 ? gPlayerAnim_link_normal_walk_free
                                                         : gPlayerAnim_link_normal_run_free);
    float rate = animation == 0 ? 1.f : std::clamp(speed / (animation == 1 ? 1.4f : 3.5f), .65f, 1.5f);
    const auto jumpBase = NativeSkatePresentationMath::JumpBase(jumpAir, jumpTime, landingTime, speed);
    if (jumpBase.kind) {
        anim = (LinkAnimationHeader*)(jumpBase.kind == 1   ? gPlayerAnim_link_normal_jump
                                      : jumpBase.kind == 2 ? gPlayerAnim_link_normal_run_jump
                                                           : gPlayerAnim_link_normal_landing_free);
        animationFrame = Animation_GetLastFrame(anim) * jumpBase.progress;
    } else
        animationFrame = std::fmod(animationFrame + visualDt * 30 * rate, Animation_GetLastFrame(anim) + 1.f);
    AnimationContext_SetLoadFrame(play, anim, (s32)animationFrame, player->skelAnime.limbCount, bipedPose.data());
}
float HeadY() {
    return headValid ? head.y : 0;
}
bool CarryMatrix(const Frame& frame, float spatialScale, float out[16]) {
    if (!handValid || !frame.valid)
        return false;
    using namespace NativeSkateRetargetMath;
    // Capture the actual hand matrix (including model scale), and remove scale
    // only from its rotation. Finger/grip origin still uses the real model matrix.
    const V grip = frame.adult ? V{ 5.12f, 274.5f, -7.18f } : V{ 17.368421f, 210.078947f, 4.815789f };
    V worldGrip = { handFrame[12], handFrame[13], handFrame[14] };
    for (int row = 0; row < 3; ++row)
        (&worldGrip.x)[row] += handFrame[row] * grip.x + handFrame[4 + row] * grip.y + handFrame[8 + row] * grip.z;
    M handRotation;
    for (int col = 0; col < 3; ++col) {
        V axis = Unit(V{ handFrame[col * 4], handFrame[col * 4 + 1], handFrame[col * 4 + 2] });
        for (int row = 0; row < 3; ++row)
            handRotation.v[row][col] = (&axis.x)[row];
    }
    // Measured normal_wait_free hand basis; both actual skeletons use this pose,
    // but independent hand mesh centers and arm lengths are retained.
    M neutralHand;
    float measured[3][3] = { { .15356698f, .14146031f, .97796021f },
                             { .41457369f, -.90760582f, .06618405f },
                             { .89696479f, .39527288f, -.19802401f } };
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            neutralHand.v[r][c] = measured[r][c];
    M boardRest;
    float boardAxes[3][3] = { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } };
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            boardRest.v[r][c] = boardAxes[r][c];
    M rotation = Mul(handRotation, Mul(Inverse(neutralHand), boardRest));
    auto scale = NativeSkateBoardModel::VisualScale(
        frame.adult, CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScale"), 1),
        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleAdult"), 1),
        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleChild"), .8f));
    // Grip the deck edge at its midpoint. The deck origin/pivot stays unchanged;
    // this attachment offset exactly inverts the scaled model grip point.
    V center =
        worldGrip - Mul(rotation, V{ .1f * NativeSkateUnits::kOotUnitsPerMeter * scale.final / spatialScale, 0, 0 });
    std::fill_n(out, 16, 0.f);
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r)
            out[c * 4 + r] = rotation.v[r][c];
    out[12] = center.x;
    out[13] = center.y;
    out[14] = center.z;
    out[15] = 1;
    return true;
}
const char* AnimationName() {
    return !biped ? "NATIVE" : animation == 0 ? "IDLE" : animation == 1 ? "WALK" : "RUN";
}
void TraceNativePose(Player* player, const Frame& frame, const NativeSkateRuntime::SkaterPose* pose,
                     const NativeSkateUnits::Anchor& anchor, const float* d, const float* board, float pitch) {
    if (owner != player || !pose || !frame.valid)
        return;
    auto sample = neutral;
    NativeSkateRetarget::Apply(player, frame, sample.data(), pose, anchor, d, board, pitch);
}
void Draw(PlayState* play, Player* player, const Frame& frame, const NativeSkateRuntime::SkaterPose* pose,
          const NativeSkateUnits::Anchor& anchor, const char* state, const float* riderDiagnostics, const float* board,
          float bodyPitch) {
    if (!play || !player || !frame.valid || owner != player)
        return;
    if (age != gSaveContext.linkAge)
        Begin(play, player);
    drawJoints = biped ? bipedPose : neutral;
    headValid = handValid = false;
    NativeSkateRetarget::FrameStart();
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.LinkRetarget"), 1) && std::strcmp(state, "BipedGround") != 0 &&
        std::strcmp(state, "BipedAir") != 0)
        NativeSkateRetarget::Apply(player, frame, drawJoints.data(), pose, anchor, riderDiagnostics, board, bodyPitch);
    const float duration = NativeSkatePresentationMath::Bound(
        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.PresentationBlendTime"), .2f), .2f, .1f, .35f);
    if (havePose && blendTime < duration)
        BlendJoints(drawJoints.data(), blendPose.data(), NativeSkatePresentationMath::Smooth(blendTime / duration));
    else if (biped && havePose && poseTime < .15f)
        BlendJoints(drawJoints.data(), blendPose.data(), NativeSkatePresentationMath::Smooth(poseTime / .15f));
    if (!biped && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.LinkRetarget"), 1))
        NativeSkateRetarget::Contact(player, frame, drawJoints.data(), pose, anchor, riderDiagnostics, board);
    float plantTarget[3];
    if (!biped && NativeSkateHandplantTarget(plantTarget))
        NativeSkateRetarget::HandplantHand(player, frame, drawJoints.data(), pose, plantTarget,
                                           NativeSkateHandplantWeight());
    if (!biped)
        NativeSkateRetarget::GrabHands(player, frame, drawJoints.data(), pose, anchor, riderDiagnostics);
    const bool carry = biped && riderDiagnostics && riderDiagnostics[40] > 0 &&
                       CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.BipedCarryBoard"), 1) != 0;
    if (carryTick != (pose ? pose->tick : previousTick)) {
        carryWeight = std::clamp(carryWeight + (carry ? 1.f : -1.f) * visualDt / .15f, 0.f, 1.f);
        carryTick = (pose ? pose->tick : previousTick);
    }
    if (biped && carryWeight > 0)
        NativeSkateRetarget::CarryArm(player, drawJoints.data(), neutral.data(), pose,
                                      NativeSkatePresentationMath::Smooth(carryWeight));
    renderJump = biped && (jumpAir || landingTime < .30f);
    renderJumpCarry = carry;
    renderJumpUpper = !swordActive && !NativeSkateObjectDropper::CarryingProp();
    renderJumpPose = NativeSkatePresentationMath::JumpPose(jumpAir, jumpTime, landingTime, carry);
    if (biped && NativeSkateObjectDropper::CarryingProp())
        NativeSkateRetarget::PropArms(player, drawJoints.data(), neutral.data(), pose);
    // The stock melee animation and real hand/sword post-draw callback supply
    // the upper body and swept AT quads; native root/legs/contact stay intact.
    if (swordActive)
        for (int limb = PLAYER_LIMB_UPPER; limb < PLAYER_LIMB_MAX; ++limb)
            drawJoints[limb] = player->skelAnime.jointTable[limb];
    lastPose = drawJoints;
    havePose = true;
    auto* joints = player->skelAnime.jointTable;
    const auto head = player->headLimbRot, upper = player->upperLimbRot;
    const auto upperYaw = player->upperLimbYawSecondary, rootPitch = player->unk_6C2;
    const float compression = player->unk_6C4;
    const auto movement = player->skelAnime.movementFlags;
    player->skelAnime.jointTable = drawJoints.data();
    player->skelAnime.movementFlags = 0; // normal child-root proportion conversion
    player->headLimbRot = { 0, 0, 0 };
    player->upperLimbRot = { 0, 0, 0 };
    player->upperLimbYawSecondary = 0;
    player->unk_6C2 = 0;
    player->unk_6C4 = 0;
    Matrix_Push();
    Vec3s rotation = { 0, frame.yaw, 0 };
    // Absolute host render frame, installed once. No actor matrix accumulation,
    // shape.yOffset, native skeleton basis, board roll, pitch or shuv parent.
    Matrix_SetTranslateRotateYXZ(frame.position[0], frame.position[1], frame.position[2], &rotation);
    Vec3f zero = { 0, 0, 0 };
    Matrix_MultVec3f(&zero, &renderedRoot);
    rendered = true;
    Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
    // Same graphics setup as Player_Draw; Actor_Draw has already installed
    // the real Player object segment and lights for this callback.
    func_80093C80(play);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    Player_DrawImpl(play, player->skelAnime.skeleton, player->skelAnime.jointTable, player->skelAnime.dListCount, 0,
                    player->currentTunic, player->currentBoots, player->actor.shape.face, RootOnlyOverride,
                    RootOnlyPost, player);
    Matrix_Pop();
    if (renderJump && jumpAir && jumpTime >= .12f && !jumpDiagnosticLogged &&
        CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0)) {
        SPDLOG_INFO(
            "[NativeSkate] FINAL_JUMP_RENDER age={} carried={} waistZ={} thighZ={} shinZ={} upperArmZ={} forearmZ={} kneeY={} footY={}",
            frame.adult ? "Adult" : "Child", carry, finalJumpRot[PLAYER_LIMB_WAIST].z,
            finalJumpRot[PLAYER_LIMB_L_THIGH].z, finalJumpRot[PLAYER_LIMB_L_SHIN].z,
            finalJumpRot[PLAYER_LIMB_L_SHOULDER].z, finalJumpRot[PLAYER_LIMB_L_FOREARM].z,
            finalJumpPosition[PLAYER_LIMB_L_SHIN].y, finalJumpPosition[PLAYER_LIMB_L_FOOT].y);
        jumpDiagnosticLogged = true;
    }
    renderJump = false;
    player->skelAnime.jointTable = joints;
    player->skelAnime.movementFlags = movement;
    player->headLimbRot = head;
    player->upperLimbRot = upper;
    player->upperLimbYawSecondary = upperYaw;
    player->unk_6C2 = rootPitch;
    player->unk_6C4 = compression;
}
void Debug(PlayState* play, Player* player, const Frame& f, const float nativeRoot[3], bool proxy, const float* board,
           bool carry, float spatialScale) {
    if (!play || !player)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    GfxPrint_SetPos(&p, 1, 10);
    GfxPrint_Printf(&p, "RIDER PRESENTATION:%s AGE:%s", proxy ? "PROXY" : "LINK", f.adult ? "ADULT" : "CHILD");
    GfxPrint_SetPos(&p, 1, 11);
    GfxPrint_Printf(&p, "LINK ROOT:%.2f %.2f %.2f", f.position[0], f.position[1], f.position[2]);
    GfxPrint_SetPos(&p, 1, 12);
    GfxPrint_Printf(&p, "HEAD Y:%.2f CAM AT/EYE:%.2f %.2f", HeadY(), GET_ACTIVE_CAM(play)->at.y,
                    GET_ACTIVE_CAM(play)->eye.y);
    GfxPrint_SetPos(&p, 1, 13);
    GfxPrint_Printf(&p, "CAM TARGET OFFSET:%.2f",
                    NativeSkatePresentationMath::Bound(
                        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.CameraTargetOffset"), 0), 0, -20, 20));
    const float ex = renderedRoot.x - f.position[0], ey = renderedRoot.y - f.position[1],
                ez = renderedRoot.z - f.position[2];
    GfxPrint_SetPos(&p, 1, 14);
    if (rendered && !proxy)
        GfxPrint_Printf(&p, "ROOT ERROR:%.2f %.2f %.2f / %.3f", ex, ey, ez, std::sqrt(ex * ex + ey * ey + ez * ez));
    else
        GfxPrint_Printf(&p, "ROOT ERROR:N/A DRAW:%s", proxy ? "PROXY" : "PENDING");
    GfxPrint_SetPos(&p, 1, 15);
    GfxPrint_Printf(&p, "ANIM:%s CARRY:%u BOARD SCALE:%.2f", AnimationName(),
                    biped && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.BipedCarryBoard"), 1),
                    NativeSkatePresentationMath::Bound(
                        CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScale"), 1), 1, .5f, 1.25f));
    GfxPrint_SetPos(&p, 1, 16);
    GfxPrint_Printf(
        &p, "BOARD:%s TOP:%.2f PARENT:NONE", carry ? "HAND" : "NATIVE",
        board ? board[13] + board[5] *
                                NativeSkatePresentationMath::Bound(
                                    CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScale"), 1), 1, .5f, 1.25f) *
                                NativeSkateUnits::kOotUnitsPerMeter * .02f / spatialScale
              : 0);
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateLink
