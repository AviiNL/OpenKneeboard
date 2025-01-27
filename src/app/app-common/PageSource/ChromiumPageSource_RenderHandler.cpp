/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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

#include "ChromiumPageSource_RenderHandler.hpp"

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/hresult.hpp>

namespace OpenKneeboard {

ChromiumPageSource::RenderHandler::RenderHandler(
  std::shared_ptr<ChromiumPageSource> pageSource)
  : mPageSource(pageSource) {
  mSize = pageSource->mSettings.mInitialSize;
  check_hresult(pageSource->mDXResources->mD3D11Device->CreateFence(
    0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.put())));
}

ChromiumPageSource::RenderHandler::~RenderHandler() {
}

void ChromiumPageSource::RenderHandler::GetViewRect(
  CefRefPtr<CefBrowser>,
  CefRect& rect) {
  rect = {0, 0, mSize.Width<int>(), mSize.Height<int>()};
}

void ChromiumPageSource::RenderHandler::OnPaint(
  CefRefPtr<CefBrowser> browser,
  PaintElementType elementType,
  const RectList& dirtyRects,
  const void* buffer,
  int width,
  int height) {
  static std::once_flag sWarnOnce;
  std::call_once(sWarnOnce, []() {
    dprint.Warning(
      "In ChromiumRenderHandler::OnPaint() - should always be using "
      "OnAcceleratedPaint() instead, unless we're in a VM for testing");
  });

  const auto pageSource = mPageSource.lock();
  if (!pageSource) {
    return;
  }
  auto dxr = pageSource->mDXResources.get();

  // This path is inefficient as it should never be hit in real usage;
  // it's only here so we can test in clean VMs
  D3D11_TEXTURE2D_DESC desc {
    .Width = static_cast<UINT>(width),
    .Height = static_cast<UINT>(height),
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE
      | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX,
  };
  D3D11_SUBRESOURCE_DATA data {
    .pSysMem = buffer,
    .SysMemPitch = static_cast<UINT>(4 * width),
  };
  wil::com_ptr<ID3D11Texture2D> texture;
  check_hresult(pageSource->mDXResources->mD3D11Device->CreateTexture2D(
    &desc, &data, texture.put()));

  auto resource = texture.query<IDXGIResource1>();
  wil::unique_handle handle;
  check_hresult(resource->CreateSharedHandle(
    nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, handle.put()));

  CefAcceleratedPaintInfo info;
  info.format = CEF_COLOR_TYPE_BGRA_8888;
  info.shared_texture_handle = handle.get();
  info.extra.source_size = {width, height};
  info.extra.visible_rect = {0, 0, width, height};
  this->OnAcceleratedPaint(browser, elementType, dirtyRects, info);
}

void ChromiumPageSource::RenderHandler::OnAcceleratedPaint(
  CefRefPtr<CefBrowser>,
  PaintElementType,
  const RectList& dirtyRects,
  const CefAcceleratedPaintInfo& info) {
  auto pageSource = mPageSource.lock();
  if (!pageSource) {
    return;
  }
  auto dxr = pageSource->mDXResources.get();
  const auto frameCount = mFrameCount + 1;
  const auto frameIndex = frameCount % mFrames.size();
  auto& frame = mFrames.at(frameIndex);

  const PixelSize sourceSize {
    static_cast<uint32_t>(info.extra.visible_rect.width),
    static_cast<uint32_t>(info.extra.visible_rect.height),
  };

  // CEF explicitly bans us from caching the texture for this HANDLE; we need
  // to re-open it every frame
  wil::com_ptr<ID3D11Texture2D> sourceTexture;
  dxr->mD3D11Device->OpenSharedResource1(
    info.shared_texture_handle, IID_PPV_ARGS(sourceTexture.put()));

  auto mutex = sourceTexture.try_query<IDXGIKeyedMutex>();
  if (mutex) {
    mutex->AcquireSync(0, 500);
  }
  const scope_exit releaseMutex([&mutex] {
    if (mutex) {
      mutex->ReleaseSync(0);
    }
  });

  if ((!frame.mTexture) || frame.mSize != sourceSize) {
    frame = {};
    OPENKNEEBOARD_ALWAYS_ASSERT(info.format == CEF_COLOR_TYPE_BGRA_8888);
    wil::com_ptr<ID3D11Texture2D> texture;
    D3D11_TEXTURE2D_DESC desc {
      .Width = sourceSize.mWidth,
      .Height = sourceSize.mHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    check_hresult(
      dxr->mD3D11Device->CreateTexture2D(&desc, nullptr, texture.put()));
    wil::com_ptr<ID3D11ShaderResourceView> srv;
    check_hresult(dxr->mD3D11Device->CreateShaderResourceView(
      texture.get(), nullptr, srv.put()));
    frame = {
      .mSize = sourceSize,
      .mTexture = std::move(texture),
      .mShaderResourceView = std::move(srv),
    };
  }

  std::unique_lock lock(*dxr);

  auto ctx = dxr->mD3D11ImmediateContext.get();
  ctx->CopySubresourceRegion(
    frame.mTexture.get(), 0, 0, 0, 0, sourceTexture.get(), 0, nullptr);
  check_hresult(ctx->Signal(mFence.get(), frameCount));
  mFrameCount = frameCount;
  pageSource->evNeedsRepaintEvent.Emit();
}

void ChromiumPageSource::RenderHandler::SetSize(const PixelSize& size) {
  mSize = size;
}

PixelSize ChromiumPageSource::RenderHandler::GetSize() const {
  return mSize;
}

void ChromiumPageSource::RenderHandler::RenderPage(
  RenderContext rc,
  const PixelRect& rect) {
  if (mFrameCount == 0) {
    return;
  }
  const auto pageSource = mPageSource.lock();
  if (!pageSource) {
    return;
  }

  const auto& frame = mFrames.at(mFrameCount % mFrames.size());
  auto& spriteBatch = pageSource->mSpriteBatch;

  auto d3d = rc.d3d();
  auto ctx = pageSource->mDXResources->mD3D11ImmediateContext.get();

  check_hresult(ctx->Wait(mFence.get(), mFrameCount));

  spriteBatch.Begin(d3d.rtv(), rc.GetRenderTarget()->GetDimensions());
  spriteBatch.Draw(frame.mShaderResourceView.get(), {0, 0, frame.mSize}, rect);
  spriteBatch.End();
}

}// namespace OpenKneeboard