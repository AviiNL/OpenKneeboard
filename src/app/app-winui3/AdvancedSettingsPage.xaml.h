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

// clang-format off
#include "pch.h"
// clang-format on

#include "AdvancedSettingsPage.g.h"
#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>

using namespace winrt::Microsoft::UI::Xaml;

namespace OpenKneeboard {
class KneeboardState;
};

namespace winrt::OpenKneeboardApp::implementation {
struct AdvancedSettingsPage
  : AdvancedSettingsPageT<AdvancedSettingsPage>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<
      AdvancedSettingsPage> {
  AdvancedSettingsPage();
  ~AdvancedSettingsPage();

  bool MultipleProfiles() const noexcept;
  OpenKneeboard::fire_and_forget MultipleProfiles(bool value) noexcept;

  bool Bookmarks() const noexcept;
  OpenKneeboard::fire_and_forget Bookmarks(bool value) noexcept;

  uint8_t AppWindowViewMode() const noexcept;
  OpenKneeboard::fire_and_forget AppWindowViewMode(uint8_t value) noexcept;

  bool EnableMouseButtonBindings() const noexcept;
  OpenKneeboard::fire_and_forget EnableMouseButtonBindings(bool value) noexcept;

  bool GazeInputFocus() const noexcept;
  OpenKneeboard::fire_and_forget GazeInputFocus(bool value) noexcept;

  bool LoopPages() const noexcept;
  OpenKneeboard::fire_and_forget LoopPages(bool) noexcept;

  bool LoopTabs() const noexcept;
  OpenKneeboard::fire_and_forget LoopTabs(bool) noexcept;

  bool LoopProfiles() const noexcept;
  OpenKneeboard::fire_and_forget LoopProfiles(bool) noexcept;

  bool LoopBookmarks() const noexcept;
  OpenKneeboard::fire_and_forget LoopBookmarks(bool) noexcept;

  bool InGameHeader() const noexcept;
  OpenKneeboard::fire_and_forget InGameHeader(bool) noexcept;

  bool InGameFooter() const noexcept;
  OpenKneeboard::fire_and_forget InGameFooter(bool) noexcept;

  bool InGameFooterFrameCount() const noexcept;
  OpenKneeboard::fire_and_forget InGameFooterFrameCount(bool) noexcept;

  OpenKneeboard::fire_and_forget RestoreDoodleDefaults(
    Windows::Foundation::IInspectable,
    Windows::Foundation::IInspectable) noexcept;
  OpenKneeboard::fire_and_forget RestoreTextDefaults(
    Windows::Foundation::IInspectable,
    Windows::Foundation::IInspectable) noexcept;
  OpenKneeboard::fire_and_forget RestoreQuirkDefaults(
    Windows::Foundation::IInspectable,
    Windows::Foundation::IInspectable) noexcept;

  uint32_t MinimumPenRadius();
  OpenKneeboard::fire_and_forget MinimumPenRadius(uint32_t value);
  uint32_t PenSensitivity();
  OpenKneeboard::fire_and_forget PenSensitivity(uint32_t value);

  uint32_t MinimumEraseRadius();
  OpenKneeboard::fire_and_forget MinimumEraseRadius(uint32_t value);
  uint32_t EraseSensitivity();
  OpenKneeboard::fire_and_forget EraseSensitivity(uint32_t value);

  float TextPageFontSize();
  OpenKneeboard::fire_and_forget TextPageFontSize(float value);

  bool TintEnabled();
  OpenKneeboard::fire_and_forget TintEnabled(bool value);
  float TintBrightness();
  OpenKneeboard::fire_and_forget TintBrightness(float value);
  winrt::Windows::UI::Color Tint();
  OpenKneeboard::fire_and_forget Tint(winrt::Windows::UI::Color value);

  bool Quirk_OculusSDK_DiscardDepthInformation() const noexcept;
  OpenKneeboard::fire_and_forget Quirk_OculusSDK_DiscardDepthInformation(
    bool value) noexcept;

  uint8_t Quirk_OpenXR_Upscaling() const noexcept;
  OpenKneeboard::fire_and_forget Quirk_OpenXR_Upscaling(uint8_t value) noexcept;

  bool CanChangeElevation() const noexcept;

  int32_t DesiredElevation() const noexcept;
  OpenKneeboard::fire_and_forget DesiredElevation(int32_t value) noexcept;

 private:
  winrt::apartment_context mUIThread {};
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct AdvancedSettingsPage : AdvancedSettingsPageT<
                                AdvancedSettingsPage,
                                implementation::AdvancedSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
