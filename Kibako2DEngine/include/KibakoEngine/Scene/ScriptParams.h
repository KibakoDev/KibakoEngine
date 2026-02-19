#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include "KibakoEngine/Scene/Scene2D.h" // for ScriptValue alias

namespace AstroVoid::ScriptParams {

    using Params = std::unordered_map<std::string, KibakoEngine::ScriptValue>;

    inline float GetFloat(const Params& p, const char* key, float def)
    {
        if (!key) return def;
        auto it = p.find(key);
        if (it == p.end()) return def;

        const auto& v = it->second;
        if (const float* f = std::get_if<float>(&v)) return *f;
        if (const int* i = std::get_if<int>(&v)) return static_cast<float>(*i);
        if (const bool* b = std::get_if<bool>(&v)) return *b ? 1.0f : 0.0f;
        return def;
    }

    inline int GetInt(const Params& p, const char* key, int def)
    {
        if (!key) return def;
        auto it = p.find(key);
        if (it == p.end()) return def;

        const auto& v = it->second;
        if (const int* i = std::get_if<int>(&v)) return *i;
        if (const float* f = std::get_if<float>(&v)) return static_cast<int>(*f);
        if (const bool* b = std::get_if<bool>(&v)) return *b ? 1 : 0;
        return def;
    }

    inline bool GetBool(const Params& p, const char* key, bool def)
    {
        if (!key) return def;
        auto it = p.find(key);
        if (it == p.end()) return def;

        const auto& v = it->second;
        if (const bool* b = std::get_if<bool>(&v)) return *b;
        if (const int* i = std::get_if<int>(&v)) return (*i != 0);
        if (const float* f = std::get_if<float>(&v)) return (*f != 0.0f);
        return def;
    }

    inline std::string GetString(const Params& p, const char* key, const std::string& def = {})
    {
        if (!key) return def;
        auto it = p.find(key);
        if (it == p.end()) return def;

        const auto& v = it->second;
        if (const std::string* s = std::get_if<std::string>(&v)) return *s;
        return def;
    }

} // namespace AstroVoid::ScriptParams