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

// clang-format off
// <Unknwn.h> must be included before <winrt/base.h> for com_ptr::as<> to work
// correctly
#include <Unknwn.h>
// clang-format on

#include <OpenKneeboard/tracing.h>

#include <shims/source_location>

#include <pplawait.h>
#include <ppltasks.h>

#include <winrt/Windows.Foundation.h>
#pragma warning(push)
#pragma warning(disable : 26820)// Potentially expensive copy operation
#include <winrt/base.h>
#pragma warning(pop)

#include <stop_token>

namespace OpenKneeboard {

inline auto random_guid() {
  winrt::guid ret;
  winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID*>(&ret)));
  return ret;
}

inline winrt::Windows::Foundation::IAsyncAction
make_stoppable(std::stop_token token, auto action, std::source_location loc) {
  const auto src = std::format("{}", loc);
  if (token.stop_requested()) {
    co_return;
  }

  try {
    auto op = ([](auto action) -> winrt::Windows::Foundation::IAsyncAction {
      auto tok = co_await winrt::get_cancellation_token();
      tok.enable_propagation();
      co_await action();
    })(action);
    const std::stop_callback callback(token, [&op, &src]() {
      TraceLoggingWrite(
        gTraceProvider,
        "make_stoppable()/cancel",
        TraceLoggingString(src.c_str(), "Source"));
      op.Cancel();
    });
    co_await op;
    co_return;
  } catch (const winrt::hresult_canceled&) {
    TraceLoggingWrite(
      gTraceProvider,
      "make_stoppable()/cancelled",
      TraceLoggingString(src.c_str(), "Source"));
  }
}

inline winrt::Windows::Foundation::IAsyncAction resume_on_signal(
  std::stop_token token,
  HANDLE handle,
  winrt::Windows::Foundation::TimeSpan timeout = {},
  std::source_location loc = std::source_location::current()) {
  return make_stoppable(
    token,
    [handle, timeout]() { return winrt::resume_on_signal(handle, timeout); },
    loc);
}

inline winrt::Windows::Foundation::IAsyncAction resume_after(
  std::stop_token token,
  winrt::Windows::Foundation::TimeSpan timeout,
  std::source_location loc = std::source_location::current()) {
  return make_stoppable(
    token, [timeout]() { return winrt::resume_after(timeout); }, loc);
}

}// namespace OpenKneeboard

template <class CharT>
struct std::formatter<winrt::guid, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  template <class FormatContext>
  auto format(const winrt::guid& guid, FormatContext& fc) const {
    auto ret = winrt::to_hstring(guid);
    if constexpr (std::same_as<CharT, wchar_t>) {
      return std::formatter<std::basic_string_view<CharT>, CharT>::format(
        std::basic_string_view<CharT> {ret.data(), ret.size()}, fc);
    } else {
      return std::formatter<std::basic_string_view<CharT>, CharT>::format(
        std::basic_string_view<CharT> {winrt::to_string(ret)}, fc);
    }
  }
};
