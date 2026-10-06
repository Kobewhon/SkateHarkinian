#include "NativeSkateGrindCompiler.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <optional>
namespace NativeSkateGrindCompiler {
static std::optional<nlohmann::json> cachedOverrides;
static std::string cachedPath, parseError;
static bool loaded = false;
void InvalidateOverrides() {
    cachedOverrides.reset();
    loaded = false;
    parseError.clear();
}
Overrides LoadOverrides(const std::string& path, int scene, std::string& error) {
    Overrides out;
    if (!loaded || cachedPath != path) {
        InvalidateOverrides();
        cachedPath = path;
        loaded = true;
        std::ifstream file(path);
        if (file)
            try {
                nlohmann::json j;
                file >> j;
                cachedOverrides = std::move(j);
            } catch (const std::exception& e) {
                parseError = e.what();
            }
    }
    if (!parseError.empty()) {
        error = parseError;
        return out;
    }
    if (!cachedOverrides)
        return out;
    try {
        const auto& j = *cachedOverrides;
        if (j.at("version").get<int>() != 1)
            throw std::runtime_error("Unsupported surface override version");
        for (const auto& e : j.value("edges", nlohmann::json::array())) {
            if (e.at("scene").get<int>() != scene)
                continue;
            auto id = std::stoull(e.at("id").get<std::string>(), nullptr, 16);
            Override o;
            auto action = e.value("action", std::string());
            o.reject = action == "reject";
            o.accept = action == "accept";
            o.split = e.value("split", false);
            o.joinGroup = e.value("joinGroup", 0u);
            auto cat = e.value("category", std::string());
            for (int i = 0; i < 6; ++i)
                if (cat == Name((Category)i))
                    o.category = i;
            out[id] = o;
        }
    } catch (const std::exception& e) {
        out.clear();
        error = e.what();
    }
    return out;
}
} // namespace NativeSkateGrindCompiler
