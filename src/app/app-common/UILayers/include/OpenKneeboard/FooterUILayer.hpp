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

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/UILayerBase.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <chrono>
#include <memory>

namespace OpenKneeboard {

class KneeboardState;
struct APIEvent;
struct GameInstance;

class FooterUILayer final : public UILayerBase, private EventReceiver {
 public:
  FooterUILayer(const audited_ptr<DXResources>& dxr, KneeboardState*);
  virtual ~FooterUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  [[nodiscard]] task<void> Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

 private:
  void Tick();
  void OnAPIEvent(const APIEvent&);
  void OnGameChanged(DWORD processID, const std::shared_ptr<GameInstance>&);

  audited_ptr<DXResources> mDXResources;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;
  winrt::com_ptr<ID2D1Brush> mForegroundBrush;
  std::optional<D2D1_SIZE_F> mLastRenderSize;

  DWORD mCurrentGamePID {};
  std::optional<std::chrono::seconds> mMissionTime;
  std::optional<std::chrono::seconds> mUTCOffset;

  KneeboardState* mKneeboard {nullptr};

  // Used for frame counter only
  SHM::Reader mSHM;

  using Clock = std::chrono::system_clock;
  using Duration = std::chrono::seconds;
  using TimePoint = std::chrono::time_point<Clock, Duration>;

  enum class RenderState {
    UpToDate,
    Stale,
  };
  TimePoint mLastRenderAt {};
  RenderState mRenderState {RenderState::Stale};
};

}// namespace OpenKneeboard
