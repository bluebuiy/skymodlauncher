
#include "modmgr.h"


std::vector<ModId> GetModList(ModMgr& mgr)
{
    std::vector<ModId> ret;
    for (auto&& mf : mgr.inst.modFileManifests)
    {
        ret.emplace_back(mf.first);
    }
    return ret;
}

std::optional<ModManifest> GetModManifest(ModMgr& mgr, ModId id)
{
    auto it = mgr.inst.modFileManifests.find(id);
    if (it == mgr.inst.modFileManifests.end())
    {
        return {};
    }
    return it->second;
}

ModId CreateModManifest(ModMgr& mgr, ModManifest const & mft)
{
    // not happy with this
    for (auto&& mf : mgr.inst.modFileManifests)
    {
        auto mfi = mf.second;
        if (mfi.sourceType == mft.sourceType)
        {
            if (mfi.sourceType == FileSource::CollectionBundle && mfi.nxmColRev == mft.nxmColRev && mfi.nxmColSlug == mft.nxmColSlug && mfi.version == mft.version && mfi.nxmColBundleFile == mft.nxmColBundleFile)
            {
                return mf.first;
            }
            else if (mfi.sourceType == FileSource::Independent && mfi.url == mft.url)
            {
                return mf.first;
            }
            else if (mfi.sourceType == FileSource::Nexus && mfi.nxmFileId == mft.nxmFileId && mfi.nxmModId == mft.nxmModId)
            {
                return mf.first;
            }
        }
    }

    ModId ret = {++mgr.inst.idCounter};
    mgr.inst.modFileManifests.emplace(ret, mft);
    return ret;
}


ModId FindModManifest(ModMgr& mgr, ModManifest const & mft)
{
    for (auto&& mf : mgr.inst.modFileManifests)
    {
        auto mfi = mf.second;
        if (mfi.sourceType == mft.sourceType)
        {
            if (mfi.sourceType == FileSource::CollectionBundle && mfi.nxmColRev == mft.nxmColRev && mfi.nxmColSlug == mft.nxmColSlug && mfi.version == mft.version)
            {
                return mf.first;
            }
            else if (mfi.sourceType == FileSource::Independent && mfi.url == mft.url)
            {
                return mf.first;
            }
            else if (mfi.sourceType == FileSource::Nexus && mfi.nxmFileId == mft.nxmFileId && mfi.nxmModId == mft.nxmModId)
            {
                return mf.first;
            }
        }
    }
    return ModId{0};
}

std::vector<ModInstallId> GetModInstalls(ModMgr& mgr)
{
    std::vector<ModInstallId> ret;
    for (auto&& mi : mgr.inst.modInstalls)
    {
        ret.emplace_back(mi.first);
    }
    return ret;
}

std::optional<ModInstall> GetModInstall(ModMgr& mgr, ModInstallId id)
{
    auto it = mgr.inst.modInstalls.find(id);
    if (it == mgr.inst.modInstalls.end())
    {
        return {};
    }
    return it->second;
}

void SetInstallIndex(ModMgr& mgr, ModInstallId id, int index)
{
    auto inst = mgr.inst.modInstalls.find(id);

    if (inst == mgr.inst.modInstalls.end())
    {
        return;
    }

    int from = inst->second.loadIndex;

    for (auto&& install : mgr.inst.modInstalls)
    {
        int change = 0;
        if (install.second.loadIndex > from)
        {
            change--;
        }
        if (install.second.loadIndex >= index)
        {
            change++;
        }
        install.second.loadIndex += change;
    }
    inst->second.loadIndex = index;
}


