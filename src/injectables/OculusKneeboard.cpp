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
#include "OculusKneeboard.h"

#include <OpenKneeboard/dprint.h>

// clang-format off
#include <windows.h>
#include <detours.h>
// clang-format on

#include "OVRProxy.h"

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OculusKneeboard::OculusKneeboard() {
  mRenderCacheKeys.fill(~(0ui64));
}

void OculusKneeboard::InstallHook(Renderer* renderer) {
  dprintf("{} {:#018x}", __FUNCTION__, (uint64_t)this);
  mRenderer = renderer;
  mEndFrameHook.InstallHook({
    .onEndFrame = std::bind_front(&OculusKneeboard::OnOVREndFrame, this),
  });
}

OculusKneeboard::~OculusKneeboard() {
  this->UninstallHook();
  if (mRenderer) {
    delete mRenderer;
  }
}

void OculusKneeboard::UninstallHook() {
  mEndFrameHook.UninstallHook();
}

OculusKneeboard::Pose OculusKneeboard::GetHMDPose(double predictedTime) {
  auto ovr = OVRProxy::Get();
  const auto state = ovr->ovr_GetTrackingState(mSession, predictedTime, false);
  const auto& p = state.HeadPose.ThePose.Position;
  const auto& o = state.HeadPose.ThePose.Orientation;

  return {
    .mPosition = {p.x, p.y, p.z},
    .mOrientation = {o.x, o.y, o.z, o.w},
  };
}

ovrResult OculusKneeboard::OnOVREndFrame(
  ovrSession session,
  long long frameIndex,
  const ovrViewScaleDesc* viewScaleDesc,
  ovrLayerHeader const* const* layerPtrList,
  unsigned int layerCount,
  const decltype(&ovr_EndFrame)& next) {
  mSession = session;

  static bool sFirst = true;
  if (sFirst) {
    dprint(__FUNCTION__);
    sFirst = false;
  }
  auto passthrough = std::bind_front(
    next, session, frameIndex, viewScaleDesc, layerPtrList, layerCount);

  if (!mRenderer) {
    return passthrough();
  }

  auto shm = mRenderer->GetSHM();

  if (!(shm && *shm)) {
    return passthrough();
  }

  const auto d3d11 = mRenderer->GetD3D11Device();
  if (!d3d11) {
    return passthrough();
  }

  const auto snapshot
    = shm->MaybeGet(d3d11.get(), mRenderer->GetConsumerKind());
  if (!snapshot.IsValid()) {
    return passthrough();
  }
  const auto config = snapshot.GetConfig();

  // Copy existing layers to a mutable struct
  //
  // - drop empty layers so we have more space
  // - discard depth information if desired
  std::vector<const ovrLayerHeader*> newLayers;
  newLayers.reserve(layerCount + snapshot.GetLayerCount());
  std::vector<ovrLayerEyeFov> withoutDepthInformation;

  for (unsigned int i = 0; i < layerCount; ++i) {
    const auto layer = layerPtrList[i];

    if (!layer) {
      continue;
    }

    if (
      config.mVR.mQuirks.mOculusSDK_DiscardDepthInformation
      && layer->Type == ovrLayerType_EyeFovDepth) {
      withoutDepthInformation.push_back({});
      auto& newLayer = withoutDepthInformation.back();
      memcpy(&newLayer, layer, sizeof(ovrLayerEyeFov));
      newLayer.Header.Type = ovrLayerType_EyeFov;
      newLayers.push_back(&newLayer.Header);
      continue;
    }

    newLayers.push_back(layer);
  }

  auto ovr = OVRProxy::Get();
  const auto predictedTime
    = ovr->ovr_GetPredictedDisplayTime(session, frameIndex);

  if (!mSwapchain) [[unlikely]] {
    mSwapchain = mRenderer->CreateSwapChain(
      session, {TextureWidth * MaxLayers, TextureHeight});
    if (!mSwapchain) {
      dprint("Failed to make an OVR swapchain");
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }
  }

  const auto kneeboardLayerCount = snapshot.GetLayerCount();
  std::vector<ovrLayerQuad> kneeboardLayers;
  kneeboardLayers.reserve(kneeboardLayerCount);
  std::vector<Renderer::LayerRenderInfo> layerRenderInfo;
  layerRenderInfo.reserve(kneeboardLayerCount);

  uint16_t topMost = kneeboardLayerCount - 1;
  bool needRender = false;
  for (uint8_t layerIndex = 0; layerIndex < kneeboardLayerCount; ++layerIndex) {
    const auto& layer = *snapshot.GetLayerConfig(layerIndex);

    Renderer::LayerRenderInfo renderInfo {
      .mLayerIndex = layerIndex,
      .mDestRect = {
        {layerIndex * TextureWidth, 0},
        {layer.mImageWidth, layer.mImageHeight},
      },
      .mVR = this->GetRenderParameters(
        snapshot, layer, this->GetHMDPose(predictedTime)),
    };

    if (renderInfo.mVR.mIsLookingAtKneeboard) {
      topMost = layerIndex;
    }

    auto cacheKey = mRenderCacheKeys.at(layerIndex);
    if (mRenderCacheKeys.at(layerIndex) != renderInfo.mVR.mCacheKey) {
      needRender = true;
    }

    kneeboardLayers.push_back({
      .Header = { 
        .Type = ovrLayerType_Quad,
        .Flags = {ovrLayerFlag_HighQuality},
      },
      .ColorTexture = mSwapchain,
      .Viewport = {
        .Pos = {
          static_cast<int>(renderInfo.mDestRect.mOrigin.mX),
          static_cast<int>(renderInfo.mDestRect.mOrigin.mY),
        },
        .Size = {
          static_cast<int>(renderInfo.mDestRect.mSize.mWidth),
          static_cast<int>(renderInfo.mDestRect.mSize.mHeight),
        },
      },
      .QuadPoseCenter = GetOvrPosef(renderInfo.mVR.mKneeboardPose),
      .QuadSize = {renderInfo.mVR.mKneeboardSize.x, renderInfo.mVR.mKneeboardSize.y},
    });
    newLayers.push_back(&kneeboardLayers.back().Header);
    layerRenderInfo.push_back(std::move(renderInfo));
  }

  if (needRender) {
    int swapchainTextureIndex = -1;
    ovr->ovr_GetTextureSwapChainCurrentIndex(
      session, mSwapchain, &swapchainTextureIndex);
    if (swapchainTextureIndex < 0) {
      dprintf(" - invalid swap chain index ({})", swapchainTextureIndex);
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }

    mRenderer->RenderLayers(
      mSwapchain,
      static_cast<uint32_t>(swapchainTextureIndex),
      snapshot,
      layerRenderInfo.size(),
      layerRenderInfo.data());

    auto error = ovr->ovr_CommitTextureSwapChain(session, mSwapchain);
    if (error) {
      dprintf("Commit failed with {}", error);
      OPENKNEEBOARD_BREAK;
      return false;
    }

    for (const auto& it: layerRenderInfo) {
      mRenderCacheKeys[it.mLayerIndex] = it.mVR.mCacheKey;
    }
  }

  if (topMost != kneeboardLayerCount - 1) {
    std::swap(kneeboardLayers.back(), kneeboardLayers.at(topMost));
  }

  return next(
    session,
    frameIndex,
    viewScaleDesc,
    newLayers.data(),
    static_cast<unsigned int>(newLayers.size()));
}

ovrPosef OculusKneeboard::GetOvrPosef(const Pose& pose) {
  const auto& p = pose.mPosition;
  const auto& o = pose.mOrientation;
  return {
    .Orientation = {o.x, o.y, o.z, o.w},
    .Position = {p.x, p.y, p.z},
  };
}

}// namespace OpenKneeboard
