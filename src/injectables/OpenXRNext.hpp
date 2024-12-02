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

// This file is mostly independent of graphics APIs, however we need to
// pull in vulkan here to get the definition of `xrCreateVulkanDeviceKHR()`
//
// ... but, as that means we need to pull in openxr_platform.h now and that
// can only be included once, we need to pull in all the supported APIs
#include <shims/vulkan/vulkan.h>

#include <d3d11.h>
#include <d3d12.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <shims/openxr/openxr.h>

#include <openxr/openxr_platform.h>

#include <utility>

#define OPENKNEEBOARD_HOOKED_OPENXR_FUNCS(CORE_FN, EXT_FN) \
  CORE_FN(xrCreateSession) \
  CORE_FN(xrDestroySession) \
  CORE_FN(xrDestroyInstance) \
  CORE_FN(xrEndFrame) \
  EXT_FN(XR_KHR_vulkan_enable2, xrCreateVulkanDeviceKHR) \
  EXT_FN(XR_KHR_vulkan_enable2, xrCreateVulkanInstanceKHR)

#define OPENKNEEBOARD_NEXT_OPENXR_FUNCS(CORE_FN, EXT_FN) \
  OPENKNEEBOARD_HOOKED_OPENXR_FUNCS(CORE_FN, EXT_FN) \
  CORE_FN(xrAcquireSwapchainImage) \
  CORE_FN(xrCreateReferenceSpace) \
  CORE_FN(xrCreateSwapchain) \
  CORE_FN(xrDestroySpace) \
  CORE_FN(xrDestroySwapchain) \
  CORE_FN(xrEnumerateInstanceExtensionProperties) \
  CORE_FN(xrEnumerateSwapchainFormats) \
  CORE_FN(xrEnumerateSwapchainImages) \
  CORE_FN(xrGetInstanceProperties) \
  CORE_FN(xrGetSystemProperties) \
  CORE_FN(xrLocateSpace) \
  CORE_FN(xrReleaseSwapchainImage) \
  CORE_FN(xrWaitSwapchainImage)

namespace OpenKneeboard {

class OpenXRNext final {
 public:
  OpenXRNext(XrInstance, PFN_xrGetInstanceProcAddr);

#define DECLARE_FN_PTR(func) PFN_##func func {nullptr};
#define DECLARE_EXT_FN_PTR(ext, func) DECLARE_FN_PTR(func)
  DECLARE_FN_PTR(xrGetInstanceProcAddr)
  OPENKNEEBOARD_NEXT_OPENXR_FUNCS(DECLARE_FN_PTR, DECLARE_EXT_FN_PTR)
#undef DECLARE_FN_PTR
#undef DECLARE_EXT_FN_PTR
};

}// namespace OpenKneeboard
