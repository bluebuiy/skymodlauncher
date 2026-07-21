

#include "fomod_ui.h"
#include "modmgr.h"
#include "imgui.h"
#include "prochelper.h"

#include <iostream>
#include <format>

bool InitFomod(
    ModMgr & mgr,
    std::filesystem::path const & tmpDir,
    std::filesystem::path const & realRoot,
    std::filesystem::path const & fomodConf,
    std::filesystem::path const & installDst,
    std::string const & installDir,
    ModId const & modFile
)
{
    if (mgr.fomodState.has_value())
    {
        std::cout << "Fomod already in progress" << std::endl;
        return false;
    }

    auto mf = mgr.inst.modFileManifests.find(modFile);
    if (mf == mgr.inst.modFileManifests.end())
    {
        std::cout << "Invalid mod id" << std::endl;
        return false;
    }
    if (mf->second.installInstances.empty())
    {
        std::cout << "Mod is not installing" << std::endl;
        return false;
    }

    std::filesystem::path fomdPath = fomodConf;

    auto downloadData = std::find_if(mgr.downloadSessions.begin(), mgr.downloadSessions.end(), [&](ModDownloadRt const & m){
        return m.id == modFile;
    });

    if (downloadData == mgr.downloadSessions.end())
    {
        return false;
    }

    auto ld = fomod::Load(fomdPath);

    if (!ld.has_value())
    {
        std::cout << "Failed to load fomod at " << fomdPath << std::endl;
        return false;
    }

    mgr.fomodState.emplace();
    mgr.fomodState->fomod = std::move(*ld);
    mgr.fomodState->stage = FomodStage::Initial;

    mgr.fomodState->installPrefix = installDst;
    mgr.fomodState->realRoot = realRoot;
    mgr.fomodState->tmpDir = tmpDir;

    // TODO load the info file
    mgr.fomodState->hName = downloadData->fileName;
    mgr.fomodState->name = std::filesystem::path(downloadData->fileName).stem();
    mgr.fomodState->openPopup = true;
    mgr.fomodState->modId = modFile;
    mgr.fomodState->installId = mf->second.installInstances[0];
    mgr.fomodState->installDir = installDir;

    return true;
}

void RenderFomod(ModMgr& mgr)
{
    if (!mgr.fomodState.has_value())
    {
        return;
    }

    auto& fomod = *mgr.fomodState;

    if (fomod.openPopup)
    {
        fomod.openPopup = false;
        ImGui::OpenPopup("FOMOD");
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("FOMOD", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        // this is very bizare
        //mgr.fomodState.reset();
        return;
    }

    bool doStage = false;
    bool doInstall = false;

    if (fomod.stage == FomodStage::Initial)
    {
        if (ImGui::Button("Start"))
        {
            doStage = true;
        }
        ImGui::Text("%s", fomod.hName.c_str());
    }
    else if (fomod.stage == FomodStage::Steps)
    {

        if (ImGui::Button("Next"))
        {
            doStage = true;
        }
        ImGui::SameLine();
        ImGui::Text("  %s", fomod.ssInfo.name.c_str());
        ImGui::Separator();
        ImGui::BeginTable("fomodtbl", 2);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        if (fomod.hoveredOption < 0 || fomod.hoveredOption >= fomod.ssInfo.options.size())
        {
            ImGui::Button("Empty Spacer", ImVec2(100, 60));
        }
        else
        {
            ImGui::Button("Image placeholder!", ImVec2(100, 60));
            ImGui::TextWrapped("%s", fomod.ssInfo.options[fomod.hoveredOption].optionText.c_str());
        }

        ImGui::TableSetColumnIndex(1);

        int sel = -1;
        bool newVal = false;
        for (int i = 0; i < fomod.ssInfo.options.size(); ++i)
        {
            auto&& opt = fomod.ssInfo.options[i];
            bool s = opt.selected;
            if (ImGui::Checkbox(opt.name.c_str(), &s))
            {
                sel = i;
                newVal = s;
            }
            if (ImGui::IsItemHovered())
            {
                fomod.hoveredOption = i;
            }
        }

        if (sel != -1)
        {
            auto stype = fomod.ssInfo.optionType;
            if (stype == fomod::SelectionType::ExactlyOne)
            {
                if (newVal)
                {
                    for (auto&& o : fomod.ssInfo.options)
                    {
                        o.selected = false;
                    }
                    fomod.ssInfo.options[sel].selected = true;
                }
                else
                {
                    fomod.ssInfo.options[sel].selected = false;
                }
            }
            else if (stype == fomod::SelectionType::AtLeastOne)
            {
                if (!newVal)
                {
                    int c = 0;
                    for (auto&& o : fomod.ssInfo.options)
                    {
                        if (o.selected) ++c;
                    }
                    if (c > 1)
                    {
                        fomod.ssInfo.options[sel].selected = false;
                    }
                }
                else
                {
                    fomod.ssInfo.options[sel].selected = true;
                }
            }
            else if (stype == fomod::SelectionType::AtMostOne)
            {
                if (newVal)
                {
                    for (auto&& o : fomod.ssInfo.options)
                    {
                        o.selected = false;
                    }
                    fomod.ssInfo.options[sel].selected = true;
                }
                else
                {
                    fomod.ssInfo.options[sel].selected = false;
                }
            }
            else
            {
                fomod.ssInfo.options[sel].selected = newVal;
            }
        }

        ImGui::EndTable();

    }
    else if (fomod.stage == FomodStage::Complete)
    {
        if (ImGui::Button("Finish"))
        {
            doInstall = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Recompute Actions"))
        {
            fomod::SubstepInfo ss = fomod::PrepareInstallActions(fomod.fomod, fomod.eval);
            // TODO check file status
            fomod.fileActions = fomod::GetInstallActions(fomod.fomod, fomod.eval, ss);
        }
        ImGui::Text("File Actions:");
        for (auto&& move : fomod.fileActions.actions)
        {
            ImGui::Text("%-7s", (move.action == fomod::FileAction::DirToDir ? "Folder" : "File"));
            ImGui::SameLine();
            ImGui::Text("%-32s", move.from.c_str());
            ImGui::SameLine();
            ImGui::Text("Data/%-32s", move.to.c_str());
        }
    }

    if (doStage)
    {
        bool applied = false;
        if (fomod.stage == FomodStage::Steps)
        {
            applied = fomod::ApplySubstep(fomod.fomod, fomod.eval, fomod.ssInfo);
        }
        else
        {
            applied = true;
        }
        if (applied)
        {
            fomod.stage = FomodStage::Steps;
            while (true)
            {
                fomod.ssInfo = fomod::PrepareSubstep(fomod.fomod, fomod.eval);
                // TODO check file status
                bool visible = fomod::EvalSubstep(fomod.fomod, fomod.eval, fomod.ssInfo);
                if (visible)
                {
                    break;
                }
            }
            if (fomod::Configured(fomod.fomod, fomod.eval))
            {
                fomod.stage = FomodStage::Complete;
                fomod::SubstepInfo ss = fomod::PrepareInstallActions(fomod.fomod, fomod.eval);
                // TODO check file status
                fomod.fileActions = fomod::GetInstallActions(fomod.fomod, fomod.eval, ss);
            }
            else
            {
                // load images
            }
        }
    }
    else if (doInstall)
    {
        {
            std::filesystem::path fomodIntermediate = mgr.config.projectDir / ".mod_staging" / "fomodtmp" / fomod.installDir;

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
            bool ok = ApplyFomodFileActions(mgr, fomod.fileActions, mgr.config.projectDir / fomod.realRoot, fomodIntermediate);

            if (!ok)
            {
                std::cout << "Failed to install into temporary directory" << std::endl;
                AddInstallMessage(mgr, fomod.installId, "Failed to install into temporary directory");
                goto fomodcleanup;
            }

            std::filesystem::path guessedRoot = fomodIntermediate;
            ModInstallType installType = GuessInstallType(fomodIntermediate, guessedRoot);
            if (guessedRoot != fomodIntermediate)
            {
                std::cout << "Guessed mod root is not the install root" << std::endl;
                AddInstallMessage(mgr, fomod.installId, "Guessed mod root is not the install root");
                ok = false;
            }

            if (installType == ModInstallType::Conflicting)
            {
                std::cout << "Fomod created a mod that looks wrong" << std::endl;
                AddInstallMessage(mgr, fomod.installId, "Fomod created a mod that looks wrong");
                ok = false;
            }

            std::filesystem::path destination = fomod.installPrefix;
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
                AddInstallMessage(mgr, fomod.installId, "Failed to move staged fomod to install destination");
                ok = false;
            }

            //bool ok = ApplyFomodFileActions(mgr, fomod.fileActions, mgr.config.projectDir / fomod.realRoot, fomod.installPrefix);

            auto inst = mgr.inst.modInstalls.find(fomod.installId);
            if (inst != mgr.inst.modInstalls.end())
            {
                if (ok)
                {
                    inst->second.ok = true;
                }
            }
            else
            {
                std::cout << "Install disappeared!" << std::endl;
            }

            DiscoverPlugins(mgr);

        }

    fomodcleanup:

        // remove staging
        std::vector<std::string> rmCmd = {
            "/usr/bin/rm",
            "-f",
            "-r",
            mgr.config.projectDir / fomod.tmpDir
        };
        if (!LaunchProc(rmCmd, "/"))
        {
            std::cout << "Failed to delete fomod tmp dir: " << mgr.config.projectDir / fomod.tmpDir << std::endl;
        }

        mgr.fomodState.reset();
        ImGui::CloseCurrentPopup();

    }

    ImGui::EndPopup();

}

std::string NormalizePath(std::string const & str)
{
    std::string f(str);
    for (char& c : f)
    {
        if (isalpha(c))
        {
            c = tolower(c);
        }
    }
    return f;
}

std::filesystem::path NormPath(std::filesystem::path const & p)
{
    auto parent = p.parent_path();
    std::string f = p.filename();
    for (char& c : f)
    {
        if (isalpha(c))
        {
            c = tolower(c);
        }
    }
    return parent / f;
}

struct DirNormListing
{
    std::filesystem::path dirPath;
    std::filesystem::path dstPath;
    std::vector<std::filesystem::path> children;
    int index = 0;
};

// moves the contents of src to inside the destination target directory dst
// if a file already exists, skips it.
bool MoveDirNormalizePaths(std::filesystem::path const & src, std::filesystem::path const & dst)
{
    try
    {
        std::filesystem::create_directories(dst);
        std::vector<DirNormListing> stack;
        {
            auto& next = stack.emplace_back();
            for (auto dit = std::filesystem::directory_iterator(src); dit != std::filesystem::directory_iterator(); ++dit)
            {
                next.children.emplace_back(dit->path());
            }
            next.dstPath = dst;
        }

        while (stack.size() > 0)
        {
            if (stack.back().index < stack.back().children.size())
            {
                std::filesystem::path currentPath = stack.back().children[stack.back().index];
                std::filesystem::path curDstPath = stack.back().dstPath;
                ++stack.back().index;
                if (std::filesystem::is_directory(currentPath))
                {
                    auto& next = stack.emplace_back();
                    next.dirPath = currentPath;
                    std::string np = NormalizePath(currentPath.filename());
                    if (np == "data" && stack.size() == 2)
                    {
                        // hopefully nobody put a data directory in the root of a data mod 
                        if ("Data" != currentPath.filename())
                        {
                            std::cout << "Detected incorrectly capitalized Data root path: " << currentPath << std::endl;
                        }
                        np = "Data";
                    }
                    next.dstPath = curDstPath / np;
                    for (auto dit = std::filesystem::directory_iterator(currentPath); dit != std::filesystem::directory_iterator(); ++dit)
                    {
                        next.children.emplace_back(dit->path());
                    }
                    std::filesystem::create_directories(next.dstPath);
                }
                else
                {
                    auto norm = curDstPath / NormalizePath(currentPath.filename());
                    if (!std::filesystem::exists(norm))
                    {
                        std::filesystem::create_hard_link(currentPath, norm);
                    }
                }
            }
            else
            {
                stack.pop_back();
            }
        }
    }
    catch (std::filesystem::filesystem_error const & err)
    {
        std::cout << "Failed to move & normalize files: " << err.what() << std::endl;
        std::cout << "Path 1: " << err.path1() << std::endl;
        std::cout << "Path 2: " << err.path2() << std::endl;
        return false;
    }
    return true;
}

std::optional<std::filesystem::path> FindCasedPath(std::filesystem::path const & pathIn)
{
    auto path = pathIn;
    path.make_preferred();
    auto pi = path.begin();
    std::filesystem::path result = *pi;
    ++pi;
    while (pi != path.end())
    {
        bool found = false;
        if (*pi == "")
        {
            found = true;
        }
        else
        {
            for (auto it = std::filesystem::directory_iterator(result); it != std::filesystem::directory_iterator(); ++it)
            {
                //std::cout << it->path() << "   " << result << "   " << *pi << std::endl;
                if (0 == strcasecmp(it->path().filename().c_str(), pi->c_str()))
                {
                    found = true;
                    result = it->path();
                    break;
                }
            }
        }
        if (!found)
        {
            return {};
        }
        ++pi;
    }
    return result;
}

bool ApplyFomodFileActions(ModMgr & mgr, fomod::InstallActions & fileActions, std::filesystem::path const & staging, std::filesystem::path const & prefix)
{
    bool success = true;

    std::filesystem::create_directories(prefix);
    // were gonna use -n, no clobber, so we can do the moves high to low and remove unecessary copies.
    std::reverse(fileActions.actions.begin(), fileActions.actions.end());
    std::filesystem::path wd = staging;
    for (auto&& action : fileActions.actions)
    {

        if (action.action == fomod::FileAction::FileToFile)
        {
            // i THINK empty == "/"? but it really should be fully explicit
            if (action.to.empty() || action.to == "/")
            {
                action.to = std::filesystem::path(action.from).filename();
            }
            else if (action.to.back() == '/')
            {
                action.to = std::filesystem::path(action.to) / std::filesystem::path(action.from).filename();
            }
            try
            {
                std::filesystem::path fromPath = action.from;
                std::filesystem::path toPath = action.to;
                std::filesystem::path dstPath = prefix / std::filesystem::path(NormalizePath(toPath));
                std::optional<std::filesystem::path> realFrom = FindCasedPath(staging / std::filesystem::path(fromPath));
                if (realFrom)
                {
                    if (!std::filesystem::exists(dstPath))
                    {
                        std::filesystem::create_directories(dstPath.parent_path());
                        std::filesystem::create_hard_link(*realFrom, prefix / std::filesystem::path(NormalizePath(toPath)));
                    }
                }
                else
                {
                    success = false;
                    std::cout << "!!!!!!!!!!!! Failed to find content file: " << action.from << std::endl;
                }
            }
            catch (std::filesystem::filesystem_error const & err)
            {
                success = false;
                std::cout << "Failed to normailze file: " << err.what() << std::endl;
                std::cout << "Path 1: " << err.path1() << std::endl;
                std::cout << "Path 2: " << err.path2() << std::endl;
                std::cout << "!!!!!!!!!!!! Failed to install correctly!" << std::endl;
            }
        }
        else if (action.action == fomod::FileAction::DirToDir)
        {
            // i think??????????
            if (action.to == "/")
            {
                action.to = "";
            }

            std::optional<std::filesystem::path> realFrom = FindCasedPath(staging / std::filesystem::path(action.from));
            if (realFrom)
            {
                std::filesystem::path normTo = NormalizePath(action.to);
                realFrom->make_preferred();
                normTo.make_preferred();
                std::filesystem::create_directories(prefix / normTo);
                if (!MoveDirNormalizePaths(*realFrom, prefix / normTo))
                {
                    success = false;
                    std::cout << "!!!!!!!!!!!! Failed to install correctly!" << std::endl;
                }
            }
            else
            {
                success = false;
                std::cout << "!!!!!!!!!!!! Failed to find content directory: " << action.from << std::endl;
            }
        }
    }
    return success;
}

