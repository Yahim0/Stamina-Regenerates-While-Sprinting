#include "Config.h"
#include "SimpleIni.h"

namespace OblivionSprint {
    void Config::loadConfig() noexcept
    {
        logger::info("Loading config");

        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(R"(.\Data\SKSE\Plugins\OblivionSprint.ini)") != 0) {
            logger::error("Failed to Load config file.");
            return;
        }

        staminaRegenWhileSprint = ini.GetBoolValue("General", "staminaRegenWhileSprint");
        regenMultiplier = (float) ini.GetDoubleValue("General", "regenMultiplier");

        logger::info("Loaded config");
        logger::info("");
    }
}