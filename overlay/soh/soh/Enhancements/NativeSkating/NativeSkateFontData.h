#pragma once
#include <cstdint>
#include <cstddef>
namespace NativeSkateHud {
// Validate the packaged SFNT directory before handing bytes to ImGui/STB.
inline bool ValidFontData(const void* bytes, size_t size) {
    if (!bytes || size < 12 || size > 8 * 1024 * 1024)
        return false;
    auto* p = static_cast<const uint8_t*>(bytes);
    auto word = [&](size_t i) {
        return uint32_t(p[i]) << 24 | uint32_t(p[i + 1]) << 16 | uint32_t(p[i + 2]) << 8 | p[i + 3];
    };
    if (word(0) != 0x4f54544fu && word(0) != 0x00010000u)
        return false;
    size_t count = (size_t(p[4]) << 8) | p[5];
    if (!count || count > (size - 12) / 16)
        return false;
    bool cmap = false, head = false, maxp = false, outline = false;
    for (size_t i = 0; i < count; ++i) {
        size_t entry = 12 + i * 16;
        auto tag = word(entry);
        size_t off = word(entry + 8), len = word(entry + 12);
        if (off > size || len > size - off)
            return false;
        cmap |= tag == 0x636d6170 && len >= 4;
        head |= tag == 0x68656164 && len >= 54;
        maxp |= tag == 0x6d617870 && len >= 6;
        outline |= (tag == 0x43464620 || tag == 0x676c7966) && len >= 4;
    }
    return cmap && head && maxp && outline;
}
} // namespace NativeSkateHud
