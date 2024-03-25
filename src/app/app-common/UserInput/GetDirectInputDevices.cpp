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
#include <OpenKneeboard/GetDirectInputDevices.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/utf8.h>

#include <hidusage.h>

using DeviceInstances = std::vector<DIDEVICEINSTANCE>;

namespace OpenKneeboard {

static bool IsGameController(LPCDIDEVICEINSTANCE inst) {
  switch (inst->wUsagePage) {
    case HID_USAGE_PAGE_GENERIC:
      switch (inst->wUsage) {
        case HID_USAGE_GENERIC_GAMEPAD:
        case HID_USAGE_GENERIC_JOYSTICK:
          return true;
        default:
          return false;
      }
    case HID_USAGE_PAGE_SIMULATION:
      switch (inst->wUsage) {
        case HID_USAGE_SIMULATION_FLIGHT_SIMULATION_DEVICE:
        case HID_USAGE_SIMULATION_FLIGHT_CONTROL_STICK:
        case HID_USAGE_SIMULATION_FLIGHT_COMMUNICATIONS:
        case HID_USAGE_SIMULATION_COLLECTIVE_CONTROL:
        case HID_USAGE_SIMULATION_CYCLIC_CONTROL:
        case HID_USAGE_SIMULATION_FLIGHT_YOKE:
        case HID_USAGE_SIMULATION_AIRPLANE_SIMULATION_DEVICE:
        case HID_USAGE_SIMULATION_HELICOPTER_SIMULATION_DEVICE:
          return true;
        default:
          return false;
      }
  }
  return false;
}

namespace {
struct EnumDeviceContext {
  DeviceInstances mDeviceInstances {};
  bool mIncludeMice {false};
};

BOOL CALLBACK EnumDeviceCallback(LPCDIDEVICEINSTANCE inst, LPVOID untypedCtx) {
  auto ctx = reinterpret_cast<EnumDeviceContext*>(untypedCtx);
  auto devType = inst->dwDevType & 0xff;
  // vjoystick devices self-report as 6DOF 1st-person controllers
  if (
    devType == DI8DEVTYPE_KEYBOARD || IsGameController(inst)
    || (devType == DI8DEVTYPE_MOUSE && ctx->mIncludeMice)) {
    ctx->mDeviceInstances.push_back(*inst);
  } else {
    TraceLoggingWrite(
      gTraceProvider,
      "SkipDIDevice",
      TraceLoggingValue(to_utf8(inst->tszInstanceName).c_str(), "Name"),
      TraceLoggingValue(inst->dwDevType, "DeviceType"),
      TraceLoggingValue(inst->wUsagePage, "UsagePage"),
      TraceLoggingValue(inst->wUsage, "Usage"));
  }
  return DIENUM_CONTINUE;
}
}// namespace

DeviceInstances GetDirectInputDevices(IDirectInput8W* di, bool includeMice) {
  EnumDeviceContext ctx {
    .mIncludeMice = includeMice,
  };
  di->EnumDevices(
    DI8DEVCLASS_ALL, &EnumDeviceCallback, &ctx, DIEDFL_ATTACHEDONLY);
  return ctx.mDeviceInstances;
}

}// namespace OpenKneeboard
