
#pragma once

#include "nxmurl.h"
#include "nlohmann/json.hpp"

#include <string>
#include <stdint.h>

struct ModMgr;

enum class CollectionStatus
{
    None,
    FetchingInfo,
    WaitingForInstallButton,
    FetchingBundleLink,
    DownloadingBundle,
    DownloadingMods,
    InstallingMods,
    InstallWaitingFailedMods,
    ConfigureLoadOrder,
    Installed
};

struct CollectionInfo
{
    int64_t totalSize = 0;
    int modCount = 0;
    // link to get the link for the download (api.nxm.com/v2/...)
    std::string downloadLinkLink;
    // link to download collection bundle from cdn
    std::string downloadLink;
    std::string description;
    std::string name;
    std::string summary;
    std::string tileImgUrl;
    std::string headerImgUrl;
};

struct NxmCollection
{
    NxmCollectionUrl url;

    // if it failed on an install step, error is set to true and
    // the step can be retried.  The amount of work it redos is dependent on
    // how good each step's implementation is.
    bool error = false;
    CollectionInfo info;
    std::vector<std::string> installErrorInfo;

    CollectionStatus status = CollectionStatus::None;
    nlohmann::json bundleDefinition;


    std::string installingCurrentMod;
    int installIndex = -1;
    

};


void StartNXMCollectionInstall(ModMgr& mgr, NxmCollectionUrl const & url);
void GetCollectionBundleLink(ModMgr& mgr);
void DownloadCollectionBundle(ModMgr& mgr);
void DownloadCollectionMods(ModMgr& mgr);
void UpdateInstallCollectionMods(ModMgr& mgr);
void ApplyCollectionLoadOrder(ModMgr& mgr);






