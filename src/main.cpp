#include <windows.h>
#include <string>
#include "logger.h"
#include "config.h"
#include "features/features.h"

static HMODULE g_hModule = NULL;

DWORD WINAPI MainThread(LPVOID lpParam) {
    // Determine path of the DLL to locate the INI file alongside it
    char dllPath[MAX_PATH];
    GetModuleFileNameA(g_hModule, dllPath, MAX_PATH);

    std::string pathStr(dllPath);
    size_t lastSlash = pathStr.find_last_of("\\/");
    std::string baseDir = (lastSlash != std::string::npos) ? pathStr.substr(0, lastSlash + 1) : "";

    std::string iniPath = baseDir + "NFSTR_DefinitiveEdition.ini";
    std::string logPath = baseDir + "NFSTR_DefinitiveEdition.log";

    // 1. Load Configuration
    Config::Load(iniPath);

    // 2. Initialize Logger, then log the config (LogSummary must run after Init,
    //    or its output is dropped because the logger isn't ready during Load).
    Logger::Init(logPath, g_Config.DebugLog != 0);
    Logger::Log("NFSTR_DefinitiveEdition thread started.");
    Config::LogSummary();

    // 3. Initialize Features
    Features::InitGarageCarRender();
    Features::InitExtraUIOptions();
    Features::InitTrackRules();
    Features::InitTrafficControls();
    Features::InitVehicleAssists();
    Features::InitEngineAudioSlewFix();
    Features::InitParticleFix();
    Features::InitGinsuDiagnostics();
    Features::InitFramerateUnlocker();

    Logger::Log("All features initialized successfully.");

    // 4. Background Ticker Loop
    while (true) {
        Features::UpdateFramerateUnlocker();
        Features::UpdateParticleFix();
        Features::UpdateDifficulty();     // decides whether the mode is engaged
        Features::UpdateDifficultyText();
        Features::UpdatePlayerVehicle();  // reads that decision, so it runs after
        Features::UpdateRenderSettings();
        Features::UpdateSettingsProbe();
        Sleep(16); // ~60 Hz tick
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        Logger::Close();
        break;
    }
    return TRUE;
}
