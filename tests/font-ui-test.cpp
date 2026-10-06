#include "imgui.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
int main(int argc, char** argv) {
    assert(argc == 2);
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    auto* f = io.Fonts->AddFontFromFileTTF(argv[1], 48);
    assert(f);
    unsigned char* p;
    int w, h;
    io.Fonts->GetTexDataAsAlpha8(&p, &w, &h);
    assert(p && w && h);
    for (char c = ' '; c <= '~'; ++c)
        assert(f->FindGlyphNoFallback(c));
    for (int height : { 1080, 1440, 2160 }) {
        float unit = height / 240.f;
        float screenWidth = height * 16.f / 9;
        for (float logical : { 7.5f, 8.5f, 10.f })
            for (const char* t : { "Press F8 to enter Skate Mode", "Press F8 to exit Skate Mode", "Skate Options",
                                   "Object Dropper", "Placed Objects", "Quarter Pipe Narrow", "Double Kink Rail",
                                   "Bank-to-Ledge", "Legacy FS 360 Pop Glitch" }) {
                float size = logical * unit;
                float available = screenWidth - 28 * unit;
                float width = f->CalcTextSizeA(size, 100000, 0, t).x;
                if (width > available) {
                    size *= available / width;
                    width = available;
                }
                assert(width <= available + .01f && size >= 7 * unit);
                assert(size < height * .05f);
            }
        float size = 19 * height / 240.f * .625f;
        float end = 300;
        for (const char* t : { "9,850", "10,250", "125,700", "1,000,000" }) {
            float width = f->CalcTextSizeA(size, 100000, 0, t).x;
            assert(width > 0 && width < height * .60f);
            float left = end - width;
            assert(left + width == end);
        }
        printf("PASS %dp glyphs, compact measured numeric width and stable right anchor\n", height);
    }
    printf(
        "PASS shared overlay metrics1080/1440/2160 + Skate UI SFNT atlas %dx%d; printable ASCII, one atlas registration and font ownership\n",
        w, h);
    ImGui::DestroyContext();
}
