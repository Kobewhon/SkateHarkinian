#pragma once
#include <cmath>
#include <cstdint>
namespace NativeSkateOwnership {
enum class Mode { DISABLED, SUSPENDED, ACTIVE, FAILED, RECOVERING };
enum class Reason {
    STARTUP,
    USER_F8_DISABLE,
    USER_ENABLE,
    CONSOLE_DISABLE,
    TEMP_SUSPEND_DIALOGUE,
    TEMP_SUSPEND_CUTSCENE,
    SCENE_TRANSITION,
    PLAYER_REPLACED,
    DEATH,
    RUNTIME_FAILURE,
    SELF_HEAL,
    F9_RECOVERY,
    RESUME,
    UNKNOWN_BUG
};
inline const char* Name(Reason r) {
    switch (r) {
#define R(x)        \
    case Reason::x: \
        return #x
        R(STARTUP);
        R(USER_F8_DISABLE);
        R(USER_ENABLE);
        R(CONSOLE_DISABLE);
        R(TEMP_SUSPEND_DIALOGUE);
        R(TEMP_SUSPEND_CUTSCENE);
        R(SCENE_TRANSITION);
        R(PLAYER_REPLACED);
        R(DEATH);
        R(RUNTIME_FAILURE);
        R(SELF_HEAL);
        R(F9_RECOVERY);
        R(RESUME);
        default:
            return "UNKNOWN_BUG";
#undef R
    }
}
inline const char* Name(Mode m) {
    switch (m) {
        case Mode::ACTIVE:
            return "ACTIVE";
        case Mode::SUSPENDED:
            return "SUSPENDED";
        case Mode::FAILED:
            return "FAILED";
        case Mode::RECOVERING:
            return "RECOVERING";
        default:
            return "DISABLED";
    }
}
struct Observation {
    bool requested = false, owned = false, valid = false, unsafe = false, paused = false, replaced = false,
         callbacks = false;
    float positionError = 0;
};
enum class Action { NONE, DISABLE, SUSPEND, RECOVER, ACTIVATE };
inline Action Decide(const Observation& o) {
    if (!o.requested)
        return o.owned ? Action::DISABLE : Action::NONE;
    if (o.paused)
        return Action::NONE;
    if (o.unsafe)
        return o.owned ? Action::SUSPEND : Action::NONE;
    if (o.replaced ||
        (o.owned && (!o.valid || !o.callbacks || !std::isfinite(o.positionError) || o.positionError > 80.f)))
        return Action::RECOVER;
    if (!o.owned)
        return o.valid ? Action::ACTIVATE : Action::RECOVER;
    return Action::NONE;
}
} // namespace NativeSkateOwnership
