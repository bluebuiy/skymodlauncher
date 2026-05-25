

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
    }

    void adl_serializer<ModInfo>::from_json(json const & j, ModInfo& mi)
    {
        JPULL(j, mi, enabled);
        JPULL(j, mi, loadIndex);
        JPULL(j, mi, modFile);
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
    }

    void adl_serializer<ModMgrInst>::from_json(json const & j, ModMgrInst& mi)
    {
        JPULL(j, mi, version);
        JPULL(j, mi, mods);
        JPULL(j, mi, customExec);
        JPULL(j, mi, pluginList);
        JPULL(j, mi, customVariables);
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

}

