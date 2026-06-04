
#include "modmgr_collection.h"

#include "curlasync.h"
#include "modmgr.h"

#include "intrusive/dg.h"

#include <format>
#include <iostream>
#include <fstream>

struct ColInfoDlFinished
{
    ModMgr *mgr = nullptr;
    NxmCollectionUrl colUrl;
    void operator()(CurlEasyTaskResult &result);
};

struct ColLinkDlFinished
{
    ModMgr *mgr = nullptr;
    void operator()(CurlEasyTaskResult &result);
};

struct ColBundleDlFinished
{
    ModMgr *mgr = nullptr;
    void operator()(CurlEasyTaskResult &result);
};

constexpr char NXM_GQL_ENDPOINT[] = "https://api.nexusmods.com/v2/graphql";

bool str_to_int64(std::string const &str, int64_t &out)
{
    try
    {
        size_t end = 0;
        int64_t i = std::stoll(str, &end, 10);
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

void ColInfoDlFinished::operator()(CurlEasyTaskResult &result)
{
    if (result.canceled || result.cError != CURLE_OK || result.httpCode != 200)
    {
        std::cout << "Failed to fetch collection info: " << result.cError << " " << result.httpCode << " " << result.canceled << std::endl;
        mgr->collection.error = true;
        return;
    }

    nlohmann::json jr = nlohmann::json::parse(result.data, nullptr, false);
    if (!jr.is_object())
    {
        std::cout << "Invalid response" << std::endl;
        mgr->collection.error = true;
        return;
    }
    else if (jr.contains("errors"))
    {
        try
        {
            std::string errMsg = jr.at("errors").at(0).at("message");
            std::cout << "gql error: " << errMsg << std::endl;
        }
        catch (...)
        {
            std::cout << "Unknown error: " << result.httpCode << std::endl;
        }
    }

    try
    {
        auto const &data = jr.at("data").at("collectionRevision");
        auto const &col = data.at("collection");

        str_to_int64(data.at("totalSize").get<std::string>(), mgr->collection.info.totalSize);
        mgr->collection.info.modCount = data.at("modCount").get<int>();
        mgr->collection.info.downloadLinkLink = data.at("downloadLink").get<std::string>();
        mgr->collection.info.description = col.at("description").get<std::string>();
        mgr->collection.info.name = col.at("name").get<std::string>();
        mgr->collection.info.summary = col.at("summary").get<std::string>();

        mgr->collection.status = CollectionStatus::WaitingForInstallButton;
    }
    catch (...)
    {
        mgr->collection.error = true;
    }
}

void ColLinkDlFinished::operator()(CurlEasyTaskResult &result)
{
    if (result.canceled || result.cError != CURLE_OK || result.httpCode != 200)
    {
        std::cout << "Failed to fetch collection bundle link: " << result.cError << " " << result.httpCode << " " << result.canceled << std::endl;
        mgr->collection.error = true;
        return;
    }

    auto jr = nlohmann::json::parse(result.data, nullptr, false);
    if (!jr.is_object())
    {
        std::cout << "Invalid json" << std::endl;
        mgr->collection.error = true;
        return;
    }
    std::string link;
    try
    {
        link = jr.at("download_links").at(0).at("URI").get<std::string>();
    }
    catch (...)
    {
        std::cout << "Failed to extract download link" << std::endl;
        mgr->collection.error = true;
        return;
    }
    mgr->collection.info.downloadLink = link;
    mgr->collection.status = CollectionStatus::DownloadingBundle;

    DownloadCollectionBundle(*mgr);
}

void ColBundleDlFinished::operator()(CurlEasyTaskResult &result)
{
    if (result.canceled || result.cError != CURLE_OK || result.httpCode != 200)
    {
        std::cout << "Failed to download collection bundle: " << result.cError << " " << result.httpCode << " " << result.canceled << std::endl;
        mgr->collection.error = true;
        return;
    }

    result.file.destroy();

    std::string ccname = std::format("{}-{}", mgr->collection.url.slug, mgr->collection.url.rev);
    std::filesystem::path outDir = mgr->config.projectDir / ".mod_staging" / ccname;
    std::vector<std::string> extractCmd = {
        "/usr/bin/7z",
        "-aos",
        "-bd",
        "-bsp0",
        "-bso0",
        "x",
        std::format("-o{}", std::string(outDir)),
        mgr->config.projectDir / "download" / ccname};

    if (!LaunchProc(extractCmd, "/"))
    {
        std::cout << "Failed to extract bundle" << std::endl;
        mgr->collection.error = true;
        return;
    }

    DownloadCollectionMods(*mgr);
}

// TODO add markdown renderer: https://github.com/enkisoftware/imgui_markdown.git

void StartNXMCollectionInstall(ModMgr &mgr, NxmCollectionUrl const &url)
{
    if (mgr.collection.status != CollectionStatus::None)
    {
        return;
    }
    mgr.collection.status = CollectionStatus::FetchingInfo;
    mgr.collection.url = url;

    ColInfoDlFinished f;
    f.mgr = &mgr;
    f.colUrl = url;

    nlohmann::json payload;
    payload["query"] = R"(query GetColRevInfo(
        $slug: String,
        $revision: Int,
        $viewAdultContent: Boolean,
        $domain: String
    ) {
        collectionRevision(slug: $slug, viewAdultContent: $viewAdultContent, domainName: $domain, revision: $revision) {
            totalSize,
            modCount,
            downloadLink,
            collection {
                description,
                name,
                summary,
                tileImage {
                    url
                },
                headerImage {
                    url
                }
            }
        }
    })";
    payload["operationName"] = "GetColRevInfo";
    payload["variables"] = {
        {"slug", url.slug},
        {"viewAdultContent", true},
        {"domain", url.game},
        {"revision", url.rev}};

    auto task = CreateTask<CurlEasyTask>(std::move(f));

    std::stringstream ss;
    ss << payload;
    task.task->postDataStr = ss.str();

    task.task->type = HttpType::Post;
    task.task->SetHeader("apikey", mgr.config.nexusApiKey);
    task.task->SetUrl(NXM_GQL_ENDPOINT);
    task.task->contentType = "application/json";

    task.Start(mgr.curlEngine);
}

void GetCollectionBundleLink(ModMgr &mgr)
{
    ColLinkDlFinished res;
    res.mgr = &mgr;

    auto task = CreateTask<CurlEasyTask>(std::move(res));
    task.task->SetHeader("apikey", mgr.config.nexusApiKey);
    std::string url = std::format("https://api.nexusmods.com{}", mgr.collection.info.downloadLinkLink);
    task.task->SetUrl(url.c_str());
    task.task->type = HttpType::Get;

    mgr.collection.status = CollectionStatus::FetchingBundleLink;
    task.Start(mgr.curlEngine);
}

void DownloadCollectionBundle(ModMgr &mgr)
{
    std::cout << "Downloading collection bundle at " << mgr.collection.info.downloadLink << std::endl;

    ColBundleDlFinished res;

    res.mgr = &mgr;

    auto task = CreateTask<CurlEasyTask>(std::move(res));
    task.task->SetHeader("apikey", mgr.config.nexusApiKey);
    task.task->SetUrl(mgr.collection.info.downloadLink.c_str());
    task.task->type = HttpType::Get;
    std::filesystem::path colbundlePath = mgr.config.projectDir / "download" / std::format("{}-{}", mgr.collection.url.slug, mgr.collection.url.rev);
    std::cout << "Saving bundle at " << colbundlePath << std::endl;
    std::filesystem::create_directories(colbundlePath.parent_path());
    FileWrapper fw = fopen(colbundlePath.c_str(), "wb");
    if (!fw)
    {
        std::cout << "Failed to open bundle file" << std::endl;
        return;
    }
    task.task->file = std::move(fw);
    mgr.collection.status = CollectionStatus::DownloadingBundle;
    task.Start(mgr.curlEngine);
}

void DownloadCollectionMods(ModMgr &mgr)
{
    auto bundlePath = mgr.config.projectDir / ".mod_staging" / std::format("{}-{}", mgr.collection.url.slug, mgr.collection.url.rev) / "collection.json";
    std::ifstream bundleFile(bundlePath);
    if (!bundleFile)
    {
        std::cout << "Failed to load collection definition " << bundlePath << std::endl;
        mgr.collection.error = true;
        return;
    }

    mgr.collection.bundleDefinition = nlohmann::json::parse(bundleFile, nullptr, false);
    bundleFile.close();
    if (mgr.collection.bundleDefinition.is_discarded())
    {
        std::cout << "Invalid json in bundle definition" << std::endl;
        mgr.collection.error = true;
        return;
    }

    std::string game = mgr.collection.bundleDefinition["info"]["domainName"];

    for (auto &&mod : mgr.collection.bundleDefinition["mods"])
    {
        if (mod["source"]["type"].get<std::string>() != "nexus")
        {
            std::cout << "Skipping non-nexus mod: " << mod["name"].get<std::string>();
            continue;
        }
        int modId = mod["source"]["modId"].get<int>();
        int fileId = mod["source"]["fileId"].get<int>();
        auto it = std::find_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [modId, fileId](ModDownloadRt const &dl)
                               { return dl.fileId == fileId && dl.modId == modId; });
        std::string type = mod["details"]["type"].get<std::string>();
        ModInstallType installType = ModInstallType::Data;
        if (type == "dinput" || type == "enb")
        {
            installType = ModInstallType::Root;
        }
        else if (!type.empty())
        {
            std::cout << "Unknown install type: " << type << std::endl;
        }
        std::string modDomain = mod["domainName"].get<std::string>();
        if (it == mgr.downloadSessions.end())
        {
            NxmModFileUrl fileUrl;
            fileUrl.game = modDomain;
            fileUrl.fileId = fileId;
            fileUrl.modId = modId;
            StartNXMModDownload(mgr, fileUrl, mod["name"].get<std::string>(), installType);
        }
        else if (it->state == ModDlState::ModPaused)
        {
            // pause doenst do anything yet but just in case
            it->unpause = true;
        }
        else if (it->state != ModDlState::Complete && it->state != ModDlState::ModDownload && it->state != ModDlState::UrlQuery)
        {
            // sneakily rename and delete it so we can try to redownload it
            it->remove = true;
            it->modId = -1;
            it->fileId = -1;

            NxmModFileUrl fileUrl;
            fileUrl.game = modDomain;
            fileUrl.fileId = fileId;
            fileUrl.modId = modId;
            StartNXMModDownload(mgr, fileUrl, mod["name"].get<std::string>(), installType);
        }
        else
        {
            it->fileId = fileId;
            ;
            it->modId = modId;
            it->hName = mod["name"].get<std::string>();
        }
    }

    mgr.collection.status = CollectionStatus::DownloadingMods;
}

void UpdateInstallCollectionMods(ModMgr &mgr)
{
    if (mgr.cookingInstall.has_value() || mgr.fomodState.has_value())
    {
        return;
    }
    mgr.collection.installingCurrentMod.clear();
    int nextMod = mgr.collection.installIndex + 1;
    if (nextMod >= mgr.collection.bundleDefinition["mods"].size())
    {
        mgr.collection.installIndex = -1;
        mgr.collection.status = CollectionStatus::ConfigureLoadOrder;
        return;
    }
    ++mgr.collection.installIndex;
    auto modInfo = mgr.collection.bundleDefinition["mods"][nextMod];

    int modId = modInfo["source"]["modId"].get<int>();
    int fileId = modInfo["source"]["fileId"].get<int>();

    auto it = std::find_if(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](ModInfo const &m)
                           { return m.modId == modId && m.fileId == fileId; });

    if (it != mgr.inst.mods.end())
    {
        std::cout << "Skipping installed mod: " << modInfo["name"].get<std::string>() << std::endl;
        return;
    }

    mgr.collection.installingCurrentMod = modInfo["name"].get<std::string>();
    std::cout << "Intalling mod: " << mgr.collection.installingCurrentMod << std::endl;

    FomodAuto::Config fomodConfig;
    bool confGood = true;
    if (modInfo.contains("choices"))
    {
        try
        {
            for (auto &&step : modInfo["choices"]["options"])
            {
                auto &stepData = fomodConfig.steps.emplace_back();
                stepData.name = step["name"].get<std::string>();
                for (auto &&group : step["groups"])
                {
                    auto &groupData = stepData.groups.emplace_back();
                    groupData.name = group["name"].get<std::string>();
                    for (auto &&choice : group["choices"])
                    {
                        auto &choiceData = groupData.choices.emplace_back();
                        choiceData.index = choice["idx"].get<int>();
                        choiceData.name = choice["name"].get<std::string>();
                    }
                }
            }
        }
        catch (...)
        {
            confGood = false;
            mgr.collection.installErrorInfo.emplace_back(modInfo["name"].get<std::string>());
        }
    }

    if (confGood)
    {
        InstallDownloadedFile(mgr, fileId, modId, fomodConfig);
    }
    else
    {
        std::cout << "Failed to load fomod configuration for " << it->modFile << std::endl;
    }
}

struct ModDepNode
{
    intrusive::dg_inject<ModDepNode> dg;
    ModFileRef mod;
    std::string name;
};

void ApplyCollectionLoadOrder(ModMgr &mgr)
{
    std::vector<ModDepNode> nodes;
    intrusive::directed_graph<ModDepNode, &ModDepNode::dg> dg;
    std::vector<std::unique_ptr<intrusive::dg_edge<ModDepNode>>> edges;

    for (auto &&mod : mgr.inst.mods)
    {
        ModDepNode &dn = nodes.emplace_back();
        dn.name = mod.modFile;
        dn.mod.fileId = mod.fileId;
        dn.mod.modId = mod.modId;
    }

    for (auto&& node : nodes)
    {
        dg.add(&node);
    }

    for (auto &&rule : mgr.collection.bundleDefinition["modRules"])
    {
        std::string type = rule["type"];
        if (type == "conflicts")
        {
            std::cout << "Ignoring 'conflicts' load rule" << std::endl;
            continue;
        }
        else if (type != "before" && type != "after")
        {
            std::cout << "Unknown load rule: " << type << std::endl;
            continue;
        }
        std::string srcFile = rule["source"]["fileExpression"].get<std::string>();
        std::string refFile = rule["reference"]["fileExpression"].get<std::string>();
        auto refIt = std::find_if(nodes.begin(), nodes.end(), [&](ModDepNode const & n) {
            return n.name == refFile;
        });
        auto srcIt = std::find_if(nodes.begin(), nodes.end(), [&](ModDepNode const & n) {
            return n.name == srcFile;
        });
        if (refIt == nodes.end())
        {
            std::cout << "Failed to find mod for load rule: " << refFile << std::endl;
            continue;
        }
        if (srcIt == nodes.end())
        {
            std::cout << "Failed to find mod for load rule: " << srcFile << std::endl;
            continue;
        }
        
        if (type == "after")
        {
            auto edge = std::make_unique<intrusive::dg_edge<ModDepNode>>();
            edge->from = &*refIt;
            edge->to = &*srcIt;
            dg.addEdge(edge.get());
            edges.emplace_back(std::move(edge));
        }
        else if (type == "before")
        {
            auto edge = std::make_unique<intrusive::dg_edge<ModDepNode>>();
            edge->from = &*srcIt;
            edge->to = &*refIt;
            dg.addEdge(edge.get());
            edges.emplace_back(std::move(edge));
        }
    }

    decltype(dg)::node_list list;
    bool r = dg.topo_sort(list);

    if (!r)
    {
        std::cout << "Failed to sort load order" << std::endl;
        mgr.collection.error = true;
        return;
    }
    else
    {
        int i = 0;
        ModDepNode* n = list.head;
        while (n)
        {
            auto mit = std::find_if(mgr.inst.mods.begin(), mgr.inst.mods.end(), [&](ModInfo const & mi) {
                return mi.modFile == n->name;
            });
            if (mit != mgr.inst.mods.end())
            {
                mit->loadIndex = i;
            }
            else
            {
                std::cout << "Failed to set load index " << i << " for " << n->name;
            }
            ++i;
            n = list.resolve(n)->right;
        }
    }

    // plugins

    int p = 0;
    for (auto&& plugin : mgr.collection.bundleDefinition["plugins"])
    {
        std::string pluginName = plugin["name"].get<std::string>();
        bool enabled = plugin["enabled"].get<bool>();

        int fi = -1;
        for (int fii = 0; fii < mgr.inst.pluginList.size(); ++fii)
        {
            if (0 == strcasecmp(mgr.inst.pluginList[fii].pluginName.c_str(), pluginName.c_str()))
            {
                fi = fii;
                break;
            }
        }
        if (fi == -1)
        {
            std::cout << "Failed to sort plugin: " << pluginName << std::endl;
        }
        else if (fi != p)
        {
            std::swap(mgr.inst.pluginList[p], mgr.inst.pluginList[fi]);
        }
    }
    

    mgr.collection.status = CollectionStatus::Installed;

}
