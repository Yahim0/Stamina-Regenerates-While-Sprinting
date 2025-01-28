namespace OblivionSprint {
    class Config {
    public:
        static void loadConfig() noexcept;

        inline static bool staminaRegenWhileSprint;

        inline static float regenMultiplier;
    };
}