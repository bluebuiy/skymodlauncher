

#include "fomod_ui.h"
#include "modmgr.h"
#include "imgui.h"
#include "prochelper.h"

#include <iostream>
#include <format>

bool InitFomod(ModMgr & mgr, std::filesystem::path const & tmpDir, std::string const & mod)
{
    if (mgr.fomodState.has_value())
    {
        std::cout << "Fomod already in progress" << std::endl;
        return false;
    }

    std::filesystem::path fomdPath = mgr.config.projectDir / tmpDir / "FOMod" / "ModuleConfig.xml";

    auto ld = fomod::Load(fomdPath);

    if (!ld.has_value())
    {
        std::cout << "Failed to load fomod at " << fomdPath << std::endl;
        return false;
    }

    mgr.fomodState.emplace();
    mgr.fomodState->fomod = std::move(*ld);
    mgr.fomodState->stage = FomodStage::Initial;

    mgr.fomodState->modName = mod;
    mgr.fomodState->tmpDir = tmpDir;

    // TODO load the info file
    mgr.fomodState->name = mod;
    mgr.fomodState->openPopup = true;

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
            //ImGui::BeginChild("4fdsl");
            ImGui::Button("Image placeholder!", ImVec2(100, 60));
            ImGui::TextWrapped("%s", fomod.ssInfo.options[fomod.hoveredOption].optionText.c_str());
            //ImGui::EndChild();
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
                    // cant deselect
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
        std::filesystem::path prefix = *WordExpand(mgr.config.modFolder);
        prefix /= fomod.modName;
        prefix /= "Data";
        std::filesystem::create_directories(prefix);
        // were gonna use -n, no clobber, so we can do the moves high to low and remove unecessary copies.
        std::reverse(fomod.fileActions.actions.begin(), fomod.fileActions.actions.end());
        std::filesystem::path wd = mgr.config.projectDir / fomod.tmpDir;
        for (auto&& action : fomod.fileActions.actions)
        {
            if (action.action == fomod::FileAction::FileToFile)
            {
                std::vector<std::string> mvCmd = {
                    "/usr/bin/mv",
                    "-n",
                    //"-v",
                    mgr.config.projectDir / fomod.tmpDir / action.from,
                    prefix / action.to
                };
                if (!LaunchProc(mvCmd, "/"))
                {
                    std::cout << "!!!!!!! Failed to move a file: " << action.from << std::endl;
                }
            }
            else if (action.action == fomod::FileAction::DirToDir)
            {
                std::vector<std::string> findCmd = {
                    "/usr/bin/find",
                    mgr.config.projectDir / fomod.tmpDir / action.from,
                    "-maxdepth",
                    "1",
                    "-mindepth",
                    "1",
                    "-print0"
                };
                // if find is given an absolute path, it outputs absolute paths
                // otherwise it outputs paths relative to the working directory
                auto findRes = LaunchProcParsePrint0(findCmd, "/");
                if (!findRes)
                {
                    std::cout << "!!!!!!!!!!! Failed to find files in " << mgr.config.projectDir / fomod.tmpDir / action.from << std::endl;
                }
                else
                {
                    for (auto&& p : *findRes)
                    {
                        std::vector<std::string> cp = {
                            "/usr/bin/cp",
                            "-rl",
                            "--update=none",
                            p,
                            "-t",
                            prefix / action.to
                        };
                        if (!LaunchProc(cp, "/"))
                        {
                            std::cout << "Failed to move a dir: " << action.from << std::endl;
                        }
                        // dont need to delete here, because the tmp folder is deleted later.
                    }
                }
            }
        }

        // create mod entry
        auto& mod = mgr.inst.mods.emplace_back();
        mod.enabled = true;
        mod.loadIndex = mgr.inst.mods.size() - 1;
        mod.modFile = fomod.modName;

        // remove staging
        std::vector<std::string> rmCmd = {
            "/usr/bin/rm",
            "-r",
            mgr.config.projectDir / fomod.tmpDir
        };
        if (!LaunchProc(rmCmd, "/"))
        {
            std::cout << "Failed to delete fomod tmp dir: " << mgr.config.projectDir / fomod.tmpDir << std::endl;
        }

        DiscoverPlugins(mgr);

        mgr.fomodState.reset();
        ImGui::CloseCurrentPopup();
    }



    ImGui::EndPopup();

}


