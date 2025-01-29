using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

#include "Plugin.h"
#include "Config.h"

namespace OblivionSprint {
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

using namespace OblivionSprint;

class OblivionSprintHook {
    static inline float getRate(RE::Actor* a) {
        float totalRate;
        float rate =
            a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRate) * 0.01f;  // use the formula from regen_stamina_withwait
        if (rate <= 0.0f) {
            totalRate = 0.0f;
        } else {
            totalRate = rate * a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        }
        if (totalRate > 0) {
            if (a->IsInCombat()){
                totalRate *= RE::GameSettingCollection::GetSingleton()->GetSetting("fCombatStaminaRegenRateMult")->GetFloat();
            }
            totalRate *= a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRateMult) * 0.01f;
        }
        return totalRate;
    }

    static float getEquippedWeightRegen(RE::Actor* actor) {
        if (actor == nullptr) {
            return 0;
        }
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        float rate = getRate(actor) * RE::GetSecondsSinceLastFrame();
        logger::info("rate: {}, regenMult: {}, rateMult: {}, drain: {}, wholeDrain: {}", rate, Config::regenMultiplier, rate * Config::regenMultiplier, drain, -rate * Config::regenMultiplier + drain);
        return -rate * Config::regenMultiplier + drain;
    }

    static float getEquippedWeightCompensates(RE::Actor* actor) {
        if (actor == nullptr) {
            return 0;
        }
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        float rate = getRate(actor) * RE::GetSecondsSinceLastFrame();
        return std::max(0.0f, -rate * Config::regenMultiplier + drain);
    }

    static float getEquippedWeightCompBandB(RE::Actor* actor) {
        if (actor == nullptr) {
            return 0;
        }
        float drain = 8.0f * RE::GetSecondsSinceLastFrame();
        float rate = getRate(actor) * RE::GetSecondsSinceLastFrame();
        return std::max(0.0f, -rate * Config::regenMultiplier + drain);
    }

    static float getEquippedWeightCompImperious(RE::Actor* a) {
        if (a == nullptr) {
            return 0;
        }
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(a), RE::GetSecondsSinceLastFrame());
        if (strcmp(RE::PlayerCharacter::GetSingleton()->GetRace()->GetFullName(), "Kahjiit")) {
            drain = drain + 15;
        }
        float rate = getRate(a) * RE::GetSecondsSinceLastFrame();
        return std::max(0.0f, -rate + drain);
    }


    static float getSprintStaminaDrain(float weight, float) {
        return weight;
    }

    static inline REL::Relocation<decltype(getEquippedWeightRegen)> oldGetEquippedWeight;
    static inline REL::Relocation<decltype(getSprintStaminaDrain)> oldGetSprintStaminaDrain;

public:
    static void hookRegen() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightRegen);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookCompensates() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightCompensates);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookCompBandB() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightCompBandB);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookCompImperious() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightCompImperious);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
};

extern "C" DLLEXPORT bool SKSEPlugin_Load(const LoadInterface* skse) {
    initializeLogging();
    logger::info("'{} {}' is loading, game version '{}'...", Plugin::Name, Plugin::VersionString, REL::Module::get().version().string());
    Init(skse);
    SKSE::AllocTrampoline(1 << 10);
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            Config::loadConfig();
            if (Config::staminaRegenWhileSprint) {
                OblivionSprintHook::hookRegen();
            }
            else {
                if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("BladeAndBlunt.esp")) {
                    logger::info("BladeAndBlunt.esp detected.");
                    OblivionSprintHook::hookCompBandB();
                }
                else if (RE::TESDataHandler::GetSingleton()->LookupLoadedModByName("Imperious - Races of Skyrim.esp")) {
                    logger::info("Imperious - Races of Skyrim.esp detected.");
                    OblivionSprintHook::hookCompImperious();
                }
                else {
                    OblivionSprintHook::hookCompensates();
                }
                
            }
        }
    });

    logger::info("{} has finished loading.", Plugin::Name);
    return true;
}

