
#pragma once

#include "nxmurl.h"
#include "fomodauto.h"

#include "enums.h"

#include "nlohmann/json.hpp"

#include <string>
#include <vector>
#include <optional>
#include <stdint.h>

// references a mod manifest
struct ModId
{
    int id = 0;

    //explicit operator std::string() const
    //{
    //    return std::to_string(id);
    //}
};

struct ModInstallId
{
    int id = 0;
};

struct ModDownloadId
{
    int id = 0;
};

inline bool operator==(ModId const & a, ModId const & b)
{
    return a.id == b.id;
}

inline bool operator!=(ModId const & a, ModId const & b)
{
    return a.id != b.id;
}

inline bool operator==(ModInstallId const & a, ModInstallId const & b)
{
    return a.id == b.id;
}

inline bool operator!=(ModInstallId const & a, ModInstallId const & b)
{
    return a.id != b.id;
}

inline bool operator==(ModDownloadId const & a, ModDownloadId const & b)
{
    return a.id == b.id;
}

inline bool operator!=(ModDownloadId const & a, ModDownloadId const & b)
{
    return a.id != b.id;
}



template <>
struct std::hash<ModId>
{
    size_t operator()(ModId const & id) const
    {
        return id.id;
    }
};

template <>
struct std::hash<ModInstallId>
{
    size_t operator()(ModInstallId const & id) const
    {
        return id.id;
    }
};

template <>
struct std::hash<ModDownloadId>
{
    size_t operator()(ModDownloadId const & id) const
    {
        return id.id;
    }
};

struct ModManifest
{
    FileSource sourceType;

    // files from nxm have a mod id and file id
    int nxmModId = 0;
    int nxmFileId = 0;
    // the "game"
    std::string nxmDomain;

    // bundled files come from a collection
    std::string nxmColSlug;
    int nxmColRev = 0;
    std::string nxmColBundleFile;

    // nxm sourced mods have versions
    std::string version;
    
    // direct files have a download url
    std::string url;

    // Local files have a path
    std::string path;
    
    // The "human readable" name, not the name on the download.
    // This is the same as the "name" field in the mod list in a collection.
    // Not unique, can be empty
    std::string name;

    // In a collection, the "logicalName".
    // for nxm downloaded mods, this is also the "name" field in the file metadata
    std::string logicalName;

    ModDownloadId downloadInstance;
    std::vector<ModInstallId> installInstances;

    ModInstallType installType = ModInstallType::Undetermined;
};

struct ModDownload
{
    uint64_t size = 0;
    uint64_t progress = 0;

    // name of the downloaded file
    std::string fileName;
    ModDlState state;

    ModId modInstance;
};

struct ModInstall
{
    std::string name;
    std::string installDir;
    int loadIndex = 0;
    bool enabled;
    ModInstallType installType;

    ModId modInstance;

    bool ok = false;

    std::vector<std::string> installMessages;
};

struct NxmCollectionModPatch
{
    std::string filePath;
    std::string hash;
};

struct NxmCollectionMod
{
    ModId modInstance;
    std::vector<NxmCollectionModPatch> patches;
    std::optional<FomodAuto::Config> fomodConfig;
    std::optional<ManualInstallConfig> manualConfig;
    ModInstallType installType;
};

struct NxmCollectionData
{
    //std::vector<NxmCollectionMod> mods;
    std::string name;
    std::string author;
    std::string desc;
    std::string installInstructions;
    std::string domainName;
    
    nlohmann::json jdata;
};

struct ModLoadRule
{
    // before < after
    ModId before;
    ModId after;
};

struct PluginLoadRule
{
    std::string before;
    std::string after;
};

 

