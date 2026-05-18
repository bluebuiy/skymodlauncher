
#include "modmgr.h"

#include "imgui.h"
#include "prochelper.h"

#include <fstream>
#include <iostream>
#include <format>
#include <filesystem>
#include <unistd.h>
#include <unordered_set>
#include <ranges>

void RenderTestUi(ModMgr& mgr)
{
#if 0
    static char strBuf[128] = {0};
    static char strBufOut[256] = {0};
    static bool failRep = false;
    if (ImGui::Begin("Tests"))
    {
        ImGui::InputText("Var rep test", strBuf, sizeof(strBuf));
        ImGui::Checkbox("Fail rep", &failRep);
        if (ImGui::Button("Replace"))
        {
            std::string str(strBuf);
            auto rep = ReplaceEnvVariables(mgr, str, failRep);
            if (rep)
            {
                strncpy(strBufOut, rep->c_str(), 128);
                strBufOut[127] = 0;
            }
        }
        ImGui::TextWrapped("%s", strBufOut);
    }
    ImGui::End();
#endif
}

bool WritePluginsTxt(ModMgr& mgr, std::filesystem::path const & path)
{
    std::ofstream stream(path);
    if (!stream)
    {
        return false;
    }

    /*
        I'm using the gog offline version.  Seems to be that
        the anniversary edition content, dlc, and update.esp are
        automatically detected and injected into plugins.txt.
        Plugins from external mods must be added with a "*"
        otherwise they get ignored.  The game reads and re-writes
        plugins.txt every time, and anything it doesn't like
        is removed, including cc and dlc plugins.
    */  

    for (auto&& plugin : mgr.inst.pluginList)
    {
        stream << "*" << plugin.pluginName << std::endl;
    }

    return true;
}

void LaunchSKSE(ModMgr& mgr)
{
#if 0
    std::string lowerLayers;
    std::sort(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](auto& a, auto& b){ return a.loadIndex < b.loadIndex; });
    auto p = std::filesystem::path(mgr.config.modFolder);
    lowerLayers += mfix(mgr.config.installRoot);
    if (!mgr.inst.mods.empty())
    {
        lowerLayers += ":";
    }
    for (int i = 0; i < mgr.inst.mods.size(); ++i)
    {
        int index = mgr.inst.mods.size() - i - 1;
        lowerLayers += mfix(p / mgr.inst.mods[index].modFile);
        if (index != 0)
        {
            lowerLayers += ":";
        }
    }

    auto pluginPath = std::filesystem::path(mgr.config.appData) / "Plugins.txt";
    if (!WritePluginsTxt(mgr, pluginPath))
    {
        std::cout << "Failed to write plugins.txt" << std::endl;
        return;
    }

    std::string overwrite = mfix(mgr.config.projectDir / "overwrite");
    std::string work = mfix(mgr.config.projectDir / "work");
    std::string mountCmd = std::format("mount -t overlay none -o lowerdir={},upperdir={},workdir={} {}\n", lowerLayers, overwrite, work, mfix(mgr.config.installRoot));
    std::string invoke = std::format("wine {}/skse64_loader.exe", mfix(mgr.config.installRoot));
    std::ofstream shFile(mgr.config.projectDir / "launch.sh");
    if (!shFile)
    {
        std::cout << "Failed to write launch.sh" << std::endl;
        return;
    }
    //shFile << "#!/bin/sh\n";
    shFile << "pwd" << std::endl;
    shFile << "echo \"setting up fs\"" << std::endl;;
    shFile << mountCmd << std::endl;
    shFile << std::format("cd {}", mfix(mgr.config.installRoot)) << std::endl;
    shFile << "pwd" << std::endl;
    shFile << "echo \"Launching game\"" << std::endl;
    shFile << invoke << std::endl;
    shFile.close();

    std::vector<std::string> unsh;
    unsh.push_back("/usr/bin/unshare");
    unsh.push_back("--user");
    unsh.push_back("--map-root-user");
    unsh.push_back("--mount");
    unsh.push_back("sh");
    unsh.push_back("launch.sh");
    //unsh.push_back(mfix(mgr.config.projectDir / "launch.sh"));

    std::optional<std::string> output = LaunchProcForOutput(unsh, mgr.config.projectDir);
    if (!output)
    {
        std::cout << "Failed to launch" << std::endl;
    }
#else
    std::string confPath;
    if (auto o = WordExpand(shellFix(mgr.config.configPath)))
    {
        confPath = *o;
    }
    else
    {
        std::cout << "Failed to resolve config path" << std::endl;
        return;
    }
    std::vector<std::string> launchArgs = {"-c", confPath, "-e", "skse"};
    ExecToolProgram ex;
    ex.args = launchArgs;
    ForkInvoke(&ex);
#endif
}

void LaunchShell(ModMgr& mgr)
{
    std::string confPath;
    if (auto o = WordExpand(shellFix(mgr.config.configPath)))
    {
        confPath = *o;
    }
    else
    {
        std::cout << "Failed to resolve config path" << std::endl;
        return;
    }
    std::vector<std::string> launchArgs = {"-c", confPath, "-x", "/usr/bin/ls -al /home/max/.wine/drive_c/GOG\\ Games/Skyrim\\ Anniversary\\ Edition/"};
    ExecToolProgram ex;
    ex.args = launchArgs;
    ForkInvoke(&ex);
}

void RenderNewModDialog(ModMgr& mgr)
{
    if (ImGui::Begin("Add New Mod", &mgr.makingNewMod))
    {
        ImGui::InputText("Mod folder", &mgr.newMod.modFile);
        ImGui::Checkbox("Add Enabled" , &mgr.newMod.enabled);
        ImGui::NewLine();
        ImGui::NewLine();
        if (ImGui::Button("Submit"))
        {
            mgr.newMod.loadIndex = mgr.inst.mods.size();
            mgr.inst.mods.emplace_back(mgr.newMod);
            auto path = WordExpand(mgr.config.modFolder);
            if (path)
            {
                std::filesystem::create_directories(std::filesystem::path(*path) / mgr.newMod.modFile);
            }
            mgr.newMod.modFile.clear();
        }
    }
    ImGui::End();
}

void DiscoverPlugins(ModMgr& mgr)
{
    // plugins are always in ./Data/

    std::unordered_set<std::string> plugins;

    std::filesystem::path modsRoot(mgr.config.modFolder);
    for (auto&& modDir : mgr.inst.mods)
    {
        if (modDir.enabled)
        {
            auto dataDir = modsRoot / modDir.modFile / "Data";

            if (std::filesystem::is_directory(dataDir))
            {
                for (std::filesystem::directory_iterator iter(dataDir); iter != std::filesystem::directory_iterator(); ++iter)
                {
                    if (!iter->is_directory())
                    {
                        auto path = iter->path();
                        auto ext = path.filename().extension();
                        if (ext == ".esp" || ext == ".esm" || ext == ".esl")
                        {
                            plugins.insert(path.filename());
                        }
                    }
                }
            }
        }
    }

    for (int i = mgr.inst.pluginList.size(); i > 0; --i)
    {
        if (plugins.find(mgr.inst.pluginList[i - 1].pluginName) == plugins.end())
        {
            mgr.inst.pluginList.erase(mgr.inst.pluginList.begin() + i - 1);
        }
        else
        {
            plugins.erase(mgr.inst.pluginList[i - 1].pluginName);
        }
    }

    for (auto&& plugin : plugins)
    {
        auto&& add = mgr.inst.pluginList.emplace_back();
        add.enabled = true;
        add.pluginName = plugin;
    }
}

void RenderModMgrSettings(ModMgr& mgr)
{
    if (ImGui::Begin("Settings", &mgr.settingsOpen))
    {
        ImGui::InputText("Game Root", &mgr.config.installRoot);
        ImGui::InputText("My Games/Skyrim", &mgr.config.mgRoot);
        ImGui::InputText("Mod folder", &mgr.config.modFolder);
        ImGui::InputText("AppData/Skyrim", &mgr.config.appData);
        ImGui::Separator();
        ImGui::InputText("Config path", &mgr.config.configPath);
        ImGui::InputText("Instance path", &mgr.config.instPath);
    }
    ImGui::End();
}

void RenderPluginsList(ModMgr& mgr)
{
    if (ImGui::Begin("Plugins"))
    {
        if (ImGui::Button("Refresh Plugins"))
        {
            DiscoverPlugins(mgr);
        }

        int move = -1;
        int dir = 0;

        for (int i = 0; i < mgr.inst.pluginList.size(); ++i)
        {
            ImGui::PushID(mgr.inst.pluginList[i].pluginName.c_str());
            {
                ImGui::Text("%3d", i);
                ImGui::SameLine();
                ImGui::Checkbox("", &mgr.inst.pluginList[i].enabled);
                ImGui::SameLine();
                ImGui::BeginDisabled(i == 0);
                if (ImGui::Button("^"))
                {
                    move = i;
                    dir = -1;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(i == mgr.inst.pluginList.size() -1);
                if (ImGui::Button("V"))
                {
                    move = i;
                    dir = 1;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::Text("%s", mgr.inst.pluginList[i].pluginName.c_str());
            }
            ImGui::PopID();
        }

        if (move != -1)
        {
            if (dir == 1 && move != mgr.inst.pluginList.size() - 1)
            {
                std::swap(mgr.inst.pluginList[move], mgr.inst.pluginList[move + 1]);
            }
            else if (dir == -1 && move != 0)
            {
                std::swap(mgr.inst.pluginList[move], mgr.inst.pluginList[move - 1]);
            }
        }
    }
    ImGui::End();
}

void RenderModMgr(ModMgr& mgr)
{
    RenderTestUi(mgr);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Save"))
        {
            SaveModMgr(mgr);
        }
        
    }
    ImGui::EndMainMenuBar();
    
    if (ImGui::Begin("Mod Manager"))
    {
        if (ImGui::Button("Launch SKSE"))
        {
            LaunchSKSE(mgr);
        }

        if (ImGui::Button("Launch a terminal"))
        {
            LaunchShell(mgr);
        }

        if (ImGui::Button("Settings"))
        {
            mgr.settingsOpen = true;
        }

        if (ImGui::Button("Add Mod"))
        {
            mgr.makingNewMod = true;
        }

        ImGui::Text("Sort order");
        if (ImGui::RadioButton("Load order", mgr.sortMode == 0))
        {
            mgr.sortMode = 0;
        }
        if (ImGui::RadioButton("Name", mgr.sortMode == 1))
        {
            mgr.sortMode = 1;
        }
        ImGui::Checkbox("Enable mod removal (doesnt delete the mod folder)", &mgr.enableRemove);
        ImGui::Text("Mod list");
        ImGui::Separator();
        std::sort(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](ModInfo const & a, ModInfo const & b){
            if (mgr.sortMode == 0)
            {
                return a.loadIndex < b.loadIndex;
            }
            else if (mgr.sortMode == 1)
            {
                return a.modFile < b.modFile;
            }
            else
            {
                // ??
                return true;
            }
        });
        int del = -1;
        int mvUp = -1;
        int mvDown = -1;
        for (int i = 0; i < mgr.inst.mods.size(); ++i)
        {
            ImGui::PushID(mgr.inst.mods[i].modFile.c_str());
                ImGui::Text(" %3d ", mgr.inst.mods[i].loadIndex);
                ImGui::SameLine();
                ImGui::Checkbox(" ", &mgr.inst.mods[i].enabled);
                ImGui::SameLine();
                ImGui::BeginDisabled(mgr.inst.mods[i].loadIndex == 0);
                if (ImGui::Button("^"))
                {
                    mvUp = i;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(mgr.inst.mods[i].loadIndex == mgr.inst.mods.size() - 1);
                if (ImGui::Button("V"))
                {
                    mvDown = i;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                
                ImGui::Text("%-32s", mgr.inst.mods[i].modFile.c_str());
                if (mgr.enableRemove)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        del = i;
                    }
                }
            ImGui::PopID();
        }
        ImGui::Separator();
        if (del != -1)
        {
            mgr.inst.mods.erase(mgr.inst.mods.begin() + del);
        }
        else if (mvUp != -1)
        {
            int l = mgr.inst.mods[mvUp].loadIndex;
            for (auto&& m : mgr.inst.mods)
            {
                if (m.loadIndex == l - 1)
                {
                    m.loadIndex++;
                    break;
                }
            }
            mgr.inst.mods[mvUp].loadIndex--;
        }
        else if (mvDown != -1)
        {
            int l = mgr.inst.mods[mvDown].loadIndex;
            for (auto&& m : mgr.inst.mods)
            {
                if (m.loadIndex == l + 1)
                {
                    m.loadIndex--;
                    break;
                }
            }
            mgr.inst.mods[mvDown].loadIndex++;
        }
    }
    ImGui::End();

    RenderPluginsList(mgr);

    if (mgr.makingNewMod)
    {
        RenderNewModDialog(mgr);
    }

    if (mgr.settingsOpen)
    {
        RenderModMgrSettings(mgr);
    }
}


void SaveModMgr(ModMgr& mgr)
{
    if (mgr.config.configPath.empty())
    {
        std::cout << "Config path not set, set one in the settings or invoke with \"-p config.json\"" << std::endl;
        return;
    }
    std::optional<std::string> cp = WordExpand(mgr.config.configPath);
    if (!cp)
    {
        std::cout << "Failed to resolve config path" << std::endl;
        return;
    }
    std::ofstream file(*cp);
    if (!file)
    {
        std::cout << "Failed to open file to save config" << std::endl;
        return;
    }

    std::optional<std::string> ip = WordExpand(mgr.config.instPath);
    if (!ip)
    {
        std::cout << "Failed to resolve instance path" << std::endl;
    }
    std::ofstream instfile(*ip);
    if (!instfile)
    {
        std::cout << "Failed to open instance file to save instance" << std::endl;
        return;
    }

    instfile << nlohmann::json(mgr.inst);
    instfile.close();

    file << nlohmann::json(mgr.config);
    file.close();
}

std::optional<std::string> DetectAppDataLocal(ModMgr& mgr)
{
    // cmd is injecting /r/n into the output... sigh
    std::vector<std::string> appPathCmd = {"/usr/bin/wine", "cmd", "/c", "echo %localappdata%"};
    auto adpath = LaunchProcForOutput(appPathCmd, mfix("~"));
    if (!adpath)
    {
        std::cout << "Failed to determine %localappdata%, add it manually in settings." << std::endl;
    }
    else
    {
        std::string winepath = *adpath;
        removecrlf(winepath);
        std::vector<std::string> wineConvCmd = {"/usr/bin/winepath", "-u", winepath};
        auto lp = LaunchProcForOutput(wineConvCmd, mfix("~"));
        if (!lp)
        {
            std::cout << "Failed to convert %localappdata% winepath to linux path. add it manually in settings." << std::endl;
            std::cout << "%localappdata% = " << *adpath << std::endl;
        }
        else
        {
            std::string linpath = *lp;
            removecrlf(linpath);
            std::cout << "Detected %localappdata%: " << linpath << std::endl;
            return std::filesystem::path(linpath) / "Skyrim Special Edition GOG";
        }
    }
    return {};
}

void LoadBuiltinTools(ModMgr& mgr)
{
    auto sksePath = WordExpand(shellFix(std::filesystem::path(mgr.config.installRoot) / "skse64_loader.exe"));
    if (!sksePath)
    {
        std::cout << "Failed to resolve the install path" << std::endl;
        return;
    }
    ModExec& skse = mgr.inst.builtinExec.emplace_back();
    skse.execName = "skse";
    skse.execPath = "/usr/bin/wine";
    
    skse.args = {*sksePath};
}

bool LoadModMgr(ModMgr& mgr, std::string const& filePath, bool createNew)
{
    mgr.config.configPath = filePath;
    std::filesystem::path pth;

    std::optional<std::string> o = WordExpand(filePath);
    if (o)
    {
        pth = *o;
    }
    else
    {
        std::cout << "Invalid file path" << std::endl;
        return false;
    }
    if (std::filesystem::exists(pth))
    {
        {
            std::cout << "Loading config file " << filePath << std::endl;
            std::ifstream file(pth);
            if (!file)
            {
                std::cout << "Failed to open config file" << std::endl;
                return false;
            }
            nlohmann::json obj;
            obj << file;
            file.close();
            mgr.config = obj.get<ModMgrConfig>();
            mgr.config.configPath = filePath;
            mgr.config.projectDir = pth.parent_path();
            
            if (mgr.config.appData.empty())
            {
                std::cout << "Determining %localappdata%..." << std::endl;
                auto adpath = DetectAppDataLocal(mgr);
                if (adpath)
                {
                    mgr.config.appData = *adpath;
                }
                else
                {
                    std::cout << "Failed to detect %localappdata%, set it manually in settings." << std::endl;
                }
            }

            std::cout << "Loaded" << std::endl;
            if (mgr.config.version != ModMgrConfig::VERSION)
            {
                std::cout << "Config version mismatch, terminating." << std::endl;
                exit(1);
            }
            if (mgr.verbose)
            {
                std::cout << "exe: " << mgr.config.installRoot << std::endl;
                std::cout << "mg: " << mgr.config.mgRoot << std::endl;
                std::cout << "mods: " << mgr.config.modFolder << std::endl;
                std::cout << "inst: " << mgr.config.instPath << std::endl;
                std::cout << "ad: " << mgr.config.appData << std::endl;
            }
        }
        {
            std::cout << "Loading instance" << std::endl;
            auto instPath = WordExpand(mgr.config.instPath);
            if (!instPath)
            {
                std::cout << "Failed to resolve instance file path, terminating." << std::endl;
                exit(1);
            }
            std::ifstream file(*instPath);
            if (!file)
            {
                std::cout << "Failed to open instance file, terminating." << std::endl;
                exit(1);
            }
            nlohmann::json obj;
            obj << file;
            file.close();
            mgr.inst = obj.get<ModMgrInst>();
            if (mgr.inst.version != ModMgrInst::VERSION)
            {
                std::cout << "Instance version mismatch, terminating." << std::endl;
                exit(1);
            }
        }
        DiscoverPlugins(mgr);
    }
    else if (createNew && std::filesystem::is_directory(pth.parent_path()))
    {
        std::cout << "Failed to find config file, creating new instance at " << filePath << std::endl;
        std::filesystem::path p = pth;
        p = p.parent_path();

        mgr.config.configPath = pth;
        mgr.config.instPath = p / "instance.json";
        mgr.config.modFolder = p / "mods";
        mgr.config.installRoot = "~/.wine/drive_c/GOG Games/Skyrim Anniversary Edition/";
        mgr.config.mgRoot = "~/Documents/My Games/Skyrim Special Edition GOG/";
        mgr.config.version = ModMgrConfig::VERSION;
        mgr.config.projectDir = p;
        
        auto adl = DetectAppDataLocal(mgr);
        if (adl)
        {
            mgr.config.appData = *adl;
        }
        else
        {
            std::cout << "Failed to determine %localappdata%, set it manually in settings." << std::endl;
        }
        
        std::filesystem::create_directories(mgr.config.modFolder);
        

        SaveModMgr(mgr);
    }
    else
    {
        std::cout << "Failed to create or load an instance " << " (createNew=" << createNew << ") at " << pth.parent_path() << std::endl;
        return false;
    }
    
    LoadBuiltinTools(mgr);

    return true;
}

std::optional<std::string> DoReplaceVars(std::string const & in, std::unordered_map<std::string, std::string> const & vars, bool failUnknownVariable)
{
    // really jank parser :D
    std::string ret;
    size_t i = 0;
    size_t beg = -1;     // start of var name
    bool d = false;     // $
    bool o = false;     // {
    while (true)
    {
        while (i < in.size())
        {
            auto ch = in[i];
            if (d == false)
            {
                if (ch == '$')
                {
                    d = true;
                }
                else
                {
                    ret.push_back(ch);
                }
            }
            else if (d == true && o == false)
            {
                if (ch == '{')
                {
                    o = true;
                    beg = i + 1;
                }
                else
                {
                    ret.push_back('$');
                    ret.push_back(ch);
                    d = false;
                }
            }
            else if (d == true && o == true)
            {
                if (ch == '}')
                {
                    d = false;
                    o = false;

                    size_t end = i;
                    std::string name = in.substr(beg, end - beg);
                    auto elem = vars.find(name);
                    if (elem != vars.end())
                    {
                        ret.insert(ret.end(), elem->second.begin(), elem->second.end());
                        beg = -1;
                    }
                    else
                    {
                        if (failUnknownVariable)
                        {
                            return {};
                        }
                        else
                        {
                            ret.push_back('$');
                            ret.push_back('{');
                            ret.insert(ret.end(), name.begin(), name.end());
                            ret.push_back('}');
                        }
                    }
                }
            }
            ++i;
        }
        if (d == true)
        {
            ret.push_back('$');
            d = false;
        }
        else if (o == true)
        {
            // TODO fail on malformed subsitution
            d = false;
            o = false;
            i = beg;
            beg = -1;
            ret.push_back('$');
            ret.push_back('{');
        }
        else if (i < in.size())
        {
            
        }
        else
        {
            break;
        }
    }
    return ret;
}

std::optional<std::string> ReplaceEnvVariables(ModMgr& mgr, std::string const & in, bool failOnUnknownVariable)
{
    std::unordered_map<std::string, std::string> varMap;
    varMap.emplace("GAME_ROOT_DIR", mgr.config.installRoot);
    varMap.emplace("PLUGINTXT_DIR", mgr.config.appData);
    varMap.emplace("INI_DIR", mgr.config.mgRoot);
    varMap.emplace("OVERWRITE_DIR", std::filesystem::path(mgr.config.projectDir) / "overwrite");
    varMap.emplace("PROJECT_DIR", mgr.config.projectDir);

    // duplicates get ignored!
    for (auto&& cv : mgr.inst.customVariables)
    {
        auto it = varMap.find(cv.name);
        if (it != varMap.end())
        {
            std::cout << "Redefinition of ${" << cv.name << "}, using new value:\n" << it->second << std::endl;
        }
        varMap.insert_or_assign(cv.name, cv.value);
    }

    return DoReplaceVars(in, varMap, failOnUnknownVariable);
}


