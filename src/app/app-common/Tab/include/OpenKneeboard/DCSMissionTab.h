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
#pragma once

#include <OpenKneeboard/PageSourceWithDelegates.h>

#include "DCSTab.h"
#include "TabBase.h"

namespace OpenKneeboard {

struct DXResources;
class DCSExtractedMission;
class FolderPageSource;

class DCSMissionTab final : public TabBase,
                            public DCSTab,
                            public PageSourceWithDelegates {
 public:
  DCSMissionTab(const DXResources&, KneeboardState*);
  DCSMissionTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    utf8_string_view title);
  virtual ~DCSMissionTab();
  virtual utf8_string GetGlyph() const override;

  virtual void Reload() override;

 protected:
  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path&,
    const std::filesystem::path&) override;

 private:
  std::filesystem::path mMission;
  std::string mAircraft;
  std::shared_ptr<DCSExtractedMission> mExtracted;
  std::shared_ptr<FolderPageSource> mPageSource;
};

}// namespace OpenKneeboard
