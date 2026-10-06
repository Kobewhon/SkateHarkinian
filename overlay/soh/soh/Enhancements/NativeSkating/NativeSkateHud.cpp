#include "NativeSkateHud.h"
#include "NativeSkateFontData.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include <cstdio>
#include <imgui_internal.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <ship/window/gui/resource/Font.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>
#include "NativeSkateObjectDropper.h"
extern "C" {
#include "global.h"
float OTRGetDimensionFromRightEdge(float);
float OTRGetDimensionFromLeftEdge(float);
}
extern "C" int NativeSkateOwnsCurrentPlayer();
namespace NativeSkateHud {
static ImFont* skateFont = nullptr;
static std::shared_ptr<Ship::Font> fontResource;
static const State* liveScore = nullptr;
struct OverlayLine {
    std::string text;
    float x, y, size;
    int r, g, b, a;
    bool right;
};
static std::array<OverlayLine, 24> overlays;
static size_t overlayCount = 0;
bool FontReady() {
    return skateFont != nullptr;
}
void ClearOverlays() {
    overlayCount = 0;
}
// Called by game draw; rendered during ImGui's normal GUI pass with the same
// cached font as scoring. Bounded slots retain string capacity between frames.
void Overlay(const char* text, float x, float y, float size, int r, int g, int b, int a, bool right) {
    if (!skateFont || overlayCount == overlays.size())
        return;
    auto& line = overlays[overlayCount++];
    line.text = text;
    line.x = x;
    line.y = y;
    line.size = size;
    line.r = r;
    line.g = g;
    line.b = b;
    line.a = a;
    line.right = right;
}
static void DrawOverlays() {
    if (!skateFont || !overlayCount || !gPlayState)
        return;
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    auto* w = ImGui::FindWindowByID(gui->GetMainGameWindowID());
    if (!w)
        return;
    auto lo = w->InnerRect.Min, hi = w->InnerRect.Max;
    float unit = (hi.y - lo.y) / 240.f;
    auto* dl = ImGui::GetForegroundDrawList(w->Viewport);
    dl->PushClipRect(lo, hi, true);
    for (size_t i = 0; i < overlayCount; ++i) {
        const auto& line = overlays[i];
        float size = line.size * unit;
        ImVec2 pos(line.right ? hi.x - line.x * unit : lo.x + line.x * unit, lo.y + line.y * unit);
        // Fixed available width avoids panel/name oscillation; measured fit handles
        // expanded catalog names at every game viewport resolution.
        float width = skateFont->CalcTextSizeA(size, FLT_MAX, 0, line.text.c_str()).x;
        float available = line.right ? pos.x - lo.x - 12 * unit : hi.x - pos.x - 12 * unit;
        if (width > available && available > 0) {
            size *= available / width;
            width = available;
        }
        if (line.right)
            pos.x -= width;
        dl->AddText(skateFont, size, ImVec2(pos.x + unit, pos.y + unit), IM_COL32(0, 0, 0, line.a), line.text.c_str());
        dl->AddText(skateFont, size, pos, IM_COL32(line.r, line.g, line.b, line.a), line.text.c_str());
    }
    dl->PopClipRect();
}
class ScoreWindow final : public Ship::GuiWindow {
  public:
    ScoreWindow() : GuiWindow("gWindows.NativeSkateScore", true, "SkateHarkinian Score") {
    }
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override {
    }
    void Draw() override {
        DrawOverlays();
        auto* p = gPlayState;
        if (!skateFont || !liveScore || !p || !NativeSkateOwnsCurrentPlayer() || NativeSkateObjectDropper::Active() ||
            !CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Enabled"), 1) || p->pauseCtx.state ||
            p->gameOverCtx.state || p->msgCtx.msgMode || p->transitionTrigger != TRANS_TRIGGER_OFF ||
            Player_InCsMode(p) || GameInteractor_NoUIActive() || p->interfaceCtx.healthAlpha == 0)
            return;
        const auto& s = *liveScore;
        int alpha = s.Alpha();
        if (!alpha)
            return;
        auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
        auto* w = ImGui::FindWindowByID(gui->GetMainGameWindowID());
        if (!w)
            return;
        auto lo = w->InnerRect.Min, hi = w->InnerRect.Max;
        float unit = (hi.y - lo.y) / 240.f;
        float scale = CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.ScoreHudScale"), .625f);
        if (!std::isfinite(scale))
            scale = .625f;
        scale = std::clamp(scale, .5f, 1.f);
        auto* dl = ImGui::GetForegroundDrawList(w->Viewport);
        dl->PushClipRect(lo, hi, true);
        float x = lo.x + (48 + std::clamp(CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0), 0, 40)) * unit,
              y = lo.y + 116 * unit;
        auto at = [&](float xx, float yy) { return ImVec2(x + xx * unit * scale, y + yy * unit * scale); };
        auto text = [&](const std::string& t, float xx, float yy, float size, ImU32 color, bool right = false) {
            float pixels = size * unit * scale;
            ImVec2 pos = at(xx, yy);
            if (right) {
                float width = skateFont->CalcTextSizeA(pixels, FLT_MAX, 0, t.c_str()).x;
                float available = (xx - 43) * unit * scale;
                if (width > available && available > 0) {
                    pixels *= available / width;
                    width = available;
                }
                pos.x -= width;
            }
            dl->AddText(skateFont, pixels, ImVec2(pos.x + unit * scale, pos.y + unit * scale), IM_COL32(0, 0, 0, alpha),
                        t.c_str());
            dl->AddText(skateFont, pixels, pos, color, t.c_str());
        };
        auto white = IM_COL32(245, 245, 245, alpha), cyan = IM_COL32(95, 188, 235, alpha);
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.TrickFeed"), 1)) {
            int row = 4 - (int)s.history.size();
            for (const auto& name : s.history)
                text(name, 0, float(row++ * 11), 10, white);
        }
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Combo"), 1)) {
            dl->AddCircleFilled(at(18, 64), 18 * unit * scale, IM_COL32(13, 57, 104, alpha));
            dl->AddCircleFilled(at(18, 64), 16 * unit * scale, IM_COL32(34, 131, 205, alpha));
            dl->AddCircleFilled(at(18, 64), 13 * unit * scale, IM_COL32(33, 163, 232, alpha));
            dl->AddCircleFilled(at(18, 64), 11 * unit * scale, IM_COL32(28, 132, 205, alpha));
            char mult[32];
            snprintf(mult, sizeof(mult), "x%.3g", s.multiplier);
            float width = skateFont->CalcTextSizeA(10 * unit * scale, FLT_MAX, 0, mult).x / (unit * scale);
            text(mult, 18 - width / 2, 58, 10, white);
            text("TRICK: " + ScoreText(Points(s.current)), 43, 78, 9, cyan);
            text("TOTAL: " + ScoreText(s.line.total), 230, 90, 9, cyan, true);
            text("LINE", 43, 43, 9, cyan);
        }
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Score"), 1))
            text(ScoreText(s.line.Display()), 230, 50, 19, white, true);
        dl->PopClipRect();
    }
};
void InitializeFont() {
    static bool attempted = false;
    if (attempted)
        return;
    attempted = true;
    auto data = std::make_shared<Ship::ResourceInitData>();
    data->Format = RESOURCE_FORMAT_BINARY;
    data->Type = (uint32_t)RESOURCE_TYPE_FONT;
    data->ResourceVersion = 0;
    data->Path = "SkateHarkinian/Fonts/SkateUI.ttf";
    fontResource = std::dynamic_pointer_cast<Ship::Font>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(data->Path, false, data));
    if (fontResource && ValidFontData(fontResource->Data, fontResource->DataSize)) {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        snprintf(cfg.Name, sizeof(cfg.Name), "SkateHarkinian UI");
        skateFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fontResource->Data, (int)fontResource->DataSize, 48,
                                                               &cfg, ImGui::GetIO().Fonts->GetGlyphRangesDefault());
    }
    if (!skateFont) {
        SPDLOG_WARN("[SkateHarkinian] Skate UI font unavailable; previous score font retained");
        return;
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->AddGuiWindow(std::make_shared<ScoreWindow>());
}

static void Text(GfxPrint* p, int x, int y, const std::string& text) {
    GfxPrint_SetPosPx(p, 0, y);
    for (char c : text) {
        Gfx* start = p->dList;
        GfxPrint_Printf(p, "%c", c);
        for (Gfx* q = start; q + 2 < p->dList; ++q)
            if ((q->words.w0 >> 24) == G_TEXRECT) {
                auto a = q->words.w0, b = q->words.w1, st = (q + 1)->words.w1, dt = (q + 2)->words.w1;
                Gfx* out = q;
                gSPWideTextureRectangle(out++, ((b >> 12) & 4095) + x * 4, b & 4095, ((a >> 12) & 4095) + x * 4,
                                        a & 4095, (b >> 24) & 7, st >> 16, st & 65535, dt >> 16, dt & 65535);
                q += 2;
            }
    }
}
// HUD units use OoT's 320x240 logical viewport; widescreen left-edge mapping
// keeps the cluster anchored independently of output pixel resolution.
constexpr int kAnchorLeft = 48, kFeedTop = 116, kBadgeY = 180, kScoreY = 170, kLineY = 194;
static void ScaledText(GfxPrint* p, int x, int y, const std::string& text, float scale, int alpha, int r = 245,
                       int g = 245, int b = 245) {
    auto pass = [&](int xx, int yy, int rr, int gg, int bb) {
        GfxPrint_SetColor(p, rr, gg, bb, alpha);
        GfxPrint_SetPosPx(p, 0, 0);
        for (char c : text) {
            Gfx* start = p->dList;
            GfxPrint_Printf(p, "%c", c);
            for (Gfx* q = start; q + 2 < p->dList; ++q)
                if ((q->words.w0 >> 24) == G_TEXRECT) {
                    auto a = q->words.w0, z = q->words.w1, st = (q + 1)->words.w1, dt = (q + 2)->words.w1;
                    Gfx* out = q;
                    gSPWideTextureRectangle(
                        out++, (int)(((z >> 12) & 4095) * scale) + xx * 4, (int)((z & 4095) * scale) + yy * 4,
                        (int)(((a >> 12) & 4095) * scale) + xx * 4, (int)((a & 4095) * scale) + yy * 4, (z >> 24) & 7,
                        st >> 16, st & 65535, (int)((dt >> 16) / scale), (int)((dt & 65535) / scale));
                    q += 2;
                }
        }
    };
    pass(x + 1, y + 1, 0, 0, 0);
    pass(x, y, r, g, b);
}
static void Badge(Gfx*& gfx, int x, int y, int alpha) {
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    auto disc = [&](int radius, int r, int g, int b) {
        gDPSetPrimColor(gfx++, 0, 0, r, g, b, alpha);
        for (int yy = -radius; yy <= radius; ++yy) {
            int width = (int)std::sqrt(float(radius * radius - yy * yy));
            gSPWideTextureRectangle(gfx++, (x - width) * 4, (y + yy) * 4, (x + width + 1) * 4, (y + yy + 1) * 4, 0, 0,
                                    0, 0, 0);
        }
    };
    disc(18, 13, 57, 104);
    disc(16, 34, 131, 205);
    disc(13, 33, 163, 232);
    disc(11, 28, 132, 205);
    gDPPipeSync(gfx++);
}
// One affine transform scales the entire style cluster, including rings,
// shadows, text sampling and spatial animation, around its existing anchor.
constexpr float kScoreHudBaseScale = 0.625f;
static void ScaleCluster(Gfx* begin, Gfx* end, int anchorX, int anchorY, float scale) {
    auto signed24 = [](uint32_t v) { return (int32_t)(v << 8) >> 8; };
    auto point = [&](int v, int anchor) { return (int)std::lround(anchor * 4 + (v - anchor * 4) * scale); };
    for (Gfx* q = begin; q + 2 < end; ++q)
        if ((q->words.w0 >> 24) == G_TEXRECT_WIDE) {
            int xh = signed24(q->words.w0 & 0xffffff), yh = signed24(q->words.w1 & 0xffffff);
            int xl = signed24((q + 1)->words.w0 & 0xffffff), yl = signed24((q + 1)->words.w1 & 0xffffff);
            q->words.w0 = (q->words.w0 & 0xff000000) | (point(xh, anchorX) & 0xffffff);
            q->words.w1 = point(yh, anchorY) & 0xffffff;
            (q + 1)->words.w0 = ((q + 1)->words.w0 & 0xff000000) | (point(xl, anchorX) & 0xffffff);
            (q + 1)->words.w1 = point(yl, anchorY) & 0xffffff;
            auto rates = (q + 2)->words.w1;
            int dx = (int16_t)(rates >> 16), dy = (int16_t)(rates & 65535);
            (q + 2)->words.w1 = ((uint32_t)(uint16_t)std::lround(dx / scale) << 16) | (uint16_t)std::lround(dy / scale);
            q += 2;
        }
}
void Draw(PlayState* play, const State& s) {
    liveScore = &s;
    if (skateFont)
        return;
    if (!play || !CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Enabled"), 1))
        return;
    auto* player = GET_PLAYER(play);
    if (!player || play->pauseCtx.state || play->gameOverCtx.state || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->msgCtx.msgMode || Player_InCsMode(play) || GameInteractor_NoUIActive() ||
        play->interfaceCtx.healthAlpha == 0 ||
        (player->stateFlags1 &
         (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_CUTSCENE)))
        return;
    const int alpha = s.Alpha();
    if (!alpha)
        return;
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    Gfx* opa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    Gfx* clusterBegin = gfx;
    const bool style = CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.SkateStyleScoreHUD"), 1);
    const int left = (int)OTRGetDimensionFromLeftEdge(kAnchorLeft) +
                     std::clamp(CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0), 0, 40);
    if (style && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Combo"), 1))
        Badge(gfx, left + 18, kBadgeY, alpha);
    GfxPrint p;
    GfxPrint_Init(&p);
    GfxPrint_Open(&p, gfx);
    if (style) {
        const float available =
            std::min(190.f, std::max(64.f, OTRGetDimensionFromRightEdge(SCREEN_WIDTH - 12.f) - left - 43.f));
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.TrickFeed"), 1)) {
            int row = 0;
            const int count = (int)s.history.size();
            for (const auto& name : s.history) {
                const bool newest = row == count - 1;
                int opacity = alpha * (newest ? 255 : 100 + row * 35) / 255;
                ScaledText(&p, left,
                           kFeedTop + (4 - count + row) * 11 +
                               (int)(11.f * std::max(0.f, 1.f - float(s.tick - s.historyTick) / 8.f)),
                           name.substr(0, std::max(8, (int)((available + 43.f) / (8.f * (newest ? 1.15f : 1.f))))),
                           newest ? 1.15f : 1.f, opacity);
                ++row;
            }
        }
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Combo"), 1)) {
            char multiplierText[32];
            std::snprintf(multiplierText, sizeof(multiplierText), "x%.3g", s.multiplier);
            std::string mult = multiplierText;
            float badgeScale = std::min(1.f, 32.f / (mult.size() * 8.f));
            ScaledText(&p, left + 18 - (int)(mult.size() * 4 * badgeScale), kBadgeY - 4, mult, badgeScale, alpha);
            auto text = "LINE: " + ScoreText(s.line.Display(s.combo));
            float scale = std::min(1.f, available / (text.size() * 8.f));
            ScaledText(&p, left + 43, kLineY, text, scale, alpha, 95, 188, 235);
        }
        if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Score"), 1)) {
            auto value = ScoreText(s.line.Display());
            float scale = std::min(1.85f + .12f * std::max(0.f, 1.f - float(s.tick - s.scorePopTick) / 8.f),
                                   available / (value.size() * 8.f));
            ScaledText(&p, left + 43, kScoreY, value, scale, alpha);
        }
        gfx = GfxPrint_Close(&p);
        GfxPrint_Destroy(&p);
        float hudScale = CVarGetFloat(CVAR_ENHANCEMENT("NativeSkate.ScoreHudScale"), kScoreHudBaseScale);
        if (!std::isfinite(hudScale))
            hudScale = kScoreHudBaseScale;
        ScaleCluster(clusterBegin, gfx, left, kFeedTop, std::clamp(hudScale, .5f, 1.f));
        gSPEndDisplayList(gfx++);
        Graph_BranchDlist(opa, gfx);
        POLY_OPA_DISP = gfx;
        return;
    }
    int right = (int)OTRGetDimensionFromRightEdge(SCREEN_WIDTH - 16.f) -
                std::clamp(CVarGetInteger(CVAR_COSMETIC("HUD.Margin.R"), 0), 0, 40);
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.PosY"), 0) < 0)
        right -= 110;
    const int y = std::clamp(96 + CVarGetInteger(CVAR_COSMETIC("HUD.Minimap.PosY"), 0), 90, 108);
    auto line = [&](const std::string& text, int yy) {
        int x = std::max(8, right - (int)text.size() * 8);
        GfxPrint_SetColor(&p, 0, 0, 0, alpha);
        Text(&p, x + 1, yy + 1, text);
        GfxPrint_SetColor(&p, 245, 245, 235, alpha);
        Text(&p, x, yy, text);
    };
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.TrickFeed"), 1)) {
        std::string label = s.label;
        size_t split = label.size() > 25 ? label.rfind(' ', 25) : std::string::npos;
        if (split != std::string::npos) {
            line(label.substr(0, split), y);
            line(label.substr(split + 1, 25), y + 10);
        } else
            line(label.substr(0, 25), y);
    }
    char buffer[64];
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Score"), 1)) {
        line(ScoreText(s.line.Display()), y + 22);
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.HUD.Combo"), 1) && (s.line.active || s.line.completed)) {
        std::snprintf(buffer, sizeof(buffer), "x%.1f", s.multiplier);
        line(std::string(buffer) + " LINE " + ScoreText(s.line.Display(s.combo)), y + 34);
    }
    if (!s.notice.empty() && s.tick - s.noticeTick < 90)
        line(s.notice, y + 44);
    gfx = GfxPrint_Close(&p);
    GfxPrint_Destroy(&p);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
}
} // namespace NativeSkateHud
