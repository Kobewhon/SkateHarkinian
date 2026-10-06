
#include <map>
#include <deque>
#include <string>
#include <functional>
#include <cassert>
#include <cstdio>
#include "NativeSkateBoardAppearance.h"
#define CVAR_ENHANCEMENT(x) "gEnhancements." x
using WidgetType = int;
enum {
    WIDGET_SEPARATOR_TEXT,
    WIDGET_CVAR_CHECKBOX,
    WIDGET_CVAR_SLIDER_FLOAT,
    WIDGET_CVAR_COMBOBOX,
    WIDGET_BUTTON,
    WIDGET_TEXT,
    SECTION_COLUMN_1
};
struct OptionsBase {
    double lo = 0, hi = 0, def = 0;
    int defaultIndex = 0;
    std::map<int, const char*> choices;
    OptionsBase& Min(float v) {
        lo = v;
        return *this;
    }
    OptionsBase& Max(float v) {
        hi = v;
        return *this;
    }
    OptionsBase& DefaultValue(double v) {
        def = v;
        return *this;
    }
    OptionsBase& DefaultIndex(int v) {
        defaultIndex = v;
        def = v;
        return *this;
    }
    OptionsBase& Format(const char*) {
        return *this;
    }
    OptionsBase& Tooltip(const char*) {
        return *this;
    }
    OptionsBase& Size(int) {
        return *this;
    }
    OptionsBase& ComboMap(std::map<int, const char*> v) {
        choices = v;
        return *this;
    }
};
using CheckboxOptions = OptionsBase;
using FloatSliderOptions = OptionsBase;
using ComboboxOptions = OptionsBase;
using ButtonOptions = OptionsBase;
namespace Sizes {
constexpr int Inline = 0;
}
struct WidgetInfo {
    std::string name, cvar;
    int type;
    OptionsBase options;
    std::function<void(WidgetInfo&)> callback;
    WidgetInfo& CVar(const char* v) {
        cvar = v;
        return *this;
    }
    WidgetInfo& RaceDisable(bool) {
        return *this;
    }
    WidgetInfo& Options(OptionsBase v) {
        options = v;
        return *this;
    }
    WidgetInfo& Callback(std::function<void(WidgetInfo&)> v) {
        callback = v;
        return *this;
    }
};
std::map<std::string, double> vars;
void CVarSetInteger(const char* s, int v) {
    vars[s] = v;
}
void CVarSetFloat(const char* s, float v) {
    vars[s] = v;
}
struct FakeGui {
    int saves = 0;
    void SaveConsoleVariablesNextFrame() {
        ++saves;
    }
    FakeGui* GetWindow() {
        return this;
    }
    FakeGui* GetGui() {
        return this;
    }
} gui;
namespace Ship {
struct Context {
    static FakeGui* GetRawInstance() {
        return &gui;
    }
};
} // namespace Ship
struct Path {
    std::string sidebarName;
    int column;
};
std::deque<WidgetInfo> widgets;
bool sidebar = false;
void AddSidebarEntry(const char* root, std::string name, int count) {
    assert(std::string(root) == "Enhancements" && name == "SkateHarkinian" && count == 1);
    sidebar = true;
}
WidgetInfo& AddWidget(Path& p, const char* n, int t) {
    assert(sidebar && p.sidebarName == "SkateHarkinian");
    widgets.push_back({ n, "", t });
    return widgets.back();
}
void Register() {
    Path path;
    path.sidebarName = "SkateHarkinian";
    AddSidebarEntry("Enhancements", path.sidebarName, 1);
    path.column = SECTION_COLUMN_1;
    AddWidget(path, "Visuals", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Board Appearance", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.BoardAppearance"))
        .RaceDisable(false)
        .Options(
            ComboboxOptions()
                .ComboMap({ { 0, "Default Skateboard" },
                            { 1, "Deku Shield" },
                            { 2, "Hylian Shield" },
                            { 3, "Mirror Shield" } })
                .DefaultIndex(0)
                .Tooltip(
                    "Actual OoT shield deck models with skateboard trucks and wheels. Physics, collision and board ownership remain unchanged."));
    AddWidget(path, "VHS Health HUD", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.VHSHealthHUD"))
        .RaceDisable(false)
        .Options(
            CheckboxOptions().DefaultValue(true).Tooltip("Draw health as cassette icons matching the VHS pickups."));
    AddWidget(path, "VHS World Health Pickups", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.VHSWorldPickups"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Draw normal recovery-heart pickups as VHS cassettes. Healing is unchanged."));

    AddWidget(path, "Skate 3 Style Score HUD", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.SkateStyleScoreHUD"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Use a blue multiplier badge and linked-line score display. Scoring remains enabled with either style."));

    AddWidget(path, "Score HUD Scale: %.3fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.ScoreHudScale"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(0.5f).Max(1.0f).DefaultValue(0.625f).Format("%.3fx").Tooltip(
            "Scale only the Skate 3-style score HUD. Default 0.625x is 62.5% of its original size."));

    AddWidget(path, "Object Dropper", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Enable Placement Mode", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Freeze the player and place temporary room-scoped props. Back exits through the browser. F9 clears props."));
    AddWidget(path, "Show Placement HUD", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.ShowHUD"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true));
    AddWidget(path, "Placement Distance: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.Distance"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(60.f).Max(300.f).DefaultValue(110.f).Format("%.0f"));
    AddWidget(path, "Placement Move Speed: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.ObjectDropper.MoveSpeed"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(20.f).Max(200.f).DefaultValue(100.f).Format("%.0f"));

    AddWidget(path, "Audio", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Skate Sounds", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Audio.Enabled"))
        .RaceDisable(false)
        .Options(
            CheckboxOptions().DefaultValue(true).Tooltip("Wheel rolling, board impacts, grinds and board handling."));
    AddWidget(path, "Skate Sound Volume: %.0f%%", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Audio.Volume"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(0.f).Max(100.f).DefaultValue(100.f).Format("%.0f%%").Tooltip(
            "Skate sound volume from zero to full."));
    AddWidget(path, "Cheats", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Cheats / Gameplay Modifiers", WIDGET_TEXT);
    AddWidget(path, "No Bailing", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Cheats.NoBail"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Suppress ordinary skating wipeouts. Obstacles remain solid; water and emergency recovery still work."));
    AddWidget(path, "Ollie Height: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Cheats.OllieHeightMultiplier"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(0.5f).Max(3.0f).DefaultValue(1.f).Format("%.2fx"));
    AddWidget(path, "Board Speed: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Cheats.BoardSpeedMultiplier"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(0.5f).Max(3.0f).DefaultValue(1.f).Format("%.2fx"));
    AddWidget(path, "Push Acceleration: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Cheats.PushAccelerationMultiplier"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(0.5f).Max(3.0f).DefaultValue(1.f).Format("%.2fx"));
    AddWidget(path, "Air Control: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Cheats.AirControlMultiplier"))
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(0.0f).Max(2.0f).DefaultValue(1.f).Format("%.2fx"));
    AddWidget(path, "Legacy FS 360 Pop Glitch", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("NativeSkate.Cheats.LegacyFs360PopGlitch"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Recreates the classic FS 360 Pop Shuvit super-pop technique using FS360 -> trigger -> Triangle/Y timing."));
    AddWidget(path, "Reset Skate Cheats to Defaults", WIDGET_BUTTON)
        .Options(ButtonOptions().Size(Sizes::Inline))
        .Callback([](WidgetInfo&) {
            CVarSetInteger(CVAR_ENHANCEMENT("NativeSkate.Cheats.NoBail"), 0);
            CVarSetInteger(CVAR_ENHANCEMENT("NativeSkate.Cheats.LegacyFs360PopGlitch"), 0);
            CVarSetFloat(CVAR_ENHANCEMENT("NativeSkate.Cheats.OllieHeightMultiplier"), 1.f);
            CVarSetFloat(CVAR_ENHANCEMENT("NativeSkate.Cheats.BoardSpeedMultiplier"), 1.f);
            CVarSetFloat(CVAR_ENHANCEMENT("NativeSkate.Cheats.PushAccelerationMultiplier"), 1.f);
            CVarSetFloat(CVAR_ENHANCEMENT("NativeSkate.Cheats.AirControlMultiplier"), 1.f);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        });
}

int requestedChoice = 0;
int CVarGetInteger(const char* name, int fallback) {
    auto it = vars.find(name);
    return it == vars.end() ? fallback : (int)it->second;
}
void CVarChanged(const char*) {
    gui.SaveConsoleVariablesNextFrame();
}
template <class T> bool Combobox(const char*, int* value, const std::map<T, const char*>&, const ComboboxOptions&) {
    *value = requestedChoice;
    return true;
}
template <typename T = int32_t>
bool CVarCombobox(const char* label, const char* cvarName, const std::map<T, const char*>& comboMap,
                  const ComboboxOptions& options = {}) {
    bool dirty = false;
    int32_t value = CVarGetInteger(cvarName, options.defaultIndex);
    if (Combobox<T>(label, &value, comboMap, options)) {
        CVarSetInteger(cvarName, value);
        CVarChanged(cvarName);
        dirty = true;
    }
    return dirty;
}
int main() {
    Register();
    bool cheats = false, reset = false, appearance = false;
    int count = 0;
    for (auto& w : widgets) {
        if (w.name == "Cheats")
            cheats = true;
        if (w.cvar.find("NativeSkate.Cheats.") != std::string::npos) {
            ++count;
            assert(cheats);
            bool boolean = w.type == WIDGET_CVAR_CHECKBOX;
            assert(w.options.def == (boolean ? 0 : 1));
            if (!boolean) {
                assert(w.options.lo == (w.name.find("Air Control") == 0 ? 0 : .5));
                assert(w.options.hi == (w.name.find("Air Control") == 0 ? 2 : 3));
            }
            vars[w.cvar] = 3;
        }
        if (w.name == "Board Appearance") {
            appearance = true;
            assert(w.cvar == "gEnhancements.NativeSkate.BoardAppearance" && w.options.def == 0 &&
                   w.options.choices.size() == 4 && std::string(w.options.choices.at(1)) == "Deku Shield");
            for (int i = 0; i < 4; ++i) {
                auto style = NativeSkateBoardAppearance::Resolve(i);
                assert((int)style == i);
                assert((NativeSkateBoardAppearance::ResolveBoardVisual(i).style !=
                        NativeSkateBoardAppearance::Style::Default) == (i != 0));
            }
            assert(NativeSkateBoardAppearance::Resolve(-1) == NativeSkateBoardAppearance::Style::Default);
            assert(NativeSkateBoardAppearance::Resolve(4) == NativeSkateBoardAppearance::Style::Default);
        }
        if (w.name == "Reset Skate Cheats to Defaults") {
            w.callback(w);
            reset = true;
        }
    }
    assert(count == 6 && reset && appearance && gui.saves == 1);
    for (int choice = 0; choice < 4; ++choice) {
        requestedChoice = choice;
        CVarCombobox("Board Appearance", "gEnhancements.NativeSkate.BoardAppearance",
                     std::map<int, const char*>{ { 0, "Default" }, { 1, "Deku" }, { 2, "Hylian" }, { 3, "Mirror" } });
        assert(CVarGetInteger("gEnhancements.NativeSkate.BoardAppearance", -1) == choice);
    }
    assert(gui.saves == 5);
    vars.erase("gEnhancements.NativeSkate.BoardAppearance");
    assert(vars.size() == 6);
    for (auto& v : vars)
        assert(
            v.second ==
            (v.first.find("NoBail") != std::string::npos || v.first.find("LegacyFs360") != std::string::npos ? 0 : 1));
    puts(
        "PASS compiled production Enhancements/SkateHarkinian registration, Cheats bindings/defaults/ranges/reset, one Board Appearance combo with four resources and invalid fallback");
}
