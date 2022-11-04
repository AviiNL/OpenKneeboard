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
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/config.h>
#include <Windows.h>

#include <format>

namespace OpenKneeboard::Filesystem {

static std::filesystem::path GetTemporaryDirectoryRoot() {
  wchar_t tempDirBuf[MAX_PATH];
  auto tempDirLen = GetTempPathW(MAX_PATH, tempDirBuf);
  return std::filesystem::path {std::wstring_view {tempDirBuf, tempDirLen}}
  / ProjectNameW;
}

std::filesystem::path GetTemporaryDirectory() {
  static std::filesystem::path sCache;
  if (!sCache.empty()) {
    return sCache;
  }

  wchar_t tempDirBuf[MAX_PATH];
  auto tempDirLen = GetTempPathW(MAX_PATH, tempDirBuf);
  auto tempDir = GetTemporaryDirectoryRoot()
    / std::format("{:%F %H-%M-%S} {}",
                  std::chrono::system_clock::now(),
                  GetCurrentProcessId());

  if (!std::filesystem::exists(tempDir)) {
    std::filesystem::create_directory(tempDir);
  }

  sCache = std::filesystem::canonical(tempDir);

  return sCache;
}

void CleanupTemporaryDirectories() {
  const auto root = GetTemporaryDirectoryRoot();
  if (!std::filesystem::exists(root)) {
    return;
  }

  std::error_code ignored;
  for (const auto& it: std::filesystem::directory_iterator(root)) {
    std::filesystem::remove_all(it.path(), ignored);
  }
}

std::filesystem::path GetRuntimeDirectory() {
  static std::filesystem::path sCache;
  if (!sCache.empty()) {
    return sCache;
  }

  wchar_t exePathStr[MAX_PATH];
  const auto exePathStrLen = GetModuleFileNameW(NULL, exePathStr, MAX_PATH);

  const std::filesystem::path exePath
    = std::filesystem::canonical(std::wstring_view {exePathStr, exePathStrLen});

  sCache = exePath.parent_path();
  return sCache;
}

ScopedDeleter::ScopedDeleter(const std::filesystem::path& path) : mPath(path) {
}

ScopedDeleter::~ScopedDeleter() noexcept {
  if (std::filesystem::exists(mPath)) {
    std::filesystem::remove_all(mPath);
  }
}

};// namespace OpenKneeboard::Filesystem
