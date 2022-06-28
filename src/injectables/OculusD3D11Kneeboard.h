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

#include <OpenKneeboard/config.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <shims/winrt.h>

#include <array>

#include "IDXGISwapChainPresentHook.h"
#include "OculusKneeboard.h"

namespace OpenKneeboard {

class OculusD3D11Kneeboard final : public OculusKneeboard::Renderer {
 public:
  OculusD3D11Kneeboard();
  virtual ~OculusD3D11Kneeboard();

  void UninstallHook();

  static bool Render(
    ID3D11Device*,
    const std::vector<winrt::com_ptr<ID3D11RenderTargetView>>&,
    ovrSession,
    ovrTextureSwapChain,
    const SHM::Snapshot&,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&);

 protected:
  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    uint8_t layerIndex) override final;

  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&) override final;

 private:
  std::array<std::vector<winrt::com_ptr<ID3D11RenderTargetView>>, MaxLayers>
    mRenderTargetViews;
  winrt::com_ptr<ID3D11Device> mD3D = nullptr;

  OculusKneeboard mOculusKneeboard;
  IDXGISwapChainPresentHook mDXGIHook;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next);
};
}// namespace OpenKneeboard
