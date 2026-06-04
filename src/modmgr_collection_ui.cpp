
#include "modmgr_collection_ui.h"
#include "modmgr_collection.h"
#include "modmgr.h"

#include "imgui.h"


void RenderCollectionWindow(ModMgr& mgr)
{
    if (mgr.collection.status == CollectionStatus::None)
    {
        return;
    }

    if (ImGui::Begin("Collection"))
    {
        ImGui::Text("%s", mgr.collection.info.name.c_str());
        if (ImGui::Button("Cancel"))
        {
            mgr.collection = {};
        }
        if (mgr.collection.status == CollectionStatus::FetchingInfo)
        {
            if (mgr.collection.error)
            {
                ImGui::Text("Error loading collection info for %s:%d", mgr.collection.url.slug.c_str(), mgr.collection.url.rev);
                if (ImGui::Button("Retry"))
                {
                    auto url = mgr.collection.url;
                    mgr.collection = {};
                    StartNXMCollectionInstall(mgr, url);
                }
            }
            else
            {
                ImGui::Text("Loading info...");
            }
        }
        else if (mgr.collection.status == CollectionStatus::WaitingForInstallButton)
        {
            ImGui::Text("Compressed Asset Size: %.1f GB", ((float)mgr.collection.info.totalSize) / 1'000'000'000);
            bool doInstall = ImGui::Button("INSTALL");
            ImGui::Text("%s", mgr.collection.info.name.c_str());
            ImGui::TextWrapped("%s", mgr.collection.info.summary.c_str());
            ImGui::TextWrapped("%s", mgr.collection.info.description.c_str());

            if (doInstall)
            {
                GetCollectionBundleLink(mgr);
            }
        }
        else if (mgr.collection.status == CollectionStatus::FetchingBundleLink)
        {
            if (mgr.collection.error)
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
        else if (mgr.collection.status == CollectionStatus::DownloadingBundle)
        {
            if (mgr.collection.error)
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
        else if (mgr.collection.status == CollectionStatus::DownloadingMods)
        {
            if (mgr.collection.error)
            {
                if (ImGui::Button("Retry"))
                {
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
                ImGui::Text("Waiting for %d remaining mods to download", count);
                ImGui::Text("%d mods failed", err);
                if (count == err)
                {
                    if (err > 0)
                    {
                        mgr.collection.error = true;
                    }
                    else
                    {
                        mgr.collection.installIndex = -1;
                        mgr.collection.status = CollectionStatus::InstallingMods;
                    }
                }
            }
        }
        else if (mgr.collection.status == CollectionStatus::InstallingMods)
        {
            if (mgr.collection.error)
            {
                if (ImGui::Button("Retry"))
                {
                    mgr.collection.installIndex = -1;
                    mgr.collection.status = CollectionStatus::InstallingMods;
                }
                ImGui::Text("%d mods failed to install", (int)mgr.collection.installErrorInfo.size());
            }
            else
            {
                ImGui::Text("Installing mods");
                ImGui::Text("%s", mgr.collection.installingCurrentMod.c_str());
                UpdateInstallCollectionMods(mgr);
            }
        }
        else if (mgr.collection.status == CollectionStatus::ConfigureLoadOrder)
        {
            ApplyCollectionLoadOrder(mgr);
        }
        else if (mgr.collection.status == CollectionStatus::Installed)
        {
            ImGui::Text("Done");
        }
        

    }
    ImGui::End();
}







