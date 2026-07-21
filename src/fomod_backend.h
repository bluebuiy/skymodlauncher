
#pragma once

#include <string>
#include "fomod.h"
#include "mod.h"

struct ModMgr;

bool PerformFomodInstall(
    ModMgr& mgr,
    std::string const & tmpName,
    ModInstallId installId,
    fomod::InstallActions & fileActions,
    std::string const & realRoot,
    std::filesystem::path const & installPrefix
);


