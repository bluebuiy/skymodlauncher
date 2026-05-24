
#include "modmgr.h"

#include <fstream>
#include <iostream>
#include <format>
#include <filesystem>
#include <unistd.h>
#include <unordered_set>

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

void LaunchExec(ModMgr& mgr, std::string const & execName)
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
    std::vector<std::string> launchArgs = {"-c", confPath, "-e", execName};
    ExecToolProgram ex;
    ex.args = launchArgs;
    ForkInvoke(&ex);
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
    auto sksePath = std::filesystem::path("${GAME_ROOT_DIR}") / "skse64_loader.exe";
    ModExec& skse = mgr.inst.builtinExec.emplace_back();
    skse.execName = "skse";
    skse.execPath = "${WINE_CMD}";
    
    skse.args = {sksePath};
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
    bool sf = false;
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
                    sf = false;
                }
                else if (ch == '#')
                {
                    d = true;
                    sf = true;
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
                        std::string val = elem->second;
                        if (sf)
                        {
                            val = shellFix(val);
                        }
                        ret.insert(ret.end(), val.begin(), val.end());
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
    varMap.emplace("WINE_CMD", "/usr/bin/wine");

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
