
#pragma once

#include "nlohmann/json.hpp"

#include "modmgr.h"
#include "enums.h"

NLOHMANN_JSON_SERIALIZE_ENUM(ModInstallType, {
    {ModInstallType::Undetermined, "Undetermined"},
    {ModInstallType::Conflicting, "Conflicting"},
    {ModInstallType::Data, "Data"},
    {ModInstallType::Root, "Root"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(FileSource, {
    {FileSource::Unknown, "Unknown"},
    {FileSource::Nexus, "Nexus"},
    {FileSource::Independent, "Independent"},
    {FileSource::Manual, "Manual"},
    {FileSource::CollectionBundle, "CollectionBundle"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ModDlState, {
    {ModDlState::None, "None"},
    {ModDlState::UrlQuery, "UrlQuery"},
    {ModDlState::ModDownload, "ModDownload"},
    {ModDlState::ModPaused, "ModPaused"},
    {ModDlState::Error, "Error"},
    {ModDlState::Complete, "Complete"},
    {ModDlState::Canceled, "Canceled"},
})

namespace nlohmann
{
    template <typename T>
    struct adl_serializer<std::optional<T>>
    {
        static void to_json(json& j, std::optional<T> const & mi)
        {
            if (mi)
            {
                adl_serializer<T>::to_json(j, *mi);
                //j = *mi;
            }
            else
            {
                j = nullptr;
            }
        }

        static void from_json(json const & j, std::optional<T>& mi)
        {
            if (j.is_null())
            {
                mi = {};
            }
            else
            {
                T t;
                adl_serializer<T>::from_json(j, t);
                mi = std::move(t);
            }
        }
    };

    template <>
    struct adl_serializer<ModManifest>
    {
        static void to_json(json& j, ModManifest const & mi);

        static void from_json(json const & j, ModManifest& mi);
    };

    template <>
    struct adl_serializer<ModMgrConfig>
    {
        static void to_json(json& j, ModMgrConfig const & cfg);
        
        static void from_json(json const & j, ModMgrConfig & cfg);
    };

    template <>
    struct adl_serializer<ModMgrInst>
    {
        static void to_json(json& j, ModMgrInst const & cfg);
        
        static void from_json(json const & j, ModMgrInst & cfg);
    };

    template <>
    struct adl_serializer<ModExec>
    {
        static void to_json(json& j, ModExec const & cfg);
        
        static void from_json(json const & j, ModExec & cfg);
    };

    template <>
    struct adl_serializer<ModPlugin>
    {
        static void to_json(json& j, ModPlugin const & cfg);
        
        static void from_json(json const & j, ModPlugin & cfg);
    };

    template <>
    struct adl_serializer<CustomVariable>
    {
        static void to_json(json& j, CustomVariable const & cfg);
        
        static void from_json(json const & j, CustomVariable & cfg);
    };

    template <>
    struct adl_serializer<ModDownload>
    {
        static void to_json(json& j, ModDownload const & cfg);
        
        static void from_json(json const & j, ModDownload & cfg);
    };

    template <>
    struct adl_serializer<NxmCollectionUrl>
    {
        static void to_json(json& j, NxmCollectionUrl const & cfg);
        
        static void from_json(json const & j, NxmCollectionUrl & cfg);
    };

    template <>
    struct adl_serializer<ModId>
    {
        static void to_json(json& j, ModId const & cfg);
        
        static void from_json(json const & j, ModId & cfg);
    };

    template <>
    struct adl_serializer<ModInstallId>
    {
        static void to_json(json& j, ModInstallId const & cfg);
        
        static void from_json(json const & j, ModInstallId & cfg);
    };

    template <>
    struct adl_serializer<NxmCollection>
    {
        static void to_json(json& j, NxmCollection const & cfg);
        
        static void from_json(json const & j, NxmCollection & cfg);
    };

    template <>
    struct adl_serializer<ModInstall>
    {
        static void to_json(json& j, ModInstall const & cfg);
        
        static void from_json(json const & j, ModInstall & cfg);
    };
}



