#include "NativeSkateWorldDebug.h"
#include "../NativeSkateProxyGeometry.h"
#include <cmath>
#include <algorithm>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
extern "C" {
#include "global.h"
}
namespace NativeSkateWorldDebug {
Contact Measure(const NativeSkateGeometry::Result& r, const NativeSkateRuntime::Snapshot& s,
                const NativeSkateUnits::Anchor& a, const float* d) {
    Contact c;
    float board[3];
    if (!a.Position(s.board + 12, board))
        return c;
    NativeSkateGeometry::Point p{ board[0] / NativeSkateUnits::kOotUnitsPerMeter,
                                  board[1] / NativeSkateUnits::kOotUnitsPerMeter,
                                  board[2] / NativeSkateUnits::kOotUnitsPerMeter };
    float floor = 0;
    c.floorValid = NativeSkateGeometry::Floor(r.triangles, p, floor, c.normal);
    c.ground = c.floorValid ? floor * NativeSkateUnits::kOotUnitsPerMeter : 0;
    float rotation[16];
    if (!NativeSkateProxyGeometry::BoardRotation(s.board, rotation)) {
        c.floorValid = false;
        return c;
    }
    const auto shape = NativeSkateProxyGeometry::NativeShape(d);
    float units = a.unitsPerMeter;
    c.deckBottom =
        board[1] - units * (std::abs(rotation[1]) * .1f + std::abs(rotation[5]) * .02f + std::abs(rotation[9]) * .4f);
    c.visualBottom = c.deckBottom;
    for (int side : { -1, 1 })
        for (float z : { shape.frontZ, shape.backZ }) {
            float y =
                board[1] + units * (rotation[1] * shape.wheelX * side + rotation[5] * shape.wheelY + rotation[9] * z);
            float bottom = y - units * (std::abs(rotation[1]) * .0125f +
                                        (std::abs(rotation[5]) + std::abs(rotation[9])) * shape.radius);
            c.visualBottom = std::min(c.visualBottom, bottom);
        }
    c.clearance = c.floorValid ? c.visualBottom - c.ground : 0;
    return c;
}
void Draw(PlayState* play, const NativeSkateGeometry::Result& r, const NativeSkateWorld::Snapshot& source,
          const NativeSkateSceneProfile::Profile& profile, const NativeSkateRuntime::Snapshot& s,
          const NativeSkateUnits::Anchor& a, float scale, const float* d, bool riderDebug) {
    if (!play)
        return;
    bool world = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.World.Debug"), 0) != 0;
    bool debug = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Debug"), 0) != 0;
    bool custom = std::any_of(r.triangles.begin(), r.triangles.end(), [](const auto& t) { return t.visible; });
    if (!world && !debug && !custom)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    if (world || custom) {
        Matrix_Push();
        Matrix_Translate(0, 0, 0, MTXMODE_NEW);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPPipeSync(POLY_XLU_DISP++);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
        for (const auto& t : r.triangles) {
            if (!world && !t.visible)
                continue;
            Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * 3);
            if (!v)
                break;
            int i = 0;
            for (auto p : { t.a, t.b, t.c }) {
                v[i] = {};
                for (int k = 0; k < 3; ++k)
                    v[i].v.ob[k] = (s16)std::lrint(p[k] * NativeSkateUnits::kOotUnitsPerMeter);
                ++i;
            }
            const unsigned colors[][3] = { { 30, 180, 255 }, { 30, 220, 80 }, { 255, 130, 20 },  { 240, 60, 210 },
                                           { 180, 130, 60 }, { 240, 50, 50 }, { 100, 100, 100 }, { 255, 255, 0 } };
            auto color = colors[(int)t.surface];
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, color[0], color[1], color[2],
                            t.visible     ? 210
                            : t.generated ? 150
                                          : 45);
            gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 3, 0);
            gSP1Triangle(POLY_XLU_DISP++, 0, 1, 2, 0);
        }
        Matrix_Pop();
    }
    if (!world && !debug)
        return;
    auto c = Measure(r, s, a, d);
    float board[3], com[3];
    a.Position(s.board + 12, board);
    a.Position(s.trajectory, com);
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 255, 255, 255, 255);
    if (!riderDebug) {
        GfxPrint_SetPos(&p, 1, 10);
        GfxPrint_Printf(&p, "RUST WORLD OWNERSHIP / CONTACT");
        GfxPrint_SetPos(&p, 1, 11);
        GfxPrint_Printf(&p, "TRIANGLE GROUND Y: %.3f VALID: %u", c.ground, c.floorValid);
        GfxPrint_SetPos(&p, 1, 12);
        GfxPrint_Printf(&p, "RUST DECK Y: %.4f COM Y: %.4f", s.board[13], s.trajectory[1]);
        GfxPrint_SetPos(&p, 1, 13);
        GfxPrint_Printf(&p, "HOST DECK CENTER/BOTTOM: %.2f %.2f", board[1], c.deckBottom);
        GfxPrint_SetPos(&p, 1, 14);
        GfxPrint_Printf(&p, "VISUAL BOTTOM: %.2f CLEARANCE: %.2f", c.visualBottom, c.clearance);
        GfxPrint_SetPos(&p, 1, 15);
        GfxPrint_Printf(&p, "PENETRATION: %.2f CONTACTS: %.0f", c.floorValid ? std::max(0.f, -c.clearance) : 0,
                        d ? d[9] : 0);
        GfxPrint_SetPos(&p, 1, 16);
        GfxPrint_Printf(&p, "CONTACT NORMAL: %.3f %.3f %.3f", s.groundNormal[0], s.groundNormal[1], s.groundNormal[2]);
    }
    if (!riderDebug) {
        GfxPrint_SetPos(&p, 1, 17);
        GfxPrint_Printf(&p, "UNITS/M: %.3f WHEEL R: %.4fm", a.unitsPerMeter, d ? d[19] : 0);
        GfxPrint_SetPos(&p, 1, 18);
        GfxPrint_Printf(&p, "SCENE: %d NATIVE SPACE: x%.2f", profile.sceneId, scale);
        GfxPrint_SetPos(&p, 1, 19);
        GfxPrint_Printf(&p, "SOURCE/SKATE TRIANGLES: %u %u", source.accepted, (unsigned)r.triangles.size());
        GfxPrint_SetPos(&p, 1, 20);
        GfxPrint_Printf(&p, "STAIRS/EDGES/RAMPS/BANKS: %u %u %u %u", r.stairs, r.edges, r.ramps, r.banks);
        GfxPrint_SetPos(&p, 1, 21);
        GfxPrint_Printf(&p, "REJECTED: %u SCENE GEOMETRY SCALE: 1.0", source.rejected + r.rejected);
    }
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateWorldDebug
