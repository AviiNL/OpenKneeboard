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
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/ProfileSettings.hpp>
#include <OpenKneeboard/Settings.hpp>

#include <OpenKneeboard/json/LegacyNonVRSettings.hpp>
#include <OpenKneeboard/json/VRSettings.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <shims/winrt/base.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

namespace OpenKneeboard {

template <class T>
static void MaybeSetFromJSON(T& out, const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Filesystem::GetSettingsDirectory() / path;
  if (!std::filesystem::exists(fullPath)) {
    return;
  }

  try {
    std::ifstream f(fullPath.c_str());
    nlohmann::json json;
    f >> json;
    if constexpr (std::same_as<T, nlohmann::json>) {
      out = json;
    } else {
      OpenKneeboard::from_json(json, out);
    }
  } catch (const nlohmann::json::exception& e) {
    dprint(
      "Error reading JSON from file '{}': {}", fullPath.string(), e.what());
    OPENKNEEBOARD_BREAK;
  }
}

template <class T>
static void MaybeSaveJSON(
  const T& parentValue,
  const T& value,
  const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Filesystem::GetSettingsDirectory() / path;

  nlohmann::json j;
  // If a profile already modified a setting, keep that setting even if it
  // matches the parent now
  if (std::filesystem::exists(fullPath)) {
    std::ifstream f(fullPath);
    try {
      f >> j;
    } catch (const nlohmann::json::exception& e) {
      dprint(
        "Error reading JSON from file '{}': {}", fullPath.string(), e.what());
    }
  }

  const auto parentPath = fullPath.parent_path();
  if (!std::filesystem::exists(parentPath)) {
    std::filesystem::create_directories(parentPath);
  }

  to_json_with_default(j, parentValue, value);
  if (j.is_object() && j.size() > 0) {
    std::ofstream f(fullPath);
    f << std::setw(2) << j << std::endl;
    return;
  }

  if (std::filesystem::exists(fullPath)) {
    std::filesystem::remove(fullPath);
  }
}

/** Used for GamesList and TabsList, where we don't want to merge configs -
 * either inherit, or overwrite */
template <>
void MaybeSaveJSON<nlohmann::json>(
  const nlohmann::json& parentValue,
  const nlohmann::json& value,
  const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Filesystem::GetSettingsDirectory() / path;

  if (value == parentValue && std::filesystem::exists(fullPath)) {
    std::filesystem::remove(fullPath);
    return;
  }

  const auto parentPath = fullPath.parent_path();
  if (!std::filesystem::exists(parentPath)) {
    std::filesystem::create_directories(parentPath);
  }

  std::ofstream f(fullPath);
  f << std::setw(2) << value << std::endl;
}

void Settings::Save(
  const winrt::guid defaultProfile,
  const winrt::guid activeProfile) const {
  const Settings previousSettings
    = Settings::Load(defaultProfile, activeProfile);

  if (previousSettings == *this) {
    return;
  }

  Settings parentSettings;
  if (activeProfile != defaultProfile) {
    parentSettings = Settings::Load(defaultProfile, defaultProfile);
  }

  const auto profileDir = std::filesystem::path {"Profiles"}
    / ProfileSettings::Profile::GetDirectoryName(activeProfile);

#define IT(cpptype, x) \
  MaybeSaveJSON(parentSettings.m##x, this->m##x, profileDir / #x ".json");
  OPENKNEEBOARD_PER_PROFILE_SETTINGS_SECTIONS
#undef IT
#define IT(cpptype, x) \
  MaybeSaveJSON(parentSettings.m##x, this->m##x, #x ".json");
  OPENKNEEBOARD_GLOBAL_SETTINGS_SECTIONS
#undef IT
}

template <>
void from_json_postprocess<Settings>(const nlohmann::json& j, Settings& s) {
  // Backwards-compatibility

  if (j.contains("DirectInputV2")) {
    s.mDirectInput = j.at("DirectInputV2");
  }
  if (j.contains("Doodle")) {
    s.mDoodles = j.at("Doodle");
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  Settings,
  mGames,
  mTabs,
  mApp,
  mDirectInput,
  mDoodles,
  mText,
  mTabletInput,
  mViews,
  mVR)

#define RESET_IT(cpptype, name, path_suffix) \
  void Settings::Reset##name##Section( \
    const winrt::guid defaultProfile, const winrt::guid activeProfile) { \
    if (defaultProfile == activeProfile) { \
      m##name = {}; \
    } else { \
      m##name = Settings::Load(defaultProfile, defaultProfile).m##name; \
    } \
    const auto path = Filesystem::GetSettingsDirectory() / path_suffix; \
    if (std::filesystem::exists(path)) { \
      std::filesystem::remove(path); \
    } \
  }
#define IT(cpptype, name) \
  RESET_IT( \
    cpptype, \
    name, \
    "Profiles" / ProfileSettings::Profile::GetDirectoryName(activeProfile) \
      / #name ".json")
OPENKNEEBOARD_PER_PROFILE_SETTINGS_SECTIONS
#undef IT
#define IT(cpptype, name) RESET_IT(cpptype, name, #name ".json")
OPENKNEEBOARD_GLOBAL_SETTINGS_SECTIONS
#undef IT
#undef RESET_IT

// v1.2 -> v1.3
static void MigrateToProfiles(
  Settings& settings,
  const winrt::guid defaultProfile,
  const winrt::guid activeProfile) {
  if (defaultProfile != activeProfile) {
    return;
  }

  if (std::filesystem::exists(
        Filesystem::GetSettingsDirectory() / "Profiles")) {
    return;
  }

  auto legacySettingsFile
    = Filesystem::GetSettingsDirectory() / "Settings.json";
  if (!std::filesystem::exists(legacySettingsFile)) {
    return;
  }

  dprint("Migrating from legacy Settings.json");
  MaybeSetFromJSON(settings, legacySettingsFile);
  std::filesystem::remove(legacySettingsFile);
  settings.Save(defaultProfile, defaultProfile);
}

// v1.7 introduced 'ViewsSettings'
static void MigrateToViewsSettings(Settings& settings) {
  const auto& oldVR = settings.mVR.mDeprecated;

  IndependentViewVRSettings vrConfig {
    .mPose = oldVR.mPrimaryLayer,
    .mMaximumPhysicalSize = {
      oldVR.mMaxWidth,
      oldVR.mMaxHeight,
    },
    .mEnableGazeZoom = oldVR.mEnableGazeZoom,
    .mZoomScale = oldVR.mZoomScale,
    .mGazeTargetScale = oldVR.mGazeTargetScale,
    .mOpacity = oldVR.mOpacity,
  };

  const ViewSettings primary {
    .mName = _("Kneeboard 1"),
    .mVR = ViewVRSettings::Independent(vrConfig),
    .mNonVR = ViewNonVRSettings {
      .mEnabled = true,
      .mConstraints = static_cast<const NonVRConstrainedPosition&>(settings.mDeprecatedNonVR),
      .mOpacity = settings.mDeprecatedNonVR.mOpacity,
    },
  };

  if (settings.mApp.mDeprecated.mDualKneeboards.mEnabled) {
    const ViewSettings secondary {
      .mName = _("Kneeboard 2"),
      .mVR = ViewVRSettings::HorizontalMirrorOf(primary.mGuid),
    };
    settings.mViews.mViews = {primary, secondary};
  } else {
    settings.mViews.mViews = {primary};
  }
}

Settings Settings::Load(
  const winrt::guid defaultProfile,
  const winrt::guid activeProfile) {
  dprint("Reading profile '{}' from disk", activeProfile);
  std::optional<Settings> parentSettings;
  Settings settings;

  MigrateToProfiles(settings, defaultProfile, activeProfile);

  if (activeProfile != defaultProfile) {
    dprint(
      "Recursing to profile {}'s parent profile {}",
      activeProfile,
      defaultProfile);
    settings = Settings::Load(defaultProfile, defaultProfile);
    parentSettings = settings;
  }

  const auto profileDir = Filesystem::GetSettingsDirectory() / "Profiles"
    / ProfileSettings::Profile::GetDirectoryName(activeProfile);

#define IT(cpptype, x) MaybeSetFromJSON(settings.m##x, profileDir / #x ".json");
  OPENKNEEBOARD_PER_PROFILE_SETTINGS_SECTIONS
#undef IT
  MaybeSetFromJSON(settings.mDeprecatedNonVR, profileDir / "NonVR.json");
#define IT(cpptype, x) MaybeSetFromJSON(settings.m##x, #x ".json");
  OPENKNEEBOARD_GLOBAL_SETTINGS_SECTIONS
#undef IT

  if (
    parentSettings
    && settings.mApp.mDeprecated.mDualKneeboards
      != parentSettings->mApp.mDeprecated.mDualKneeboards
    && (!std::filesystem::exists(profileDir / "Views.json"))) {
    MigrateToViewsSettings(settings);
  }

  // Split up and moved out of profiles in v1.9 (#547)
  const auto perProfileAppSettings = profileDir / "App.json";
  if (std::filesystem::exists(perProfileAppSettings)) {
    if (!std::filesystem::exists(profileDir / "UI.json")) {
      MaybeSetFromJSON(settings.mUI, perProfileAppSettings);
    }
    MaybeSetFromJSON(settings.mApp, perProfileAppSettings);
    std::filesystem::remove(perProfileAppSettings);
  }

  return settings;
}

}// namespace OpenKneeboard
