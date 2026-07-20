
#pragma once

#include "prochelper.h"
#include "fomod_ui.h"
#include "curlasync.h"
#include "asyncproc.h"
#include "nxmurl.h"
#include "modmgr_collection.h"
#include "fomodauto.h"
#include "enums.h"
#include "mod.h"
#include "intrusive/dg.h"

#include <vector>
#include <string>
#include <filesystem>
#include <curl/curl.h>
#include <future>

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
    // working directory
    std::string wd;
    // ie ~/.wine/GOG Games/Skyrim Anniversary Edition/skse64_launcher.exe
    std::vector<std::string> args;
    bool updatePluginList = false;
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

struct ModRuleWrapper
{
    // the same data, just one is a->b and one is b<-a
    std::unordered_map<ModId, std::vector<ModId>> aThenB;
    std::unordered_map<ModId, std::vector<ModId>> bBeforeA;

    void GetRulesForMod(ModId m, std::vector<ModId> & before, std::vector<ModId> & after)
    {
        before.clear();
        after.clear();

        auto ai = aThenB.find(m);
        if (ai != aThenB.end())
        {
            after = ai->second;
        }

        auto bi = bBeforeA.find(m);
        if (bi != bBeforeA.end())
        {
            before = bi->second;
        }
    }

    // a then b
    void AddRule(ModId a, ModId b)
    {
        auto it = aThenB.emplace(a, std::vector<ModId>{});
        it.first->second.push_back(b);

        auto it2 = bBeforeA.emplace(b, std::vector<ModId>{});
        it2.first->second.push_back(a);
    }

    // a then b
    void RemoveRule(ModId a, ModId b)
    {
        auto it = aThenB.find(a);
        if (it != aThenB.end())
        {
            auto rmi = std::remove(it->second.begin(), it->second.end(), b);
            if (rmi != it->second.end())
            {
                it->second.erase(rmi);
            }
        }

        auto it2 = bBeforeA.find(b);
        if (it2 != bBeforeA.end())
        {
            auto rmi = std::remove(it2->second.begin(), it2->second.end(), a);
            if (rmi != it2->second.end())
            {
                it2->second.erase(rmi);
            }
        }

        if (it != aThenB.end() && it->second.empty())
        {
            aThenB.erase(it);
        }
        if (it2 != bBeforeA.end() && it2->second.empty())
        {
            bBeforeA.erase(it2);
        }
    }

};

struct ModMgrInst
{
    static constexpr int VERSION = 2;

    int version = 0;
    std::vector<ModExec> customExec;
    std::vector<ModExec> builtinExec;
    std::vector<ModPlugin> pluginList;
    std::vector<CustomVariable> customVariables;
    std::vector<NxmCollectionUrl> collections;

    std::unordered_map<ModId, ModManifest> modFileManifests;
    std::unordered_map<ModInstallId, ModInstall> modInstalls;
    //std::unordered_map<ModId, ModDownload> modFileDownloads;
    //std::unordered_map<NxmCollectionUrl, NxmCollection> collections;
    std::optional<NxmCollection> collection;

    std::vector<ModDownload> downloads;

    ModRuleWrapper modRules;

    std::vector<ModLoadRule> modRulesRaw;
    std::vector<PluginLoadRule> pluginRulesRaw;

    int idCounter = 0;
};

struct ModDownloadRt
{
    CurlTask task;
    
    std::filesystem::path outFile;
    std::string game;
    std::string fileName;
    std::string expires;
    std::string key;
    //std::string userId;
    ModDlState state = ModDlState::None;
    
    ModId id;

    // imgui action cache
    bool remove = false;
    bool cancel = false;
    bool pause = false;
    bool unpause = false;
};

struct NewModInfo
{

};

struct ModMgr
{
    ModMgrConfig config;
    ModMgrInst inst;

    bool verbose = false;

    CustomVariable newVariable;
    ModExec newExec;

    //////// ImGui state cache ////////

    std::string currentExec;

    bool openTestUi = false;

    bool addingExec = false;
    bool modifyingExec = false;
    bool selectingExec = false;
    bool enableRemove = false;
    int sortMode = 0;
    bool enableSetOk = false;
    bool modifyingManifest = false;
    ModId modifiedManifest;
    bool settingsOpen = false;
    bool foundSkyrimExe = false;
    bool foundSkyrimIni = false;
    bool openCollectionInput = false;
    bool openCustomModRules = false;
    bool openCustomPluginRules = false;
    std::string modSearch;
    std::string dlSearch;
    std::string sharedModSearch;
    bool filterModRuleRelatives = false;
    bool filterModRuleFloating = false;
    ModId modRuleSelected;

    bool modListType = false;

    ModManifest newMod;

    NxmCollectionUrl inputCollection;

    std::optional<FomodUI> fomodState;
    std::optional<ProcessTask> cookingInstall;

    // 

    int urlPipe = -1;

    std::vector<ModDownloadRt> downloadSessions;

    // TODO switch to weak ptr for external refs?
    std::shared_ptr<AsyncTaskProcessor<CurlAsyncEngine>> curlEngine;
    std::shared_ptr<AsyncTaskProcessor<AsyncProcessEngine>> processEngine;

};

struct ModDepNode
{
    intrusive::dg_inject<ModDepNode> dg;
    ModInstallId mod;
    std::string name;
};

std::vector<ModId> GetModList(ModMgr& mgr);
std::optional<ModManifest> GetModManifest(ModMgr& mgr, ModId id);
ModId CreateModManifest(ModMgr& mgr, ModManifest const & mft);
ModId FindModManifest(ModMgr& mgr, ModManifest const & mft);
//ModDownload StartModDownload(ModMgr& mgr, ModId id);
std::vector<ModInstallId> GetModInstalls(ModMgr& mgr);
std::optional<ModInstall> GetModInstall(ModMgr& mgr, ModInstallId);
//ModInstallId CreateEmptyInstall(ModMgr& mgr, std::string const & name);
void SetInstallIndex(ModMgr& mgr, ModInstallId id, int index);
void ClearInstallErrors(ModMgr& mgr, ModInstallId id);
void AddInstallMessage(ModMgr& mgr, ModInstallId id, std::string const & msg);

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

void CorrectLoadIndexes(ModMgr& mgr);

bool LoadModMgr(ModMgr& mgr, std::string const& filePath, bool createNew);


CurlTask CreateNxmApiQuery(ModMgr& mgr, std::string const & url, std::function<void(CurlEasyTaskResult&)> cb);
CurlTask CreateNxmGqlQuery(ModMgr& mgr, std::string const& query, std::string const & opName, nlohmann::json const & params, std::function<void(CurlEasyTaskResult &)> cb);

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
void InitializeNXMModDownload(ModMgr& mgr, NxmModFileUrl const & url, std::optional<std::string> name, ModInstallType installType);
void InitializeNXMModDownload2(ModMgr& mgr, ModId id);
void StartNXMCollectionInstall(ModMgr& mgr, NxmCollectionUrl const & url);
void InitializeIndependentDownload(ModMgr& mgr, ModId id);

void UpdateDownloads(ModMgr& mgr);

void UninstallMod(ModMgr& mgr, ModId id);

void InstallMod(ModMgr& mgr, ModId id, std::optional<NxmCollectionUrl> collection);

void DeleteMod(ModMgr& mgr, ModId id);

void InitMgr(ModMgr& mgr);
void CleanupMgr(ModMgr& mgr);


void ApplyModLoadRules(ModMgr& mgr);

void CopyManifestProperties(ModManifest const & src, ModManifest & dst);

std::string NormalizePath(std::string const & str);

#include "modmgr_json.h"

