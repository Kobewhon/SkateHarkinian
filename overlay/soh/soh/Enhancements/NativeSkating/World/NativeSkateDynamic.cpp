#include "NativeSkateDynamic.h"
#include "../NativeSkateObjectDropper.h"
#include "../NativeSkateUnits.h"
#include "../NativeSkatePerf.h"
#include <cmath>
#include <chrono>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
extern "C" {
#include "global.h"
}
namespace NativeSkateDynamic {
static std::array<float, 16> Frame(const Actor& a) {
    MtxF m;
    SkinMatrix_SetTranslateRotateYXZScale(&m, a.scale.x, a.scale.y, a.scale.z, a.shape.rot.x, a.shape.rot.y,
                                          a.shape.rot.z, a.world.pos.x, a.world.pos.y, a.world.pos.z);
    std::array<float, 16> out{};
    const Vec3f points[4] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 0 } };
    Vec3f transformed[4];
    for (int i = 0; i < 4; ++i)
        SkinMatrix_Vec3fMtxFMultXYZ(&m, (Vec3f*)&points[i], &transformed[i]);
    for (int i = 0; i < 3; ++i) {
        out[i * 4] = transformed[i].x - transformed[3].x;
        out[i * 4 + 1] = transformed[i].y - transformed[3].y;
        out[i * 4 + 2] = transformed[i].z - transformed[3].z;
    }
    out[12] = transformed[3].x / NativeSkateUnits::kOotUnitsPerMeter;
    out[13] = transformed[3].y / NativeSkateUnits::kOotUnitsPerMeter;
    out[14] = transformed[3].z / NativeSkateUnits::kOotUnitsPerMeter;
    out[15] = 1;
    return out;
}
bool World::Update(const PlayState* play, NativeSkateRuntime::Session& runtime) {
    auto begin = std::chrono::steady_clock::now();
    std::array<bool, BG_ACTOR_MAX> alive{};
    updates = triangles = 0;
    for (uint32_t slot = 0; slot < BG_ACTOR_MAX; ++slot) {
        auto discover = std::chrono::steady_clock::now();
        const auto& d = play->colCtx.dyna;
        unsigned flags = d.bgActorFlags[slot];
        const auto& bg = d.bgActors[slot];
        if (!(flags & 1) || (flags & (2 | 4)) || !bg.actor || !bg.colHeader)
            continue;
        uint32_t id = slot + 1;
        auto& mesh = meshes[id];
        const bool fresh = mesh.actor != bg.actor || mesh.header != bg.colHeader;
        auto frame = Frame(*bg.actor);
        bool finite = true;
        for (float f : frame)
            finite &= std::isfinite(f);
        if (!finite)
            continue;
        NativeSkatePerf::metrics[NativeSkatePerf::DynaDiscovery].Add(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - discover).count());
        if (fresh) {
            NativeSkatePerf::Scope timing(NativeSkatePerf::DynaMesh);
            mesh = {};
            mesh.actor = bg.actor;
            mesh.header = bg.colHeader;
            const auto& h = *bg.colHeader;
            if (!h.polyList || !h.vtxList)
                continue;
            uint64_t hash = 1469598103934665603ull;
            auto mix = [&](uint64_t value) {
                for (int b = 0; b < 8; ++b) {
                    hash ^= (value >> (b * 8)) & 255;
                    hash *= 1099511628211ull;
                }
            };
            mix(h.numVertices);
            mix(h.numPolygons);
            for (unsigned i = 0; i < h.numVertices; ++i) {
                auto v = h.vtxList[i];
                mix(v.x);
                mix(v.y);
                mix(v.z);
            }
            for (unsigned i = 0; i < h.numPolygons; ++i) {
                const auto& poly = h.polyList[i];
                for (auto v : poly.vtxData)
                    mix(v);
                mix(poly.normal.x);
                mix(poly.normal.y);
                mix(poly.normal.z);
            }
            auto resourceKey = std::make_pair((const void*)bg.colHeader, hash);
            const auto* cached = resources.Find(resourceKey);
            if (cached) {
                mesh.local = cached->local;
                mesh.grinds = cached->grinds;
            } else {

                for (unsigned i = 0; i < h.numPolygons; ++i) {
                    const auto& poly = h.polyList[i];
                    if (poly.flags_vIA & 0x4000)
                        continue;
                    float v[3][3];
                    bool valid = true;
                    for (int j = 0; j < 3; ++j) {
                        unsigned index = COLPOLY_VTX_INDEX(poly.vtxData[j]);
                        if (index >= h.numVertices) {
                            valid = false;
                            break;
                        }
                        auto x = h.vtxList[index];
                        v[j][0] = x.x / NativeSkateUnits::kOotUnitsPerMeter;
                        v[j][1] = x.y / NativeSkateUnits::kOotUnitsPerMeter;
                        v[j][2] = x.z / NativeSkateUnits::kOotUnitsPerMeter;
                    }
                    if (!valid)
                        continue;
                    float ax = v[1][0] - v[0][0], ay = v[1][1] - v[0][1], az = v[1][2] - v[0][2],
                          bx = v[2][0] - v[0][0], by = v[2][1] - v[0][1], bz = v[2][2] - v[0][2];
                    float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
                    if (nx * nx + ny * ny + nz * nz < 1e-12f)
                        continue;
                    if (nx * poly.normal.x + ny * poly.normal.y + nz * poly.normal.z < 0)
                        for (int k = 0; k < 3; ++k)
                            std::swap(v[1][k], v[2][k]);
                    for (auto& point : v)
                        for (float f : point)
                            mesh.local.push_back(f);
                }
                {
                    NativeSkatePerf::Scope compileTiming(NativeSkatePerf::DynaCompile);
                    mesh.grinds = NativeSkateGrindCompiler::Compile(mesh.local);
                }
                size_t bytes = sizeof(Mesh) + mesh.local.capacity() * 4 +
                               mesh.grinds.edges.capacity() * sizeof(NativeSkateGrindCompiler::Edge) +
                               mesh.grinds.splines.capacity() * sizeof(NativeSkateGrindCompiler::Spline);
                for (const auto& line : mesh.grinds.splines)
                    bytes += line.points.capacity() * sizeof(NativeSkateGrindCompiler::Point);
                resources.Put(resourceKey, mesh, bytes);
            }
        }
        // Explicit object providers are authoritative, independent of cached collision
        // classification. Fresh instance/dirty notifications regenerate them once.
        if (fresh) {
            std::vector<std::vector<NativeSkateGrindCompiler::Point>> paths;
            if (NativeSkateObjectDropper::LocalGrinds(bg.actor, paths)) {
                mesh.objectId = NativeSkateObjectDropper::GrindOwner(bg.actor);
                mesh.grinds = {};
                for (auto& path : paths)
                    if (path.size() > 1) {
                        NativeSkateGrindCompiler::Spline line;
                        line.id = id;
                        line.category = NativeSkateGrindCompiler::Category::Rail;
                        line.points = std::move(path);
                        mesh.grinds.splines.push_back(std::move(line));
                    }
            }
        }

        triangles += (unsigned)(mesh.local.size() / 9);
        if (mesh.local.empty())
            continue;
        alive[slot] = true;
        const bool changed = fresh || frame != mesh.frame;
        if (changed) {
            NativeSkatePerf::Scope timing(NativeSkatePerf::DynaTransform);
            if (!runtime.Dynamic(id, fresh ? &mesh.local : nullptr, frame.data(), true,
                                 NativeSkateObjectDropper::RidingSurface(bg.actor)))
                return false;
            ++updates;
        }
        bool enabled = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.Grinds.Enabled"), 1) != 0;
        if (fresh || changed || enabled != mesh.grindsEnabled) {
            NativeSkatePerf::Scope timing(NativeSkatePerf::DynaGrindTransform);
            std::vector<float> points;
            std::vector<uint32_t> offsets{ 0 };
            bool registerMesh = fresh || (enabled != mesh.grindsEnabled);
            if (registerMesh && enabled)
                for (const auto& line : mesh.grinds.splines) {
                    for (const auto& p : line.points)
                        for (float f : p)
                            points.push_back(f);
                    offsets.push_back((uint32_t)points.size() / 3);
                }
            if (!runtime.DynamicGrinds(id, registerMesh ? &points : nullptr, registerMesh ? &offsets : nullptr,
                                       frame.data(), true))
                return false;
            mesh.grindsEnabled = enabled;
            if (mesh.objectId && registerMesh)
                SPDLOG_INFO("[DYNAMIC_GRIND] UPLOAD object={} provider={} splines={} enabled={}", mesh.objectId, id,
                            mesh.grinds.splines.size(), enabled);
            else if (mesh.objectId && changed)
                SPDLOG_DEBUG("[DYNAMIC_GRIND] TRANSFORM object={} provider={}", mesh.objectId, id);
        }
        mesh.frame = frame;
    }
    const std::array<float, 16> identity = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    for (auto i = meshes.begin(); i != meshes.end();)
        if (!alive[i->first - 1]) {
            if (!runtime.Dynamic(i->first, nullptr, identity.data(), false) ||
                !runtime.DynamicGrinds(i->first, nullptr, nullptr, identity.data(), false))
                return false;
            if (i->second.objectId)
                SPDLOG_INFO("[DYNAMIC_GRIND] REMOVE object={} provider={}", i->second.objectId, i->first);
            i = meshes.erase(i);
            ++updates;
        } else
            ++i;
    updateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    return true;
}
void World::Draw(PlayState* play, NativeSkateRuntime::Session& runtime) const {
    const bool debugMesh = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.World.DynamicDebug"), 0) != 0;
    const bool debugGrinds = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.GrindDebug"), 0) != 0;
    if (!play || (!debugMesh && !debugGrinds))
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Matrix_Push();
    Matrix_Translate(0, 0, 0, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 160, 60, 255, 90);
    if (debugMesh)
        for (const auto& entry : meshes) {
            const auto& m = entry.second;
            for (size_t i = 0; i < m.local.size(); i += 9) {
                Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Vtx));
                if (!v)
                    break;
                for (int j = 0; j < 3; ++j) {
                    v[j] = {};
                    for (int k = 0; k < 3; ++k) {
                        float x = m.frame[12 + k];
                        for (int axis = 0; axis < 3; ++axis)
                            x += m.frame[axis * 4 + k] * m.local[i + j * 3 + axis];
                        v[j].v.ob[k] = (s16)std::lrint(x * NativeSkateUnits::kOotUnitsPerMeter);
                    }
                }
                gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 3, 0);
                gSP1Triangle(POLY_XLU_DISP++, 0, 1, 2, 0);
            }
        }
    // Origins and face normals use the same transformed triangle frame as native collision.
    auto line = [&](std::array<float, 3> a, std::array<float, 3> b) {
        Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
        if (!v)
            return;
        for (int j = 0; j < 4; ++j) {
            v[j] = {};
            for (int k = 0; k < 3; ++k)
                v[j].v.ob[k] = (s16)std::lrint((j < 2 ? a[k] : b[k]) + ((k == 0) ? (j % 2 ? -.5f : .5f) : 0));
        }
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    };
    if (debugGrinds) {
        auto grindLine = [&](std::array<float, 3> a, std::array<float, 3> b) {
            float dx = b[0] - a[0], dz = b[2] - a[2], l = std::hypot(dx, dz);
            float rx = l > 1e-5f ? dz / l : 1.f, rz = l > 1e-5f ? -dx / l : 0.f;
            Vtx* v = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
            if (!v)
                return;
            for (int j = 0; j < 4; ++j) {
                auto p = j < 2 ? a : b;
                float side = j % 2 ? -.6f : .6f;
                v[j] = {};
                v[j].v.ob[0] = (s16)std::lrint(p[0] + rx * side);
                v[j].v.ob[1] = (s16)std::lrint(p[1] + .5f);
                v[j].v.ob[2] = (s16)std::lrint(p[2] + rz * side);
            }
            gSPVertex(POLY_XLU_DISP++, (uintptr_t)v, 4, 0);
            gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
        };
        for (const auto& entry : meshes) {
            const auto& m = entry.second;
            if (!m.objectId)
                continue;
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, m.grindsEnabled ? 30 : 255, m.grindsEnabled ? 255 : 30,
                            m.grindsEnabled ? 220 : 30, 235);
            for (const auto& path : m.grinds.splines)
                for (size_t i = 1; i < path.points.size(); ++i) {
                    std::array<float, 3> p[2]{};
                    for (int j = 0; j < 2; ++j)
                        for (int k = 0; k < 3; ++k) {
                            p[j][k] = m.frame[12 + k];
                            for (int a = 0; a < 3; ++a)
                                p[j][k] += m.frame[a * 4 + k] * path.points[i - 1 + j][a];
                            p[j][k] *= NativeSkateUnits::kOotUnitsPerMeter;
                        }
                    grindLine(p[0], p[1]);
                }
        }
    }
    if (!debugMesh) {
        Matrix_Pop();
        return;
    }
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 220, 80, 230);
    for (const auto& entry : meshes) {
        const auto& m = entry.second;
        std::array<float, 3> origin{}, tip{};
        for (int k = 0; k < 3; ++k)
            origin[k] = tip[k] = m.frame[12 + k] * NativeSkateUnits::kOotUnitsPerMeter;
        tip[1] += 12;
        line(origin, tip);
        for (size_t i = 0; i < m.local.size(); i += 9) {
            std::array<float, 3> p[3]{};
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k) {
                    p[j][k] = m.frame[12 + k];
                    for (int a = 0; a < 3; ++a)
                        p[j][k] += m.frame[a * 4 + k] * m.local[i + j * 3 + a];
                }
            auto n = NativeSkateGrindCompiler::Normal(NativeSkateGrindCompiler::Cross(
                NativeSkateGrindCompiler::Sub(p[1], p[0]), NativeSkateGrindCompiler::Sub(p[2], p[0])));
            std::array<float, 3> c{}, t{};
            for (int k = 0; k < 3; ++k) {
                c[k] = (p[0][k] + p[1][k] + p[2][k]) / 3 * NativeSkateUnits::kOotUnitsPerMeter;
                t[k] = c[k] + n[k] * 8;
            }
            line(c, t);
        }
    }
    std::vector<std::array<float, 7>> contacts;
    if (runtime.Contacts(contacts)) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        for (const auto& c : contacts) {
            std::array<float, 3> a{}, b{};
            for (int k = 0; k < 3; ++k) {
                a[k] = c[k] * NativeSkateUnits::kOotUnitsPerMeter;
                b[k] = a[k] + c[k + 3] * 10;
            }
            line(a, b);
        }
    }
    Matrix_Pop();
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    GfxPrint_SetColor(&p, 220, 160, 255, 255);
    GfxPrint_SetPos(&p, 1, 21);
    GfxPrint_Printf(&p, "DYNA %u TRI %u UPDATES %u %.2fms", (unsigned)meshes.size(), triangles, updates, updateMs);
    GfxPrint_SetPos(&p, 1, 22);
    GfxPrint_Printf(&p, "IDs:");
    for (const auto& e : meshes)
        GfxPrint_Printf(&p, " %u", e.first);
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateDynamic
