
#include "modmgr.h"
#include "fomod_ui.h"
#include "nxmurl.h"
#include "asyncproc.h"

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

// TODO search overwrite too
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
    ret.hFileName = mdrt.hName;
    ret.fileName = mdrt.fileName;
    ret.fileId = mdrt.fileId;
    ret.modId = mdrt.modId;
    ret.game = mdrt.game;
    ret.installType = (mdrt.installType == ModInstallType::Data ? "data" : "root");
    return ret;
}

ModDownloadRt MdrtFromMd(ModDownload const & md)
{
    ModDownloadRt ret;
    ret.hName = md.hFileName;
    ret.fileName = std::move(md.fileName);
    ret.fileId = md.fileId;
    ret.modId = md.modId;
    ret.game = std::move(md.game);
    ret.state = ModDlState::Complete;
    ret.installType = (md.installType == "root" ? ModInstallType::Root : ModInstallType::Data);
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
        HandleNXMUrl(mgr, buff);
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
    // WHY IS IT THROWING WTF IS end FOR THEN
    try
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
    catch (...)
    {
        return false;
    }
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

struct ModDownloadFinished
{
    ModMgr* mgr = nullptr;
    int fileId = 0;
    int modId = 0;
    void operator()(CurlEasyTaskResult& result)
    {
        auto it = std::find_if(mgr->downloadSessions.begin(), mgr->downloadSessions.end(), [&](ModDownloadRt const & m) {
            return m.fileId == fileId && m.modId == modId;
        });
        if (result.cError != CURLE_OK || result.httpCode != 200)
        {
            if (it != mgr->downloadSessions.end())
            {
                std::cout << "Error downloading mod info: " << it->fileName << std::endl;
                it->state = ModDlState::Error;
            }
            std::cout << "Curl status: " << result.cError << std::endl;
            std::cout << "Http status: " << result.httpCode << std::endl;
            return;
        }
        else if (result.canceled)
        {
            it->state = ModDlState::Canceled;
        }
        else
        {
            it->state = ModDlState::Complete;
        }
    }
};

struct ModUrlFinished
{
    ModMgr* mgr = nullptr;
    int fileId = 0;
    int modId = 0;

    void operator()(CurlEasyTaskResult& result)
    {
        auto it = std::find_if(mgr->downloadSessions.begin(), mgr->downloadSessions.end(), [&](ModDownloadRt const & m) {
            return m.fileId == fileId && m.modId == modId;
        });
        if (result.cError != CURLE_OK || result.httpCode != 200)
        {
            if (it != mgr->downloadSessions.end())
            {
                std::cout << "Error downloading mod file info: " << " " << fileId << " " << modId << std::endl;
                it->state = ModDlState::Error;
            }
            std::cout << "Curl status: " << result.cError << std::endl;
            std::cout << "Http status: " << result.httpCode << std::endl;
            return;
        }

        std::string dlUrl = GetDownloadUrl(result.data);
        std::string modName = GetModNameFromDownloadUrl(dlUrl);
        if (modName == "")
        {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            modName = std::format("unknown-mod-{}-{}", ts.tv_sec, ts.tv_nsec / 100000);
        }

        auto& dlState = *it;
        dlState.fileName = modName;
        dlState.outFile = mgr->config.projectDir / "download" / modName;

        std::filesystem::create_directories(dlState.outFile.parent_path());

        FileWrapper outFile = fopen(dlState.outFile.c_str(), "wb");
        if (!outFile)
        {
            dlState.state = ModDlState::Error;
            return;
        }

        ModDownloadFinished dlFinished;
        dlFinished.mgr = mgr;
        dlFinished.fileId = fileId;
        dlFinished.modId = modId;
        auto downloadModTask = CreateTask<CurlEasyTask>(dlFinished);

        CURLU* url = curl_url();
        curl_url_set(url, CURLUPART_URL, dlUrl.c_str(), CURLU_ALLOW_SPACE | CURLU_URLENCODE);
        char* urlEncodedUrl = nullptr;
        curl_url_get(url, CURLUPART_URL, &urlEncodedUrl, 0);
        std::cout << urlEncodedUrl << std::endl;
        downloadModTask.task->SetUrl(urlEncodedUrl);
        curl_free(urlEncodedUrl);
        curl_url_cleanup(url);

        downloadModTask.task->file = std::move(outFile);
        downloadModTask.task->type = HttpType::Get;
        downloadModTask.task->ClearHeaders();
        if (mgr->verbose)
        {
            curl_easy_setopt(downloadModTask.task->ez, CURLOPT_VERBOSE, 1);
        }

        it->state = ModDlState::ModDownload;

        it->task = downloadModTask;
        downloadModTask.Start(mgr->curlEngine);
    }
};

// nxm://skyrimspecialedition/collections/vietdt/revisions/14
// nxm://skyrimspecialedition/mods/181164/files/757381?key=fgpPrnBzmIJVAjta6KHCEQ&expires=1780339902&user_id=28860775

void HandleNXMUrl(ModMgr& mgr, std::string const & urlStr)
{
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

    char* hostPart = nullptr;
    rc = curl_url_get(url, CURLUPART_HOST, &hostPart, 0);
    if (rc != CURLUE_OK)
    {
        return;
    }
    std::string hostStr = hostPart;
    curl_free(hostPart);

    std::vector<std::string> parts;
    for (auto&& part : pp)
    {
        parts.push_back(part);
    }

    // both mod and collection url have 5 path parts
    if (parts.size() != 5)
    {
        return;
    }

    if (parts[1] == "collections")
    {
        NxmCollectionUrl colUrl;
        colUrl.game = hostStr;
        colUrl.slug = parts[2];
        if (!str_to_int(parts[4], colUrl.rev))
        {
            return;
        }
        StartNXMCollectionInstall(mgr, colUrl);
    }
    else if (parts[1] == "mods")
    {
        NxmModFileUrl fileUrl;
        fileUrl.game = hostStr;
        if (!str_to_int(parts[2], fileUrl.modId))
        {
            return;
        }
        if (!str_to_int(parts[4], fileUrl.fileId))
        {
            return;
        }

        char* queryPart = nullptr;
        rc = curl_url_get(url, CURLUPART_QUERY, &queryPart, 0);
        if (rc != CURLUE_OK)
        {
            curl_free(queryPart);
            return;
        }
        std::string query(queryPart);
        curl_free(queryPart);
        fileUrl.key = extractQueryValue(query, "key=");
        fileUrl.expires = extractQueryValue(query, "expires=");
        StartNXMModDownload(mgr, fileUrl);
    }
}

void StartNXMModDownload(ModMgr& mgr, NxmModFileUrl const & url, std::string const & name, ModInstallType type)
{
    if (mgr.config.nexusApiKey.empty())
    {
        std::cout << "Nexus api key missing" << std::endl;
        return;
    }

    ModDownloadRt dl;

    dl.key = url.key;
    dl.expires = url.expires;
    dl.modId = url.modId;
    dl.fileId = url.fileId;
    dl.game = url.game;
    dl.hName = name;
    dl.installType = type;

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

    // these callbacks are on the main thread, because that's where we put the processor's update.
    ModUrlFinished taskFinished;
    taskFinished.mgr = &mgr;
    taskFinished.fileId = dl.fileId;
    taskFinished.modId = dl.modId;
    auto task = CreateTask<CurlEasyTask>(taskFinished);

    std::string dlInfoUrl;
    if (dl.expires.empty() || dl.key.empty())
    {
        dlInfoUrl = std::format("https://api.nexusmods.com/v1/games/{}/mods/{}/files/{}/download_link.json", dl.game, dl.modId, dl.fileId);
    }
    else
    {
        dlInfoUrl = std::format("https://api.nexusmods.com/v1/games/{}/mods/{}/files/{}/download_link.json?key={}&expires={}", dl.game, dl.modId, dl.fileId, dl.key, dl.expires);
    }
    task.task->SetUrl(dlInfoUrl.c_str());

    task.task->SetHeader("apikey", mgr.config.nexusApiKey);
    task.task->SetHeader("accept", "application/json");

    dl.state = ModDlState::UrlQuery;

    task.Start(mgr.curlEngine);

    mgr.downloadSessions.emplace_back(std::move(dl));
}


void UpdateDownloads(ModMgr& mgr)
{
    int rm = -1;
    for (int i = 0; i < mgr.downloadSessions.size(); ++i)
    {
        auto& dl = mgr.downloadSessions[i];
        if (dl.cancel)
        {
            dl.task.Stop();
        }
        else if (dl.pause)
        {
            // not implemented yet
        }
        else if (dl.remove)
        {
            if (rm == -1)
            {
                dl.task.Stop();
                rm = i;
            }
        }
    }

    if (rm != -1)
    {
        auto& dl = mgr.downloadSessions[rm];
        std::filesystem::path f = mgr.config.projectDir / "download" / dl.fileName;
        std::error_code rmErr;
        std::filesystem::remove(f, rmErr);
        mgr.downloadSessions.erase(mgr.downloadSessions.begin() + rm);
    }

    mgr.curlEngine->Update();
    mgr.processEngine->Update();

}

struct DecompressFileResult
{
    std::function<void(bool)> next;
    void operator()(AsyncProcessResult& result)
    {
        if (result.exitCode == 0)
        {
            if (next)
            {
                next(true);
            }
        }
        else
        {
            if (!next)
            {
                next(false);
            }
        }
    }
};

ProcessTask UnzipFile(std::filesystem::path const & file, std::filesystem::path const & dest, std::function<void(bool)> next)
{
    // check filetype

    // --print0 is not working
    std::vector<std::string> args = {
        "/usr/bin/file",
        // "-0",
        "-b",
        "--mime-type",
        std::string(file)
    };

    auto out = LaunchProcForOutput(args, "/");
    if (!out)
    {
        std::cout << "Failed to determine file type" << std::endl;
        return {};
    }

    std::vector<std::string> extractCmd;
    std::string type = *out;

    if (type == "application/x-7z-compressed\n" || type == "application/zip\n")
    {
        // 7z creates missing directories
        extractCmd = {
            "/usr/bin/7z",
            "-aos",
            "-bd",
            "-bsp0",
            "-bso0",
            "x",
            std::format("-o{}", std::string(dest)),
            std::string(file)
        };
    }
    // sometimes authors use a compression utility that implements PK > 4.1, gnu unzip doesnt support this.
    /*
    else if (type == "application/zip\n")
    {
        // unzip creates missing directories
        extractCmd = {
            "/usr/bin/unzip",
            "-n",
            "-q",
            "-d",
            dest,
            std::string(file)
        };
    }
    */
    else if (type == "application/x-rar\n")
    {
        extractCmd = {
            "/usr/bin/unrar",
            "-o-",
            "-idq",
            "x",
            std::format("-op{}", std::string(dest)),
            std::string(file)
        };
    }
    else
    {
        std::cout << "Unknown file type: " << type << std::endl;
        return {};
    }

    DecompressFileResult result;
    result.next = std::move(next);
    ProcessTask task = CreateTask<AsyncProcessTask>(result);
    task.task->args = std::move(extractCmd);
    return task;
}

bool FindFomod(std::filesystem::path const & modPath, std::filesystem::path& out, std::filesystem::path & realRoot)
{
    try
    {
        std::filesystem::path fomod_dir;
        bool foundFomod = false;
        for (auto it = std::filesystem::recursive_directory_iterator(modPath); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            std::string fn = it->path().filename();
            if (strcasecmp(fn.c_str(), "fomod") == 0)
            {
                if (it->is_directory())
                {
                    fomod_dir = it->path();
                    foundFomod = true;
                }
                break;
            }
        }

        if (!foundFomod)
        {
            return false;
        }

        // find ModuleConfig.xml
        bool foundFomodConf = false;
        std::filesystem::path fomodMC;
        if (foundFomod)
        {
            std::filesystem::directory_iterator dit(fomod_dir);
            for (; dit != std::filesystem::directory_iterator(); ++dit)
            {
                std::string fn = dit->path().filename();
                int m = strcasecmp(fn.c_str(), "ModuleConfig.xml");
                if (m == 0)
                {
                    fomodMC = dit->path();
                    foundFomodConf = true;
                    break;
                }
            }
        }

        if (foundFomodConf && std::filesystem::is_regular_file(fomodMC))
        {
            out = fomodMC;
            realRoot = fomod_dir.parent_path();
            return true;
        }
    }
    catch (std::filesystem::filesystem_error const & er)
    {
        std::cout << er.what() << std::endl;
        std::cout << er.path1() << std::endl;
        std::cout << er.path2() << std::endl;
    }
    return false;
}

void InstallDownloadedFile(ModMgr& mgr, int fileId, int modId, FomodAuto::Config const & conf)
{
    if (mgr.cookingInstall.has_value())
    {
        return;
    }

    auto dl = std::find_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [&](ModDownloadRt const & dlr)
    {
        return dlr.modId == modId && dlr.fileId == fileId;
    });

    // already installed, skip
    if (dl == mgr.downloadSessions.end())
    {
        return;
    }
    
    std::string fileStem = std::filesystem::path(dl->fileName).stem();
    std::filesystem::path staging = mgr.config.projectDir / ".mod_staging";
    std::filesystem::path inFile = mgr.config.projectDir / "download" / dl->fileName;

    auto task = UnzipFile(inFile, staging / fileStem, [mgr = &mgr, staging, fileStem, confCap = conf, fileId, modId](bool succeeded) {
        if (!succeeded)
        {
            mgr->cookingInstall.reset();
            return;
        }

        std::filesystem::path modFolder = *WordExpand(mgr->config.modFolder);
        std::filesystem::create_directories(modFolder / fileStem);
        std::filesystem::path installDest = modFolder / fileStem;

        auto dl = std::find_if(mgr->downloadSessions.begin(), mgr->downloadSessions.end(), [&](ModDownloadRt const & dlr)
        {
            return dlr.modId == modId && dlr.fileId == fileId;
        });

        if (dl->installType == ModInstallType::Data)
        {
            installDest /= "Data";
        }
        else if (dl->installType == ModInstallType::Root)
        {
            ; // noop
        }

        std::filesystem::path fomodPath;
        std::filesystem::path realModRoot;
        bool isFomod = FindFomod(staging / fileStem, fomodPath, realModRoot);

        if (isFomod)
        {
            auto fmopt = fomod::Load(fomodPath);
            if (!fmopt)
            {
                mgr->cookingInstall.reset();
                return;
            }

            FomodAuto::Config conf = confCap;
            fomod::Eval eval;

            while (true)
            {
                fomod::SubstepInfo ss = fomod::PrepareSubstep(*fmopt, eval);
                // TODO check file status?
                bool visible = fomod::EvalSubstep(*fmopt, eval, ss);
                if (visible)
                {
                    auto* step = conf.GetStep(ss.stepName);
                    if (step)
                    {
                        auto* group = step->GetGroup(ss.name);
                        if (group)
                        {
                            for (auto&& choice : ss.options)
                            {
                                auto* c = group->GetChoice(choice.name);
                                choice.selected = false;
                                if (c)
                                {
                                    choice.selected = true;
                                }
                            }
                        }
                    }
                    bool applied = fomod::ApplySubstep(*fmopt, eval, ss);
                    if (!applied)
                    {
                        std::cout << "Invalid fomod preset!" << std::endl;
                        mgr->cookingInstall.reset();
                        return;
                    }
                }

                if (fomod::Configured(*fmopt, eval))
                {
                    break;
                }
            }
            fomod::SubstepInfo ss = fomod::PrepareInstallActions(*fmopt, eval);
            // TODO check file status?
            auto actions = fomod::GetInstallActions(*fmopt, eval, ss);

            // perform file actions
            ApplyFomodFileActions(*mgr, actions, realModRoot, installDest);

            // create mod entry
            auto& mod = mgr->inst.mods.emplace_back();
            mod.enabled = true;
            mod.loadIndex = mgr->inst.mods.size() - 1;
            mod.modFile = fileStem;
            mod.fileId = fileId;
            mod.modId = modId;
            mod.hName = dl->hName;
        }
        else
        {
            // otherwise create the mod entry and move mod into it
            auto& mod = mgr->inst.mods.emplace_back();
            mod.enabled = true;
            mod.loadIndex = mgr->inst.mods.size() - 1;
            mod.modFile = fileStem;
            mod.fileId = fileId;
            mod.modId = modId;
            mod.hName = dl->hName;
            
            MoveDirNormalizePaths(staging / fileStem, installDest);
        }

        // remove staging
        std::vector<std::string> rmCmd = {
            "/usr/bin/rm",
            "-r",
            staging / fileStem,
        };
        if (!LaunchProc(rmCmd, "/"))
        {
            std::cout << "Failed to delete mod staging dir: " << staging << std::endl;
        }

        mgr->cookingInstall.reset();

        DiscoverPlugins(*mgr);

        SaveModMgr(*mgr);
    });

    mgr.cookingInstall = task;
    task.Start(mgr.processEngine);
}

void InstallDownloadedFile(ModMgr& mgr, std::string const & modName)
{
    if (mgr.fomodState.has_value() || mgr.cookingInstall)
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

    {
        auto im = std::find_if(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](ModInfo const & m){
            return m.fileId == dl->fileId && m.modId == dl->modId;
        });

        if (im != mgr.inst.mods.end())
        {
            std::cout << "Mod already installed: " << modName << std::endl;
            return;
        }
    }

    // check filetype

    // --print0 is not working
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

    std::filesystem::path staging = mgr.config.projectDir / ".mod_staging";
    std::vector<std::string> extractCmd;
    std::string type = *out;

    std::string fileStem = std::filesystem::path(modName).stem();

    if (type == "application/x-7z-compressed\n")
    {
        // 7z creates missing directories
        extractCmd = {
            "/usr/bin/7z",
            "x",
            std::format("-o{}", std::string(staging / fileStem)),
            mgr.config.projectDir / "download" / modName
        };
    }
    else if (type == "application/zip\n")
    {
        // unzip creates missing directories
        extractCmd = {
            "/usr/bin/unzip",
            "-d",
            staging / fileStem,
            mgr.config.projectDir / "download" / modName
        };
    }
    else if (type == "application/x-rar\n")
    {
        extractCmd = {
            "/usr/bin/unrar",
            "x",
            std::format("-op{}", std::string(staging / fileStem)),
            mgr.config.projectDir / "download" / modName
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

    std::filesystem::path fomodPath;
    std::filesystem::path realModRoot;
    bool isFomod = FindFomod(staging / fileStem, fomodPath, realModRoot);

    std::filesystem::path modFolder = *WordExpand(mgr.config.modFolder);
    std::filesystem::create_directories(modFolder / fileStem);
    std::filesystem::path installDest = modFolder / fileStem;
    if (dl->installType == ModInstallType::Data)
    {
        installDest /= "Data";
    }
    else if (dl->installType == ModInstallType::Root)
    {
        ; // noop
    }

    if (isFomod)
    {
        ModFileRef fileRef;
        fileRef.modId = dl->modId;
        fileRef.fileId = dl->fileId;
        InitFomod(mgr, staging / fileStem, realModRoot, fomodPath, installDest, fileRef);
    }
    else
    {
        if (MoveDirNormalizePaths(staging / fileStem, installDest))
        {
            auto& mod = mgr.inst.mods.emplace_back();
            mod.enabled = true;
            mod.loadIndex = mgr.inst.mods.size() - 1;
            mod.modFile = fileStem;
            mod.fileId = dl->fileId;
            mod.modId = dl->modId;
            mod.hName = dl->hName;

            // remove staging
            std::vector<std::string> rmCmd = {
                "/usr/bin/rm",
                "-r",
                staging / fileStem
            };
            if (!LaunchProc(rmCmd, "/"))
            {
                std::cout << "Failed to delete fomod tmp dir: " << staging / fileStem << std::endl;
            }

            DiscoverPlugins(mgr);
            
            SaveModMgr(mgr);
        }
    }

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

    mgr.curlEngine = std::make_shared<AsyncTaskProcessor<CurlAsyncEngine>>();
    mgr.processEngine = std::make_shared<AsyncTaskProcessor<AsyncProcessEngine>>();
}

void CleanupMgr(ModMgr& mgr)
{
    for (auto&& task : mgr.downloadSessions)
    {
        task.task.Stop();
    }

    if (mgr.cookingInstall)
    {
        mgr.cookingInstall->Stop();
    }
    mgr.cookingInstall.reset();

    mgr.downloadSessions.clear();

    mgr.curlEngine.reset();

    curl_global_cleanup();
}




