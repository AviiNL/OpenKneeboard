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

void BMSRadioLogTab::Reload() {
  mPageSource->Reload();
}

std::string BMSRadioLogTab::GetDebugInformation() const {
  return mDebugInformation;
}

}// namespace OpenKneeboard
