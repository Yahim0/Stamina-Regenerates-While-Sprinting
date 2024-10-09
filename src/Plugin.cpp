using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

#include "Plugin.h"

namespace plugin {
    std::optional<std::filesystem::path> getLogDirectory() {
        using namespace std::filesystem;
        PWSTR buf;
        SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &buf);
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> documentsPath{buf, CoTaskMemFree};
        path directory{documentsPath.get()};
        directory.append("My Games"sv);

        if (exists("steam_api64.dll"sv)) {
            if (exists("openvr_api.dll") || exists("Data/SkyrimVR.esm")) {
                directory.append("Skyrim VR"sv);
            } else {
                directory.append("Skyrim Special Edition"sv);
            }
        } else if (exists("Galaxy64.dll"sv)) {
            directory.append("Skyrim Special Edition GOG"sv);
        } else if (exists("eossdk-win64-shipping.dll"sv)) {
            directory.append("Skyrim Special Edition EPIC"sv);
        } else {
            return current_path().append("skselogs");
        }
        return directory.append("SKSE"sv).make_preferred();
    }

    void initializeLogging() {
        auto path = getLogDirectory();
        if (!path) {
            report_and_fail("Can't find SKSE log directory");
        }
        *path /= std::format("{}.log"sv, Plugin::Name);

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern(PLUGIN_LOGPATTERN_DEFAULT);
    }
}  // namespace plugin

using namespace plugin;

class SprintRegenHook {
    static inline float getRate(RE::Actor* a) {
        float totalRate;
        float rate =
            a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRate) * 0.01f;  // use the formula from regen_stamina_withwait
        if (rate <= 0.0f) {
            totalRate = 0.0f;
        } else {
            totalRate = rate * a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        }
        if (totalRate > 0.0f) {
            if (a->IsInCombat()){
                totalRate *= RE::GameSettingCollection::GetSingleton()->GetSetting("fCombatStaminaRegenRateMult")->GetFloat();
            }
            totalRate *= a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRateMult) * 0.01f;
        }
        return totalRate;
    }

    static float getEquippedWeight(RE::Actor* a) {
        if (a == nullptr) {
            return 0;
        }
        float w = oldGetEquippedWeight(a);
        float drain = oldGetSprintStaminaDrain(w, RE::GetSecondsSinceLastFrame());
        float rate = getRate(a) * RE::GetSecondsSinceLastFrame();
        return std::max(0.0f, -rate + drain);
    }

    static float getEquippedWeightBandB(RE::Actor* a) {
        if (a == nullptr) {
            return 0;
        }
        float t = RE::GetSecondsSinceLastFrame();
        float rate = getRate(a) * t;
        return std::max(-8.0f * t, -rate);
    }

    static float getSprintStaminaDrain(float weight, float) {
        return weight;
    }

    static inline REL::Relocation<decltype(getEquippedWeight)> oldGetEquippedWeight;
    static inline REL::Relocation<decltype(getSprintStaminaDrain)> oldGetSprintStaminaDrain;

public:
    static void hook() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(38022).address() + 0xc1, getEquippedWeight);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(38022).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookBandB() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(38022).address() + 0xc1, getEquippedWeightBandB);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(38022).address() + 0xc9, getSprintStaminaDrain);
    }
};

extern "C" DLLEXPORT bool SKSEPlugin_Load(const LoadInterface* skse) {
    initializeLogging();

    logger::info("'{} {}' is loading, game version '{}'...", Plugin::Name, Plugin::VersionString, REL::Module::get().version().string());
    Init(skse);
    SKSE::AllocTrampoline(1 << 10);
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("BladeAndBlunt.esp") != nullptr) {
                logger::info("BladeAndBlunt.esp detected.");
                SprintRegenHook::hookBandB();
            } else {
                SprintRegenHook::hook();
            }
        }
    });

    logger::info("{} has finished loading.", Plugin::Name);
    return true;
}

