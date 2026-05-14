
#pragma once

#include <vector>
#include <string>
#include <filesystem>

struct ModMgrConfig
{
    static constexpr int VERSION = 1;

    int version = 0;

    // folder with the exe
    std::string installRoot;

    // folder with skyrim.ini etc
    std::string mgRoot;

    // app data directory path, with plugins.txt
    std::string appData;

    // folder with the mod installations
    std::string modFolder;

    // config file
    std::string configPath;

    // instance file
    std::string instPath;

    // not saved - instance project dir
    std::filesystem::path projectDir;
};

struct ModExec
{
    std::string execName;
    std::string args;
};

struct ModInfo
{
    bool enabled = false;
    int loadIndex = -1;
    std::string modFile;
    std::vector<std::string> plugins;
};

struct ModPlugin
{
    std::string pluginName;
    bool enabled;
};

struct ModMgrInst
{
    static constexpr int VERSION = 1;

    int version = 0;
    std::vector<ModInfo> mods;
    std::vector<ModExec> customExec;
    std::vector<ModPlugin> pluginList;
};

struct ModMgr
{
    ModMgrConfig config;
    ModMgrInst inst;

    // current process running inside the overlay
    int procId = -1;

    bool verbose = false;

    ModInfo newMod;

    //////// ImGui state cache ////////

    bool enableRemove = false;
    int sortMode = 0;
    bool makingNewMod = false;
    bool settingsOpen = false;
    bool foundSkyrimExe = false;
    bool foundSkyrimIni = false;

};


void RenderModMgr(ModMgr& mgr);

void SaveModMgr(ModMgr& mgr);

bool LoadModMgr(ModMgr& mgr, std::string const& filePath);

#include "modmgr_json.h"

