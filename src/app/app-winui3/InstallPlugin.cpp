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
#include "InstallPlugin.h"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <wil/resource.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/utf8.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>

#include <processenv.h>
#include <shellapi.h>
#include <zip.h>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace OpenKneeboard {

static IAsyncAction ShowPluginInstallationError(
  XamlRoot& xamlRoot,
  std::filesystem::path path,
  std::string_view error) {
  dprintf("ERROR: Plugin installation error for `{}`: {}", path, error);

  StackPanel layout;
  layout.Margin({8, 8, 8, 8});
  layout.Spacing(12);
  {
    TextBlock p;
    p.Text(std::format(_(L"Couldn't install `{}`:"), path.filename()));
    p.TextWrapping(TextWrapping::WrapWholeWords);
    layout.Children().Append(p);
  }
  {
    TextBlock p;
    p.Text(winrt::to_hstring(error));
    p.TextWrapping(TextWrapping::WrapWholeWords);
    layout.Children().Append(p);
  }

  ContentDialog dialog;
  dialog.XamlRoot(xamlRoot);
  dialog.Title(winrt::box_value(to_hstring(_(L"Install Plugin"))));
  dialog.Content(layout);
  dialog.CloseButtonText(_(L"OK"));
  dialog.DefaultButton(ContentDialogButton::Close);

  co_await dialog.ShowAsync();

  co_return;
}

static IAsyncAction InstallPluginFromJSON(
  XamlRoot& xamlRoot,
  std::filesystem::path path,
  std::string json) {
  co_return;
}

static IAsyncAction InstallPluginFromPath(
  XamlRoot& xamlRoot,
  std::filesystem::path path) {
  dprintf("Attempting to install plugin `{}`", path);
  if (!std::filesystem::exists(path)) {
    dprintf("ERROR: asked to install plugin `{}`, which does not exist", path);
    OPENKNEEBOARD_BREAK;
    co_return;
  }
  if (!std::filesystem::is_regular_file(path)) {
    dprintf(
      "ERROR: asked to install plugin `{}`, which is not a regular file", path);
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  if (IsElevated() || IsShellElevated()) {
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      _("Plugins can not be installed while OpenKneeboard is running as "
        "administrator."));
    co_return;
  }

  using unique_zip_ptr
    = wil::unique_any<zip_t*, decltype(&zip_close), &zip_close>;
  using unique_zip_error = wil::
    unique_struct<zip_error_t, decltype(&zip_error_fini), &zip_error_fini>;

  int zipErrorCode {};
  unique_zip_ptr zip {
    zip_open(path.string().c_str(), ZIP_RDONLY, &zipErrorCode)};
  if (zipErrorCode) {
    unique_zip_error zerror;
    zip_error_init_with_code(&zerror, zipErrorCode);
    const auto errorString = zip_error_strerror(&zerror);
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Failed to open the file: \"{}\" ({}) "),
        zip_error_strerror(&zerror),
        zipErrorCode));
    co_return;
  }

  auto metadataFileIndex = zip_name_locate(zip.get(), "v1.json", 0);
  if (metadataFileIndex == -1) {
    co_await ShowPluginInstallationError(
      xamlRoot, path, _("Plugin does not contain required metadata `v1.json`"));
    co_return;
  }

  zip_stat_t zstat;
  zip_stat_init(&zstat);
  zip_stat_index(zip.get(), metadataFileIndex, 0, &zstat);

  if ((zstat.flags & ZIP_STAT_SIZE) == 0) {
    co_await ShowPluginInstallationError(
      xamlRoot, path, _("Metadata file `v1.json` has an unknown size"));
    co_return;
  }
  if (zstat.size > (1024 * 1024)) {
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Metadata file `v1.json` has an uncompressed size of {} bytes, which "
          "is larger than the maximum of 1MB"),
        zstat.size));
    co_return;
  }

  using unique_zip_file
    = wil::unique_any<zip_file_t*, decltype(&zip_fclose), &zip_fclose>;
  unique_zip_file metadataFile {
    zip_fopen_index(zip.get(), metadataFileIndex, 0)};

  if (!metadataFile) {
    const auto error = zip_get_error(zip.get());
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Failed to open metadata file within plugin: \"{}\" ({})"),
        zip_error_strerror(error),
        zip_error_code_zip(error)));
    co_return;
  }

  std::string buf;
  buf.resize(zstat.size, '\0');
  // Okie, this is nasty, but need C pointers...
  const auto bufBegin = buf.data();
  const auto bufEnd = bufBegin + buf.size();

  for (auto it = buf.data(); it < bufEnd;) {
    const auto bytesRead = zip_fread(metadataFile.get(), it, bufEnd - it);
    if (bytesRead == 0) {
      if (it != bufEnd) {
        co_await ShowPluginInstallationError(
          xamlRoot,
          path,
          std::format(
            _("Read {} bytes from plugin metadata file, expected {} bytes"),
            buf.size(),
            zstat.size));
        co_return;
      }
      break;
    }
    if (bytesRead > 0) {
      it += bytesRead;
      continue;
    }
    // bytesRead < 0, error
    const auto error = zip_get_error(zip.get());
    co_await ShowPluginInstallationError(
      xamlRoot,
      path,
      std::format(
        _("Reading metadata file within plugin failed after {} bytes: \"{}\" "
          "({})"),
        bytesRead,
        zip_error_strerror(error),
        zip_error_code_zip(error)));
    co_return;
  }

  // We can't `co_await` to show a dialog in a catch block, so keep it until
  // later
  std::optional<std::string> error;
  try {
    auto j = nlohmann::json::parse(buf);
    dprint(j.dump(2));
  } catch (const nlohmann::json::exception& e) {
    error
      = std::format("Couldn't parse metadata file: {} ({})", e.what(), e.id);
  }
  if (error) {
    co_await ShowPluginInstallationError(xamlRoot, path, *error);
    co_return;
  }
}

IAsyncAction InstallPlugin(
  XamlRoot xamlRoot,
  const wchar_t* const commandLine) {
  int argc {};
  const wil::unique_hlocal_ptr<PWSTR[]> argv {
    CommandLineToArgvW(commandLine, &argc)};

  for (int i = 0; i < argc; ++i) {
    const std::wstring_view arg {argv[i]};
    if (arg != L"--plugin") {
      continue;
    }
    if (i == argc - 1) {
      dprint("ERROR: `--plugin` passed, but no plugin specified.");
      OPENKNEEBOARD_BREAK;
      co_return;
    }
    co_await InstallPluginFromPath(xamlRoot, argv[i + 1]);
    co_return;
  }

  co_return;
}
}// namespace OpenKneeboard