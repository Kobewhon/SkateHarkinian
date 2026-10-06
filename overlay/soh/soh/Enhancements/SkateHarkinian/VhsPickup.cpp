#include "VhsPickup.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Vertex.h>
#include <fast/resource/type/Texture.h>
#include <ship/resource/archive/Archive.h>
#include <spdlog/spdlog.h>
#include <array>
#include <libultraship/bridge/consolevariablebridge.h>
#include <cmath>
extern "C" {
#include "global.h"
}

namespace {
// Same registered resources can be referenced by future presentation consumers.
// Geometric-center origin: +X right, +Y cassette top, +Z labelled face.
// Width 1200 model units * .01 normal pickup scale = 12 OoT world units.
alignas(2) const char kDisplayList[] = "__OTR__SkateHarkinian/VHS/Mesh/DisplayList";
bool ResourcesReady() {
    static bool attempted = false;
    static std::array<std::shared_ptr<Ship::IResource>, 4> resources;
    if (!attempted) {
        attempted = true;
        auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
        resources[0] = manager->LoadResourceProcess("SkateHarkinian/VHS/Mesh/DisplayList", true);
        resources[1] = manager->LoadResourceProcess("SkateHarkinian/VHS/Mesh/Vertices", true);
        resources[2] = manager->LoadResourceProcess("SkateHarkinian/VHS/Textures/CassetteFace", true);
        resources[3] = manager->LoadResourceProcess("SkateHarkinian/VHS/Textures/CassetteShell", true);
        for (int i = 2; i < 4; ++i) {
            const char* path =
                i == 2 ? "SkateHarkinian/VHS/Textures/CassetteFace" : "SkateHarkinian/VHS/Textures/CassetteShell";
            const int expectedWidth = i == 2 ? 256 : 64;
            const int expectedHeight = i == 2 ? 128 : 64;
            auto texture = std::dynamic_pointer_cast<Fast::Texture>(resources[i]);
            if (!texture || texture->Type != Fast::TextureType::RGBA32bpp || texture->Width != expectedWidth ||
                texture->Height != expectedHeight || texture->Flags != TEX_FLAG_LOAD_AS_RAW || !texture->ImageData) {
                SPDLOG_ERROR("VHS texture invalid/unresolved: {} (expected RAW RGBA32 {}x{})", path, expectedWidth,
                             expectedHeight);
                resources[i] = nullptr;
            } else {
                auto parent = texture->GetInitData()->Parent;
                SPDLOG_INFO("VHS texture resolved: {} — RAW RGBA32 {}x{} — archive {}", path, expectedWidth,
                            expectedHeight, parent ? parent->GetPath() : "no archive metadata");
            }
        }
        if (!std::dynamic_pointer_cast<Fast::DisplayList>(resources[0]) ||
            !std::dynamic_pointer_cast<Fast::Vertex>(resources[1]) ||
            !std::dynamic_pointer_cast<Fast::Texture>(resources[2]) ||
            !std::dynamic_pointer_cast<Fast::Texture>(resources[3])) {
            resources.fill(nullptr);
            SPDLOG_ERROR("VHS pickup archive unavailable/invalid; keeping stock recovery-heart drawing");
        }
    }
    return resources[0] != nullptr;
}
} // namespace

extern "C" int SkateHarkinian_DrawVhsRecoveryHeart(PlayState* play, Actor* actor) {
    if (!CVarGetInteger("gEnhancements.NativeSkate.VHSWorldPickups", 1))
        return 0;
    if (!ResourcesReady())
        return 0;
    // Preserve the actor's shape offset and scale animation. All additional motion
    // stays inside the draw matrix; no actor, collision, item or health fields change.
    const float phase = (play->gameplayFrames + static_cast<unsigned short>(actor->home.rot.z)) * .012f;
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x,
                     actor->world.pos.y + actor->shape.yOffset * actor->scale.y + .75f * std::sin(phase),
                     actor->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(phase, MTXMODE_APPLY);
    Matrix_RotateX(.12f, MTXMODE_APPLY);
    Matrix_Scale(actor->scale.x * .5f, actor->scale.y * .5f, actor->scale.z * .5f, MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)kDisplayList);
    // Restore the ordinary OoT opaque baseline after this self-contained material DL.
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
    Matrix_Pop();
    return 1;
}

// Shared asset entry points: placement uses the existing immutable resource cache.
// Caller supplies the world transform; pickup gameplay/animation stays unchanged.
extern "C" int SkateHarkinian_VhsObjectReady() {
    return ResourcesReady();
}
extern "C" void SkateHarkinian_DrawVhsObject(PlayState* play) {
    if (!play || !ResourcesReady())
        return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)kDisplayList);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}
