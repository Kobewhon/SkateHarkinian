#include "NativeSkateVert.h"
#pragma once
struct PlayState;
struct Actor;
#include <functional>
#include <vector>
#include <array>
#include <cstdint>
namespace NativeSkateObjectDropper {
const std::vector<NativeSkateVert::Ramp>& VertRamps();
bool Update(PlayState* play, bool nativeOwned);
void Draw(PlayState* play);
bool Active();
bool BipedPropInput(PlayState*, bool biped, bool swordDrawn);
bool CarryingProp();
bool PropInputOwned();
bool LocalGrinds(Actor*, std::vector<std::vector<std::array<float, 3>>>&);
uint64_t GrindOwner(const Actor*);
int AudioMaterial(Actor*);
bool RidingSurface(const Actor*);
void SessionMarkerSet(bool interact);
void RestoreMarkerObjects();
void Exit(const char* reason);
void Clear(const char* reason, bool sceneReplaced = false);
void Initialize(std::function<void(Actor*)> changed);
} // namespace NativeSkateObjectDropper
