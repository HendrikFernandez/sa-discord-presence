#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "plugin_sa.h"
#include "CGame.h"
#include "CMenuManager.h"
#include "CPlayerInfo.h"
#include "CRunningScript.h"
#include "CStats.h"
#include "CTheScripts.h"
#include "CTheZones.h"
#include "CTimer.h"
#include "CWorld.h"
#include "common.h"
#include "discord_rpc.h"

namespace {

constexpr DWORD kUpdateIntervalMs = 5000;
constexpr size_t kDiscordFieldMax = 127;

std::string module_directory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&module_directory), &module)) {
        return ".\\";
    }
    char path[MAX_PATH]{};
    if (!GetModuleFileNameA(module, path, MAX_PATH)) {
        return ".\\";
    }
    std::string result(path);
    const auto slash = result.find_last_of("\\/");
    return slash == std::string::npos ? ".\\" : result.substr(0, slash + 1);
}

std::string config_value(const std::string& path, const char* section,
                         const char* key, const char* fallback = "") {
    char value[512]{};
    GetPrivateProfileStringA(section, key, fallback, value, sizeof(value), path.c_str());
    return value;
}

std::string game_text_to_utf8(const char* text) {
    if (!text || !*text) return {};
    // Classic PC GXT strings are byte strings. Convert the current Windows ANSI
    // code page to UTF-8 before passing text to Discord.
    const int wide_size = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (wide_size <= 0) return {};
    std::wstring wide(static_cast<size_t>(wide_size), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &wide[0], wide_size);
    const int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                                              nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) return {};
    std::string utf8(static_cast<size_t>(utf8_size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8_size,
                        nullptr, nullptr);
    utf8.pop_back(); // remove the converted NUL
    return utf8;
}

std::string discord_field(std::string text) {
    // Remove game formatting codes (~r~, ~h~, etc.) from GXT strings.
    std::string clean;
    clean.reserve(text.size());
    bool in_format = false;
    for (const unsigned char c : text) {
        if (c == '~') {
            in_format = !in_format;
        } else if (!in_format && c >= 32) {
            clean.push_back(static_cast<char>(c));
        }
    }
    if (clean.size() <= kDiscordFieldMax) return clean;
    size_t end = kDiscordFieldMax - 3;
    while (end > 0 && (static_cast<unsigned char>(clean[end]) & 0xC0) == 0x80) --end;
    clean.resize(end);
    clean += "...";
    return clean;
}

std::string script_name(const CRunningScript* script) {
    size_t length = 0;
    while (length < sizeof(script->m_szName) && script->m_szName[length]) ++length;
    std::string name(script->m_szName, length);
    for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return name;
}

std::string mission_title(const std::string& missions_path) {
    // The active list is owned by the game. Bound traversal to its 96 script slots.
    const CRunningScript* script = CTheScripts::pActiveScripts;
    for (int i = 0; script && i < 96; ++i, script = script->m_pNext) {
        if (!script->m_bIsMission) continue;
        const std::string code = script_name(script);
        if (code.empty()) return "Unknown mission";
        return config_value(missions_path, "Missions", code.c_str(), code.c_str());
    }
    return {};
}

std::string area_name(const CVector& position) {
    if (CGame::currArea != 0) return "Interior";
    CZone* zone = CTheZones::FindSmallestZoneForPosition(position, true);
    if (zone) {
        const std::string name = game_text_to_utf8(zone->GetTranslatedName());
        if (!name.empty()) return name;
    }
    switch (CTheZones::GetLevelFromPosition(&position)) {
        case LEVEL_NAME_LOS_SANTOS: return "Los Santos";
        case LEVEL_NAME_SAN_FIERRO: return "San Fierro";
        case LEVEL_NAME_LAS_VENTURAS: return "Las Venturas";
        default: return "San Andreas";
    }
}

std::string money_text(int money) {
    const bool negative = money < 0;
    uint64_t amount = negative ? static_cast<uint64_t>(-(static_cast<int64_t>(money)))
                               : static_cast<uint64_t>(money);
    std::string digits = std::to_string(amount);
    for (size_t i = digits.size(); i > 3; i -= 3) digits.insert(i - 3, ",");
    return std::string(negative ? "-$" : "$") + digits;
}

struct Presence {
    std::string details;
    std::string state;
};

Presence sample_game(const std::string& missions_path) {
    CPlayerPed* ped = FindPlayerPed();
    if (!ped) return {"Main menu", "San Andreas"};

    const CPlayerInfo& player = CWorld::Players[CWorld::PlayerInFocus];
    std::string status;
    if (player.m_nPlayerState == PLAYERSTATE_HASDIED) {
        status = "Wasted";
    } else if (player.m_nPlayerState == PLAYERSTATE_HASBEENARRESTED) {
        status = "Busted";
    } else if (FrontEndMenuManager.m_bMenuActive || CTimer::m_UserPause) {
        status = "Paused";
    } else {
        const std::string mission = mission_title(missions_path);
        if (!mission.empty()) {
            status = "Mission: " + mission;
        } else {
            status = CTheScripts::IsPlayerOnAMission() ? "In a mission" : "Free roam";
        }
    }

    float progress = CStats::GetPercentageProgress();
    if (!std::isfinite(progress)) progress = 0.0f;
    progress = std::clamp(progress, 0.0f, 100.0f);
    char percent[16]{};
    std::snprintf(percent, sizeof(percent), "%.1f%%", progress);

    const std::string area = area_name(ped->GetPosition());
    return {discord_field(status),
            discord_field(area + " | " + percent + " complete | " + money_text(player.m_nMoney))};
}

class SaDiscordPresence {
public:
    SaDiscordPresence() {
        if (!plugin::ValidateGameVersion(false)) return;
        const std::string directory = module_directory();
        missions_path_ = directory + "SaDiscordPresence.missions.ini";
        const std::string config_path = directory + "SaDiscordPresence.ini";
        app_id_ = config_value(config_path, "Discord", "ApplicationId");
        image_key_ = config_value(config_path, "Discord", "LargeImageKey");
        if (app_id_.size() < 17 || app_id_.size() > 20 ||
            !std::all_of(app_id_.begin(), app_id_.end(),
                         [](unsigned char c) { return std::isdigit(c) != 0; })) {
            OutputDebugStringA("SaDiscordPresence: set a valid ApplicationId in SaDiscordPresence.ini\n");
            return;
        }
        plugin::Events::gameProcessEvent += [this] { update(); };
        plugin::Events::shutdownRwEvent += [this] { shutdown(); };
    }

private:
    void update() {
        if (!initialized_) {
            Discord_Initialize(app_id_.c_str(), nullptr, 0, nullptr);
            initialized_ = true;
        }
        const DWORD now = GetTickCount();
        if (sent_ && now - last_update_ < kUpdateIntervalMs) return;
        last_update_ = now;
        const Presence next = sample_game(missions_path_);
        if (sent_ && next.details == previous_.details && next.state == previous_.state) return;
        previous_ = next;
        DiscordRichPresence rpc{};
        rpc.details = previous_.details.c_str();
        rpc.state = previous_.state.c_str();
        rpc.startTimestamp = start_timestamp_;
        rpc.largeImageKey = image_key_.empty() ? nullptr : image_key_.c_str();
        rpc.largeImageText = "Grand Theft Auto: San Andreas";
        Discord_UpdatePresence(&rpc);
        sent_ = true;
    }

    void shutdown() {
        if (!initialized_) return;
        Discord_Shutdown();
        initialized_ = false;
        sent_ = false;
    }

    std::string app_id_;
    std::string image_key_;
    std::string missions_path_;
    Presence previous_;
    const int64_t start_timestamp_ = static_cast<int64_t>(std::time(nullptr));
    DWORD last_update_ = 0;
    bool initialized_ = false;
    bool sent_ = false;
};

SaDiscordPresence presence;

} // namespace
