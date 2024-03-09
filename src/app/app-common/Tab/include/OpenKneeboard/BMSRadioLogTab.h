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

#include "BMSTab.h"
#include "TabBase.h"

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/FilesystemWatcher.h>
#include <OpenKneeboard/IHasDebugInformation.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>

#include <OpenKneeboard/audited_ptr.h>

#include <shims/winrt/base.h>

#include <string>

namespace OpenKneeboard {

class FilteredLastModifiedFilePageSource;

class BMSRadioLogTab final : public TabBase,
                             public BMSTab,
                             public PageSourceWithDelegates,
                             public IHasDebugInformation {
 public:
  explicit BMSRadioLogTab(const audited_ptr<DXResources>&, KneeboardState*);
  explicit BMSRadioLogTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  virtual ~BMSRadioLogTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual std::string GetDebugInformation() const override;

  virtual PageIndex GetPageCount() const override;
  virtual void Reload() override;

 protected:
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard = nullptr;
  std::string mDebugInformation;

 private:
  struct Settings;
  winrt::apartment_context mUIThread;
  std::shared_ptr<FilteredLastModifiedFilePageSource> mPageSource;

  std::shared_ptr<FilesystemWatcher> mWatcher;
  void OnFileModified(const std::filesystem::path& logs);
};

}// namespace OpenKneeboard
