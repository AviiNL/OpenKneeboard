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

#include <OpenKneeboard/CachedLayer.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <d2d1.h>
#include <shims/winrt/base.h>

#include <memory>
#include <mutex>

namespace OpenKneeboard {

class DoodleRenderer;
class KneeboardState;

class TabWithDoodles : public virtual IPageSourceWithCursorEvents,
                       protected EventReceiver {
 public:
  TabWithDoodles(const DXResources&, KneeboardState*);
  virtual ~TabWithDoodles();

  virtual void RenderPage(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override final;
  virtual void PostCursorEvent(
    EventContext,
    const CursorEvent&,
    uint16_t pageIndex) override;

 protected:
  virtual void
  RenderPageContent(ID2D1DeviceContext*, uint16_t pageIndex, const D2D1_RECT_F&)
    = 0;
  virtual void RenderOverDoodles(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F&);

  void ClearContentCache();
  void ClearDoodles();

  KneeboardState* GetKneeboardState();

 private:
  DXResources mDXR;
  KneeboardState* mKneeboard;
  std::unique_ptr<DoodleRenderer> mDoodleRenderer;

  CachedLayer mContentLayer;
};
}// namespace OpenKneeboard
