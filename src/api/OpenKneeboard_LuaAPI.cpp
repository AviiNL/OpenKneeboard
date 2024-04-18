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
#include <OpenKneeboard/GameEvent.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <cinttypes>
#include <cstdlib>
#include <format>
#include <string>

extern "C" {
#include <lauxlib.h>
}

using OpenKneeboard::dprint;
using OpenKneeboard::dprintf;

static void push_arg_error(lua_State* state) {
  lua_pushliteral(state, "2 string arguments are required\n");
  lua_error(state);
}

static int SendToOpenKneeboard(lua_State* state) {
  int argc = lua_gettop(state);
  if (argc != 2) {
    dprint("Invalid argument count\n");
    push_arg_error(state);
    return 1;
  }

  if (!(lua_isstring(state, 1) && lua_isstring(state, 2))) {
    dprint("Non-string args\n");
    push_arg_error(state);
    return 1;
  }

  const OpenKneeboard::GameEvent ge {
    lua_tostring(state, 1),
    lua_tostring(state, 2),
  };
  ge.Send();

  return 0;
}

extern "C" int __declspec(dllexport)
#if UINTPTR_MAX == UINT64_MAX
  luaopen_OpenKneeboard_LuaAPI64(lua_State* state) {
#elif UINTPTR_MAX == UINT32_MAX
  luaopen_OpenKneeboard_LuaAPI32(lua_State* state) {
#endif
  OpenKneeboard::DPrintSettings::Set({
    .prefix = "OpenKneeboard-LuaAPI",
  });
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToOpenKneeboard);
  lua_setfield(state, -2, "sendRaw");
  return 1;
}

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.API.Lua")
 * 039d7b52-2065-5863-802b-873c638bdf88
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.API.Lua",
  (0x039d7b52, 0x2065, 0x5863, 0x80, 0x2b, 0x87, 0x3c, 0x63, 0x8b, 0xdf, 0x88));
}// namespace OpenKneeboard

static TraceLoggingThreadActivity<OpenKneeboard::gTraceProvider> gActivity;

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  wchar_t* wpgmptr {nullptr};
  char* pgmptr {nullptr};
  _get_wpgmptr(&wpgmptr);
  _get_pgmptr(&pgmptr);

  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(OpenKneeboard::gTraceProvider);
      TraceLoggingWriteStart(
        gActivity,
        "Attached",
        TraceLoggingValue(wpgmptr, "ExecutableW"),
        TraceLoggingValue(pgmptr, "ExecutableA"));
      break;
    case DLL_PROCESS_DETACH:
      TraceLoggingWriteStop(
        gActivity,
        "Attached",
        TraceLoggingValue(wpgmptr, "ExecutableW"),
        TraceLoggingValue(pgmptr, "ExecutableA"));
      TraceLoggingUnregister(OpenKneeboard::gTraceProvider);
      break;
  }
  return TRUE;
}
