
#include "modmgr_collection.h"

#include "curlasync.h"
#include "modmgr.h"

#include <iostream>

struct ColInfoDlFinished
{
    ModMgr* mgr = nullptr;
    NxmCollectionUrl colUrl;
    void operator()(CurlEasyTaskResult& result);
};


constexpr char NXM_GQL_ENDPOINT[] = "https://api.nexusmods.com/v2/graphql";

bool str_to_int64(std::string const & str, int64_t& out)
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

void ColInfoDlFinished::operator()(CurlEasyTaskResult& result)
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
        auto const & data = jr.at("data").at("collectionRevision");
        auto const & col = data.at("collection");

        str_to_int64(data.at("totalSize").get<std::string>(), mgr->collection.info.totalSize);
        mgr->collection.info.modCount = data.at("modCount").get<int>();
        mgr->collection.info.downloadLink = data.at("downloadLink").get<std::string>();
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

// TODO add markdown renderer: https://github.com/enkisoftware/imgui_markdown.git

void StartNXMCollectionInstall(ModMgr& mgr, NxmCollectionUrl const & url)
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
        {"revision", url.rev}
    };

    auto task = CreateTask<CurlEasyTask>(std::move(f));

    std::stringstream ss;
    ss << payload;
    task.task->postDataStr = ss.str();

    std::cout << task.task->postDataStr << std::endl;
    
    task.task->type = HttpType::Post;
    task.task->SetHeader("apikey", mgr.config.nexusApiKey);
    task.task->SetUrl(NXM_GQL_ENDPOINT);
    task.task->contentType = "application/json";

    task.Start(mgr.curlEngine);

}

