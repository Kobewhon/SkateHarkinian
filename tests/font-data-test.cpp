#include "NativeSkateFontData.h"
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace NativeSkateHud;
    assert(!ValidFontData(nullptr, 0));
    char bad[100]{};
    assert(!ValidFontData(bad, 100));
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(f)), {});
    assert(ValidFontData(data.data(), data.size()));
    assert(!ValidFontData(data.data(), 20));
    auto copy = data;
    copy[20] = -1;
    assert(!ValidFontData(copy.data(), copy.size()));
    puts("PASS bundled SFNT table bounds and missing/truncated/corrupt directory fallback gate");
}
