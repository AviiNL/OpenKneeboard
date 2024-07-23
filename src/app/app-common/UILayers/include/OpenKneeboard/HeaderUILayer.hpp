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

#include <OpenKneeboard/CursorClickableRegions.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/FlyoutMenuUILayer.hpp>
#include <OpenKneeboard/ISelectableToolbarItem.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/UILayerBase.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <memory>

namespace OpenKneeboard {

class FlyoutMenuUILayer;
class KneeboardState;
class KneeboardView;

class HeaderUILayer final : public UILayerBase,
                            private EventReceiver,
                            public std::enable_shared_from_this<HeaderUILayer> {
 public:
  static std::shared_ptr<HeaderUILayer>
  Create(const audited_ptr<DXResources>& dxr, KneeboardState*, KneeboardView*);
  virtual ~HeaderUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  virtual void Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

  HeaderUILayer() = delete;

 private:
  HeaderUILayer(
    const audited_ptr<DXResources>& dxr,
    KneeboardState*,
    KneeboardView*);

  void DrawHeaderText(
    const std::shared_ptr<TabView>&,
    ID2D1DeviceContext*,
    const PixelRect& rect) const;
  void DrawToolbar(
    const Context&,
    ID2D1DeviceContext*,
    const PixelRect& fullRect,
    const PixelRect& headerRect,
    PixelRect* headerTextRect);
  void LayoutToolbar(
    const Context&,
    const PixelRect& fullRect,
    const PixelRect& headerRect,
    PixelRect* headerTextRect);

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboardState {nullptr};
  winrt::com_ptr<ID2D1Brush> mHeaderBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderTextBrush;
  winrt::com_ptr<ID2D1Brush> mButtonBrush;
  winrt::com_ptr<ID2D1Brush> mDisabledButtonBrush;
  winrt::com_ptr<ID2D1Brush> mHoverButtonBrush;
  winrt::com_ptr<ID2D1Brush> mActiveButtonBrush;
  winrt::com_ptr<ID2D1Brush> mActiveViewSeparatorBrush;

  struct Button {
    D2D1_RECT_F mRect {};
    std::shared_ptr<ISelectableToolbarItem> mAction;

    bool operator==(const Button&) const noexcept;
  };

  struct Toolbar {
    std::weak_ptr<TabView> mTabView {};
    D2D1_RECT_F mRect {};
    D2D1_RECT_F mTextRect {};
    std::shared_ptr<CursorClickableRegions<Button>> mButtons;
  };
  std::optional<PixelSize> mLastRenderSize;
  std::shared_ptr<Toolbar> mToolbar;
  std::shared_ptr<FlyoutMenuUILayer> mSecondaryMenu;
  std::vector<EventHandlerToken> mTabEvents;

  void OnClick(const Button&);
  void OnTabChanged();

  bool mRecursiveCall = false;
};

}// namespace OpenKneeboard
