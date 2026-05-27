
#include "modmgr.h"
#include "fomod_ui.h"

#include <fstream>
#include <iostream>
#include <format>
#include <filesystem>
#include <unistd.h>
#include <unordered_set>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex>

#include "curlwrap.h"

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

ModDownload MdFromRt(ModDownloadRt const & mdrt)
{
    ModDownload ret;
    ret.fileName = mdrt.fileName;
    ret.fileId = mdrt.fileId;
    ret.modId = mdrt.modId;
    ret.game = mdrt.game;
    return ret;
}

ModDownloadRt MdrtFromMd(ModDownload& md)
{
    ModDownloadRt ret;
    ret.fileName = std::move(md.fileName);
    ret.fileId = md.fileId;
    ret.modId = md.modId;
    ret.game = std::move(md.game);
    ret.state = ModDlState::Complete;
    return ret;
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

    mgr.inst.downloads.clear();
    for (auto&& mdrt : mgr.downloadSessions)
    {
        if (mdrt.state == ModDlState::Complete)
        {
            mgr.inst.downloads.emplace_back(MdFromRt(mdrt));
        }
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

void CorrectLoadIndexes(ModMgr& mgr)
{
    std::sort(mgr.inst.mods.begin(), mgr.inst.mods.end(), [](ModInfo const & a, ModInfo const & b){return a.loadIndex < b.loadIndex;});
    for (int i = 0; i < mgr.inst.mods.size(); ++i)
    {
        mgr.inst.mods[i].loadIndex = i;
    }
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
            CorrectLoadIndexes(mgr);
            for (auto&& md : mgr.inst.downloads)
            {
                mgr.downloadSessions.emplace_back(MdrtFromMd(md));
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

    SetupNXMActionPipe(mgr);

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

void SetupNXMActionPipe(ModMgr& mgr)
{
    int pe = mkfifo("/tmp/skymodurl", 0666);

    int p = open("/tmp/skymodurl", O_RDONLY | O_NONBLOCK);

    if (p == -1 && pe == -1)
    {
        printf("Failed to create & open url pipe\n");
        close(p);
        unlink("/tmp/skymodurl");
        return;
    }
    else if (p == -1)
    {
        printf("Failed to open pipe\n");
        return;
    }

    struct stat st;
    if (fstat(p, &st) == -1)
    {
        printf("Failed to stat pipe\n");
        close(p);
        return;
    }

    if (!S_ISFIFO(st.st_mode))
    {
        printf("opened fd is not a fifo\n");
        close(p);
        return;
    }

    mgr.urlPipe = p;
}

void CheckNXMAction(ModMgr& mgr)
{
    int count = 0;
    char buff[1024];
    bool err = false;
    bool cont = true;
    int rdAmt = 0;
    while (cont)
    {
        int rn = read(mgr.urlPipe, buff + rdAmt, sizeof(buff) - rdAmt);
        if (rn < 0)
        {
            int e = errno;
            if (e == EAGAIN)
            {
                if (rdAmt > 0)
                {
                    if (count > 1000)
                    {
                        err = true;
                        cont = false;
                        std::cout << "Pipe timeout" << std::endl;
                    }
                    else
                    {
                        struct timespec ts;
                        ts.tv_sec = 0;
                        ts.tv_nsec = 1000000;
                        nanosleep(&ts, nullptr);
                        ++count;
                    }
                }
                else
                {
                    cont = false;
                }
            }
            else
            {
                err = true;
                cont = false;
            }
        }
        else if (rn == 0)
        {
            if (rdAmt == sizeof(buff))
            {
                err = true;
                buff[sizeof(buff) - 1] = 0;
            }
            else
            {
                buff[rdAmt] = 0;
            }
            cont = false;
        }
        else
        {
            rdAmt += rn;
        }
    }

    if (err)
    {
        close(mgr.urlPipe);
        SetupNXMActionPipe(mgr);
    }
    else if (rdAmt > 0)
    {
        std::cout << "Received NXM url: " << buff << std::endl;
        StartNXMModDownload(mgr, buff);
    }
}

void CleanupNXMAction(ModMgr& mgr)
{
    close(mgr.urlPipe);
    unlink("/tmp/skymodurl");

    
}

// nxm://skyrimspecialedition/mods/17372/files/268342?key=h2-W-GZbvPARUeEb7ZSqIw&expires=1779849752&user_id=28860775

bool str_to_int(std::string const & str, int& out)
{
    size_t end = 0;
    int i = std::stoi(str, &end, 10);
    if (end != str.size())
    {
        return false;
    }
    out = i;
    return true;
}

// prefix = "key=" 
std::string extractQueryValue(std::string const & query, std::string const & prefix)
{
    size_t keyIndexStart = query.find(prefix);

    if (keyIndexStart == std::string::npos)
    {
        return {};
    }

    keyIndexStart += prefix.size();
    size_t keyIndexEnd = query.find("&", keyIndexStart);

    std::string ret = query.substr(keyIndexStart, keyIndexEnd == std::string::npos ? keyIndexEnd : keyIndexEnd - keyIndexStart);
    return ret;
}

// ill clean this up & make it reusable later

size_t curl_str_write_cb(char* data, size_t n, size_t l, void* userp)
{
    std::string* str = static_cast<std::string*>(userp);
    str->insert(str->end(), data, data + n * l);
    return n * l;
}

void StartNXMModDownload(ModMgr& mgr, std::string const & urlStr)
{
    if (mgr.config.nexusApiKey.empty())
    {
        std::cout << "Nexus api key missing" << std::endl;
        return;
    }

    ModDownloadRt dl;
    curl::url url(curl_url());
    CURLUcode rc = curl_url_set(url, CURLUPART_URL, urlStr.c_str(), CURLU_NON_SUPPORT_SCHEME);
    if (rc != CURLUE_OK)
    {
        std::cout << "Invalid url" << std::endl;
    }
    char* pathPart = nullptr;
    rc = curl_url_get(url, CURLUPART_PATH, &pathPart, 0);
    if (rc != CURLUE_OK)
    {
        curl_free(pathPart);
        return;
    }

    std::filesystem::path pp(pathPart);
    curl_free(pathPart);

    std::vector<std::string> parts;
    for (auto&& part : pp)
    {
        parts.push_back(part);
    }

    if (parts.size() != 5)
    {
        return;
    }

    // extract key

    char* queryPart = nullptr;
    rc = curl_url_get(url, CURLUPART_QUERY, &queryPart, 0);
    if (rc != CURLUE_OK)
    {
        curl_free(queryPart);
        return;
    }

    std::string query(queryPart);
    curl_free(queryPart);

    // game

    char* gamePart = nullptr;
    rc = curl_url_get(url, CURLUPART_HOST, &gamePart, 0);
    if (rc != CURLUE_OK)
    {
        curl_free(gamePart);
        return;
    }

    std::string game(gamePart);
    curl_free(gamePart);

    dl.key = extractQueryValue(query, "key=");
    dl.expires = extractQueryValue(query, "expires=");

    if (!str_to_int(parts[2], dl.modId))
    {
        return;
    }
    if (!str_to_int(parts[4], dl.fileId))
    {
        return;
    }
    dl.game = game;

    // check if the file is already in the download list
    auto dupIt = std::find_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [&](ModDownloadRt const & dlr)
    {
        return dlr.modId == dl.modId && dlr.fileId == dl.fileId;
    });
    if (dupIt != mgr.downloadSessions.end())
    {
        std::cout << "Detected duplicate mod" << std::endl;
        return;
    }
    
    curl::curl curlSes = curl_easy_init();

    auto cer = curl_easy_setopt(curlSes, CURLOPT_WRITEFUNCTION, curl_str_write_cb);
    if (cer)
    {
        return;
    }

    std::unique_ptr<std::string> modInfo = std::make_unique<std::string>();

    cer = curl_easy_setopt(curlSes, CURLOPT_WRITEDATA, modInfo.get());
    if (cer)
    {
        return;
    }

    std::string fullUrl = std::format("https://api.nexusmods.com/v1/games/{}/mods/{}/files/{}/download_link.json?key={}&expires={}", dl.game, dl.modId, dl.fileId, dl.key, dl.expires);
    std::cout << fullUrl << std::endl;
    cer = curl_easy_setopt(curlSes, CURLOPT_URL, fullUrl.c_str());
    if (cer)
    {
        return;
    }
    dl.modUrlInfo = std::move(modInfo);

    std::string authHeader = std::format("apikey: {}", mgr.config.nexusApiKey);
    curl_slist* hlist = nullptr;
    hlist = curl_slist_append(hlist, authHeader.c_str());
    hlist = curl_slist_append(hlist, "accept: application/json");

    curl_easy_setopt(curlSes, CURLOPT_HTTPHEADER, hlist);
    //curl_easy_setopt(curlSes, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curlSes, CURLOPT_HTTPGET, 1);

    dl.state = ModDlState::UrlQuery;

    CURLMcode mer = curl_multi_add_handle(mgr.curlMulti, curlSes);
    if (mer != CURLM_OK)
    {
        curl_slist_free_all(hlist);
        return;
    }

    dl.headers = hlist;
    dl.dl = curlSes.release();
    mgr.downloadSessions.emplace_back(std::move(dl));
}

std::string GetDownloadUrl(std::string const & urlInfo)
{
    try
    {
        nlohmann::json j = nlohmann::json::parse(urlInfo);
        return j[0]["URI"];
    }
    catch (...)
    {
        return {};
    }
}

std::string GetModNameFromDownloadUrl(std::string const & url)
{
    std::regex r(R"(https://.*?\.com/(.*?/)?[0-9]+?/[0-9]+?/(.*?)\?.*$)");

    std::smatch m;
    if (!std::regex_search(url, m, r))
    {
        return {};
    }

    return m[2];
}


void UpdateDownloads(ModMgr& mgr)
{
   
    for (auto&& dl : mgr.downloadSessions)
    {
        if (dl.remove || dl.cancel)
        {
            curl_multi_remove_handle(mgr.curlMulti, dl.dl);
            curl_easy_cleanup(dl.dl);
            dl.dl = nullptr;
            curl_slist_free_all(dl.headers);
            dl.headers = nullptr;
            if (dl.modFile)
            {
                fclose(dl.modFile);
            }
            dl.modFile = nullptr;
        }
        else if (dl.pause && dl.state == ModDlState::ModDownload)
        {
            curl_easy_pause(dl.dl, CURLPAUSE_RECV);
            dl.state = ModDlState::ModPaused;
        }
        else if (dl.unpause && dl.state == ModDlState::ModPaused)
        {
            curl_easy_pause(dl.dl, CURLPAUSE_CONT);
            dl.state = ModDlState::ModDownload;
        }

        if (dl.remove)
        {
            std::string path = std::format("{}/download/{}", *WordExpand(mgr.config.projectDir), dl.fileName);
            std::cout << "Unlink: " << path << std::endl;
            if (unlink(path.c_str()))
            {
                std::cout << errno << std::endl;
            }
        }

        dl.remove = false;
        dl.cancel = false;
        dl.pause = false;
        dl.unpause = false;
    }

    auto remStart = std::remove_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [](ModDownloadRt const & dl)
    {
        return dl.remove == true;
    });
    mgr.downloadSessions.erase(remStart, mgr.downloadSessions.end());

    int h = 0;
    auto c = curl_multi_perform(mgr.curlMulti, &h);
    if (c != CURLM_OK)
    {
        // clear all
        std::cout << "Error" << std::endl;
    }

    struct CurlRes
    {
        CURL* curl;
        CURLcode result;
    };

    std::vector<CurlRes> done;

    int msgRem = 0;
    while (CURLMsg* msg = curl_multi_info_read(mgr.curlMulti, &msgRem))
    {
        if (msg->msg == CURLMSG_DONE)
        {
            done.push_back(CurlRes{msg->easy_handle, msg->data.result});
            auto cme = curl_multi_remove_handle(mgr.curlMulti, msg->easy_handle);
            if (cme != CURLM_OK)
            {
                std::cout << "Failed to remove" << std::endl;
            }
        }
    }

    
    // now update the download stages

    for (auto&& cs : done)
    {
        bool cleanup = false;
        auto it = std::find_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [&](ModDownloadRt const & a){return a.dl == cs.curl;});
        if (it == mgr.downloadSessions.end())
        {
            // shouldn't happen
            std::cout << "Corrupted download sessions!" << std::endl;
        }
        else
        {
            auto& dlState = *it;
            if (cs.result != CURLE_OK)
            {
                dlState.state = ModDlState::Error;
                cleanup = true;
            }
            else if (dlState.state == ModDlState::UrlQuery)
            {
                int code = 0;
                curl_easy_getinfo(dlState.dl, CURLINFO_RESPONSE_CODE, &code);
                std::cout << code << std::endl;
                if (code != 200)
                {
                    it->state = ModDlState::Error;
                    cleanup = true;
                }
                else
                {
                    // TODO setup mod download
                    std::string dlUrl = GetDownloadUrl(*(it->modUrlInfo));
                    std::string modName = GetModNameFromDownloadUrl(dlUrl);
                    bool duplicate = false;
                    if (modName == "")
                    {
                        timespec ts;
                        clock_gettime(CLOCK_MONOTONIC, &ts);
                        modName = std::format("unknonw-mod-{}-{}", ts.tv_sec, ts.tv_nsec / 100000);
                    }
                    dlState.fileName = modName;
                    dlState.outFile = std::format("{}/download/{}", *WordExpand(mgr.config.projectDir), modName);
                    std::filesystem::create_directories(dlState.outFile.parent_path());
                    FILE* outFile = fopen(dlState.outFile.c_str(), "wb");
                    if (!outFile)
                    {
                        cleanup = true;
                        it->state = ModDlState::Error;
                    }
                    else
                    {
                        dlState.modFile = outFile;

                        CURLU* url = curl_url();
                        curl_url_set(url, CURLUPART_URL, dlUrl.c_str(), CURLU_ALLOW_SPACE | CURLU_URLENCODE);
                        char* urlEncodedUrl = nullptr;
                        curl_url_get(url, CURLUPART_URL, &urlEncodedUrl, 0);
                        std::cout << urlEncodedUrl << std::endl;
                        auto ce = curl_easy_setopt(dlState.dl, CURLOPT_URL, urlEncodedUrl);
                        curl_free(urlEncodedUrl);
                        if (ce != CURLE_OK)
                        {
                            cleanup = true;
                            std::cout << "Bad url: " << dlUrl << std::endl;
                        }
                        else
                        {
                            curl_easy_setopt(dlState.dl, CURLOPT_WRITEDATA, dlState.modFile);
                            curl_easy_setopt(dlState.dl, CURLOPT_WRITEFUNCTION, fwrite);
                            curl_easy_setopt(dlState.dl, CURLOPT_HTTPGET, 1);
                            curl_easy_setopt(dlState.dl, CURLOPT_HTTPHEADER, nullptr);
                            curl_easy_setopt(dlState.dl, CURLOPT_VERBOSE, 1);
                            if (curl_multi_add_handle(mgr.curlMulti, dlState.dl) != CURLM_OK)
                            {
                                cleanup = true;
                                it->state = ModDlState::Error;
                            }
                            else
                            {
                                it->state = ModDlState::ModDownload;
                            }
                        }
                    }
                }
            }
            else if (dlState.state == ModDlState::ModDownload)
            {
                int code = 0;
                auto ce = curl_easy_getinfo(it->dl, CURLINFO_RESPONSE_CODE, &code);
                if (ce != CURLE_OK)
                {
                    std::cout << "Error?"<< std::endl;
                }
                if (code != 200)
                {
                    it->state = ModDlState::Error;
                }
                else
                {
                    it->state = ModDlState::Complete;
                }
                cleanup = true;
            }
            else if (dlState.state == ModDlState::ModPaused)
            {
                int code = 0;
                auto ce = curl_easy_getinfo(it->dl, CURLINFO_RESPONSE_CODE, &code);
                if (code != 200)
                {
                    it->state = ModDlState::Error;
                    cleanup = true;
                }
                else
                {
                    // tried to pause, but it completed between then and now.
                    it->state = ModDlState::Complete;
                }
                
            }
            #if 0
            else if (dlState.state == ModDlState::Canceled)
            {
                int code = 0;
                auto ce = curl_easy_getinfo(it->dl, CURLINFO_RESPONSE_CODE, &code);
                if (code != 200)
                {
                    it->state = ModDlState::Error;
                }
                else
                {
                    // tried to cancel, but it completed between then and now.
                    it->state = ModDlState::Complete;
                }
                cleanup = true;
            }
            #endif
        }
        if (cleanup)
        {
            curl_easy_cleanup(it->dl);
            it->dl = nullptr;
            if (it->modFile)
            {
                fclose(it->modFile);
            }
            it->modFile = nullptr;
            curl_slist_free_all(it->headers);
            it->headers = nullptr;
        }
    }

}


void InstallDownloadedFile(ModMgr& mgr, std::string const & modName)
{
    if (mgr.fomodState.has_value())
    {
        return;
    }

    auto dl = std::find_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [&](ModDownloadRt const & dlr)
    {
        return dlr.fileName == modName;
    });

    if (dl == mgr.downloadSessions.end())
    {
        return;
    }

    // check filetype

    // --print0 is not working, but it works from terminal.  I checked the output from the pipe directly.
    std::vector<std::string> args = {
        "/usr/bin/file",
        // "-0",
        "-b",
        "--mime-type",
        std::format("{}/download/{}", *WordExpand(mgr.config.projectDir), dl->fileName)
    };

    auto out = LaunchProcForOutput(args, "/");
    if (!out)
    {
        std::cout << "Failed to determine file type" << std::endl;
        return;
    }

    std::vector<std::string> extractCmd;
    std::string type = *out;

    std::string fileStem = std::filesystem::path(modName).stem();

    if (type == "application/x-7z-compressed\n")
    {
        // 7z creates missing directories
        extractCmd = {
            "/usr/bin/7z",
            "x",
            std::format("-o{}/{}/Data/", *WordExpand(mgr.config.modFolder), fileStem),
            std::format("{}/download/{}", *WordExpand(mgr.config.projectDir), modName)
        };
    }
    else if (type == "application/zip\n")
    {
        // unzip creates missing directories
        extractCmd = {
            "/usr/bin/unzip",
            "-d",
            std::format("{}/{}/Data/", *WordExpand(mgr.config.modFolder), fileStem),
            std::format("{}/download/{}", *WordExpand(mgr.config.projectDir), modName)
        };
    }
    else
    {
        std::cout << "Unknown file type: " << type << std::endl;
        return;
    }

    if (!LaunchProc(extractCmd, "/"))
    {
        std::cout << "Failed to extract mod contents, check output above." << std::endl;
        return;
    }

    // create the mod entry
    auto& mod = mgr.inst.mods.emplace_back();
    mod.enabled = true;
    mod.loadIndex = mgr.inst.mods.size() - 1;
    mod.modFile = fileStem;

    // check if there's a fomod
    {
        std::filesystem::path projDir(*WordExpand(mgr.config.projectDir));
        std::filesystem::path modFolder = *WordExpand(mgr.config.modFolder);
        std::filesystem::path tmpDir = projDir / "fomod_tmp" / fileStem;
        std::filesystem::path fomod_test = modFolder / fileStem / "Data" / "FOMod";
        if (std::filesystem::is_directory(fomod_test) & std::filesystem::is_regular_file(fomod_test / "ModuleConfig.xml"))
        {
            std::filesystem::create_directories(tmpDir);
            // move mod contents to a temporary holding directory
            std::vector<std::string> mvCmd = {"/usr/bin/mv", std::string(modFolder / fileStem / "Data"), tmpDir};
            LaunchProc(mvCmd, "/");
            // now mod/Data is at proj/fomod_tmp/mod/Data/
            // initialize fomod
            InitFomod(mgr, std::filesystem::path() / "fomod_tmp" / fileStem, fileStem);
        }
    }

    SaveModMgr(mgr);
}



void DeleteMod(ModMgr& mgr, std::string & modFile)
{
    std::filesystem::path path = std::filesystem::path(*WordExpand(mgr.config.modFolder)) / modFile;
    
    std::vector<std::string> args = {
        "/usr/bin/rm",
        "-r",
        path
    };
    LaunchProc(args, "/");
}

void InitMgr(ModMgr& mgr)
{
    curl_global_init(CURL_GLOBAL_ALL);

    mgr.curlMulti = curl_multi_init();
    curl_multi_setopt(mgr.curlMulti, CURLMOPT_MAXCONNECTS, 8);
}

void CleanupMgr(ModMgr& mgr)
{

    for (auto&& dlSession : mgr.downloadSessions)
    {
        curl_easy_pause(dlSession.dl, CURLPAUSE_RECV);
        curl_easy_cleanup(dlSession.dl);
    }

    mgr.downloadSessions.clear();

    curl_multi_cleanup(mgr.curlMulti);
    mgr.curlMulti = nullptr;

    curl_global_cleanup();
}




