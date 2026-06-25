
#include "modmgr_collection_ui.h"
#include "modmgr_collection.h"
#include "modmgr.h"

#include "imgui.h"

#include <iostream>
#include <format>


void RenderCollectionWindow(ModMgr& mgr)
{
    if (!mgr.inst.collection || mgr.inst.collection->status == CollectionStatus::None)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
    if (ImGui::Begin("Collection"))
    {
        ImGui::Text("%s", mgr.inst.collection->info.name.c_str());
        if (ImGui::Button("Cancel"))
        {
            mgr.inst.collection = {};
        }
        if (mgr.inst.collection->status == CollectionStatus::FetchingInfo)
        {
            if (mgr.inst.collection->error)
            {
                ImGui::Text("Error loading collection info for %s:%d", mgr.inst.collection->url.slug.c_str(), mgr.inst.collection->url.rev);
                if (ImGui::Button("Retry"))
                {
                    auto url = mgr.inst.collection->url;
                    mgr.inst.collection = {};
                    StartNXMCollectionInstall(mgr, url);
                }
            }
            else
            {
                ImGui::Text("Loading info...");
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::WaitingForInstallButton)
        {
            ImGui::Text("Compressed Asset Size: %.1f GB", ((float)mgr.inst.collection->info.totalSize) / 1'000'000'000);
            bool doInstall = ImGui::Button("INSTALL");
            ImGui::Text("%s", mgr.inst.collection->info.name.c_str());
            ImGui::TextWrapped("%s", mgr.inst.collection->info.summary.c_str());
            ImGui::TextWrapped("%s", mgr.inst.collection->info.description.c_str());

            if (doInstall)
            {
                bool foundBundle = false;
                std::filesystem::path collectionData(mgr.config.projectDir / ".mod_staging" / std::format("{}-{}", mgr.inst.collection->url.slug, mgr.inst.collection->url.rev) / "collection.json");
                if (std::filesystem::is_regular_file(collectionData))
                {
                    std::cout << "Found collection bundle, skipping download" << std::endl;
                    DownloadCollectionMods(mgr);
                }
                else
                {
                    GetCollectionBundleLink(mgr);
                }
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::FetchingBundleLink)
        {
            if (mgr.inst.collection->error)
            {
                if (ImGui::Button("Retry"))
                {
                    GetCollectionBundleLink(mgr);
                }
            }
            else
            {
                ImGui::Text("Fetching bundle info");
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::DownloadingBundle)
        {
            if (mgr.inst.collection->error)
            {
                if (ImGui::Button("Retry"))
                {
                    DownloadCollectionBundle(mgr);
                }
            }
            else
            {
                ImGui::Text("Downloading bundle ");
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::DownloadingMods)
        {
            if (mgr.inst.collection->error)
            {
                if (ImGui::Button("Retry"))
                {
                    mgr.inst.collection->error = false;
                    DownloadCollectionMods(mgr);
                }
                ImGui::Text("Will only re-download failed mods");
            }
            else
            {
                ImGui::Text("Downloading mods");
                int count = 0;
                int err = 0;
                for (auto dl : mgr.downloadSessions)
                {
                    if (dl.state != ModDlState::Complete)
                    {
                        ++count;
                    }
                    if (dl.state == ModDlState::Error || dl.state == ModDlState::Canceled)
                    {
                        ++err;
                    }
                }
                ImGui::Text("Waiting for %d remaining mods to download", count - err);
                ImGui::Text("%d mods failed", err);
                if (count == err)
                {
                    if (err > 0)
                    {
                        mgr.inst.collection->error = true;
                    }
                    else
                    {
                        mgr.inst.collection->installIndex = -1;
                        mgr.inst.collection->status = CollectionStatus::InstallingMods;
                    }
                }
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::InstallingMods)
        {
            ImGui::Text("Installing mods");
            ImGui::Text("%d/%d   %s", mgr.inst.collection->installIndex, (int)mgr.inst.collection->bundleDefinition["mods"].size(), mgr.inst.collection->installingCurrentMod.c_str());
            UpdateInstallCollectionMods(mgr);
        }
        else if (mgr.inst.collection->status == CollectionStatus::InstallWaitingFailedMods)
        {
            if (mgr.inst.collection->error)
            {
                if (ImGui::Button("Retry"))
                {
                    mgr.inst.collection->installIndex = -1;
                    mgr.inst.collection->error = false;
                    mgr.inst.collection->status = CollectionStatus::InstallingMods;
                }
                ImGui::Text("%d mods failed to install, or look wrong.", (int)mgr.inst.collection->installErrorInfo.size());
                ImGui::TextWrapped("You should manually check or install the failed mods, then click retry to continue.");
                ImGui::TextWrapped("Some mods will have residual files in their mod folder.");
                ImGui::Separator();
                for (auto&& msg : mgr.inst.collection->installErrorInfo)
                {
                    ImGui::Text("%s", msg.c_str());
                }
            }
            else
            {
                // shouldnt get here
                if (ImGui::Button("Continue"))
                {
                    mgr.inst.collection->status = CollectionStatus::ConfigureLoadOrder;
                }
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::ConfigureLoadOrder)
        {
            if (mgr.inst.collection->error)
            {
                if (ImGui::Button("Retry load order"))
                {
                    mgr.inst.collection->error = false;
                    ApplyCollectionLoadOrder(mgr);
                }
            }
            else
            {
                ApplyCollectionLoadOrder(mgr);
            }
        }
        else if (mgr.inst.collection->status == CollectionStatus::Installed)
        {
            ImGui::Text("Done");
            if (ImGui::Button("Reapply Load Order"))
            {
                ApplyCollectionLoadOrder(mgr);
            }
            ImGui::TextWrapped("Check output log for potential issues.  Improperly packaged mods, or mods set to the wrong install type, may not be installed correctly.  This program does not attempt to guess how a mod should be installed. FOMOD mods should be fine.");
            ImGui::TextWrapped("SKSE will never install correctly due to how it is packaged, and must be manually installed into a mod.");
        }
        

    }
    ImGui::End();
}







