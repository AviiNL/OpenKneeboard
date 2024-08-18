/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DCSExtractedMission.hpp>
#include <OpenKneeboard/DCSMissionTab.hpp>
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/FolderPageSource.hpp>

#include <OpenKneeboard/dprint.hpp>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSMissionTab::DCSMissionTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : DCSMissionTab(dxr, kbs, {}, _("Mission")) {
}

DCSMissionTab::DCSMissionTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title),
    DCSTab(kbs),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mDebugInformation(_("No data from DCS.")) {
}

DCSMissionTab::~DCSMissionTab() {
  this->RemoveAllEventListeners();
}

std::string DCSMissionTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string DCSMissionTab::GetStaticGlyph() {
  return "\uF0E3";
}

task<void> DCSMissionTab::Reload() {
  if (mMission.empty()) {
    co_return;
  }

  // Free the filesystem watchers etc before potentially
  // deleting the extracted mission
  co_await this->SetDelegates({});

  if ((!mExtracted) || mExtracted->GetZipPath() != mMission) {
    mExtracted = DCSExtractedMission::Get(mMission);
  }

  mDebugInformation = to_utf8(mMission) + "\n";

  const auto root = mExtracted->GetExtractedPath();

  std::vector<std::filesystem::path> paths {
    std::filesystem::path("KNEEBOARD") / "IMAGES",
  };

  if (!mAircraft.empty()) {
    paths.push_back(std::filesystem::path("KNEEBOARD") / mAircraft / "IMAGES");
  }

  std::vector<std::shared_ptr<IPageSource>> sources;

  for (const auto& path: paths) {
    if (std::filesystem::exists(root / path)) {
      sources.push_back(
        co_await FolderPageSource::Create(mDXR, mKneeboard, root / path));
      mDebugInformation += std::format("\u2714 miz:\\{}\n", to_utf8(path));
    } else {
      mDebugInformation += std::format("\u274c miz:\\{}\n", to_utf8(path));
    }
  }

  if (mDebugInformation.ends_with('\n')) {
    mDebugInformation.pop_back();
  }

  dprint("Mission tab: " + mDebugInformation);
  evDebugInformationHasChanged.Emit(mDebugInformation);
  co_await this->SetDelegates(sources);
}

std::string DCSMissionTab::GetDebugInformation() const {
  return mDebugInformation;
}

OpenKneeboard::fire_and_forget DCSMissionTab::OnAPIEvent(
  APIEvent event,
  [[maybe_unused]] std::filesystem::path installPath,
  [[maybe_unused]] std::filesystem::path savedGamePath) {
  if (event.name == DCS::EVT_MISSION) {
    const auto missionZip = this->ToAbsolutePath(event.value);
    if (missionZip.empty() || !std::filesystem::exists(missionZip)) {
      dprintf("MissionTab: mission '{}' does not exist", event.value);
      co_return;
    }

    if (missionZip == mMission) {
      co_return;
    }

    mMission = missionZip;
    co_await this->Reload();
    co_return;
  }

  if (event.name == DCS::EVT_AIRCRAFT) {
    if (event.value == mAircraft) {
      co_return;
    }
    mAircraft = event.value;
    co_await this->Reload();
    co_return;
  }
}

}// namespace OpenKneeboard
