
#include "modmgr_collection.h"

#include "curlasync.h"
#include "modmgr.h"

#include "intrusive/dg.h"

#include <format>
#include <iostream>
#include <fstream>
#include <unordered_map>

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

    std::cout << "Fetching collection data: " << url.game << "-" << url.slug << "-" << url.rev << std::endl;

    mgr.collection.url = url;
    mgr.collection.status = CollectionStatus::FetchingInfo;

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
        if (mgr.collection.error)
        {
            mgr.collection.status = CollectionStatus::InstallWaitingFailedMods;
        }
        else
        {
            mgr.collection.status = CollectionStatus::ConfigureLoadOrder;
        }
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

    std::optional<ManualInstallConfig> manualConfig;
    std::optional<FomodAuto::Config> fomodConfig;
    bool confGood = true;
    if (modInfo.contains("choices"))
    {
        fomodConfig = FomodAuto::Config();
        try
        {
            for (auto &&step : modInfo["choices"]["options"])
            {
                auto &stepData = fomodConfig->steps.emplace_back();
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
    else if (modInfo.contains("hashes"))
    {
        manualConfig = ManualInstallConfig();
        for (auto&& item : modInfo["hashes"])
        {
            auto& m = manualConfig->paths.emplace_back();
            m.md5 = item["md5"].get<std::string>();
            m.path = item["path"].get<std::string>();
        }
    }


    if (confGood)
    {
        InstallDownloadedFile(mgr, fileId, modId, fomodConfig, manualConfig);
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

struct PluginDepNode
{
    std::string group;
    std::string pluginName;
    bool dummy = false;
    intrusive::dg_inject<PluginDepNode> dg;
};

void ApplyCollectionLoadOrder(ModMgr &mgr)
{
    // mod order
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
                std::cout << "Ignoring 'conflicts' load rule for: " << std::endl;
                if (rule["reference"].contains("logicalFileName"))
                {
                    std::cout << "    " << rule["reference"]["logicalFileName"].get<std::string>();
                }
                else if (rule["reference"].contains("fileExpression"))
                {
                    std::cout << "    " << rule["reference"]["fileExpression"].get<std::string>();
                }
                if (rule["reference"].contains("versionMatch"))
                {
                    std::cout << "    " << rule["reference"]["versionMatch"].get<std::string>();
                }
                std::cout << std::endl;

                if (rule["source"].contains("logicalFileName"))
                {
                    std::cout << "    " << rule["source"]["logicalFileName"].get<std::string>();
                }
                else if (rule["source"].contains("fileExpression"))
                {
                    std::cout << "    " << rule["source"]["fileExpression"].get<std::string>();
                }
                if (rule["source"].contains("versionMatch"))
                {
                    std::cout << "    " << rule["source"]["versionMatch"].get<std::string>();
                }

                std::cout << std::endl;
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
                    ++i;
                }
                else
                {
                    std::cout << "Failed to set load index " << i << " for " << n->name;
                }
                n = list.resolve(n)->right;
            }
        }
    }

    // plugins

    int p = 0;
    for (auto&& plugin : mgr.collection.bundleDefinition["plugins"])
    {
        std::string pluginName = plugin["name"].get<std::string>();
        bool enabled = plugin["enabled"].get<bool>();

        for (int fii = 0; fii < mgr.inst.pluginList.size(); ++fii)
        {
            if (0 == strcasecmp(mgr.inst.pluginList[fii].pluginName.c_str(), pluginName.c_str()))
            {
                mgr.inst.pluginList[fii].enabled = enabled;
                break;
            }
        }
    }

    // sort plugins

    if (mgr.collection.bundleDefinition.contains("pluginRules"))
    {
        std::unordered_map<std::string, std::unique_ptr<PluginDepNode>> pluginNodes;
        std::unordered_map<std::string, std::vector<std::string>> pluginGroups;
        std::vector<std::unique_ptr<intrusive::dg_edge<PluginDepNode>>> edges;
        intrusive::directed_graph<PluginDepNode, &PluginDepNode::dg> dg;

        for (auto&& pluginRule : mgr.collection.bundleDefinition["pluginRules"]["plugins"])
        {
            std::unique_ptr<PluginDepNode> node = std::make_unique<PluginDepNode>();
            std::string name = pluginRule["name"].get<std::string>();
            name = NormalizePath(name);
            std::string group;
            if (pluginRule.contains("group"))
            {
                group = pluginRule["group"].get<std::string>();
            }
            else
            {
                // apparently the default group is not explicitly assigned
                group = "default";
            }
            node->group = group;
            auto it = pluginGroups.find(group);
            if (it == pluginGroups.end())
            {
                it = pluginGroups.emplace(group, std::vector<std::string>{}).first;
            }
            it->second.emplace_back(name);
            node->pluginName = name;
            pluginNodes.emplace(name, std::move(node));
        }

        for (auto&& plugin : mgr.collection.bundleDefinition["plugins"])
        {
            std::string pname = plugin["name"].get<std::string>();
            pname = NormalizePath(pname);
            auto it = pluginNodes.find(pname);
            if (it == pluginNodes.end())
            {
                auto node = std::make_unique<PluginDepNode>();
                node->pluginName = pname;
                node->group = "default";
                pluginNodes.emplace(pname, std::move(node));
            }
        }
        

        for (auto&& node : pluginNodes)
        {
            dg.add(node.second.get());
        }

        for (auto&& pluginRule : mgr.collection.bundleDefinition["pluginRules"]["plugins"])
        {
            std::string pluginName = pluginRule["name"].get<std::string>();
            pluginName = NormalizePath(pluginName);
            auto src = pluginNodes.find(pluginName);
            if (pluginRule.contains("after"))
            {
                for (auto&& after : pluginRule["after"])
                {
                    auto ne = std::make_unique<intrusive::dg_edge<PluginDepNode>>();
                    auto ref = pluginNodes.find(after.get<std::string>());
                    if (ref != pluginNodes.end())
                    {
                        ne->to = ref->second.get();
                        ne->from = src->second.get();
                        dg.addEdge(ne.get());
                        edges.emplace_back(std::move(ne));
                    }
                }
            }
        }

        auto addGroupRules = [&](std::string const & groupName, std::string const & afterName)
        {
            auto gSrcIt = pluginGroups.find(groupName);
            if (gSrcIt == pluginGroups.end())
            {
                std::cout << "Failed to find plugin group: " << groupName << std::endl;
                auto dummy = std::make_unique<PluginDepNode>();
                dummy->dummy = true;
                dummy->group = groupName;
                std::string dummyPluginName = std::format("dummy-{}", groupName);
                dg.add(dummy.get());
                pluginNodes.emplace(dummyPluginName, std::move(dummy));
                pluginGroups.emplace(groupName, std::vector<std::string>{dummyPluginName});
            }
            auto gRefIt = pluginGroups.find(afterName);
            if (gRefIt == pluginGroups.end())
            {
                std::cout << "Failed to find plugin group: " << afterName << std::endl;
                auto dummy = std::make_unique<PluginDepNode>();
                dummy->dummy = true;
                dummy->group = afterName;
                std::string dummyPluginName = std::format("dummy-{}", afterName);
                dg.add(dummy.get());
                pluginNodes.emplace(dummyPluginName, std::move(dummy));
                gRefIt = pluginGroups.emplace(afterName, std::vector<std::string>{dummyPluginName}).first;
            }
            // re-fetch in case of rehash
            gSrcIt = pluginGroups.find(groupName);
            if (gRefIt != pluginGroups.end() && gSrcIt != pluginGroups.end())
            {
                for (auto&& srcName : gSrcIt->second)
                {
                    for (auto&& refName : gRefIt->second)
                    {
                        auto srcIt = pluginNodes.find(srcName);
                        auto refIt = pluginNodes.find(refName);
                        if (srcIt != pluginNodes.end() && refIt != pluginNodes.end())
                        {
                            //std::cout << srcIt->second->group << "  ->  " << refIt->second->group << std::endl;
                            auto ne = std::make_unique<intrusive::dg_edge<PluginDepNode>>();
                            ne->from = refIt->second.get();
                            ne->to = srcIt->second.get();
                            dg.addEdge(ne.get());
                            edges.emplace_back(std::move(ne));
                        }
                        else
                        {
                            std::cout << "Error" << std::endl;
                        }
                    }
                }
            }
        };

        for (auto&& groupRule : mgr.collection.bundleDefinition["pluginRules"]["groups"])
        {
            std::string groupName = groupRule["name"];
            // doesnt look like there's a before
            if (groupRule.contains("after"))
            {
                for (auto&& groupAfter : groupRule["after"])
                {
                    addGroupRules(groupName, groupAfter.get<std::string>());
                }
            }
        }

        addGroupRules("default", "Early Loaders");
        addGroupRules("Late Loaders", "default");

        decltype(dg)::node_list list;
        bool r = dg.topo_sort(list);

        if (!r)
        {
            std::cout << "Failed to sort plugins" << std::endl;
            mgr.collection.error = true;
            return;
        }
        else
        {
            int i = 0;
            PluginDepNode* n = list.head;
            while (n)
            {
                if (!n->dummy)
                {
                    int pi = -1;
                    for (int pii = 0; pii < mgr.inst.pluginList.size(); ++pii)
                    {
                        if (mgr.inst.pluginList[pii].pluginName == n->pluginName)
                        {
                            pi = pii;
                            break;
                        }
                    }
                    if (pi != -1)
                    {
                        if (i != pi && i < mgr.inst.pluginList.size())
                        {
                            std::swap(mgr.inst.pluginList[i], mgr.inst.pluginList[pi]);
                        }
                        ++i;
                    }
                    else
                    {
                        std::cout << "Failed to set plugin " << n->pluginName << " to load index " << i << std::endl;
                    }
                }
                n = list.resolve(n)->right;
            }
        }

    }
    

    mgr.collection.status = CollectionStatus::Installed;

}
