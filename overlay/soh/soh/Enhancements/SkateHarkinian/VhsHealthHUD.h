#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Stock fractional texture ranges: 1..5 quarter, 6..10 half, 11..15 three-quarter. */
static inline int SkateHarkinian_VhsHealthState(int index, int fullCount, int fraction) {
    if (index > fullCount)
        return 0;
    if (index < fullCount || fraction == 0)
        return 4;
    return fraction <= 5 ? 1 : (fraction <= 10 ? 2 : 3);
}
int SkateHarkinian_VhsHealthHUDReady(void);
const void* SkateHarkinian_VhsHealthTexture(int index, int fullCount, int fraction, int defense);
void* SkateHarkinian_VhsHealthVertices(void);
#ifdef __cplusplus
}
#endif
