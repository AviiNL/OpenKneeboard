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
#include <OpenKneeboard/CachedLayer.hpp>
#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/SHM.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <DirectXColors.h>

namespace OpenKneeboard {

CachedLayer::CachedLayer(const audited_ptr<DXResources>& dxr) : mDXR(dxr) {
}

CachedLayer::~CachedLayer() {
}

void CachedLayer::Render(
  const PixelRect& destRect,
  Key cacheKey,
  RenderTarget* rt,
  std::function<void(RenderTarget*, const PixelSize&)> impl,
  const std::optional<PixelSize>& providedCacheDimensions) {
  std::scoped_lock lock(mCacheMutex);

  const PixelSize cacheDimensions
    = providedCacheDimensions ? *providedCacheDimensions : destRect.mSize;

  if (cacheDimensions.IsEmpty()) [[unlikely]] {
    OPENKNEEBOARD_BREAK;
    return;
  }

  if (mCacheDimensions != cacheDimensions || !mCache) {
    mKey = ~Key {0};
    mCache = nullptr;
    mCacheDimensions = cacheDimensions;
    D3D11_TEXTURE2D_DESC textureDesc {
      .Width = cacheDimensions.mWidth,
      .Height = cacheDimensions.mHeight,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
    };
    winrt::check_hresult(
      mDXR->mD3D11Device->CreateTexture2D(&textureDesc, nullptr, mCache.put()));
    winrt::check_hresult(mDXR->mD3D11Device->CreateShaderResourceView(
      mCache.get(), nullptr, mCacheSRV.put()));
    mCacheRenderTarget = RenderTarget::Create(mDXR, mCache);
  }

  if (mKey != cacheKey) {
    {
      auto d3d = mCacheRenderTarget->d3d();
      mDXR->mD3D11ImmediateContext->ClearRenderTargetView(
        d3d.rtv(), DirectX::Colors::Transparent);
    }
    impl(mCacheRenderTarget.get(), cacheDimensions);
    mKey = cacheKey;
  }

  auto d3d = rt->d3d();

  const PixelRect sourceRect {
    {0, 0},
    mCacheDimensions,
  };

  auto sb = mDXR->mSpriteBatch.get();

  sb->Begin(d3d.rtv(), rt->GetDimensions());
  sb->Draw(mCacheSRV.get(), sourceRect, destRect);
  sb->End();
}

void CachedLayer::Reset() {
  std::scoped_lock lock(mCacheMutex);

  mKey = ~Key {0};
  mCache = nullptr;
  mCacheRenderTarget = nullptr;
  mCacheSRV = nullptr;
}

}// namespace OpenKneeboard
