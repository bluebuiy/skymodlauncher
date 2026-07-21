
#include "enums.h"
#include "modmgr.h"

#include <iostream>

bool PerformFomodInstall(
    ModMgr& mgr,
    std::string const & tmpName,
    ModInstallId installId,
    fomod::InstallActions & fileActions,
    std::string const & realRoot,
    std::filesystem::path const & installPrefix
)
{
    std::filesystem::path fomodIntermediate = mgr.config.projectDir / ".mod_staging" / "fomodtmp" / tmpName;

    if (std::filesystem::is_directory(fomodIntermediate))
    {
        std::vector<std::string> clearTmp = {
            "/usr/bin/rm", "-f", "-r", fomodIntermediate
        };
        if (!LaunchProc(clearTmp, "/"))
        {
            std::cout << "Failed to remove previous fomod staging directory" << std::endl;
        }
    }
    // install to the temporary directory
    bool ok = ApplyFomodFileActions(mgr, fileActions, mgr.config.projectDir / realRoot, fomodIntermediate);

    if (!ok)
    {
        std::cout << "Failed to install into temporary directory" << std::endl;
        AddInstallMessage(mgr, installId, "Failed to install into temporary directory");
        return false;
    }

    std::filesystem::path guessedRoot = fomodIntermediate;
    ModInstallType installType = GuessInstallType(fomodIntermediate, guessedRoot);
    if (guessedRoot != fomodIntermediate)
    {
        std::cout << "Guessed mod root is not the install root" << std::endl;
        AddInstallMessage(mgr, installId, "Guessed mod root is not the install root");
        ok = false;
    }

    if (installType == ModInstallType::Conflicting)
    {
        std::cout << "Fomod created a mod that looks wrong" << std::endl;
        AddInstallMessage(mgr, installId, "Fomod created a mod that looks wrong");
        ok = false;
    }

    std::filesystem::path destination = installPrefix;
    if (installType == ModInstallType::Data)
    {
        destination = destination / "Data";
    }

    // move tmp install dir to actual install dir
    std::vector<std::string> installCmd = {
        "/usr/bin/mv",
        "-T",
        fomodIntermediate,
        destination
    };

    if (!LaunchProc(installCmd, "/"))
    {
        std::cout << "Failed to move staged fomod to install destination" << std::endl;
        AddInstallMessage(mgr, installId, "Failed to move staged fomod to install destination");
        ok = false;
    }
    
    return ok;
}

