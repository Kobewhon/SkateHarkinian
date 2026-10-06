#include "NativeSkateGrindDebug.h"
#include "../NativeSkateUnits.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
extern "C" {
#include "global.h"
}
namespace NativeSkateGrindDebug {
void Draw(PlayState* play, const NativeSkateGrindCompiler::Result& r, bool ready) {
    int debug = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Grinds.Debug"), 0);
    if (!play || !debug)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Matrix_Push();
    Matrix_Translate(0, 0, 0, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    for (const auto& e : r.edges) {
        if (e.category == NativeSkateGrindCompiler::Category::Reject && debug < 2)
            continue;
        unsigned char red = 30, green = 230, blue = 70;
        if (e.category == NativeSkateGrindCompiler::Category::Rail) {
            red = 20;
            green = 100;
            blue = 255;
        } else if (e.category == NativeSkateGrindCompiler::Category::Curb ||
                   e.category == NativeSkateGrindCompiler::Category::Coping) {
            red = 20;
            green = 230;
            blue = 230;
        } else if (e.category == NativeSkateGrindCompiler::Category::Reject) {
            red = 240;
            green = 30;
            blue = 30;
        } else if (e.category == NativeSkateGrindCompiler::Category::RoofEdge) {
            red = 250;
            green = 230;
            blue = 20;
        }
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, red, green, blue, 220);
        auto dir = NativeSkateGrindCompiler::Normal(NativeSkateGrindCompiler::Sub(e.b, e.a));
        auto side = NativeSkateGrindCompiler::Normal(NativeSkateGrindCompiler::Cross(dir, { 0, 1, 0 }));
        Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
        if (!v)
            break;
        for (int j = 0; j < 4; ++j) {
            v[j] = {};
            const auto& point = j < 2 ? e.a : e.b;
            float sign = j % 2 ? -1.f : 1.f;
            for (int k = 0; k < 3; ++k)
                v[j].v.ob[k] = (s16)std::lrint((point[k] + side[k] * sign * .012f + (k == 1 ? .01f : 0)) *
                                               NativeSkateUnits::kOotUnitsPerMeter);
        }
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    }
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 230);
    for (const auto& s : r.splines) {
        if (s.points.size() < 2)
            continue;
        for (const auto& point : { s.points.front(), s.points.back() }) {
            Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
            if (!v)
                break;
            const float d[4][3] = { { -.035f, 0, 0 }, { 0, 0, .035f }, { .035f, 0, 0 }, { 0, 0, -.035f } };
            for (int j = 0; j < 4; ++j) {
                v[j] = {};
                for (int k = 0; k < 3; ++k)
                    v[j].v.ob[k] = (s16)std::lrint((point[k] + d[j][k] + (k == 1 ? .015f : 0)) *
                                                   NativeSkateUnits::kOotUnitsPerMeter);
            }
            gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 4, 0);
            gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
        }
        auto start = s.points.front(),
             dir = NativeSkateGrindCompiler::Normal(NativeSkateGrindCompiler::Sub(s.points[1], start));
        auto side = NativeSkateGrindCompiler::Normal(NativeSkateGrindCompiler::Cross(dir, { 0, 1, 0 }));
        Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Vtx));
        if (!v)
            break;
        for (int j = 0; j < 3; ++j) {
            v[j] = {};
            for (int k = 0; k < 3; ++k) {
                float x = start[k] + dir[k] * (j == 0 ? .2f : .12f) +
                          (j == 1   ? .035f
                           : j == 2 ? -.035f
                                    : 0) *
                              side[k] +
                          (k == 1 ? .02f : 0);
                v[j].v.ob[k] = (s16)std::lrint(x * NativeSkateUnits::kOotUnitsPerMeter);
            }
        }
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 3, 0);
        gSP1Triangle(POLY_XLU_DISP++, 0, 1, 2, 0);
    }
    Matrix_Pop();
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 240, 255, 240, 255);
    GfxPrint_SetPos(&p, 1, 23);
    GfxPrint_Printf(&p, "GRIND SURFACES: %s SCENE %d", ready ? "READY" : "COMPILING", play->sceneNum);
    GfxPrint_SetPos(&p, 1, 24);
    GfxPrint_Printf(&p, "TRI %u EDGES %u SEAMS %u", r.triangles, (unsigned)r.edges.size(), r.seams);
    GfxPrint_SetPos(&p, 1, 25);
    GfxPrint_Printf(&p, "CAND %u SPLINES %u LENGTH %.1fm", r.candidates, (unsigned)r.splines.size(), r.length);
    GfxPrint_SetPos(&p, 1, 26);
    GfxPrint_Printf(&p, "PREP ms %.2f %.2f %.2f", r.extractionMs, r.classificationMs, r.weldMs);
    uint64_t nearest = 0;
    float distance = 1e30f;
    auto* player = GET_PLAYER(play);
    if (player)
        for (const auto& line : r.splines)
            for (const auto& point : line.points) {
                float dx = point[0] - player->actor.world.pos.x / NativeSkateUnits::kOotUnitsPerMeter,
                      dy = point[1] - player->actor.world.pos.y / NativeSkateUnits::kOotUnitsPerMeter,
                      dz = point[2] - player->actor.world.pos.z / NativeSkateUnits::kOotUnitsPerMeter;
                float d = dx * dx + dy * dy + dz * dz;
                if (d < distance) {
                    distance = d;
                    nearest = line.id;
                }
            }
    GfxPrint_SetPos(&p, 1, 27);
    GfxPrint_Printf(&p, "NEAREST %016llx", (unsigned long long)nearest);
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateGrindDebug
