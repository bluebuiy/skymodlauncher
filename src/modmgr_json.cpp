

#include "modmgr_json.h"

#include <iostream>


#define JPULL(j, o, field) do { try { \
    j.at(#field).get_to(o.field);\
} catch (...) {std::cout << "Failed to find " << #field << std::endl;} } while (false)

#define JPULL2(j, field) do { try { \
    j.at(#field).get_to(field);\
} catch (...) {std::cout << "Failed to find " << #field << std::endl;} } while (false)

#define JPUT(j, o, field) do { j[#field] = o.field; } while (false)
#define JPUT2(j, field) do { j[#field] = field; } while (false)

namespace nlohmann
{
    void adl_serializer<ModManifest>::to_json(json& j, ModManifest const & mi)
    {
        JPUT(j, mi, sourceType);
        JPUT(j, mi, nxmModId);
        JPUT(j, mi, nxmFileId);
        JPUT(j, mi, nxmDomain);
        JPUT(j, mi, nxmColSlug);
        JPUT(j, mi, nxmColRev);
        JPUT(j, mi, nxmColBundleFile);
        JPUT(j, mi, version);
        JPUT(j, mi, url);
        JPUT(j, mi, name);
        JPUT(j, mi, logicalName);
        JPUT(j, mi, installType);

        //JPUT(j, mi, downloadInstance);
        JPUT(j, mi, installInstances);
    }

    void adl_serializer<ModManifest>::from_json(json const & j, ModManifest& mi)
    {
        JPULL(j, mi, sourceType);
        JPULL(j, mi, nxmModId);
        JPULL(j, mi, nxmFileId);
        JPULL(j, mi, nxmDomain);
        JPULL(j, mi, nxmColSlug);
        JPULL(j, mi, nxmColRev);
        JPULL(j, mi, nxmColBundleFile);
        JPULL(j, mi, version);
        JPULL(j, mi, url);
        JPULL(j, mi, name);
        JPULL(j, mi, logicalName);
        JPULL(j, mi, installType);

        //JPULL(j, mi, downloadInstance);
        JPULL(j, mi, installInstances);
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
        JPUT(j, mi, customExec);
        JPUT(j, mi, pluginList);
        JPUT(j, mi, customVariables);
        JPUT(j, mi, downloads);
        JPUT(j, mi, collection);
        JPUT(j, mi, modFileManifests);
        JPUT(j, mi, modInstalls);
        JPUT(j, mi, idCounter);
        JPUT(j, mi, modRulesRaw);
        JPUT(j, mi, pluginRulesRaw);
    }

    void adl_serializer<ModMgrInst>::from_json(json const & j, ModMgrInst& mi)
    {
        JPULL(j, mi, version);
        JPULL(j, mi, customExec);
        JPULL(j, mi, pluginList);
        JPULL(j, mi, customVariables);
        JPULL(j, mi, downloads);
        JPULL(j, mi, collection);
        JPULL(j, mi, modFileManifests);
        JPULL(j, mi, modInstalls);
        JPULL(j, mi, idCounter);
        JPULL(j, mi, modRulesRaw);
        JPULL(j, mi, pluginRulesRaw);
    }
    
    void adl_serializer<ModExec>::to_json(json& j, ModExec const & me)
    {
        JPUT(j, me, execName);
        JPUT(j, me, execPath);
        JPUT(j, me, wd);
        JPUT(j, me, args);
        JPUT(j, me, updatePluginList);
    }

    void adl_serializer<ModExec>::from_json(json const & j, ModExec& me)
    {
        JPULL(j, me, execName);
        JPULL(j, me, execPath);
        JPULL(j, me, wd);
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
        JPUT(j, md, state);
        JPUT(j, md, modInstance);
    }

    void adl_serializer<ModDownload>::from_json(json const & j, ModDownload& md)
    {
        JPULL(j, md, fileName);
        JPULL(j, md, state);
        JPULL(j, md, modInstance);
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

    void adl_serializer<ModId>::to_json(json& j, ModId const & cv)
    {
        j = std::to_string(cv.id);
    }

    void adl_serializer<ModId>::from_json(json const & j, ModId& cv)
    {
        int id = 0;
        try {
            std::string idstr;
            j.get_to(idstr);
            id = atoi(idstr.c_str());
        }
        catch (...)
        {

        }
        cv.id = id;
    }

    void adl_serializer<ModInstallId>::to_json(json& j, ModInstallId const & cv)
    {
        j = std::to_string(cv.id);
    }

    void adl_serializer<ModInstallId>::from_json(json const & j, ModInstallId& cv)
    {
        int id = 0;
        try {
            std::string idstr;
            j.get_to(idstr);
            id = atoi(idstr.c_str());
        }
        catch (...)
        {

        }
        cv.id = id;
    }

    void adl_serializer<NxmCollection>::to_json(json& j, NxmCollection const & cv)
    {
        JPUT(j, cv, url);
        JPUT(j, cv, error);
        JPUT(j, cv, installErrorInfo);
        JPUT(j, cv, status);
    }

    void adl_serializer<NxmCollection>::from_json(json const & j, NxmCollection& cv)
    {
        JPULL(j, cv, url);
        JPULL(j, cv, error);
        JPULL(j, cv, installErrorInfo);
        JPULL(j, cv, status);
    }


    void adl_serializer<ModInstall>::to_json(json& j, ModInstall const & cv)
    {
        JPUT(j, cv, name);
        JPUT(j, cv, installDir);
        JPUT(j, cv, loadIndex);
        JPUT(j, cv, enabled);
        JPUT(j, cv, installType);
        JPUT(j, cv, modInstance);
        JPUT(j, cv, installMessages);
        JPUT(j, cv, ok);
    }

    void adl_serializer<ModInstall>::from_json(json const & j, ModInstall& cv)
    {
        JPULL(j, cv, name);
        JPULL(j, cv, installDir);
        JPULL(j, cv, loadIndex);
        JPULL(j, cv, enabled);
        JPULL(j, cv, installType);
        JPULL(j, cv, modInstance);
        JPULL(j, cv, installMessages);
        JPULL(j, cv, ok);
    }

    void adl_serializer<ModLoadRule>::to_json(json& j, ModLoadRule const & cv)
    {
        JPUT(j, cv, before);
        JPUT(j, cv, after);
    }

    void adl_serializer<ModLoadRule>::from_json(json const & j, ModLoadRule& cv)
    {
        JPULL(j, cv, before);
        JPULL(j, cv, after);
    }

    void adl_serializer<PluginLoadRule>::to_json(json& j, PluginLoadRule const & cv)
    {
        JPUT(j, cv, before);
        JPUT(j, cv, after);
    }

    void adl_serializer<PluginLoadRule>::from_json(json const & j, PluginLoadRule& cv)
    {
        JPULL(j, cv, before);
        JPULL(j, cv, after);
    }

}

