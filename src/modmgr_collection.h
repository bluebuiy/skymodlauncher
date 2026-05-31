
#pragma once

#include "nxmurl.h"

#include <string>
#include <stdint.h>

struct ModMgr;

enum class CollectionStatus
{
    None,
    FetchingInfo,
    WaitingForInstallButton,
    DisplayInfo,
    DownloadingMods,
    InstallingMods,
    ConfigureLoadOrder,
    Installed
};

struct CollectionInfo
{
    int64_t totalSize = 0;
    int modCount = 0;
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

    CollectionStatus status = CollectionStatus::None;

};


void StartNXMCollectionInstall(ModMgr& mgr, NxmCollectionUrl const & url);








