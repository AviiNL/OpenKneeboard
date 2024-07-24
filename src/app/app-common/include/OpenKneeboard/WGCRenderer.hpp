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
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/PreferredSize.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.System.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/handles.hpp>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;

namespace D3D11 {
class SpriteBatch;
}

// A page source using Windows::Graphics::Capture
class WGCRenderer : public virtual EventReceiver,
                    public IHasDisposeAsync,
                    public std::enable_shared_from_this<WGCRenderer> {
 public:
  virtual ~WGCRenderer();
  virtual IAsyncAction DisposeAsync() noexcept override;

  PreferredSize GetPreferredSize() const;

  bool HaveCaptureItem() const;
  void Render(RenderTarget*, const PixelRect& rect);

  struct Options {
    bool mCaptureCursor {false};

    constexpr bool operator==(const Options&) const noexcept = default;
  };

  Event<> evNeedsRepaintEvent;
  Event<> evContentChangedEvent;

  winrt::fire_and_forget ForceResize(PixelSize);

 protected:
  WGCRenderer(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Options& options);
  winrt::fire_and_forget Init() noexcept;

  virtual winrt::Windows::Foundation::IAsyncAction InitializeContentToCapture()
    = 0;
  virtual std::optional<float> GetHDRWhiteLevelInNits() const = 0;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const
    = 0;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem()
    = 0;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) const = 0;
  virtual PixelSize GetSwapchainDimensions(const PixelSize& captureSize) const
    = 0;
  virtual void PostFrame();

 private:
  ProcessShutdownBlock mBlockShutdownUntilDestroyed;
  static constexpr int32_t SwapchainLength = 3;
  WGCRenderer() = delete;

  void PreOKBFrame();
  void OnWGCFrame();

  DisposalState mDisposal;
  ThreadGuard mThreadGuard;
  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  Options mOptions;

  PageID mPageID {};

  PixelSize mSwapchainDimensions;

  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
    mWinRTD3DDevice {nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool mFramePool {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession mCaptureSession {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem mCaptureItem {nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame mNextFrame {
    nullptr};

  PixelSize mCaptureSize {};
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;
  bool mNeedsRepaint {true};
};

}// namespace OpenKneeboard
