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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/WGCPageSource.h>
#include <OpenKneeboard/WindowCaptureControl.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/weak_wrap.h>

#include <winrt/Microsoft.Graphics.Display.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>

#include <wil/cppwinrt.h>
#include <wil/cppwinrt_helpers.h>

#include <Windows.Graphics.Capture.Interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>

#include <mutex>

#include <dwmapi.h>
#include <libloaderapi.h>
#include <shellapi.h>
#include <wow64apiset.h>

namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDX = winrt::Windows::Graphics::DirectX;

namespace OpenKneeboard {

winrt::fire_and_forget WGCPageSource::Init() noexcept {
  const auto keepAlive = shared_from_this();

  // Requires Windows 11
  bool supportsBorderRemoval = false;
  try {
    supportsBorderRemoval
      = winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        L"Windows.Graphics.Capture.GraphicsCaptureSession",
        L"IsBorderRequired");
    if (supportsBorderRemoval) {
      co_await WGC::GraphicsCaptureAccess::RequestAccessAsync(
        WGC::GraphicsCaptureAccessKind::Borderless);
    }

  } catch (const winrt::hresult_class_not_registered&) {
    supportsBorderRemoval = false;
  }

  co_await wil::resume_foreground(mDQC.DispatcherQueue());
  {
    co_await this->InitializeInCaptureThread();
    const std::unique_lock d2dlock(*mDXR);

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item {nullptr};
    try {
      item = this->CreateWGCaptureItem();
    } catch (const winrt::hresult_error& e) {
      OPENKNEEBOARD_BREAK;
      co_return;
    }
    if (!item) {
      dprint("Failed to create Windows::Graphics::CaptureItem");
      co_return;
    }

    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
      mDXR->mDXGIDevice.get(),
      reinterpret_cast<IInspectable**>(winrt::put_abi(mWinRTD3DDevice))));

    // WGC does not support direct capture of sRGB
    mFramePool = WGC::Direct3D11CaptureFramePool::Create(
      mWinRTD3DDevice,
      this->GetPixelFormat(),
      WGCPageSource::SwapchainLength,
      item.Size());
    mFramePool.FrameArrived(
      [weak = weak_from_this()](const auto&, const auto&) {
        auto self = weak.lock();
        if (self) {
          self->OnFrame();
        }
      });

    mCaptureSession = mFramePool.CreateCaptureSession(item);
    mCaptureSession.IsCursorCaptureEnabled(mOptions.mCaptureCursor);
    if (supportsBorderRemoval) {
      mCaptureSession.IsBorderRequired(false);
    }
    mCaptureSession.StartCapture();

    mCaptureItem = item;
  }

  co_await mUIThread;
  this->evContentChangedEvent.Emit();
  this->evAvailableFeaturesChangedEvent.Emit();
  this->evNeedsRepaintEvent.Emit();
}

WGCPageSource::WGCPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const Options& options)
  : mDXR(dxr), mOptions(options) {
  if (!WGC::GraphicsCaptureSession::IsSupported()) {
    return;
  }

  mDQC = winrt::Windows::System::DispatcherQueueController::
    CreateOnDedicatedThread();

  AddEventListener(
    kneeboard->evFrameTimerPostEvent, [this]() { this->ReleaseNextFrame(); });
}

winrt::fire_and_forget WGCPageSource::ReleaseNextFrame() {
  if (!(mDQC && mNextFrame)) {
    co_return;
  }

  auto next = std::move(mNextFrame);
  mNextFrame = {nullptr};

  co_await wil::resume_foreground(mDQC.DispatcherQueue());

  // Not using the scoped one as it needs to be disposed in the same thread
  TraceLoggingWrite(gTraceProvider, "WGCPageSource::ReleaseNextFrame()");
  next = {nullptr};
}

// Destruction is handled in final_release instead
WGCPageSource::~WGCPageSource() = default;

winrt::fire_and_forget WGCPageSource::final_release(
  std::unique_ptr<WGCPageSource> p) {
  p->RemoveAllEventListeners();
  if (!p->mDQC) {
    co_await p->mUIThread;
    co_return;
  }

  // Switch to DQ thread to clean up the Windows.Graphics.Capture objects that
  // were created in that thread
  co_await winrt::resume_foreground(p->mDQC.DispatcherQueue());
  if (p->mFramePool) {
    p->mCaptureSession.Close();
    p->mFramePool.Close();
    p->mCaptureSession = {nullptr};
    p->mCaptureItem = {nullptr};
    p->mFramePool = {nullptr};
    p->mNextFrame = {nullptr};
  }

  co_await p->mUIThread;
  co_await p->mDQC.ShutdownQueueAsync();
  p->mDQC = {nullptr};
  const std::unique_lock d2dlock(*(p->mDXR));
  p->mTexture = nullptr;
}

PageIndex WGCPageSource::GetPageCount() const {
  if (mCaptureItem) {
    return 1;
  }
  return 0;
}

std::vector<PageID> WGCPageSource::GetPageIDs() const {
  if (mCaptureItem) {
    return {mPageID};
  }
  return {};
}

PreferredSize WGCPageSource::GetPreferredSize(PageID) {
  if (!mTexture) {
    return {};
  }

  return {
    this->GetContentRect(mCaptureSize).mSize,
    ScalingKind::Bitmap,
  };
}

void WGCPageSource::RenderPage(
  RenderTarget* rt,
  PageID,
  const PixelRect& rect) {
  if (!(mTexture && mCaptureItem)) {
    return;
  }
  auto d3d = rt->d3d();

  auto color = DirectX::Colors::White;

  const auto whiteLevel = this->GetHDRWhiteLevelInNits();
  if (whiteLevel) {
    const auto dimming = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL / (*whiteLevel);
    color = {dimming, dimming, dimming, 1};
  }

  auto sb = mDXR->mSpriteBatch.get();
  sb->Begin(d3d.rtv(), rt->GetDimensions());
  const auto sourceRect = this->GetContentRect(mCaptureSize);
  sb->Draw(mShaderResourceView.get(), sourceRect, rect, color);
  sb->End();

  mNeedsRepaint = false;
}

void WGCPageSource::OnFrame() {
  EventDelay delay;
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "WGCPageSource::OnFrame");
  scope_guard traceOnException([&activity]() {
    if (std::uncaught_exceptions() > 0) {
      TraceLoggingWriteStop(
        activity,
        "WGCPageSource::OnFrame",
        TraceLoggingValue("UncaughtExceptions", "Result"));
    }
  });
  auto frame = mFramePool.TryGetNextFrame();
  if (!frame) {
    TraceLoggingWriteStop(
      activity, __FUNCTION__, TraceLoggingValue("NoFrame", "Result"));
    return;
  }
  TraceLoggingWriteTagged(activity, "HaveFrame");

  auto wgdxSurface = frame.Surface();
  auto interopSurface = wgdxSurface.as<
    ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<IDXGISurface> nativeSurface;
  winrt::check_hresult(
    interopSurface->GetInterface(IID_PPV_ARGS(nativeSurface.put())));
  auto d3dSurface = nativeSurface.as<ID3D11Texture2D>();
  D3D11_TEXTURE2D_DESC surfaceDesc {};
  d3dSurface->GetDesc(&surfaceDesc);
  const auto captureSize = frame.ContentSize();

  const auto swapchainDimensions
    = ((captureSize.Width <= mSwapchainDimensions.mWidth)
       && (captureSize.Height <= mSwapchainDimensions.mHeight))
    ? mSwapchainDimensions
    : this->GetSwapchainDimensions({
      static_cast<uint32_t>(captureSize.Width),
      static_cast<uint32_t>(captureSize.Height),
    });

  TraceLoggingWriteTagged(activity, "WaitingForLock");
  const std::unique_lock d2dlock(*mDXR);
  TraceLoggingWriteTagged(activity, "Locked");

  auto ctx = mDXR->mD3D11ImmediateContext.get();

  if (swapchainDimensions != mSwapchainDimensions) {
    OPENKNEEBOARD_TraceLoggingScope(
      "RecreatePool",
      TraceLoggingValue(swapchainDimensions.mWidth, "Width"),
      TraceLoggingValue(swapchainDimensions.mHeight, "Height"));
    mSwapchainDimensions = swapchainDimensions;
    mFramePool.Recreate(
      mWinRTD3DDevice,
      this->GetPixelFormat(),
      2,
      swapchainDimensions
        .StaticCast<int32_t, winrt::Windows::Graphics::SizeInt32>());
    return;
  }

  if (mTexture) {
    D3D11_TEXTURE2D_DESC desc {};
    mTexture->GetDesc(&desc);
    if (surfaceDesc.Width != desc.Width || surfaceDesc.Height != desc.Height) {
      TraceLoggingWriteTagged(activity, "ResettingTexture");
      mTexture = nullptr;
    }
  }

  if (!mTexture) {
    OPENKNEEBOARD_TraceLoggingScope("CreateTexture");
    auto desc = surfaceDesc;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = 0;

    winrt::check_hresult(
      mDXR->mD3D11Device->CreateTexture2D(&desc, nullptr, mTexture.put()));
    mShaderResourceView = nullptr;
    winrt::check_hresult(mDXR->mD3D11Device->CreateShaderResourceView(
      mTexture.get(), nullptr, mShaderResourceView.put()));
  }

  mCaptureSize = {
    static_cast<uint32_t>(captureSize.Width),
    static_cast<uint32_t>(captureSize.Height),
  };
  TraceLoggingWriteTagged(
    activity,
    "CaptureSize",
    TraceLoggingValue(captureSize.Width, "Width"),
    TraceLoggingValue(captureSize.Height, "Height"));

  const auto contentRect = this->GetContentRect(mCaptureSize);

  const D3D11_BOX box {
    .left = static_cast<UINT>(
      std::min(contentRect.Left(), mSwapchainDimensions.Width())),
    .top = static_cast<UINT>(
      std::min(contentRect.Top(), mSwapchainDimensions.Height())),
    .front = 0,
    .right = static_cast<UINT>(
      std::min(contentRect.Right(), mSwapchainDimensions.Width())),
    .bottom = static_cast<UINT>(
      std::min(contentRect.Bottom(), mSwapchainDimensions.Height())),
    .back = 1,
  };

  TraceLoggingWriteTagged(activity, "CopySubresourceRegion");
  ctx->CopySubresourceRegion(
    mTexture.get(), 0, 0, 0, 0, d3dSurface.get(), 0, &box);
  TraceLoggingWriteTagged(activity, "evNeedsRepaint");
  this->evNeedsRepaintEvent.Emit();
  TraceLoggingWriteStop(
    activity, "WGCPageSource::OnFrame", TraceLoggingValue("Success", "Result"));

  // Keep alive to limit DWM/WGC framerate
  mNextFrame = frame;
  {
    OPENKNEEBOARD_TraceLoggingScope("WGCPageSource::PostFrame");
    this->PostFrame();
  }
}

void WGCPageSource::PostFrame() {
}

winrt::fire_and_forget WGCPageSource::ForceResize(const PixelSize& size) {
  mFramePool.Recreate(
    mWinRTD3DDevice,
    this->GetPixelFormat(),
    WGCPageSource::SwapchainLength,
    size.StaticCast<int32_t, winrt::Windows::Graphics::SizeInt32>());
  co_return;
}

}// namespace OpenKneeboard
