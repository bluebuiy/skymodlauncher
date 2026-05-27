
#pragma once

#include "fomod.h"


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
    FomodStage stage;
    std::filesystem::path tmpDir;
    std::string modName;
    int hoveredOption = -1;
    bool openPopup = false;
};

bool InitFomod(ModMgr & mgr, std::filesystem::path const & tmpDir, std::string const & mod);
void RenderFomod(ModMgr& mgr);


