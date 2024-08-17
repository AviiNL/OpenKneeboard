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

#include <OpenKneeboard/HWNDPageSource.hpp>
#include <OpenKneeboard/ITabWithSettings.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>
#include <OpenKneeboard/TabBase.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/handles.hpp>

namespace OpenKneeboard {

class WindowCaptureTab final : public TabBase,
                               public ITabWithSettings,
                               public PageSourceWithDelegates {
 public:
  struct WindowSpecification {
    std::string mExecutablePathPattern;
    std::filesystem::path mExecutableLastSeenPath;
    std::string mWindowClass;
    std::string mTitle;

    constexpr bool operator==(const WindowSpecification&) const noexcept
      = default;
  };
  struct MatchSpecification : public WindowSpecification {
    enum class TitleMatchKind : uint8_t {
      Ignore = 0,
      Exact = 1,
      Glob = 2,
    };

    TitleMatchKind mMatchTitle {TitleMatchKind::Ignore};
    bool mMatchWindowClass {true};
    bool mMatchExecutable {true};

    constexpr bool operator==(const MatchSpecification&) const noexcept
      = default;
  };
  struct Settings {
    MatchSpecification mSpec {};
    bool mSendInput = false;
    HWNDPageSource::Options mCaptureOptions {};

    constexpr bool operator==(const Settings&) const noexcept = default;
  };

  WindowCaptureTab() = delete;
  static std::shared_ptr<WindowCaptureTab> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const MatchSpecification&);
  static std::shared_ptr<WindowCaptureTab> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json& settings);
  static winrt::fire_and_forget final_release(
    std::unique_ptr<WindowCaptureTab>);
  virtual ~WindowCaptureTab();
  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  [[nodiscard]]
  virtual task<void> Reload() final override;
  virtual nlohmann::json GetSettings() const override;

  MatchSpecification GetMatchSpecification() const;
  void SetMatchSpecification(const MatchSpecification&);

  bool IsInputEnabled() const;
  void SetIsInputEnabled(bool);

  using CaptureArea = HWNDPageSource::CaptureArea;
  CaptureArea GetCaptureArea() const;
  void SetCaptureArea(CaptureArea);

  bool IsCursorCaptureEnabled() const;
  void SetCursorCaptureEnabled(bool);

  static std::unordered_map<HWND, WindowSpecification> GetTopLevelWindows();
  static std::optional<WindowSpecification> GetWindowSpecification(HWND);

  virtual void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID)
    override final;

 private:
  WindowCaptureTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const Settings&);
  winrt::fire_and_forget TryToStartCapture();
  task<bool> TryToStartCapture(HWND hwnd);
  bool WindowMatches(HWND hwnd);

  winrt::fire_and_forget OnWindowClosed();

  static void WinEventProc_NewWindowHook(
    HWINEVENTHOOK,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime);
  task<void> OnNewWindow(HWND hwnd);

  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard {nullptr};
  MatchSpecification mSpec;
  bool mSendInput = false;
  HWND mHwnd {};
  HWNDPageSource::Options mCaptureOptions {};
  std::shared_ptr<HWNDPageSource> mDelegate;

  unique_hwineventhook mEventHook;
  static std::mutex gHookMutex;
  static std::map<HWINEVENTHOOK, std::weak_ptr<WindowCaptureTab>> gHooks;
};

}// namespace OpenKneeboard
