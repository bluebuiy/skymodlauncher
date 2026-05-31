
#pragma once

#include "prochelper.h"
#include "fomod_ui.h"
#include "curlasync.h"
#include "nxmurl.h"
#include "modmgr_collection.h"

#include <vector>
#include <string>
#include <filesystem>
#include <curl/curl.h>


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

    std::string nexusApiKey;
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

struct ModDownload
{
    std::string fileName;
    int modId;
    int fileId;
    std::string game;
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
    std::vector<ModDownload> downloads;
};

enum class ModDlState
{
    None = 0,
    UrlQuery,
    ModDownload,
    ModPaused,
    Error,
    Complete,
    Canceled,
};

struct ModDownloadRt
{
    CurlTask task;
    
    std::filesystem::path outFile;
    std::string game;
    std::string fileName;
    int modId;
    int fileId;
    std::string expires;
    std::string key;
    //std::string userId;
    ModDlState state = ModDlState::None;

    // imgui action cache
    bool remove = false;
    bool cancel = false;
    bool pause = false;
    bool unpause = false;
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

    NxmCollection collection;

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

    std::optional<FomodUI> fomodState;

    // 

    int urlPipe = -1;

    std::vector<ModDownloadRt> downloadSessions;

    // TODO switch to weak ptr for external refs?
    std::shared_ptr<AsyncTaskProcessor<CurlAsyncEngine>> curlEngine;

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

void LaunchExec(ModMgr& mgr, std::string const & execName);
void DiscoverPlugins(ModMgr& mgr);
std::optional<std::string> DetectAppDataLocal(ModMgr& mgr);

void CheckNXMAction(ModMgr& mgr);
void SetupNXMActionPipe(ModMgr& mgr);
void CleanupNXMAction(ModMgr& mgr);
void HandleNXMUrl(ModMgr& mgr, std::string const & urlStr);
void StartNXMModDownload(ModMgr& mgr, NxmModFileUrl const & url);
void StartNXMCollectionInstall(ModMgr& mgr, NxmCollectionUrl const & url);

void UpdateDownloads(ModMgr& mgr);

void DeleteMod(ModMgr& mgr, std::string & modFile);

void InstallDownloadedFile(ModMgr& mgr, std::string const & modName);

void InitMgr(ModMgr& mgr);
void CleanupMgr(ModMgr& mgr);


#include "modmgr_json.h"

