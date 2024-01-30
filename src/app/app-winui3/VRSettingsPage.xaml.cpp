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
#include "VRSettingsPage.xaml.h"
#include "VRSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/RuntimeFiles.h>

#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

#include <shims/filesystem>

#include <cmath>
#include <numbers>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

static const wchar_t gOpenXRLayerSubkey[]
  = L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit";

VRSettingsPage::VRSettingsPage() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
}

fire_and_forget VRSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default VR settings, "
                           "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  mKneeboard->ResetVRSettings();

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
}

void VRSettingsPage::RecenterNow(const IInspectable&, const RoutedEventArgs&) {
  mKneeboard->PostUserAction(UserAction::RECENTER_VR);
}

void VRSettingsPage::GoToBindings(const IInspectable&, const RoutedEventArgs&) {
  Frame().Navigate(xaml_typename<InputSettingsPage>());
}

float VRSettingsPage::KneeboardX() {
  return mKneeboard->GetVRSettings().mDeprecated.mPrimaryLayer.mX;
}

void VRSettingsPage::KneeboardX(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mDeprecated.mPrimaryLayer.mX = value;
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardEyeY() {
  return -mKneeboard->GetVRSettings().mDeprecated.mPrimaryLayer.mEyeY;
}

void VRSettingsPage::KneeboardEyeY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mDeprecated.mPrimaryLayer.mEyeY = -value;
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardZ() {
  // 3D standard right-hand-coordinate system is that -z is forwards;
  // most users expect the opposite.
  return -mKneeboard->GetVRSettings().mDeprecated.mPrimaryLayer.mZ;
}

void VRSettingsPage::KneeboardZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mDeprecated.mPrimaryLayer.mZ = -value;
  mKneeboard->SetVRSettings(config);
}

static inline float RadiansToDegrees(float radians) {
  return radians * 180 / std::numbers::pi_v<float>;
}

static inline float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180;
}

float VRSettingsPage::KneeboardRX() {
  auto raw = RadiansToDegrees(
               mKneeboard->GetVRSettings().mDeprecated.mPrimaryLayer.mRX)
    + 90;
  if (raw < 0) {
    raw += 360.0f;
  }
  if (raw >= 360.0f) {
    raw -= 360.0f;
  }
  return raw <= 180 ? raw : -(360 - raw);
}

void VRSettingsPage::KneeboardRX(float degrees) {
  degrees -= 90;
  if (degrees < 0) {
    degrees += 360;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mDeprecated.mPrimaryLayer.mRX
    = DegreesToRadians(degrees <= 180 ? degrees : -(360 - degrees));
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardRY() {
  return -RadiansToDegrees(
    mKneeboard->GetVRSettings().mDeprecated.mPrimaryLayer.mRY);
}

void VRSettingsPage::KneeboardRY(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mDeprecated.mPrimaryLayer.mRY = -DegreesToRadians(value);
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardRZ() {
  return -RadiansToDegrees(
    mKneeboard->GetVRSettings().mDeprecated.mPrimaryLayer.mRZ);
}

void VRSettingsPage::KneeboardRZ(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mDeprecated.mPrimaryLayer.mRZ = -DegreesToRadians(value);
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardMaxHeight() {
  return mKneeboard->GetVRSettings().mMaxHeight;
}

void VRSettingsPage::KneeboardMaxHeight(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mMaxHeight = value;
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardMaxWidth() {
  return mKneeboard->GetVRSettings().mMaxWidth;
}

void VRSettingsPage::KneeboardMaxWidth(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mMaxWidth = value;
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardZoomScale() {
  return mKneeboard->GetVRSettings().mZoomScale;
}

void VRSettingsPage::KneeboardZoomScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mZoomScale = value;
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardGazeTargetHorizontalScale() {
  return mKneeboard->GetVRSettings().mGazeTargetScale.mHorizontal;
}

void VRSettingsPage::KneeboardGazeTargetHorizontalScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mGazeTargetScale.mHorizontal = value;
  mKneeboard->SetVRSettings(config);
}

float VRSettingsPage::KneeboardGazeTargetVerticalScale() {
  return mKneeboard->GetVRSettings().mGazeTargetScale.mVertical;
}

void VRSettingsPage::KneeboardGazeTargetVerticalScale(float value) {
  if (std::isnan(value)) {
    return;
  }
  auto config = mKneeboard->GetVRSettings();
  config.mGazeTargetScale.mVertical = value;
  mKneeboard->SetVRSettings(config);
}

uint8_t VRSettingsPage::NormalOpacity() {
  return static_cast<uint8_t>(
    std::lround(mKneeboard->GetVRSettings().mOpacity.mNormal * 100));
}

void VRSettingsPage::NormalOpacity(uint8_t value) {
  auto config = mKneeboard->GetVRSettings();
  config.mOpacity.mNormal = value / 100.0f;
  mKneeboard->SetVRSettings(config);
}

uint8_t VRSettingsPage::GazeOpacity() {
  return static_cast<uint8_t>(
    std::lround(mKneeboard->GetVRSettings().mOpacity.mGaze * 100));
}

void VRSettingsPage::GazeOpacity(uint8_t value) {
  auto config = mKneeboard->GetVRSettings();
  config.mOpacity.mGaze = value / 100.0f;
  mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::SteamVREnabled() {
  return mKneeboard->GetVRSettings().mEnableSteamVR;
}

void VRSettingsPage::SteamVREnabled(bool enabled) {
  auto config = mKneeboard->GetVRSettings();
  config.mEnableSteamVR = enabled;
  mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::GazeZoomEnabled() {
  return mKneeboard->GetVRSettings().mEnableGazeZoom;
}

void VRSettingsPage::GazeZoomEnabled(bool enabled) {
  auto config = mKneeboard->GetVRSettings();
  config.mEnableGazeZoom = enabled;
  mKneeboard->SetVRSettings(config);
}

bool VRSettingsPage::OpenXREnabled() noexcept {
  DWORD data {};
  DWORD dataSize = sizeof(data);
  const auto jsonPath
    = std::filesystem::canonical(
        RuntimeFiles::GetInstallationDirectory() / RuntimeFiles::OPENXR_JSON)
        .wstring();
  const auto result = RegGetValueW(
    HKEY_LOCAL_MACHINE,
    gOpenXRLayerSubkey,
    jsonPath.c_str(),
    RRF_RT_DWORD,
    nullptr,
    &data,
    &dataSize);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  const auto disabled = static_cast<bool>(data);
  return !disabled;
}

fire_and_forget VRSettingsPage::OpenXREnabled(bool enabled) noexcept {
  if (enabled == OpenXREnabled()) {
    co_return;
  }

  const auto newValue = enabled ? OpenXRMode::AllUsers : OpenXRMode::Disabled;
  const auto oldValue = enabled ? OpenXRMode::Disabled : OpenXRMode::AllUsers;
  co_await SetOpenXRModeWithHelperProcess(newValue, {oldValue});
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"OpenXREnabled"));
}

}// namespace winrt::OpenKneeboardApp::implementation
