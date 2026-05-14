
#pragma once

#include "json.hpp"

#include "modmgr.h"


namespace nlohmann
{
    template <>
    struct adl_serializer<ModInfo>
    {
        static void to_json(json& j, ModInfo const & mi);

        static void from_json(json const & j, ModInfo& mi);
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
}



