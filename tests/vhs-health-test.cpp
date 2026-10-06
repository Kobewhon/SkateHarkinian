#include <cassert>
#include <cstdio>

#include "VhsHealthHUD.h"

int main() {
    for (int fullCount = 0; fullCount <= 20; ++fullCount) {
        for (int index = 0; index <= 20; ++index) {
            for (int fraction = 0; fraction <= 15; ++fraction) {
                int expected = 0;
                if (index < fullCount || (index == fullCount && fraction == 0)) {
                    expected = 4;
                } else if (index == fullCount) {
                    expected = fraction <= 5 ? 1 : (fraction <= 10 ? 2 : 3);
                }
                assert(SkateHarkinian_VhsHealthState(index, fullCount, fraction) == expected);
            }
        }
    }
    std::puts("PASS unchanged VHS fractional health selector: 7056 normal/defense-independent cases");
}
