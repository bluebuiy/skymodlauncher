
#pragma once

#include "fomod.h"

#include "mod.h"


struct ModFileRef;
struct ModMgr;

enum class FomodStage
{
    Initial,
    Steps,
    Complete,
};

struct FomodUI
{
    fomod::Fomod fomod;
    fomod::Eval eval;
    fomod::SubstepInfo ssInfo;
    fomod::InstallActions fileActions;
    std::string name;
    std::string hName;
    FomodStage stage;
    std::filesystem::path tmpDir;
    std::filesystem::path realRoot;
    std::filesystem::path installPrefix;
    std::string installDir;
    int hoveredOption = -1;
    bool openPopup = false;
    ModId modId;
};

bool InitFomod(ModMgr & mgr, std::filesystem::path const & tmpDir, std::filesystem::path const & realRoot, std::filesystem::path const & conf, std::filesystem::path const & installPrefix, std::string const & installDir, ModId const & id);
void RenderFomod(ModMgr& mgr);

bool MoveDirNormalizePaths(std::filesystem::path const & src, std::filesystem::path const & dst);
bool ApplyFomodFileActions(ModMgr & mgr, fomod::InstallActions & fileActions, std::filesystem::path const & staging, std::filesystem::path const & installPrefix);


