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
#include <Windows.h>
// clang-format on

#include <OpenKneeboard/macros.hpp>

#include <exception>
#include <source_location>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

namespace OpenKneeboard {

wchar_t* GetFullPathForCurrentExecutable();

#define TraceLoggingThisExecutable() \
  TraceLoggingValue( \
    ::OpenKneeboard::GetFullPathForCurrentExecutable(), "Executable")

TRACELOGGING_DECLARE_PROVIDER(gTraceProvider);

#define OPENKNEEBOARD_TraceLoggingSourceLocation(loc) \
  TraceLoggingValue(loc.file_name(), "File"), \
    TraceLoggingValue(loc.line(), "Line"), \
    TraceLoggingValue(loc.function_name(), "Function")

#define OPENKNEEBOARD_TraceLoggingSize2D(size2d, name) \
  TraceLoggingValue(size2d.Width(), name "/Width"), \
    TraceLoggingValue(size2d.Height(), name "/Height")

#define OPENKNEEBOARD_TraceLoggingRect(pr, name) \
  TraceLoggingValue(pr.Left(), name "/Left"), \
    TraceLoggingValue(pr.Top(), name "/Top"), \
    OPENKNEEBOARD_TraceLoggingSize2D(pr, name)

#if defined(CLANG_TIDY) || defined(__CLANG__) || defined(CLANG_CL)
// We should be able to switch to the real definitions once
// we're using `/Zc:preprocessor` and OPENKNEEBOARD_VA_OPT_SUPPORTED is true
namespace ClangStubs {
class TraceLoggingScopedActivity
  : public TraceLoggingThreadActivity<gTraceProvider> {
 public:
  constexpr void Stop() {
  }
  constexpr void CancelAutoStop() {
  }
  constexpr void StopWithResult([[maybe_unused]] auto result) {
  }
};
constexpr const auto& maybe_unused(const auto& first, const auto&...) noexcept {
  return first;
}
}// namespace ClangStubs
#define OPENKNEEBOARD_TraceLoggingScope(...)
#define OPENKNEEBOARD_TraceLoggingScopedActivity(activity, ...) \
  ClangStubs::TraceLoggingScopedActivity activity;
#define OPENKNEEBOARD_TraceLoggingWrite(...)

#undef TraceLoggingProviderEnabled

#undef TraceLoggingValue
#undef TraceLoggingCountedWideString
#undef TraceLoggingString
#undef TraceLoggingBinary
#undef TraceLoggingGuid
#undef TraceLoggingHexUInt64
#undef TraceLoggingHexUInt64Array

#undef TraceLoggingWrite
#undef TraceLoggingWriteStart
#undef TraceLoggingWriteStop
#undef TraceLoggingWriteTagged

#define TraceLoggingProviderEnabled(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)

#define TraceLoggingValue(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingCountedWideString(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingString(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingBinary(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingGuid(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingHexUInt64(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingHexUInt64Array(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)

#define TraceLoggingWrite(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingWriteStart(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingWriteStop(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)
#define TraceLoggingWriteTagged(...) \
  OpenKneeboard::ClangStubs::maybe_unused(__VA_ARGS__)

#else

// TraceLoggingWriteStart() requires the legacy preprocessor :(
static_assert(_MSVC_TRADITIONAL);
// Rewrite these macros if this fails, as presumably the above was fixed :)
//
// - ##__VA_ARGS__             (common vendor extension)
// + __VA_OPT__(,) __VA_ARGS__ (standard C++20)
static_assert(!OPENKNEEBOARD_VA_OPT_SUPPORTED);
// ... but we currently depend on ##__VA_ARGS__
static_assert(OPENKNEEBOARD_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION);

/** Create and automatically start and stop a named activity.
 *
 * @param OKBTL_ACTIVITY the local variable to store the activity in
 * @param OKBTL_NAME the name of the activity (C string literal)
 *
 * @see OPENKNEEBOARD_TraceLoggingScope if you don't need the local variable
 *
 * This avoids templates and `auto` and generally jumps through hoops so that it
 * is valid both inside an implementation, and in a class definition.
 */
#define OPENKNEEBOARD_TraceLoggingScopedActivity( \
  OKBTL_ACTIVITY, OKBTL_NAME, ...) \
  const std::function<void(TraceLoggingThreadActivity<gTraceProvider>&)> \
    OPENKNEEBOARD_CONCAT2(_StartImpl, OKBTL_ACTIVITY) \
    = [&, loc = std::source_location::current()]( \
        TraceLoggingThreadActivity<gTraceProvider>& activity) { \
        TraceLoggingWriteStart( \
          activity, \
          OKBTL_NAME, \
          OPENKNEEBOARD_TraceLoggingSourceLocation(loc), \
          ##__VA_ARGS__); \
      }; \
  class OPENKNEEBOARD_CONCAT2(_Impl, OKBTL_ACTIVITY) final \
    : public TraceLoggingThreadActivity<gTraceProvider> { \
   public: \
    OPENKNEEBOARD_CONCAT2(_Impl, OKBTL_ACTIVITY) \
    (decltype(OPENKNEEBOARD_CONCAT2(_StartImpl, OKBTL_ACTIVITY))& startImpl) { \
      startImpl(*this); \
    } \
    OPENKNEEBOARD_CONCAT2(~_Impl, OKBTL_ACTIVITY)() { \
      if (mAutoStop) { \
        this->Stop(); \
      } \
    } \
    void Stop() { \
      if (mStopped) [[unlikely]] { \
        OutputDebugStringW(L"Double-stopped in Stop()"); \
        OPENKNEEBOARD_BREAK; \
        return; \
      } \
      mStopped = true; \
      mAutoStop = false; \
      const auto exceptionCount = std::uncaught_exceptions(); \
      if (exceptionCount) [[unlikely]] { \
        TraceLoggingWriteStop( \
          *this, \
          OKBTL_NAME, \
          TraceLoggingValue(exceptionCount, "UncaughtExceptions")); \
      } else { \
        TraceLoggingWriteStop(*this, OKBTL_NAME); \
      } \
    } \
    void CancelAutoStop() { \
      mAutoStop = false; \
    } \
    _OPENKNEEBOARD_TRACELOGGING_IMPL_StopWithResult(OKBTL_NAME, int); \
    _OPENKNEEBOARD_TRACELOGGING_IMPL_StopWithResult(OKBTL_NAME, const char*); \
\
   private: \
    bool mStopped {false}; \
    bool mAutoStop {true}; \
  }; \
  OPENKNEEBOARD_CONCAT2(_Impl, OKBTL_ACTIVITY) \
  OKBTL_ACTIVITY {OPENKNEEBOARD_CONCAT2(_StartImpl, OKBTL_ACTIVITY)};

// Not using templates as they're not permitted in local classes
#define _OPENKNEEBOARD_TRACELOGGING_IMPL_StopWithResult( \
  OKBTL_NAME, OKBTL_RESULT_TYPE) \
  void StopWithResult(OKBTL_RESULT_TYPE result) { \
    if (mStopped) [[unlikely]] { \
      OutputDebugStringW(L"Double-stopped in StopWithResult()"); \
      OPENKNEEBOARD_BREAK; \
      return; \
    } \
    this->CancelAutoStop(); \
    mStopped = true; \
    TraceLoggingWriteStop( \
      *this, OKBTL_NAME, TraceLoggingValue(result, "Result")); \
  }

/** Create and automatically start and stop a named activity.
 *
 * Convenience wrapper around OPENKNEEBOARD_TraceLoggingScopedActivity
 * that generates the local variable names.
 *
 * @param OKBTL_NAME the name of the activity (C string literal)
 */
#define OPENKNEEBOARD_TraceLoggingScope(OKBTL_NAME, ...) \
  OPENKNEEBOARD_TraceLoggingScopedActivity( \
    OPENKNEEBOARD_CONCAT2(_okbtlsa, __COUNTER__), OKBTL_NAME, ##__VA_ARGS__)

#define OPENKNEEBOARD_TraceLoggingWrite(OKBTL_NAME, ...) \
  TraceLoggingWrite( \
    gTraceProvider, \
    OKBTL_NAME, \
    TraceLoggingValue(__FILE__, "File"), \
    TraceLoggingValue(__LINE__, "Line"), \
    TraceLoggingValue(__FUNCTION__, "Function"), \
    ##__VA_ARGS__)

#endif

}// namespace OpenKneeboard

#ifdef CLANG_TIDY
namespace ClangStubs = OpenKneeboard::ClangStubs;
#endif