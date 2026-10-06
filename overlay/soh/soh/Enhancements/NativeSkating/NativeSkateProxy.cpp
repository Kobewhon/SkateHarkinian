#include "NativeSkateBoardAppearance.h"
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/DisplayList.h>
#include <array>
#include <memory>
#include "NativeSkateProxy.h"
#include "NativeSkateUnits.h"
#include "NativeSkateProxyGeometry.h"
#include "NativeSkateBoardModel.h"
#include "NativeSkateRiderScale.h"
#include "NativeSkatePresentationMath.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include <algorithm>
#include <cmath>
#include <cstring>
extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
}

namespace NativeSkateProxy {
namespace {
void LoadMatrix(PlayState* play) {
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
}
void Box(PlayState* play, float x, float y, float z, float sx, float sy, float sz, Color_RGBA8 color) {
    Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * 8);
    if (!v)
        return;
    // Unit cube vertices avoid quantizing small host-unit dimensions to s16.
    const float p[8][3] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
                            { -1, -1, 1 },  { 1, -1, 1 },  { 1, 1, 1 },  { -1, 1, 1 } };
    for (int i = 0; i < 8; ++i) {
        v[i].v.ob[0] = (s16)p[i][0];
        v[i].v.ob[1] = (s16)p[i][1];
        v[i].v.ob[2] = (s16)p[i][2];
        v[i].v.flag = 0;
        v[i].v.tc[0] = v[i].v.tc[1] = 0;
        v[i].v.cn[0] = color.r;
        v[i].v.cn[1] = color.g;
        v[i].v.cn[2] = color.b;
        v[i].v.cn[3] = color.a;
    }
    Matrix_Push();
    Matrix_Translate(x, y, z, MTXMODE_APPLY);
    Matrix_Scale(sx / 2.0f, sy / 2.0f, sz / 2.0f, MTXMODE_APPLY);
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_OPA_DISP++);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, color.r, color.g, color.b, color.a);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)v, 8, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSP2Triangles(POLY_OPA_DISP++, 4, 6, 5, 0, 4, 7, 6, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 4, 5, 0, 0, 5, 1, 0);
    gSP2Triangles(POLY_OPA_DISP++, 3, 2, 6, 0, 3, 6, 7, 0);
    gSP2Triangles(POLY_OPA_DISP++, 1, 5, 6, 0, 1, 6, 2, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 3, 7, 0, 0, 7, 4, 0);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BACK);
    Matrix_Pop();
}
// One cached visual resource owner; no equipment/inventory or gameplay mutation.
struct ShieldResources {
    bool attempted = false;
    std::shared_ptr<Fast::DisplayList> deck, symbol;
};
ShieldResources& Resources(NativeSkateBoardAppearance::BoardVisual visual) {
    static std::array<ShieldResources, 4> cache;
    auto& entry = cache[static_cast<int>(visual.style)];
    if (!entry.attempted) {
        entry.attempted = true;
        auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
        entry.deck = std::dynamic_pointer_cast<Fast::DisplayList>(manager->LoadResourceProcess(visual.model));
        if (visual.secondary)
            entry.symbol = std::dynamic_pointer_cast<Fast::DisplayList>(manager->LoadResourceProcess(visual.secondary));
    }
    return entry;
}
bool ShieldReady(NativeSkateBoardAppearance::BoardVisual visual) {
    if (visual.style == NativeSkateBoardAppearance::Style::Default)
        return true;
    auto& r = Resources(visual);
    return r.deck && r.deck->GetPointerSize() > 0 && r.deck->GetPointer() &&
           (!visual.secondary || (r.symbol && r.symbol->GetPointerSize() > 0 && r.symbol->GetPointer()));
}
void ShieldDeck(PlayState* play, NativeSkateBoardAppearance::BoardVisual visual, float hostPerMeter) {
    auto& r = Resources(visual);
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Matrix_Push();
    Matrix_Translate(visual.offset.x * hostPerMeter, visual.offset.y * hostPerMeter, visual.offset.z * hostPerMeter,
                     MTXMODE_APPLY);
    Matrix_RotateY(visual.rotation.y, MTXMODE_APPLY);
    Matrix_RotateX(visual.rotation.x, MTXMODE_APPLY);
    Matrix_RotateZ(visual.rotation.z, MTXMODE_APPLY);
    Matrix_Scale(visual.scale * hostPerMeter, visual.scale * hostPerMeter, visual.scale * hostPerMeter, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    // GetItem_DrawMirrorShield supplies segment 8 explicitly. Never inherit Link's
    // face/equipment segment or an unrelated actor's texture-scroll display list.
    if (visual.secondary)
        gSPSegment(POLY_OPA_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 64, 64, 1, 0, 0, 32, 32, 0, 2, 0, 1));
    LoadMatrix(play);
    gSPDisplayList(POLY_OPA_DISP++, r.deck->GetPointer());
    if (visual.secondary) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, r.symbol->GetPointer());
    }
    Matrix_Pop();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
}
void ProperBoard(PlayState* play, const NativeSkateProxyGeometry::Shape& shape, float hostPerMeter) {
    using namespace NativeSkateBoardAppearance;
    const int requested = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.BoardAppearance"), 0);
    auto visual = ResolveBoardVisual(requested);
    const bool ready = ShieldReady(visual);
    visual = WithResourceStatus(visual, ready);
    static std::array<bool, 4> warned{};
    static bool invalidWarned = false;
    if (!ready && !warned[static_cast<int>(Resolve(requested))]) {
        SPDLOG_WARN("[NativeSkate] Board appearance '{}' failed to load; using Default",
                    ResolveBoardVisual(requested).name);
        warned[static_cast<int>(Resolve(requested))] = true;
    }
    if (requested < 0 || requested > 3) {
        if (!invalidWarned) {
            SPDLOG_WARN("[NativeSkate] Invalid board appearance {}; using Default", requested);
            invalidWarned = true;
        }
    }
    const auto hardware = HardwareShape(visual, shape);
    static int lastDiagnostic = -999;
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DeveloperDiagnostics"), 0) && lastDiagnostic != requested) {
        SPDLOG_INFO(
            "[NativeSkate] BOARD APPEARANCE={} DECK MODEL={} RESOURCE RESOLVED={} VISUAL SCALE={} OFFSET=({},{},{}) ROTATION=({},{},{}) TRUCK FRONT=(0,{}, {}) TRUCK REAR=(0,{}, {})",
            visual.name, visual.model, ready ? "YES" : "NO; DEFAULT FALLBACK", visual.scale, visual.offset.x,
            visual.offset.y, visual.offset.z, visual.rotation.x, visual.rotation.y, visual.rotation.z,
            visual.hardwareY - .021f, hardware.frontZ, visual.hardwareY - .021f, hardware.backZ);
        lastDiagnostic = requested;
    }
    const bool shield = visual.style != Style::Default;
    if (shield)
        ShieldDeck(play, visual, hostPerMeter);
    struct MeshCache {
        NativeSkateProxyGeometry::Shape shape{};
        std::vector<NativeSkateBoardModel::Triangle> triangles;
    };
    static std::array<MeshCache, 4> cache;
    auto& cached = cache[static_cast<int>(visual.style)];
    if (cached.triangles.empty() || std::memcmp(&hardware, &cached.shape, sizeof(hardware)) != 0) {
        cached.triangles = NativeSkateBoardModel::Build(hardware, !shield, shield ? visual.hardwareY : 0);
        cached.shape = hardware;
    }
    const auto& mesh = cached.triangles;
    Matrix_Push();
    Matrix_Scale(hostPerMeter / 4096.f, hostPerMeter / 4096.f, hostPerMeter / 4096.f, MTXMODE_APPLY);
    LoadMatrix(play);
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    // Shield lists may change lighting, UV generation, palette and combining.
    // Hardware/default deck are vertex-color geometry with explicit isolated state.
    gDPPipeSync(POLY_OPA_DISP++);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BOTH | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_2CYCLE);
    gSPTexture(POLY_OPA_DISP++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    gDPSetAlphaCompare(POLY_OPA_DISP++, G_AC_NONE);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_PASS2);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    for (size_t base = 0; base < mesh.size(); base += 8) {
        const size_t count = std::min(size_t(8), mesh.size() - base);
        Vtx* vertices = (Vtx*)Graph_Alloc(play->state.gfxCtx, static_cast<int>(sizeof(Vtx) * count * 3));
        if (!vertices)
            break;
        for (size_t i = 0; i < count; ++i) {
            const auto& t = mesh[base + i];
            const NativeSkateBoardModel::Vertex points[] = { t.a, t.b, t.c };
            for (int j = 0; j < 3; ++j) {
                Vtx& v = vertices[i * 3 + j];
                v.v.ob[0] = s16(std::lround(points[j].x * 4096));
                v.v.ob[1] = s16(std::lround(points[j].y * 4096));
                v.v.ob[2] = s16(std::lround(points[j].z * 4096));
                v.v.flag = 0;
                v.v.tc[0] = v.v.tc[1] = 0;
                v.v.cn[0] = t.color.r;
                v.v.cn[1] = t.color.g;
                v.v.cn[2] = t.color.b;
                v.v.cn[3] = 255;
            }
        }
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)vertices, static_cast<int>(count * 3), 0);
        for (size_t i = 0; i < count; ++i)
            gSP1Triangle(POLY_OPA_DISP++, static_cast<int>(i * 3), static_cast<int>(i * 3 + 1),
                         static_cast<int>(i * 3 + 2), 0);
    }
    Matrix_Pop();
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (shield && visual.secondary)
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
}
void RiderBox(PlayState* play, float x, float y, float z, float sx, float sy, float sz, Color_RGBA8 c) {
    Box(play, x, y, z, sx, sy, sz, c);
}
void Text(PlayState* play, const NativeSkateRuntime::Snapshot& s, float speed, uint32_t collisions, uint32_t retained,
          const char* error) {
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    GfxPrint printer;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, POLY_OPA_DISP);
    GfxPrint_SetColor(&printer, 255, 255, 255, 255);
    GfxPrint_SetPos(&printer, 1, 1);
    GfxPrint_Printf(&printer, "NATIVE SKATE RUNTIME ACTIVE");
    GfxPrint_SetPos(&printer, 1, 2);
    GfxPrint_Printf(&printer, "STATE: %.32s  ID:%u", s.state, s.stateId);
    GfxPrint_SetPos(&printer, 1, 3);
    GfxPrint_Printf(&printer, "SPEED: %.2f m/s  HEADING: %.2f", speed, s.heading);
    GfxPrint_SetPos(&printer, 1, 4);
    GfxPrint_Printf(&printer, "COM m: %.2f %.2f %.2f  V m/s: %.2f %.2f %.2f", s.trajectory[0], s.trajectory[1],
                    s.trajectory[2], s.trajectoryVelocity[0], s.trajectoryVelocity[1], s.trajectoryVelocity[2]);
    GfxPrint_SetPos(&printer, 1, 5);
    GfxPrint_Printf(&printer, "DECK m: %.2f %.2f %.2f  NORMAL: %.2f %.2f %.2f", s.board[12], s.board[13], s.board[14],
                    s.groundNormal[0], s.groundNormal[1], s.groundNormal[2]);
    GfxPrint_SetPos(&printer, 1, 6);
    GfxPrint_Printf(&printer, "FLICKIT OLLIE %.1f KICK %.1f HEEL %.1f SHUV %.1f FS %.1f TRE %.1f", s.coreIntents[0],
                    s.coreIntents[2], s.coreIntents[3], s.coreIntents[4], s.coreIntents[5], s.coreIntents[6]);
    GfxPrint_SetPos(&printer, 1, 7);
    GfxPrint_Printf(&printer, "COLLISION TRIANGLES: %u  RETAINED INPUT: %u", collisions, retained);
    if (error && error[0]) {
        GfxPrint_SetColor(&printer, 255, 80, 80, 255);
        GfxPrint_SetPos(&printer, 1, 8);
        GfxPrint_Printf(&printer, "RUNTIME ERROR: %.64s", error);
    }
    POLY_OPA_DISP = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
}
} // namespace
void Draw(PlayState* play, const NativeSkateRuntime::Snapshot& s, const float rider[3], const float board[3],
          bool debug, uint32_t collisions, uint32_t retained, const char* error, float spatialScale,
          const float* dimensions, const float* biped, const char* previous, float moveDot, bool moveDotValid,
          bool drawRider, const float* visualBoard, bool drawBoard) {
    if (!play)
        return;
    const Color_RGBA8 deck = { 210, 130, 55, 255 }, truck = { 180, 190, 205, 255 }, wheel = { 45, 45, 55, 255 };
    MtxF native;
    const bool boardFrameValid = NativeSkateProxyGeometry::BoardRotation(s.board, reinterpret_cast<float*>(&native));
    native.xw = board[0];
    native.yw = board[1];
    native.zw = board[2];
    if (boardFrameValid && drawBoard) {
        if (visualBoard)
            std::copy_n(visualBoard, 16, reinterpret_cast<float*>(&native));
        Matrix_Push();
        Matrix_Put(&native);
        const auto profile = NativeSkateBoardModel::VisualScale(
            LINK_IS_ADULT, CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScale"), 1.f),
            CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleAdult"), 1.f),
            CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleChild"), .8f));
        const float visualScale = profile.final;
        Matrix_Scale(visualScale, visualScale, visualScale, MTXMODE_APPLY);
        LoadMatrix(play);
        // The sole orange primitive is DEBUG_BOARD. All board sizes/offsets are
        // authored in meters, converted once, under the unscaled native rotation.
        using namespace NativeSkateProxyGeometry;
        const auto host = [spatialScale](NativeSkateUnits::RustMeters meters) {
            return NativeSkateUnits::RustLengthToHost(meters).value / spatialScale;
        };
        const auto shape = NativeShape(dimensions);
        if (!CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugBoard"), 0))
            ProperBoard(play, shape, host({ 1.f }));
        else {
            Box(play, 0, 0, 0, host(kDeckWidth), host(kDeckThickness), host(kDeckLength), deck);
            for (float z : { shape.frontZ, shape.backZ })
                Box(play, 0, host({ shape.wheelY }), host({ z }), host({ shape.wheelX * 2 }), host({ 0.025f }),
                    host({ 0.06f }), truck);
            for (int side : { -1, 1 })
                for (float z : { shape.frontZ, shape.backZ }) {
                    Box(play, side * host({ shape.wheelX }), host({ shape.wheelY }), host({ z }), host({ 0.025f }),
                        host({ shape.radius * 2 }), host({ shape.radius * 2 }), wheel);
                }
        }
        Matrix_Pop();
    }
    if (drawRider) {
        const Color_RGBA8 suit = { 50, 190, 235, 255 }, skin = { 245, 205, 155, 255 }, pants = { 40, 75, 150, 255 };
        Matrix_Push();
        Matrix_Translate(rider[0], rider[1], rider[2], MTXMODE_NEW);
        Matrix_RotateY(std::strcmp(s.state, "BipedGround") == 0 && biped ? biped[3] : s.heading, MTXMODE_APPLY);
        Matrix_Scale(1 / spatialScale, 1 / spatialScale, 1 / spatialScale, MTXMODE_APPLY);
        const auto riderScale =
            NativeSkateRiderScale::Get(CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.RiderScaleMode"), 1),
                                       CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.RiderScale"), 1), spatialScale);
        Matrix_Translate(0, NativeSkateRiderScale::kFoot, 0, MTXMODE_APPLY);
        Matrix_Scale(riderScale.ratio, riderScale.ratio, riderScale.ratio, MTXMODE_APPLY);
        Matrix_Translate(0, -NativeSkateRiderScale::kFoot, 0, MTXMODE_APPLY);
        LoadMatrix(play);
        RiderBox(play, 0, 12, 0, 13, 24, 9, suit);
        RiderBox(play, 0, 30, 0, 10, 11, 10, skin);
        RiderBox(play, -10, 12, 0, 6, 22, 7, suit);
        RiderBox(play, 10, 12, 0, 6, 22, 7, suit);
        // Adult block proportions about the native COM; no frame-dependent root lift.
        RiderBox(play, 0, -7, 0, 13, 16, 9, pants);
        RiderBox(play, -4, -36, 0, 7, 44, 8, pants);
        RiderBox(play, 4, -36, 0, 7, 44, 8, pants);
        Matrix_Pop();
    }
    const auto riderScale =
        NativeSkateRiderScale::Get(CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.RiderScaleMode"), 1),
                                   CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.RiderScale"), 1), spatialScale);
    if (debug && std::strcmp(s.state, "BipedGround") != 0) {
        const float speed = std::sqrt(s.trajectoryVelocity[0] * s.trajectoryVelocity[0] +
                                      s.trajectoryVelocity[1] * s.trajectoryVelocity[1] +
                                      s.trajectoryVelocity[2] * s.trajectoryVelocity[2]);
        Text(play, s, speed, collisions, retained, error);
    }
    if (debug) {
        GraphicsContext* __gfxCtx = play->state.gfxCtx;
        Gfx* opa = POLY_OPA_DISP;
        Gfx* gfx = Graph_GfxPlusOne(opa);
        gSPDisplayList(OVERLAY_DISP++, gfx);
        GfxPrint p;
        GfxPrint_Init(&p);
        GfxPrint_Open(&p, gfx);
        GfxPrint_SetColor(&p, 255, 255, 255, 255);
        if (std::strcmp(s.state, "BipedGround") == 0 && biped) {
            GfxPrint_SetPos(&p, 1, 1);
            GfxPrint_Printf(&p, "BIPED: %.20s PREV: %.16s", s.state, previous);
            GfxPrint_SetPos(&p, 1, 2);
            GfxPrint_Printf(&p, "LX PHYS/HOST/RUST: %.2f %.2f %.2f", biped[14], biped[14], biped[18]);
            GfxPrint_SetPos(&p, 1, 3);
            GfxPrint_Printf(&p, "CAM YAW:%.2f BODY:%.2f RIGHT:%.2f %.2f", biped[2], biped[3], biped[19], biped[20]);
            GfxPrint_SetPos(&p, 1, 4);
            GfxPrint_Printf(&p, "MOVE YAW: %.2f WORLD: %.2f %.2f", std::atan2(biped[5], biped[6]), biped[5], biped[6]);
            GfxPrint_SetPos(&p, 1, 5);
            GfxPrint_Printf(&p, "LOCAL: %.2f %.2f ACTUAL: %.2f %.2f", biped[7], biped[8], biped[9], biped[10]);
            GfxPrint_SetPos(&p, 1, 6);
            GfxPrint_Printf(&p, "MOVE DOT: %.2f VALID:%u REMAP:%u", moveDot, moveDotValid, biped[11] != 0);
            GfxPrint_SetPos(&p, 1, 7);
            GfxPrint_Printf(&p, "FAKIE:%u MIRRORED:%u FLAGS2476:%.0f", biped[16] != 0, biped[17] != 0, biped[13]);
        }
        if (drawRider) {
            GfxPrint_SetPos(&p, 1, 8);
            GfxPrint_Printf(&p, "RIDER %s NATIVE PROXY: %.3fm",
                            riderScale.mode == 0   ? "NATIVE"
                            : riderScale.mode == 1 ? "ADULT"
                                                   : "CHILD",
                            NativeSkateRiderScale::kHeight / NativeSkateUnits::kOotUnitsPerMeter);
            GfxPrint_SetPos(&p, 1, 9);
            GfxPrint_Printf(&p, "HEIGHT TARGET/FINAL: %.2f / %.2f", riderScale.target, riderScale.finalHeight);
        }
        const auto bs = NativeSkateBoardModel::VisualScale(
            LINK_IS_ADULT, CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScale"), 1.f),
            CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleAdult"), 1.f),
            CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.BoardVisualScaleChild"), .8f));
        GfxPrint_SetPos(&p, 1, 28);
        GfxPrint_Printf(&p, "BOARD %s GLOBAL:%.2f AGE:%.2f FINAL:%.2f", LINK_IS_ADULT ? "ADULT" : "CHILD", bs.global,
                        bs.age, bs.final);
        GfxPrint_SetPos(&p, 1, 29);
        GfxPrint_Printf(&p, "PHYS SCALE:1 UNCHANGED MODEL:%s",
                        CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.DebugBoard"), 0) ? "DEBUG" : "GENERIC");
        GfxPrint_SetPos(&p, 1, 27);
        GfxPrint_Printf(
            &p, "FOOT/HEAD/BOARD TOP: %.2f %.2f %.2f", rider[1] + NativeSkateRiderScale::kFoot / spatialScale,
            rider[1] + NativeSkateRiderScale::PositionY(NativeSkateRiderScale::kHead, riderScale) / spatialScale,
            board[1] + .02f * NativeSkateUnits::kOotUnitsPerMeter / spatialScale);
        gfx = GfxPrint_Close(&p);
        GfxPrint_Destroy(&p);
        gSPEndDisplayList(gfx++);
        Graph_BranchDlist(opa, gfx);
        POLY_OPA_DISP = gfx;
    }
}
void DrawStatus(PlayState* play, const char* status, const char* error) {
    if (!play)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    GfxPrint printer;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, POLY_OPA_DISP);
    GfxPrint_SetColor(&printer, 255, 255, 255, 255);
    GfxPrint_SetPos(&printer, 1, 1);
    GfxPrint_Printf(&printer, "NATIVE SKATE: %s", status ? status : "UNKNOWN");
    if (error && error[0]) {
        GfxPrint_SetColor(&printer, 255, 80, 80, 255);
        GfxPrint_SetPos(&printer, 1, 2);
        GfxPrint_Printf(&printer, "ERROR: %.80s", error);
    }
    POLY_OPA_DISP = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
}
void DrawPushStatus(PlayState* play, uint64_t presses, uint64_t events, uint64_t applications, float delta, float dt,
                    double ticks, double samples, double hostFPS, bool xDown, const char* stop) {
    if (!play)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* polyOpa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(polyOpa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint printer;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    GfxPrint_SetColor(&printer, 255, 255, 255, 255);
    GfxPrint_SetPos(&printer, 1, 22);
    GfxPrint_Printf(&printer, "X DOWN: %u PUSH PRESSES: %u", xDown ? 1 : 0, (unsigned)presses);
    GfxPrint_SetPos(&printer, 1, 23);
    GfxPrint_Printf(&printer, "RUST PUSH EVENTS: %u APPLICATIONS: %u", (unsigned)events, (unsigned)applications);
    GfxPrint_SetPos(&printer, 1, 24);
    GfxPrint_Printf(&printer, "LAST PUSH DELTA: %.3f DT: %.6f", delta, dt);
    GfxPrint_SetPos(&printer, 1, 25);
    GfxPrint_Printf(&printer, "RUST TPS: %.1f SAMPLES/SEC: %.1f", ticks, samples);
    GfxPrint_SetPos(&printer, 1, 26);
    GfxPrint_Printf(&printer, "HOST UPDATE/SEC: %.1f STOP: %s", hostFPS, stop);
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(polyOpa, gfx);
    POLY_OPA_DISP = gfx;
}
void DrawModuleStatus(PlayState* play, uint64_t updateTicks, uint64_t drawTicks, bool requested, bool enabled,
                      bool f8Down, bool f8Pressed, bool ownership, const char* status, const char* error) {
    if (!play)
        return;
    // Match ValueViewer_SetupDraw: store the text list in opaque-buffer space,
    // branch past it there, and execute it through the HUD overlay list.
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* polyOpa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(polyOpa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint printer;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    GfxPrint_SetColor(&printer, 255, 255, 255, 255);
    GfxPrint_SetPos(&printer, 1, 10);
    GfxPrint_Printf(&printer, "NATIVE SKATE CLEAN");
    GfxPrint_SetPos(&printer, 1, 11);
    GfxPrint_Printf(&printer, "NATIVE SKATE MODULE: REGISTERED");
    GfxPrint_SetPos(&printer, 1, 12);
    GfxPrint_Printf(&printer, "UPDATE TICKS: %u  DRAW TICKS: %u", (unsigned)updateTicks, (unsigned)drawTicks);
    GfxPrint_SetPos(&printer, 1, 13);
    GfxPrint_Printf(&printer, "REQUESTED: %s  NATIVE SKATE ENABLED: %u", requested ? "ON" : "OFF", enabled ? 1 : 0);
    GfxPrint_SetPos(&printer, 1, 14);
    GfxPrint_Printf(&printer, "RUNTIME: %s", status && std::strcmp(status, "DISABLED") != 0 ? status : "NOT STARTED");
    GfxPrint_SetPos(&printer, 1, 15);
    GfxPrint_Printf(&printer, "F8 DOWN: %u  F8 PRESSED: %u", f8Down ? 1 : 0, f8Pressed ? 1 : 0);
    GfxPrint_SetPos(&printer, 1, 16);
    GfxPrint_Printf(&printer, "OWNERSHIP: %s  PROXY RIDER: %s", ownership ? "RUST" : "OOT", ownership ? "ON" : "OFF");
    GfxPrint_SetPos(&printer, 1, 17);
    GfxPrint_Printf(&printer, "PROXY BOARD: %s  COLLISION DEBUG: OFF", ownership ? "ON" : "OFF");
    GfxPrint_SetPos(&printer, 1, 18);
    GfxPrint_Printf(&printer, "ORANGE PRIMITIVE SOURCE: DEBUG_BOARD");
    GfxPrint_SetPos(&printer, 1, 19);
    GfxPrint_Printf(&printer, "BOARD SIZE METERS L/W/T: %.2f %.2f %.2f", NativeSkateProxyGeometry::kDeckLength.value,
                    NativeSkateProxyGeometry::kDeckWidth.value, NativeSkateProxyGeometry::kDeckThickness.value);
    GfxPrint_SetPos(&printer, 1, 20);
    GfxPrint_Printf(&printer, "BOARD SIZE HOST L/W/T: %.2f %.2f %.2f", NativeSkateProxyGeometry::kDeckLengthHost.value,
                    NativeSkateProxyGeometry::kDeckWidthHost.value, NativeSkateProxyGeometry::kDeckThicknessHost.value);
    if (error && error[0]) {
        GfxPrint_SetColor(&printer, 255, 80, 80, 255);
        GfxPrint_SetPos(&printer, 1, 21);
        GfxPrint_Printf(&printer, "ERROR: %.60s", error);
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(polyOpa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateProxy
