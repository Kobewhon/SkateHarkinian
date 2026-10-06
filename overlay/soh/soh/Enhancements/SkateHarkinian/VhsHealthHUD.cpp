#include "VhsHealthHUD.h"
#include <libultraship/libultra/gbi.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>
#include <fast/resource/type/Vertex.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <array>

namespace {
alignas(2) const char textures[2][5][64] = {
    { "__OTR__SkateHarkinian/VHS/HUD/Normal/Empty", "__OTR__SkateHarkinian/VHS/HUD/Normal/Quarter",
      "__OTR__SkateHarkinian/VHS/HUD/Normal/Half", "__OTR__SkateHarkinian/VHS/HUD/Normal/ThreeQuarter",
      "__OTR__SkateHarkinian/VHS/HUD/Normal/Full" },
    { "__OTR__SkateHarkinian/VHS/HUD/Defense/Empty", "__OTR__SkateHarkinian/VHS/HUD/Defense/Quarter",
      "__OTR__SkateHarkinian/VHS/HUD/Defense/Half", "__OTR__SkateHarkinian/VHS/HUD/Defense/ThreeQuarter",
      "__OTR__SkateHarkinian/VHS/HUD/Defense/Full" }
};
std::array<std::shared_ptr<Ship::IResource>, 11> resources;
bool attempted = false, ready = false;
} // namespace
extern "C" int SkateHarkinian_VhsHealthHUDReady() {
    if (!CVarGetInteger("gEnhancements.NativeSkate.VHSHealthHUD", 1))
        return 0;
    if (!attempted) {
        attempted = true;
        ready = true;
        auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
        for (int d = 0; d < 2; ++d)
            for (int s = 0; s < 5; ++s) {
                const char* path = textures[d][s] + 7;
                auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(path, true));
                resources[d * 5 + s] = texture;
                if (!texture || texture->Width != 32 || texture->Height != 32 ||
                    texture->Type != Fast::TextureType::RGBA32bpp || !texture->ImageData) {
                    SPDLOG_ERROR("VHS health HUD resource invalid: {} (using stock meter)", path);
                    ready = false;
                }
            }
        resources[10] = manager->LoadResourceProcess("SkateHarkinian/VHS/HUD/Vertices", true);
        auto vertices = std::dynamic_pointer_cast<Fast::Vertex>(resources[10]);
        ready = ready && vertices && vertices->VertexList.size() == 4;
    }
    return ready;
}
extern "C" const void* SkateHarkinian_VhsHealthTexture(int index, int fullCount, int fraction, int defense) {
    return textures[defense ? 1 : 0][SkateHarkinian_VhsHealthState(index, fullCount, fraction)];
}
extern "C" void* SkateHarkinian_VhsHealthVertices() {
    return ready ? resources[10]->GetRawPointer() : nullptr;
}
