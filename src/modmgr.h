
#pragma once

#include "prochelper.h"

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
    // unique human-readable-ish name for the tool. ie skse
    std::string execName;
    // the actual path to execute it. ie /usr/bin/wine
    std::string execPath;
    // ie ~/.wine/GOG Games/Skyrim Anniversary Edition/skse64_launcher.exe
    std::vector<std::string> args;
    bool updatePluginList = false;
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

struct CustomVariable
{
    std::string name;
    std::string value;
};

struct ModMgrInst
{
    static constexpr int VERSION = 1;

    int version = 0;
    std::vector<ModInfo> mods;
    std::vector<ModExec> customExec;
    std::vector<ModExec> builtinExec;
    std::vector<ModPlugin> pluginList;
    std::vector<CustomVariable> customVariables;
};

struct ModMgr
{
    ModMgrConfig config;
    ModMgrInst inst;

    // current process running inside the overlay
    int procId = -1;

    bool verbose = false;

    ModInfo newMod;
    CustomVariable newVariable;
    ModExec newExec;

    //////// ImGui state cache ////////

    std::string currentExec;

    bool addingExec = false;
    bool modifyingExec = false;
    bool selectingExec = false;
    bool enableRemove = false;
    int sortMode = 0;
    bool makingNewMod = false;
    bool settingsOpen = false;
    bool foundSkyrimExe = false;
    bool foundSkyrimIni = false;

};

struct ExecToolProgram : public ProcInvoke
{
    std::vector<std::string> args;
    virtual void invoke() override
    {
        ExecThisProcessUnshared(args);
    }
};

void RenderModMgr(ModMgr& mgr);

void SaveModMgr(ModMgr& mgr);

bool LoadModMgr(ModMgr& mgr, std::string const& filePath, bool createNew);


bool SetupPreMountEnv();
bool InvokeTool(ModMgr& mgr, std::string const & toolName);
bool InvokeProcess(ModMgr& mgr, std::vector<std::string> & args);

bool WritePluginsTxt(ModMgr& mgr, std::filesystem::path const & path);

std::optional<std::string> ReplaceEnvVariables(ModMgr& mgr, std::string const & in, bool failOnUnknownVariable);

#include "modmgr_json.h"

