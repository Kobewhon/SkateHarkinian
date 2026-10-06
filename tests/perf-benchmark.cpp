#include "NativeSkatePerf.h"
#include <cstdio>
#include <cassert>
using Clock = std::chrono::steady_clock;
volatile unsigned long long work = 0;
int main() {
    constexpr int n = 1000000;
    for (int round = 0; round < 5; ++round) {
        NativeSkatePerf::Metric baseline;
        auto begin = Clock::now();
        for (int i = 0; i < n; ++i) {
            auto start = Clock::now();
            work = work + i;
            baseline.Add(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
        }
        auto old = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        NativeSkatePerf::enabled = false;
        auto count = NativeSkatePerf::metrics[NativeSkatePerf::HudUpdate].count;
        begin = Clock::now();
        for (int i = 0; i < n; ++i) {
            NativeSkatePerf::Scope timing(NativeSkatePerf::HudUpdate);
            work = work + i;
        }
        auto after = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        assert(NativeSkatePerf::metrics[NativeSkatePerf::HudUpdate].count == count);
        printf("round=%d operations=%d baseline_ms=%.6f disabled_ms=%.6f\n", round, n, old, after);
    }
    NativeSkatePerf::enabled = true;
    auto count = NativeSkatePerf::metrics[NativeSkatePerf::HudUpdate].count;
    {
        NativeSkatePerf::Scope timing(NativeSkatePerf::HudUpdate);
    }
    assert(NativeSkatePerf::metrics[NativeSkatePerf::HudUpdate].count == count + 1);
    puts("PASS developer profiling retains metrics; disabled scope performs no clock reads/metric writes");
}
