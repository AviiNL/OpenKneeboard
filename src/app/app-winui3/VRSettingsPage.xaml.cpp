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

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/OpenXRMode.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <shims/filesystem>

#include <OpenKneeboard/utf8.hpp>

#include <cmath>
#include <numbers>

using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;

namespace winrt::OpenKneeboardApp::implementation {

static const wchar_t gOpenXRLayerSubkey[]
  = L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit";

VRSettingsPage::VRSettingsPage() {
  this->InitializeComponent();
  mKneeboard = gKneeboard.lock();
  this->PopulateViews();
  AddEventListener(
    mKneeboard->evCurrentProfileChangedEvent,
    {get_weak(), &VRSettingsPage::PopulateViews});
}

VRSettingsPage::~VRSettingsPage() {
  this->RemoveAllEventListeners();
}

fire_and_forget VRSettingsPage::RestoreDefaults(
  IInspectable,
  RoutedEventArgs) noexcept {
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
  mKneeboard->ResetViewsSettings();

  if (!mPropertyChangedEvent) {
    co_return;
  }

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L""));
  this->PopulateViews();
}

bool VRSettingsPage::SteamVREnabled() {
  return mKneeboard->GetVRSettings().mEnableSteamVR;
}

void VRSettingsPage::SteamVREnabled(bool enabled) {
  auto config = mKneeboard->GetVRSettings();
  config.mEnableSteamVR = enabled;
  mKneeboard->SetVRSettings(config);
}

static bool IsOpenXRAPILayerEnabled(
  const std::filesystem::path& jsonPath,
  DWORD extraFlags) {
  DWORD data {};
  DWORD dataSize = sizeof(data);

  const auto result = RegGetValueW(
    HKEY_LOCAL_MACHINE,
    gOpenXRLayerSubkey,
    jsonPath.c_str(),
    RRF_RT_DWORD | extraFlags,
    nullptr,
    &data,
    &dataSize);

  if (result != ERROR_SUCCESS) {
    return false;
  }
  const auto disabled = static_cast<bool>(data);
  return !disabled;
}

bool VRSettingsPage::OpenXR64Enabled() noexcept {
  const auto jsonPath = std::filesystem::canonical(
                          RuntimeFiles::GetInstallationDirectory()
                          / RuntimeFiles::OPENXR_64BIT_JSON)
                          .wstring();
  return IsOpenXRAPILayerEnabled(jsonPath, RRF_SUBKEY_WOW6464KEY);
}

bool VRSettingsPage::OpenXR32Enabled() noexcept {
  const auto jsonPath = std::filesystem::canonical(
                          RuntimeFiles::GetInstallationDirectory()
                          / RuntimeFiles::OPENXR_32BIT_JSON)
                          .wstring();
  return IsOpenXRAPILayerEnabled(jsonPath, RRF_SUBKEY_WOW6432KEY);
}

fire_and_forget VRSettingsPage::AddView(
  muxc::TabView tabView,
  IInspectable) noexcept {
  auto kinds = AddViewKind().Items();
  kinds.Clear();

  kinds.Append(IndependentVRViewUIKind {});
  AddViewKind().SelectedIndex(0);

  using Type = ViewVRConfig::Type;

  std::unordered_set<winrt::guid> mirrored;
  std::unordered_map<winrt::guid, uint32_t> indices;

  auto settings = mKneeboard->GetViewsSettings();
  for (const auto& view: settings.mViews) {
    const auto& vr = view.mVR;
    if (!vr.mEnabled) {
      continue;
    }

    if (vr.GetType() != Type::Independent) {
      mirrored.emplace(vr.GetMirrorOfGUID());
      continue;
    }

    HorizontalMirrorVRViewUIKind item;
    item.MirrorOf(view.mGuid);
    item.Label(
      to_hstring(std::format(_("Horizontal mirror of '{}'"), view.mName)));
    kinds.Append(item);
    indices.emplace(view.mGuid, kinds.Size() - 1);
  }

  for (const auto& view: settings.mViews) {
    if (view.mVR.GetType() != Type::Independent) {
      continue;
    }
    if (mirrored.contains(view.mGuid)) {
      continue;
    }
    if (!indices.contains(view.mGuid)) {
      continue;
    }
    AddViewKind().SelectedIndex(indices.at(view.mGuid));
    break;
  }

  if (co_await AddViewDialog().ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  ViewVRConfig vr;
  vr.mEnabled = true;

  auto item = AddViewKind().SelectedItem();
  if (item.try_as<IndependentVRViewUIKind>()) {
    vr = ViewVRConfig::Independent({});
  } else {
    auto mirror = item.as<HorizontalMirrorVRViewUIKind>();
    vr = ViewVRConfig::HorizontalMirrorOf(mirror.MirrorOf());
  }

  std::string name;
  for (size_t i = settings.mViews.size() + 1; true; ++i) {
    name = std::format(_("Kneeboard {}"), i);
    auto it = std::ranges::find(settings.mViews, name, &ViewConfig::mName);
    if (it == settings.mViews.end()) {
      break;
    }
  }

  if (
    settings.mViews.size() < 2
    && settings.mAppWindowMode == AppWindowViewMode::NoDecision) {
    const auto result = co_await AppWindowViewModeDialog().ShowAsync();
    settings.mAppWindowMode = static_cast<AppWindowViewMode>(
      AppWindowViewModeDialog().SelectedMode(result));
    if (settings.mAppWindowMode == AppWindowViewMode::NoDecision) {
      // User clicked cancel or pressed escape
      co_return;
    }
  }

  settings.mViews.push_back({.mName = name, .mVR = vr});
  mKneeboard->SetViewsSettings(settings);
  AppendViewTab(settings.mViews.back());

  TabView().SelectedIndex(TabView().TabItems().Size() - 1);
}

fire_and_forget VRSettingsPage::RemoveView(
  muxc::TabView tabView,
  muxc::TabViewTabCloseRequestedEventArgs args) noexcept {
  const auto guid = unbox_value<winrt::guid>(args.Tab().Tag());

  {
    const auto views = mKneeboard->GetViewsSettings().mViews;
    const auto it = std::ranges::find(views, guid, &ViewConfig::mGuid);

    const auto message = std::format(
      _("Do you want to completely remove \"{}\" and delete all its' "
        "settings?"),
      it->mName);

    ContentDialog dialog;
    dialog.XamlRoot(XamlRoot());
    dialog.Title(winrt::box_value(winrt::to_hstring(_("Delete view?"))));
    dialog.DefaultButton(ContentDialogButton::Primary);
    dialog.PrimaryButtonText(winrt::to_hstring(_("Delete")));
    dialog.CloseButtonText(winrt::to_hstring(_("Cancel")));
    dialog.Content(box_value(to_hstring(message)));

    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
      co_return;
    }
  }

  // While it was modal and nothing else 'should' have changed things in the
  // mean time, re-fetch just in case
  auto settings = mKneeboard->GetViewsSettings();
  std::erase_if(settings.mViews, [guid](const ViewConfig& view) {
    if (view.mGuid == guid) {
      return true;
    }
    if (view.mVR.GetType() != ViewVRConfig::Type::HorizontalMirror) {
      return false;
    }
    return view.mVR.GetMirrorOfGUID() == guid;
  });
  mKneeboard->SetViewsSettings(settings);

  auto items = tabView.TabItems();
  uint32_t selectedIndex = tabView.SelectedIndex();

  for (uint32_t i = 0; i < items.Size(); /* no increment */) {
    const auto& item = items.GetAt(i);
    const auto itemView
      = unbox_value<winrt::guid>(item.as<TabViewItem>().Tag());
    const auto it = std::ranges::find(
      settings.mViews, itemView, [](const ViewConfig& view) {
        return view.mGuid;
      });
    if (it == settings.mViews.end()) {
      if (i == selectedIndex) {
        tabView.SelectedIndex(--selectedIndex);
      }
      items.RemoveAt(i);
    } else {
      ++i;
    }
  }
}

void VRSettingsPage::AppendViewTab(const ViewConfig& view) noexcept {
  auto items = TabView().TabItems();
  TabViewItem tab;
  tab.Tag(winrt::box_value(view.mGuid));
  tab.Header(winrt::box_value(winrt::to_hstring(view.mName)));
  tab.IsClosable(items.Size() >= 1);

  VRViewSettingsControl viewSettings;
  viewSettings.ViewID(view.mGuid);
  tab.Content(viewSettings);

  TabView().TabItems().Append(tab);
}

void VRSettingsPage::PopulateViews() noexcept {
  TabView().TabItems().Clear();
  for (const auto& view: mKneeboard->GetViewsSettings().mViews) {
    AppendViewTab(view);
  }
  TabView().SelectedIndex(0);
}

fire_and_forget VRSettingsPage::OpenXR64Enabled(bool enabled) noexcept {
  if (enabled == OpenXR64Enabled()) {
    co_return;
  }

  const auto newValue = enabled ? OpenXRMode::AllUsers : OpenXRMode::Disabled;
  co_await SetOpenXR64ModeWithHelperProcess(newValue);
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"OpenXR64Enabled"));
}

fire_and_forget VRSettingsPage::OpenXR32Enabled(bool enabled) noexcept {
  if (enabled == OpenXR32Enabled()) {
    co_return;
  }

  const auto newValue = enabled ? OpenXRMode::AllUsers : OpenXRMode::Disabled;
  co_await SetOpenXR32ModeWithHelperProcess(newValue);
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"OpenXR32Enabled"));
}

}// namespace winrt::OpenKneeboardApp::implementation
