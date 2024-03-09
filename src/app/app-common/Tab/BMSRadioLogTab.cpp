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
#include <OpenKneeboard/BMSRadioLogTab.h>
#include <OpenKneeboard/FalconBMS.h>
#include <OpenKneeboard/FilteredLastModifiedFilePageSource.h>

#include <chrono>
#include <map>

using BMS = OpenKneeboard::FalconBMS;

namespace OpenKneeboard {

BMSRadioLogTab::BMSRadioLogTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : BMSRadioLogTab(dxr, kbs, {}, _("Radio Log")) {
}

BMSRadioLogTab::BMSRadioLogTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title),
    BMSTab(kbs),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mDebugInformation(_("No data from BMS.")) {
  auto path = FalconBMS::GetInstalledPath() / "User" / "Logs";
  auto filter = "RadioSubtitles-";

  mPageSource
    = FilteredLastModifiedFilePageSource::Create(dxr, kbs, filter, path);

  this->SetDelegates({mPageSource});

  // AddEventListener(mPageSource->evPageAppendedEvent,
  // this->evPageAppendedEvent);
}

BMSRadioLogTab::~BMSRadioLogTab() {
  this->RemoveAllEventListeners();
}

std::string BMSRadioLogTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string BMSRadioLogTab::GetStaticGlyph() {
  return "\uF12E";
}

PageIndex BMSRadioLogTab::GetPageCount() const {
  const auto count = mPageSource->GetPageCount();
  // We display a placeholder message
  return count == 0 ? 1 : count;
}

// void BMSRadioLogTab::OnFileModified(const std::filesystem::path& logs) {
//   std::map<time_t, std::filesystem::directory_entry> sort_by_time;

//   for (const auto& entry: std::filesystem::directory_iterator(logs)) {
//     if (
//       entry.is_regular_file()
//       && entry.path().filename().string().starts_with("RadioSubtitles-")) {
//       auto time = to_time_t(entry.last_write_time());
//       sort_by_time[time] = entry;
//     }
//   }

//   if (sort_by_time.empty()) {
//     return;
//   }

//   auto& entry = sort_by_time.rbegin()->second;

//   mPageSource->PushMessage(entry.path().string());

//   this->evContentChangedEvent.Emit();
// }

void BMSRadioLogTab::Reload() {
  mPageSource->Reload();
}

std::string BMSRadioLogTab::GetDebugInformation() const {
  return mDebugInformation;
}

}// namespace OpenKneeboard
