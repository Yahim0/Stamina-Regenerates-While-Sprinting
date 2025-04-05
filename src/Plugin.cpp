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

    static inline float getRate(RE::Actor* actor) { //Clone from ingame function
        float rate = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRate) * 0.01f;  // Formula from regen_stamina_withwait, refactored
        if (rate <= 0.0f) 
            return 0.0f;
        rate = rate * actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        if (rate > 0) {
            if (actor->IsInCombat()){
                rate *= RE::GameSettingCollection::GetSingleton()->GetSetting("fCombatStaminaRegenRateMult")->GetFloat();
            }
            rate *= actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRateMult) * 0.01f;
        }
        return rate;
    }

    static float getEquippedWeightRegen(RE::Actor* actor) { // Regen mode
        if (actor == nullptr)
            return 0;
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - (rate - drain); // The return value for this function is the 'value to be decreased' so need to return a negative value to increase stamina.
    }

    static float getEquippedWeightMitigate(RE::Actor* actor) { // Mitigate mode
        if (actor == nullptr)
            return 0;
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        float rate = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStaminaRate) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(0.0f, rate - drain); // The return value for this function is the 'value to be decreased' so need to return a negative value to increase stamina.
    }

    // BEGIN NONSENSE
    static float getEquippedWeightMitiBaB(RE::Actor* actor) { 
        if (actor == nullptr)
            return 0;
        float drain = 8.0f * RE::GetSecondsSinceLastFrame();
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(drain, rate); // Cancel BandB sprint draining by regening 8 points per seconds at maximum (4 at maximum if sneaking)
    }

    static float getEquippedWeightMitiBaBAether(RE::Actor* actor) {
        if (actor == nullptr)
            return 0;
        float drain = 8.0f * RE::GetSecondsSinceLastFrame();
        if (strstr(actor->GetRace()->GetFullName(), "Khajiit") != nullptr) {
            drain /= 2;
        }
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(drain, rate);
    }

    static float getEquippedWeightMitiBaBAetherReimag(RE::Actor* actor) {
        if (actor == nullptr)
            return 0;
        float drain = 8.0f * RE::GetSecondsSinceLastFrame();
        if (strstr(actor->GetRace()->GetFullName(), "Khajiit") != nullptr) {
            drain *= 0.75f;
        }
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(drain, rate);
    }

    static float getEquippedWeightMitiBaBImper(RE::Actor* actor) {
        if (actor == nullptr)
            return 0;
        float drain = 8.0f * RE::GetSecondsSinceLastFrame();
        if (strstr(actor->GetRace()->GetFullName(), "Khajiit") != nullptr) {
            drain += 15 * RE::GetSecondsSinceLastFrame();
        }
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(drain, rate);
    }

    static float getEquippedWeightAether(RE::Actor* actor) {
        if (actor == nullptr)
            return 0;
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        if (strstr(actor->GetRace()->GetFullName(), "Khajiit") != nullptr) {
            drain /= 2;
        }
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(0.0f, rate - drain);
    }

    static float getEquippedWeightAetherReimag(RE::Actor* actor) {
        if (actor == nullptr)
            return 0;
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        if (strstr(actor->GetRace()->GetFullName(), "Khajiit") != nullptr) {
            drain *= 0.75f;
        }
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(0.0f, rate - drain);
    }

    static float getEquippedWeightMitiImper(RE::Actor* actor) {
        if (actor == nullptr)
            return 0;
        float drain = oldGetSprintStaminaDrain(oldGetEquippedWeight(actor), RE::GetSecondsSinceLastFrame());
        if (strstr(actor->GetRace()->GetFullName(), "Khajiit") != nullptr) {
            drain += 15 * RE::GetSecondsSinceLastFrame();
        }
        float rate = getRate(actor) * Config::regenMultiplier * RE::GetSecondsSinceLastFrame();
        return - std::min(0.0f, rate - drain);
    }
    // END NONSENSE

    static float getSprintStaminaDrain(float weight, float) {
        return weight;
    }

    static inline REL::Relocation<decltype(getEquippedWeightRegen)> oldGetEquippedWeight;
    static inline REL::Relocation<decltype(getSprintStaminaDrain)> oldGetSprintStaminaDrain;

public:
    static void hookRegen() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + (REL::Module::IsVR() ? 0x125 : 0xc1), getEquippedWeightRegen);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + (REL::Module::IsVR() ? 0x12D :0xc9), getSprintStaminaDrain);
    }
    static void hookMitigate() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightMitigate);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiBaB() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightMitiBaB);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiBaBAether() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightMitiBaBAether);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiBaBAetherReimag() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightMitiBaBAetherReimag);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiBaBImper() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightMitiBaBImper);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiAether() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightAether);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiAetherReimag() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightAetherReimag);
        oldGetSprintStaminaDrain = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc9, getSprintStaminaDrain);
    }
    static void hookMitiImper() {
        oldGetEquippedWeight = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(36994, 38022)).address() + 0xc1, getEquippedWeightMitiImper);
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
                OblivionSprintHook::hookRegen(); // Works OOTB for every mod
            }
            else { // Doesn't
                if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("BladeAndBlunt.esp")) {
                    logger::info("BladeAndBlunt.esp detected.");
                    if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("Aetherius-Reimagined.esp")) {
                        logger::info("Aetherius-Reimagined.esp detected.");
                        OblivionSprintHook::hookMitiBaBAetherReimag();
                    }
                    else if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("Aetherius.esp")) {
                        logger::info("Aetherius.esp detected.");
                        OblivionSprintHook::hookMitiBaBAether();
                    }
                    else if (RE::TESDataHandler::GetSingleton()->LookupLoadedModByName("Imperious - Races of Skyrim.esp")) {
                        logger::info("Imperious - Races of Skyrim.esp detected.");
                        OblivionSprintHook::hookMitiBaBImper();
                    }
                    else {
                        OblivionSprintHook::hookMitiBaB();
                    }
                }
                else {
                    if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("Aetherius-Reimagined.esp")) {
                        logger::info("Aetherius-Reimagined.esp detected.");
                        OblivionSprintHook::hookMitiAetherReimag();
                    }
                    else if (RE::TESDataHandler::GetSingleton()->LookupLoadedLightModByName("Aetherius.esp")) {
                        logger::info("Aetherius.esp detected.");
                        OblivionSprintHook::hookMitiAether();
                    }
                    else if (RE::TESDataHandler::GetSingleton()->LookupLoadedModByName("Imperious - Races of Skyrim.esp")) {
                        logger::info("Imperious - Races of Skyrim.esp detected.");
                        OblivionSprintHook::hookMitiImper();
                    }
                    else {
                        OblivionSprintHook::hookMitigate();
                    }
                }
            }
        }
    });

    logger::info("{} has finished loading.", Plugin::Name);
    return true;
}

