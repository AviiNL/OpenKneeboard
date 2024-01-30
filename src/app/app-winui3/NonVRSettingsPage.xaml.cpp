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
// clang-format off
#include "pch.h"
#include "NonVRSettingsPage.xaml.h"
#include "NonVRSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.h>

#include <OpenKneeboard/utf8.h>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

NonVRSettingsPage::NonVRSettingsPage() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
}

fire_and_forget NonVRSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default non-VR "
                           "settings, removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  mKneeboard->ResetNonVRSettings();

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

uint8_t NonVRSettingsPage::KneeboardHeightPercent() {
  return mKneeboard->GetNonVRSettings().mDeprecated.mHeightPercent;
}

void NonVRSettingsPage::KneeboardHeightPercent(uint8_t value) {
  auto config = mKneeboard->GetNonVRSettings().mDeprecated;
  config.mHeightPercent = value;
  mKneeboard->SetNonVRSettings({config});
}

uint32_t NonVRSettingsPage::KneeboardPaddingPixels() {
  return mKneeboard->GetNonVRSettings().mDeprecated.mPaddingPixels;
}

void NonVRSettingsPage::KneeboardPaddingPixels(uint32_t value) {
  auto config = mKneeboard->GetNonVRSettings().mDeprecated;
  config.mPaddingPixels = value;
  mKneeboard->SetNonVRSettings({config});
}

float NonVRSettingsPage::KneeboardOpacity() {
  return mKneeboard->GetNonVRSettings().mDeprecated.mOpacity * 100;
}

void NonVRSettingsPage::KneeboardOpacity(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetNonVRSettings().mDeprecated;
  config.mOpacity = value / 100;
  mKneeboard->SetNonVRSettings({config});
}

uint8_t NonVRSettingsPage::KneeboardHorizontalPlacement() {
  return static_cast<uint8_t>(
    mKneeboard->GetNonVRSettings().mDeprecated.mHorizontalAlignment);
}

void NonVRSettingsPage::KneeboardHorizontalPlacement(uint8_t value) {
  auto config = mKneeboard->GetNonVRSettings().mDeprecated;
  config.mHorizontalAlignment = static_cast<Alignment::Horizontal>(value);
  mKneeboard->SetNonVRSettings({config});
}

uint8_t NonVRSettingsPage::KneeboardVerticalPlacement() {
  return static_cast<uint8_t>(
    mKneeboard->GetNonVRSettings().mDeprecated.mVerticalAlignment);
}

void NonVRSettingsPage::KneeboardVerticalPlacement(uint8_t value) {
  auto config = mKneeboard->GetNonVRSettings().mDeprecated;
  config.mVerticalAlignment = static_cast<Alignment::Vertical>(value);
  mKneeboard->SetNonVRSettings({config});
}

}// namespace winrt::OpenKneeboardApp::implementation
