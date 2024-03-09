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
#include <OpenKneeboard/FilteredLastModifiedFilePageSource.h>
#include <OpenKneeboard/PlainTextPageSource.h>

#include <OpenKneeboard/dprint.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace OpenKneeboard {

template <typename TP>
time_t to_time_t(TP tp) {
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
    tp - TP::clock::now() + std::chrono::system_clock::now());
  return std::chrono::system_clock::to_time_t(sctp);
}

FilteredLastModifiedFilePageSource::FilteredLastModifiedFilePageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::string filter)
  : PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mFilter(filter),
    mDelegate(
      std::make_unique<PlainTextPageSource>(dxr, kbs, _("[no radio log]"))) {
  this->SetDelegates({
    std::static_pointer_cast<IPageSource>(mDelegate),
  });
}

std::shared_ptr<FilteredLastModifiedFilePageSource>
FilteredLastModifiedFilePageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::string filter,
  const std::filesystem::path& path) {
  std::shared_ptr<FilteredLastModifiedFilePageSource> ret {
    new FilteredLastModifiedFilePageSource(dxr, kbs, filter)};
  if (!path.empty()) {
    ret->SetPath(path);
  }
  return ret;
}

FilteredLastModifiedFilePageSource::~FilteredLastModifiedFilePageSource() {
  this->RemoveAllEventListeners();
}

winrt::fire_and_forget FilteredLastModifiedFilePageSource::Reload() noexcept {
  const auto weakThis = this->weak_from_this();
  co_await mUIThread;
  const auto stayingAlive = this->shared_from_this();
  if (!stayingAlive) {
    co_return;
  }

  if (mPath.empty() || !std::filesystem::is_directory(mPath)) {
    EventDelay eventDelay;
    this->SetDelegates({});
    evContentChangedEvent.Emit();
    co_return;
  }

  this->SubscribeToChanges();
  this->OnFileModified(mPath);
}

void FilteredLastModifiedFilePageSource::SubscribeToChanges() {
  mWatcher = FilesystemWatcher::Create(mPath);
  AddEventListener(
    mWatcher->evFilesystemModifiedEvent, [weak = weak_from_this()](auto path) {
      if (auto self = weak.lock()) {
        self->OnFileModified(path);
      }
    });
}

void FilteredLastModifiedFilePageSource::OnFileModified(
  const std::filesystem::path& directory) {
  if (directory != mPath) {
    return;
  }
  if (!std::filesystem::is_directory(directory)) {
    return;
  }

  // decltype(mContents) newContents;
  // bool modifiedOrNew = false;

  // std::map<time_t, std::filesystem::path> sort_by_time;

  for (const auto entry: std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    if (!entry.path().filename().string().starts_with(mFilter)) {
      // mDelegate->PushMessage("Skipping " + entry.path().filename().string());
      continue;
    }

    if (!entry.path().filename().string().ends_with(".txt")) {
      // mDelegate->PushMessage("Skipping " + entry.path().filename().string());
      continue;
    }

    // mDelegate->PushMessage(
    //   "Not skipping [" + mFilter + "]: " + entry.path().filename().string());

    if (mBytes.find(entry.path()) == mBytes.end()) {
      mBytes[entry.path()] = std::filesystem::file_size(entry.path());
      continue;
    }

    // mDelegate->PushMessage("Displaying " + entry.path().string());

    std::string newContent = GetFileContent(entry.path(), mBytes[entry.path()]);
    if (!newContent.empty()) {
      mDelegate->PushMessage(newContent);
      // evContentChangedEvent.Emit();
    }

    mBytes[entry.path()] = std::filesystem::file_size(entry.path());

    // it does exist

    // mDelegate->PushMessage(value);

    // load all bytes of entry.path()
    // check if size exists in mBytes[entry.path()]
    // if not store size in mBytes[entry.path()] and \
    //   if it's a new file: print the file to the kneeboard
    //   else ignore, it's old data
    // if so, remove the first mButes[entry.path()] bytes from the loaded file
    // print everything else to the kneeboard, it's new data

    // const auto mtime = to_time_t(entry.last_write_time());
    // const auto path = entry.path();

    // sort_by_time[mtime] = path;
  }

  // if (sort_by_time.empty()) {
  //   return;
  // }

  // auto& entry = sort_by_time.rbegin()->second;
  // mDelegate = PlainTextPageSource::Create(mDXR, mKneeboard, entry);

  // delegate->evPageAppendedEvent.Emit(
  //   SuggestedPageAppendAction::SwitchToNewPage);

  // if (newContents.size() == mContents.size() && !modifiedOrNew) {
  //   dprintf(L"No actual change to {}", mPath.wstring());
  //   return;
  // }
  // dprintf(L"Real change to {}", mPath.wstring());

  // std::vector<std::shared_ptr<IPageSource>> delegates;
  // for (const auto& [path, info]: newContents) {
  // delegates.push_back(delegate);
  // }

  // EventDelay eventDelay;
  // mContents = newContents;
  // this->SetDelegates(delegate);
  // this->evPageAppendedEvent.Emit(SuggestedPageAppendAction::SwitchToNewPage);
}

std::string FilteredLastModifiedFilePageSource::GetFileContent(
  const std::filesystem::path& mPath,
  size_t foffset) const {
  auto bytes = std::filesystem::file_size(mPath) - foffset;
  if (bytes == 0) {
    return {};
  }
  std::string buffer;
  buffer.resize(bytes);
  size_t offset = 0;

  std::ifstream f(mPath, std::ios::in | std::ios::binary);
  f.seekg(foffset);
  if (!f.is_open()) {
    dprintf(L"Failed to open {}", mPath.wstring());
    // mDelegate->ClearText();
    return {};
  }
  while (bytes > 0) {
    f.read(&buffer.data()[offset], bytes);
    bytes -= f.gcount();
    offset += f.gcount();
  }

  size_t pos = 0;
  while ((pos = buffer.find("\r\n", pos)) != std::string::npos) {
    buffer.replace(pos, 2, "\n");
    pos++;
  }

  return buffer;
}

std::filesystem::path FilteredLastModifiedFilePageSource::GetPath() const {
  return mPath;
}

void FilteredLastModifiedFilePageSource::SetPath(
  const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPath = path;
  mWatcher = {};
  this->Reload();
}

std::string FilteredLastModifiedFilePageSource::GetFilter() const {
  return mFilter;
}

void FilteredLastModifiedFilePageSource::SetFilter(const std::string& filter) {
  if (filter == mFilter) {
    return;
  }
  mFilter = filter;
  mWatcher = {};
  this->Reload();
}

}// namespace OpenKneeboard
