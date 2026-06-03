
#pragma once

#include "fomod.h"


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
    int hoveredOption = -1;
    bool openPopup = false;
    int fileId = 0;
    int modId = 0;
};

bool InitFomod(ModMgr & mgr, std::filesystem::path const & tmpDir, std::filesystem::path const & realRoot, std::filesystem::path const & conf, std::filesystem::path const & installPrefix, ModFileRef const & modFile);
void RenderFomod(ModMgr& mgr);

bool MoveDirNormalizePaths(std::filesystem::path const & src, std::filesystem::path const & dst);
bool ApplyFomodFileActions(ModMgr & mgr, fomod::InstallActions & fileActions, std::filesystem::path const & staging, std::filesystem::path const & installPrefix);


