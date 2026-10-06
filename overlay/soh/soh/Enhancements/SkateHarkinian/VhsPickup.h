#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;
struct Actor;
/* Presentation only. Returns zero if the optional asset archive is unavailable. */
int SkateHarkinian_DrawVhsRecoveryHeart(struct PlayState* play, struct Actor* actor);
int SkateHarkinian_VhsObjectReady();
void SkateHarkinian_DrawVhsObject(struct PlayState* play);
#ifdef __cplusplus
}
#endif
