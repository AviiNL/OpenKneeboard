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
#include <OpenKneeboard/FalconBMS.h>
#include <OpenKneeboard/Filesystem.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/utf8.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <format>
#include <fstream>
#include <unordered_map>

#include <ShlObj.h>

namespace OpenKneeboard {
std::filesystem::path FalconBMS::GetInstalledPath() {
  const auto subkey = "SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37";
  const auto wSubKey = winrt::to_hstring(subkey);

  wchar_t buffer[MAX_PATH];
  DWORD length = sizeof(buffer) * sizeof(buffer[0]);

  if (
    RegGetValueW(
      HKEY_LOCAL_MACHINE,
      wSubKey.c_str(),
      L"baseDir",
      RRF_RT_REG_SZ,
      nullptr,
      reinterpret_cast<void*>(buffer),
      &length)
    != ERROR_SUCCESS) {
    return {};
  }

  const auto path = std::filesystem::path(
    std::wstring_view(buffer, length / sizeof(buffer[0])));
  if (!std::filesystem::is_directory(path)) {
    return {};
  }

  return std::filesystem::canonical(path);
}

static std::filesystem::path GetBMSUserPath() {
}
}// namespace OpenKneeboard