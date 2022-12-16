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
#include <OpenKneeboard/TabBase.h>

namespace OpenKneeboard {

class HWNDPageSource;

class WindowCaptureTab final : public TabBase, public PageSourceWithDelegates {
 public:
  struct WindowSpecification {
    std::filesystem::path mExecutable;
    std::string mWindowClass;
    std::string mTitle;
  };
  struct MatchSpecification : public WindowSpecification {
    enum class TitleMatchKind {
      None,
      Exact,
      Glob,
    };

    TitleMatchKind mMatchTitle {TitleMatchKind::Exact};
    bool mMatchWindowClass {true};
    bool mMatchExecutable {true};
  };

  WindowCaptureTab() = delete;
  explicit WindowCaptureTab(const DXResources&, KneeboardState*);
  explicit WindowCaptureTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    utf8_string_view title);
  virtual ~WindowCaptureTab();
  virtual utf8_string GetGlyph() const override;

  virtual void Reload() final override;

 private:
  std::shared_ptr<HWNDPageSource> mPageSource;
};

}// namespace OpenKneeboard
