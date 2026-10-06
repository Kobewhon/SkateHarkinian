#pragma once
#include "NativeSkateEvents.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <cctype>
#include <deque>
#include <limits>
#include <cstdio>
namespace NativeSkateHud {
inline std::string DisplayName(std::string value) {
    for (const char* prefix : { "ID_TRICK_", "TRICK_" })
        if (value.rfind(prefix, 0) == 0)
            value.erase(0, std::char_traits<char>::length(prefix));
    for (const char* prefix : { "FLIP_", "GRIND_", "GRAB_", "AIR_", "SPECIAL_" })
        if (value.rfind(prefix, 0) == 0) {
            value.erase(0, std::char_traits<char>::length(prefix));
            break;
        }
    for (const auto& pair :
         { std::pair<const char*, const char*>("50_50", "50-50"), { "5_O", "5-0" }, { "FIFTY_FIFTY", "50-50" } }) {
        size_t pos;
        while ((pos = value.find(pair.first)) != std::string::npos)
            value.replace(pos, std::char_traits<char>::length(pair.first), pair.second);
    }
    for (char& c : value) {
        if (c == '_')
            c = ' ';
        c = (char)std::toupper((unsigned char)c);
    }
    return value;
}
// Native events retain native floating-point reward calculations. This ledger
// commits rounded sequence rewards once, then banks the linked line once.
constexpr double kLineLinkSeconds = 2.50;
constexpr double kNativeTicksPerSecond = 60.0; // simulation clock, not rendered frames
constexpr uint64_t kLineHoldTicks = 54, kLineFadeTicks = 36;
inline uint64_t Points(float v) {
    if (!std::isfinite(v) || v <= 0)
        return 0;
    long double n = std::round((long double)v);
    return n >= std::numeric_limits<uint64_t>::max() ? std::numeric_limits<uint64_t>::max() : (uint64_t)n;
}
inline uint64_t AddPoints(uint64_t a, uint64_t b) {
    return b > std::numeric_limits<uint64_t>::max() - a ? std::numeric_limits<uint64_t>::max() : a + b;
}
inline std::string ScoreText(uint64_t value) {
    auto text = std::to_string(value);
    for (int i = (int)text.size() - 3; i > 0; i -= 3)
        text.insert(i, ",");
    return text;
}
struct Line {
    uint64_t score = 0, total = 0, completedAt = 0, lastCombo = 0, provisional = 0, generation = 0;
    double deadline = 0;
    bool active = false, inCombo = false, completed = false;
    const char* transition = "NONE";
    static double Seconds(uint64_t tick) {
        return tick / kNativeTicksPerSecond;
    }
    void Cancel(const char* reason = "LINE_CANCEL_RECOVERY") {
        score = provisional = lastCombo = 0;
        deadline = 0;
        active = inCombo = completed = false;
        ++generation;
        transition = reason;
    }
    void Start(uint64_t now) {
        if (!active) {
            score = provisional = 0;
            active = true;
            completed = false;
            ++generation;
            transition = "LINE_START";
        } else
            transition = inCombo ? "TRICK_SCORE_ADD" : "LINE_REFRESH";
        inCombo = true;
        deadline = Seconds(now) + kLineLinkSeconds;
    }
    void Observe(float combo) {
        if (active && inCombo)
            provisional = std::max(provisional, Points(combo));
    }
    void Commit(float reward, uint64_t now) {
        if (!inCombo)
            return;
        auto points = std::max(provisional, Points(reward));
        score = AddPoints(score, points);
        lastCombo = points;
        provisional = 0;
        inCombo = false;
        deadline = Seconds(now) + kLineLinkSeconds;
        transition = "COMBO_COMMIT";
    }
    void Finish(uint64_t now) {
        if (!active)
            return;
        score = AddPoints(score, provisional);
        provisional = 0;
        total = AddPoints(total, score);
        active = inCombo = false;
        completed = true;
        completedAt = now;
        transition = "LINE_COMPLETE";
    }
    void Exit(uint64_t now) {
        if (active)
            Finish(now);
        transition = "LINE_CANCEL_F8";
    }
    void Advance(uint64_t now, bool continuous = false) {
        if (active && !inCombo) {
            if (continuous)
                deadline = Seconds(now) + kLineLinkSeconds;
            else if (Seconds(now) > deadline)
                Finish(now);
        }
    }
    uint64_t Display(float = 0) const {
        return AddPoints(score, provisional);
    }
};
struct State {
    uint64_t sequence = 0, tick = 0, lastTrick = 0, noticeTick = 0;
    float current = 0, combo = 0, multiplier = 1, banked = 0;
    std::string label, notice;
    std::map<int32_t, std::pair<std::string, float>> active;
    uint64_t historyTick = 0, scorePopTick = 0;
    Line line;
    std::deque<std::string> history;
    bool labelDirty = true;
    void Clear(bool resetSequence = true) {
        line.Cancel();
        history.clear();
        if (resetSequence) {
            sequence = 0;
            tick = 0;
        }
        current = combo = 0;
        multiplier = 1;
        label.clear();
        notice.clear();
        active.clear();
        lastTrick = noticeTick = 0;
        labelDirty = true;
    }
    void Apply(const NativeSkateRuntime::Event& e) {
        if (e.sequence <= sequence || e.tick < tick || !std::isfinite(e.current) || !std::isfinite(e.combo) ||
            !std::isfinite(e.multiplier) || !std::isfinite(e.banked))
            return;
        Advance(e.tick);
        if (Points(e.combo) > Points(combo))
            scorePopTick = e.tick;
        sequence = e.sequence;
        tick = e.tick;
        combo = std::max(0.f, e.combo);
        line.Observe(combo);
        multiplier = std::max(1.f, e.multiplier);
        banked = std::max(0.f, e.banked);
        switch ((NativeSkateRuntime::EventPhase)e.phase) {
            case NativeSkateRuntime::EventPhase::Reset: {
                bool keep = label == "BAIL" && e.tick - noticeTick < 90;
                auto at = noticeTick;
                auto savedLine = line;
                Clear(false);
                line = savedLine;
                if (keep) {
                    label = "BAIL";
                    notice = "COMBO LOST";
                    lastTrick = noticeTick = at;
                }
                return;
            }
            case NativeSkateRuntime::EventPhase::Bail:
                line.Cancel("LINE_BAIL");
                history.clear();
                active.clear();
                current = combo = 0;
                label = "BAIL";
                notice = "COMBO LOST";
                lastTrick = noticeTick = e.tick;
                return;
            case NativeSkateRuntime::EventPhase::Land:
                line.Commit(e.current, e.tick);
                active.clear();
                current = e.current;
                notice = (e.flags & 8) ? "SKETCHY" : (e.flags & 4) ? "CLEAN" : "LANDED";
                noticeTick = lastTrick = e.tick;
                return;
            case NativeSkateRuntime::EventPhase::Complete:
            case NativeSkateRuntime::EventPhase::GrindExit:
                active.erase(e.id);
                labelDirty = true;
                return;
            default:
                break;
        }
        if (e.id < 0 || e.name[0] == 0)
            return;
        if (!line.active)
            history.clear();
        line.Start(e.tick);
        line.Observe(combo);
        auto found = active.find(e.id);
        if (found == active.end()) {
            active[e.id] = { DisplayName(std::string(e.name)), std::max(0.f, e.current) };
            historyTick = e.tick;
            history.push_back(active[e.id].first);
            if (history.size() > 4)
                history.pop_front();
            labelDirty = true;
        } else
            found->second.second = std::max(0.f, e.current);
        if (labelDirty)
            label.clear();
        current = 0;
        for (const auto& item : active) {
            if (labelDirty) {
                if (!label.empty())
                    label += " + ";
                label += item.second.first;
            }
            current += item.second.second;
        }
        labelDirty = false;
        lastTrick = e.tick;
        notice.clear();
    }
    void Advance(uint64_t now, bool continuous = false) {
        tick = std::max(tick, now);
        line.Advance(tick, continuous);
        if (line.completed && tick >= line.completedAt + kLineHoldTicks + kLineFadeTicks)
            Clear(false);
    }
    int Alpha() const {
        if (line.active)
            return 255;
        if (line.completed) {
            auto age = tick >= line.completedAt ? tick - line.completedAt : 0;
            return age < kLineHoldTicks ? 255
                   : age >= kLineHoldTicks + kLineFadeTicks
                       ? 0
                       : (int)((kLineHoldTicks + kLineFadeTicks - age) * 255 / kLineFadeTicks);
        }
        if (label.empty())
            return 0;
        uint64_t age = tick >= lastTrick ? tick - lastTrick : 0;
        return age < 90 ? 255 : age >= 150 ? 0 : (int)((150 - age) * 255 / 60);
    }
};
} // namespace NativeSkateHud
