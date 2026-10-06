#include "NativeSkateAudio.h"
#include "NativeSkateAudioState.h"
#include "NativeSkateObjectDropper.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/File.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <mutex>
#include <cstring>
extern "C" {
#include "global.h"
}
namespace NativeSkateAudio {
// Project-safe samples are optional archive slots: signed little-endian mono PCM, 32000 Hz.
// One preload, no decode/filesystem access in the callback or on events.
constexpr int kSurfaces = 7, kCues = 15, kLoops = 3, kVoices = 16, kVariants = 3;
constexpr int kSlotStride = kSurfaces * (kCues + kLoops);
constexpr float kSurfaceCrossfadeSeconds = .075f;
static const char* surfaces[] = { "generic", "stone", "concrete", "wood", "metal", "dirt", "grass" };
static const char* cues[] = { "push",        "pop",       "land_soft",   "land_medium", "land_hard",
                              "grind_start", "grind_end", "slide_start", "slide_end",   "bail_board",
                              "bail_body",   "pickup",    "drop",        "mount",       "dismount" };
struct Sample {
    std::vector<int16_t> pcm;
};
static std::array<Sample, kSlotStride * kVariants> samples;
struct Voice {
    int slot = -1;
    double cursor = 0;
    float volume = 0, pitch = 1;
};
static std::array<Voice, kVoices> voices;
static std::array<Voice, kLoops> loops, fadingLoops;
static std::array<float, kLoops> crossfadeRemaining{};
static std::array<uint32_t, kCues> variation{};
static std::mutex mutex;
static State state;
static bool prepared = false;
static float master = 1;
static int Slot(int kind, Surface s, int variant = 0) {
    return variant * kSlotStride + (int)s * (kCues + kLoops) + kind;
}
static void Prepare() {
    if (prepared)
        return;
    prepared = true;
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    int found = 0;
    for (int surface = 0; surface < kSurfaces; ++surface)
        for (int cue = 0; cue < kCues + kLoops; ++cue)
            for (int variant = 0; variant < kVariants; ++variant) {
                std::string name = cue < kCues        ? cues[cue]
                                   : cue == kCues     ? "rolling"
                                   : cue == kCues + 1 ? "grind_loop"
                                                      : "slide_loop";
                std::string path = "SkateHarkinian/Audio/Skate/" + std::string(surfaces[surface]) + "/" + name +
                                   (variant == 0   ? ""
                                    : variant == 1 ? "_02"
                                                   : "_03") +
                                   ".pcm";
                if (!manager->HasFile(path)) {
                    if (variant == 0)
                        SPDLOG_WARN("[SkateAudio] Missing slot {} (other sounds remain available)", path);
                    continue;
                }
                auto file = manager->LoadFile(path);
                if (!file || !file->Buffer) {
                    SPDLOG_ERROR("[SkateAudio] Failed loading {}", path);
                    continue;
                }
                auto& bytes = *file->Buffer;
                if (bytes.empty() || bytes.size() % 2 || bytes.size() > 32000 * 2 * 10) {
                    SPDLOG_ERROR("[SkateAudio] Invalid PCM slot {}", path);
                    continue;
                }
                auto& sample = samples[Slot(cue, (Surface)surface, variant)];
                sample.pcm.resize(bytes.size() / 2);
                std::memcpy(sample.pcm.data(), bytes.data(), bytes.size());
                ++found;
            }
    SPDLOG_INFO("[SkateAudio] Cached {} original Foley PCM slots (126 base slots plus optional variants)", found);
}
static int Resolve(int slot) {
    if (samples[slot].pcm.empty())
        slot = (slot / kSlotStride) * kSlotStride + slot % (kCues + kLoops);
    return samples[slot].pcm.empty() ? -1 : slot;
}
static int Pick(Cue cue, Surface surface, bool hard) {
    int kind = (int)cue;
    if (cue == Cue::BailBody) {
        int slot = Resolve(Slot(kind, surface, hard ? 1 : 0));
        return slot >= 0 ? slot : Resolve(Slot(kind, surface));
    }
    std::array<int, kVariants> choices{};
    int count = 0;
    for (int v = 0; v < kVariants; ++v) {
        int slot = Resolve(Slot(kind, surface, v));
        if (slot >= 0)
            choices[count++] = slot;
    }
    return count ? choices[variation[kind]++ % count] : -1;
}

void Stop() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& v : voices)
        v = {};
    for (auto& v : loops)
        v = {};
    for (auto& v : fadingLoops)
        v = {};
    crossfadeRemaining.fill(0);
    state.Reset();
}
void Update(PlayState* play, Player* player, const NativeSkateRuntime::Snapshot& s, bool push, bool water, bool held) {
    if (!play || !player || !CVarGetInteger("gEnhancements.NativeSkate.Audio.Enabled", 1)) {
        Stop();
        return;
    }
    Surface surface = Surface::Generic;
    if (player->actor.floorPoly) {
        int bg = player->actor.floorBgId;
        if (bg >= 0 && bg < BG_ACTOR_MAX)
            surface = (Surface)NativeSkateObjectDropper::AudioMaterial(play->colCtx.dyna.bgActors[bg].actor);
        if (surface == Surface::Generic) {
            auto material = func_80041F10(&play->colCtx, player->actor.floorPoly, bg);
            surface = material == 2                    ? Surface::Concrete
                      : material == 1 || material == 3 ? Surface::Dirt
                      : material == 8                  ? Surface::Grass
                      : material == 10                 ? Surface::Wood
                      : material == 13                 ? Surface::Metal
                                                       : Surface::Stone;
        }
    }
    Frame f;
    f.tick = s.tick;
    f.state = s.stateId;
    f.speed = std::hypot(s.trajectoryVelocity[0], s.trajectoryVelocity[2]);
    if (s.stateId == 1000)
        f.speed = std::hypot(f.speed, s.trajectoryVelocity[1]);
    f.vertical = s.trajectoryVelocity[1];
    f.surface = surface;
    f.push = push;
    f.water = water;
    // The accepted native flick intent precedes GroundAnimation launch by several ticks.
    // Latch that native intent briefly, then fire once at actual launch; never from acceleration alone.
    constexpr uint64_t kPopIntentLatchTicks = 30;
    bool popIntent = s.coreIntents[0] > 0 || s.coreIntents[2] > 0 || s.coreIntents[3] > 0 || s.coreIntents[4] > 0 ||
                     s.coreIntents[5] > 0 || s.coreIntents[6] > 0;
    bool sourceGround = (s.stateId >= 100 && s.stateId <= 105) || s.stateId == 1000,
         sourceGrind = s.stateId >= 400 && s.stateId <= 405;
    if (popIntent && (sourceGround || sourceGrind))
        state.popIntentUntil = s.tick + kPopIntentLatchTicks;
    bool launched =
        state.valid && ((s.stateId == 103 && state.previous.state != 103) ||
                        (s.stateId >= 200 && s.stateId < 300 &&
                         (state.previous.state < 200 || (state.previous.state >= 400 && state.previous.state <= 405))));
    f.pop = launched && state.popIntentUntil != 0 && s.tick <= state.popIntentUntil;
    if (f.pop || s.stateId == 300 || s.stateId == 500 || s.stateId == 501)
        state.popIntentUntil = 0;
    // Carry ownership comes from the published native pose diagnostics supplied below.
    f.held = held;
    std::lock_guard<std::mutex> lock(mutex);
    Prepare();
    float volume = CVarGetFloat("gEnhancements.NativeSkate.Audio.Volume", 100.f) / 100.f;
    master = std::isfinite(volume) ? std::clamp(volume, 0.f, 1.f) : 1.f;
    state.Read(
        f,
        [&](Cue cue, Surface material) {
            int slot = Pick(cue, material, state.impact >= 5 || state.previous.speed >= 6);
            if (slot < 0)
                return;
            for (auto& v : voices)
                if (v.slot < 0) {
                    v = { slot, 0, .25f, 1.f };
                    break;
                }
        },
        [&](int channel, Surface material, float volume,
            float pitch) { // Dirt wheels are deliberately 20% quieter; impacts and other surfaces retain their balance.
            if (channel == 0 && material == Surface::Dirt)
                volume *= .8f;
            int slot = Resolve(Slot(kCues + channel, material));
            auto& v = loops[channel];
            if (v.slot != slot) {
                fadingLoops[channel] = v;
                crossfadeRemaining[channel] = kSurfaceCrossfadeSeconds;
                v = { slot, 0, volume, pitch };
            }
            v.volume = volume;
            v.pitch = pitch;
        });
}
void Mix(int16_t* buffer, uint32_t frames) {
    std::lock_guard<std::mutex> lock(mutex);
    for (uint32_t i = 0; i < frames; ++i) {
        float sound = 0;
        auto mix = [&](Voice& v, bool loop, float gain) {
            if (v.slot < 0)
                return;
            auto& p = samples[v.slot].pcm;
            if (p.empty())
                return;
            if (v.cursor >= p.size()) {
                if (loop)
                    v.cursor = std::fmod(v.cursor, (double)p.size());
                else {
                    v.slot = -1;
                    return;
                }
            }
            size_t a = (size_t)v.cursor, b = a + 1 < p.size() ? a + 1 : loop ? 0 : a;
            float fraction = (float)(v.cursor - a);
            sound += (p[a] + (p[b] - p[a]) * fraction) * v.volume * master * gain;
            v.cursor += v.pitch;
        };
        for (int channel = 0; channel < kLoops; ++channel) {
            float outgoing = std::clamp(crossfadeRemaining[channel] / kSurfaceCrossfadeSeconds, 0.f, 1.f);
            mix(loops[channel], true, 1 - outgoing);
            if (outgoing > 0) {
                mix(fadingLoops[channel], true, outgoing);
                crossfadeRemaining[channel] = std::max(0.f, crossfadeRemaining[channel] - 1.f / 32000.f);
            } else
                fadingLoops[channel] = {};
        }
        for (auto& v : voices)
            mix(v, false, 1.f);
        for (int c = 0; c < 2; ++c)
            buffer[i * 2 + c] = (int16_t)std::clamp(buffer[i * 2 + c] + sound, -32768.f, 32767.f);
    }
}
} // namespace NativeSkateAudio
