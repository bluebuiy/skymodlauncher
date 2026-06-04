

#include "modmgr_json.h"

#include <iostream>


#define JPULL(j, o, field) do { try { \
    j.at(#field).get_to(o.field);\
} catch (...) {std::cout << "Failed to find " << #field << std::endl;} } while (false)

#define JPUT(j, o, field) do { j[#field] = o.field; } while (false)

namespace nlohmann
{
    void adl_serializer<ModInfo>::to_json(json& j, ModInfo const & mi)
    {
        JPUT(j, mi, enabled);
        JPUT(j, mi, loadIndex);
        JPUT(j, mi, modFile);
        JPUT(j, mi, modId);
        JPUT(j, mi, fileId);
        JPUT(j, mi, hName);
    }

    void adl_serializer<ModInfo>::from_json(json const & j, ModInfo& mi)
    {
        JPULL(j, mi, enabled);
        JPULL(j, mi, loadIndex);
        JPULL(j, mi, modFile);
        JPULL(j, mi, modId);
        JPULL(j, mi, fileId);
        JPULL(j, mi, hName);
    }

    void adl_serializer<ModMgrConfig>::to_json(json& j, ModMgrConfig const & cfg)
    {
        j["version"] = ModMgrConfig::VERSION;
        JPUT(j, cfg, installRoot);
        JPUT(j, cfg, mgRoot);
        JPUT(j, cfg, modFolder);
        JPUT(j, cfg, instPath);
        JPUT(j, cfg, appData);
        JPUT(j, cfg, nexusApiKey);
    }
    
    void adl_serializer<ModMgrConfig>::from_json(json const & j, ModMgrConfig & cfg)
    {
        JPULL(j, cfg, version);
        JPULL(j, cfg, installRoot);
        JPULL(j, cfg, mgRoot);
        JPULL(j, cfg, modFolder);
        JPULL(j, cfg, instPath);
        JPULL(j, cfg, appData);
        JPULL(j, cfg, nexusApiKey);
    }

    void adl_serializer<ModMgrInst>::to_json(json& j, ModMgrInst const & mi)
    {
        j["version"] = ModMgrInst::VERSION;
        JPUT(j, mi, mods);
        JPUT(j, mi, customExec);
        JPUT(j, mi, pluginList);
        JPUT(j, mi, customVariables);
        JPUT(j, mi, downloads);
        JPUT(j, mi, collection);
    }

    void adl_serializer<ModMgrInst>::from_json(json const & j, ModMgrInst& mi)
    {
        JPULL(j, mi, version);
        JPULL(j, mi, mods);
        JPULL(j, mi, customExec);
        JPULL(j, mi, pluginList);
        JPULL(j, mi, customVariables);
        JPULL(j, mi, downloads);
        JPULL(j, mi, collection);
    }
    
    void adl_serializer<ModExec>::to_json(json& j, ModExec const & me)
    {
        JPUT(j, me, execName);
        JPUT(j, me, execPath);
        JPUT(j, me, args);
        JPUT(j, me, updatePluginList);
    }

    void adl_serializer<ModExec>::from_json(json const & j, ModExec& me)
    {
        JPULL(j, me, execName);
        JPULL(j, me, execPath);
        JPULL(j, me, args);
        JPULL(j, me, updatePluginList);
    }

    void adl_serializer<ModPlugin>::to_json(json& j, ModPlugin const & mp)
    {
        JPUT(j, mp, pluginName);
        JPUT(j, mp, enabled);
    }

    void adl_serializer<ModPlugin>::from_json(json const & j, ModPlugin& mp)
    {
        JPULL(j, mp, pluginName);
        JPULL(j, mp, enabled);
    }

    void adl_serializer<CustomVariable>::to_json(json& j, CustomVariable const & cv)
    {
        JPUT(j, cv, name);
        JPUT(j, cv, value);
    }

    void adl_serializer<CustomVariable>::from_json(json const & j, CustomVariable& cv)
    {
        JPULL(j, cv, name);
        JPULL(j, cv, value);
    }

    void adl_serializer<ModDownload>::to_json(json& j, ModDownload const & md)
    {
        JPUT(j, md, fileName);
        JPUT(j, md, modId);
        JPUT(j, md, fileId);
        JPUT(j, md, game);
        JPUT(j, md, installType);
        JPUT(j, md, hFileName);
    }

    void adl_serializer<ModDownload>::from_json(json const & j, ModDownload& md)
    {
        JPULL(j, md, fileName);
        JPULL(j, md, modId);
        JPULL(j, md, fileId);
        JPULL(j, md, game);
        JPULL(j, md, installType);
        JPULL(j, md, hFileName);
    }

    void adl_serializer<NxmCollectionUrl>::to_json(json& j, NxmCollectionUrl const & cv)
    {
        JPUT(j, cv, game);
        JPUT(j, cv, rev);
        JPUT(j, cv, slug);
    }

    void adl_serializer<NxmCollectionUrl>::from_json(json const & j, NxmCollectionUrl& cv)
    {
        JPULL(j, cv, game);
        JPULL(j, cv, rev);
        JPULL(j, cv, slug);
    }
}

