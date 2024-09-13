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

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IPageSourceWithCursorEvents.hpp>
#include <OpenKneeboard/IPageSourceWithNavigation.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Dispatching.h>

#include <filesystem>
#include <memory>
#include <shared_mutex>

namespace OpenKneeboard {

class KneeboardState;
struct DXResources;
class DoodleRenderer;

class PDFFilePageSource final
  : virtual public IPageSourceWithCursorEvents,
    virtual public IPageSourceWithNavigation,
    public EventReceiver,
    public std::enable_shared_from_this<PDFFilePageSource> {
 private:
  explicit PDFFilePageSource(const audited_ptr<DXResources>&, KneeboardState*);

 public:
  PDFFilePageSource() = delete;
  virtual ~PDFFilePageSource();
  static OpenKneeboard::fire_and_forget final_release(
    std::unique_ptr<PDFFilePageSource>);

  static task<std::shared_ptr<PDFFilePageSource>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::filesystem::path& path = {});

  virtual task<void> Reload();

  virtual PageIndex GetPageCount() const final override;
  virtual std::vector<PageID> GetPageIDs() const final override;
  virtual std::optional<PreferredSize> GetPreferredSize(PageID) final override;

  std::filesystem::path GetPath() const;
  task<void> SetPath(const std::filesystem::path& path);

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

  virtual void PostCursorEvent(KneeboardViewID ctx, const CursorEvent&, PageID)
    override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  task<void> RenderPage(RenderContext, PageID, PixelRect rect) override;

 private:
  winrt::apartment_context mUIThread;
  // Useful because `wil::resume_foreground()` will *always* enqueue, never
  // execute immediately
  DispatcherQueue mUIThreadDispatcherQueue
    = DispatcherQueue::GetForCurrentThread();

  audited_ptr<DXResources> mDXR;
  mutable std::shared_mutex mMutex;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;
  std::unique_ptr<DoodleRenderer> mDoodles;

  struct DocumentResources;
  std::shared_ptr<DocumentResources> mDocumentResources;

  task<void> ReloadRenderer(std::weak_ptr<DocumentResources>);
  task<void> ReloadNavigation(std::weak_ptr<DocumentResources>);

  fire_and_forget OnFileModified(const std::filesystem::path& path);

  void RenderPageContent(
    RenderTarget* rt,
    PageID pageIndex,
    const PixelRect& rect) noexcept;
  void
  RenderOverDoodles(ID2D1DeviceContext*, PageID pageIndex, const D2D1_RECT_F&);

  PageID GetPageIDForIndex(PageIndex index) const;
};

}// namespace OpenKneeboard
