#pragma once
#include "NativeSkateGrindCompiler.h"
struct PlayState;
namespace NativeSkateGrindDebug {
void Draw(PlayState*, const NativeSkateGrindCompiler::Result&, bool ready);
}
