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

#include <OpenKneeboard/tracing.h>

#include <cstdlib>

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.RemoteControl")
 * 6dafad04-3f57-55d2-e92e-7e49710d7e46
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.RemoteControl",
  (0x6dafad04, 0x3f57, 0x55d2, 0xe9, 0x2e, 0x7e, 0x49, 0x71, 0x0d, 0x7e, 0x46));

}// namespace OpenKneeboard

namespace {

TraceLoggingThreadActivity<OpenKneeboard::gTraceProvider> gActivity;

class TraceLoggingRegistration {
 public:
  TraceLoggingRegistration() {
    wchar_t* wpgmptr {nullptr};
    char* pgmptr {nullptr};
    _get_wpgmptr(&wpgmptr);
    _get_pgmptr(&pgmptr);

    TraceLoggingRegister(OpenKneeboard::gTraceProvider);
    TraceLoggingWriteStart(
      gActivity,
      "Invocation",
      TraceLoggingValue(wpgmptr, "ExecutableW"),
      TraceLoggingValue(pgmptr, "ExecutableA"),
      TraceLoggingValue(GetCommandLineW(), "Command Line"));
  }
  ~TraceLoggingRegistration() {
    TraceLoggingWriteStop(gActivity, "Invocation");
    TraceLoggingUnregister(OpenKneeboard::gTraceProvider);
  }
};
}// namespace

static TraceLoggingRegistration gTraceLoggingRegistration;
