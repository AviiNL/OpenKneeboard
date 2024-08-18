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
#include <combaseapi.h>
#include <ctxtcall.h>
// clang-format on

#include <OpenKneeboard/StateMachine.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>

#include <atomic>
#include <coroutine>
#include <memory>

namespace OpenKneeboard::detail {

enum class TaskPromiseState : uintptr_t {
  Running = 1,
  Abandoned = 2,
  Completed = 3,
};

enum class TaskPromiseResultState {
  NoResult,
  HaveException,
  ThrownException,
  HaveResult,
  HaveVoidResult,
  ReturnedResult,
  ReturnedVoid,
};

/* Union so we can do std::atomic.
 *
 * - TaskPromiseState is sizeof(ptr), but always an invalid pointer
 * - coroutine handles actually contain a pointer
 */
union TaskPromiseWaiting {
  TaskPromiseState mState {TaskPromiseState::Running};
  std::coroutine_handle<> mNext;
};
static_assert(sizeof(TaskPromiseWaiting) == sizeof(TaskPromiseState));
static_assert(sizeof(TaskPromiseWaiting) == sizeof(std::coroutine_handle<>));

constexpr TaskPromiseWaiting TaskPromiseRunning {
  .mState = TaskPromiseState::Running};
constexpr TaskPromiseWaiting TaskPromiseAbandoned {
  .mState = TaskPromiseState::Abandoned};
constexpr TaskPromiseWaiting TaskPromiseCompleted {
  .mState = TaskPromiseState::Completed};

constexpr bool operator==(TaskPromiseWaiting a, TaskPromiseWaiting b) noexcept {
  return a.mNext == b.mNext;
}

struct TaskContext {
  winrt::com_ptr<IContextCallback> mCOMCallback;
  std::thread::id mThreadID = std::this_thread::get_id();
  StackFramePointer mCaller {nullptr};

  inline TaskContext(StackFramePointer&& caller) : mCaller(std::move(caller)) {
    if (!SUCCEEDED(CoGetObjectContext(IID_PPV_ARGS(mCOMCallback.put()))))
      [[unlikely]] {
      fatal("Attempted to create a task<> from thread without COM");
    }
  }
};

template <class TResult>
struct TaskPromiseBase;

template <class TResult>
struct TaskFinalAwaiter {
  TaskPromiseBase<TResult>& mPromise;

  bool await_ready() const noexcept {
    return false;
  }

  template <class TPromise>
  void await_suspend(std::coroutine_handle<TPromise> handle) noexcept {
    auto oldState = mPromise.mWaiting.exchange(TaskPromiseCompleted);
    if (oldState == TaskPromiseAbandoned) {
      mPromise.destroy();
      return;
    }
    if (oldState == TaskPromiseRunning) {
      return;
    }

    // Must have a valid pointer, or we have corruption
    assert(oldState != TaskPromiseCompleted);
    const auto& context = mPromise.mContext;
    if (context.mThreadID == std::this_thread::get_id()) {
      oldState.mNext.resume();
      return;
    }

    ResumeData resumeData {
      .mContext = context,
      .mCoro = oldState.mNext,
    };
    ComCallData comData {.pUserDefined = &resumeData};
    const auto result = context.mCOMCallback->ContextCallback(
      &TaskFinalAwaiter<TResult>::resume,
      &comData,
      IID_ICallbackWithNoReentrancyToApplicationSTA,
      5,
      NULL);
    if (SUCCEEDED(result)) [[likely]] {
      return;
    }
    fatal(
      "Failed to enqueue coroutine resumption for the desired thread: {:#010x}",
      static_cast<uint32_t>(result));
  }

  void await_resume() const noexcept {
  }

 private:
  struct ResumeData {
    TaskContext mContext;
    std::coroutine_handle<> mCoro;
  };

  static HRESULT resume(ComCallData* comData) {
    const auto& resumeData
      = *reinterpret_cast<ResumeData*>(comData->pUserDefined);
    if (std::this_thread::get_id() != resumeData.mContext.mThreadID)
      [[unlikely]] {
      fatal(
        "Expected to resume task in thread {}, but resumed in thread {}",
        resumeData.mContext.mThreadID,
        std::this_thread::get_id());
    }
    resumeData.mCoro.resume();
    return S_OK;
  }
};

template <class TResult>
struct Task;

template <class TResult>
struct TaskPromiseBase {
  using enum TaskPromiseResultState;
  AtomicStateMachine<
    TaskPromiseResultState,
    NoResult,
    std::array {
      Transition {NoResult, HaveResult},
      Transition {HaveResult, ReturnedResult},

      Transition {NoResult, HaveException},
      Transition {HaveException, ThrownException},

      Transition {NoResult, HaveVoidResult},
      Transition {HaveVoidResult, ReturnedVoid},
    },
    std::nullopt>
    mResultState;

  std::exception_ptr mUncaught {};

  std::atomic<TaskPromiseWaiting> mWaiting {TaskPromiseRunning};

  TaskContext mContext;

  TaskPromiseBase() = delete;

  TaskPromiseBase(TaskContext&& context) noexcept
    : mContext(std::move(context)) {
  }

  auto get_return_object(this auto& self) {
    return &self;
  }

  void unhandled_exception() {
    mResultState.Transition<NoResult, HaveException>();

    mUncaught = std::current_exception();
  }

  void abandon() {
    auto oldState = mWaiting.exchange(TaskPromiseAbandoned);
    if (oldState == TaskPromiseRunning) {
      fatal(mContext.mCaller, "result *must* be awaited");
      return;
    }
    this->destroy();
  }

  template <class Self>
  void destroy(this Self& self) {
    std::coroutine_handle<Self>::from_promise(self).destroy();
  }

  std::suspend_never initial_suspend() noexcept {
    return {};
  };

  TaskFinalAwaiter<TResult> final_suspend() noexcept {
    return {*this};
  }
};

template <class TResult>
struct TaskPromise : TaskPromiseBase<TResult> {
  [[msvc::forceinline]]
  TaskPromise()
    : TaskPromiseBase<TResult>(StackFramePointer {_ReturnAddress()}) {
  }

  TResult mResult;

  void return_value(TResult&& result) noexcept {
    using enum TaskPromiseResultState;
    mResult = std::move(result);
    this->mResultState.Transition<NoResult, HaveResult>();
  }
};

template <>
struct TaskPromise<void> : TaskPromiseBase<void> {
  [[msvc::forceinline]]
  TaskPromise()
    : TaskPromiseBase<void>(StackFramePointer {_ReturnAddress()}) {
  }

  void return_void() noexcept {
    using enum TaskPromiseResultState;
    mResultState.Transition<NoResult, HaveVoidResult>();
  }
};

template <class T>
struct TaskPromiseDeleter {
  void operator()(T* promise) const noexcept {
    promise->abandon();
  }
};

template <class T>
using TaskPromisePtr = std::unique_ptr<T, TaskPromiseDeleter<T>>;

template <class TResult>
struct TaskAwaiter {
  using promise_t = TaskPromise<TResult>;
  using promise_ptr_t = TaskPromisePtr<promise_t>;
  promise_ptr_t mPromise;

  TaskAwaiter() = delete;
  TaskAwaiter(promise_ptr_t&& init) : mPromise(std::move(init)) {
  }

  bool await_ready() const noexcept {
    switch (mPromise->mWaiting.load().mState) {
      case TaskPromiseState::Running:
        return false;
      case TaskPromiseState::Completed:
        return true;
      default:
        return false;
    }
  }

  bool await_suspend(std::coroutine_handle<> caller) {
    auto oldState = mPromise->mWaiting.exchange({.mNext = caller});
    return oldState == TaskPromiseRunning;
  }

  template <class T = TResult>
    requires std::same_as<T, void>
  void await_resume() const noexcept {
    using enum TaskPromiseResultState;
    if (mPromise->mUncaught) {
      mPromise->mResultState.Transition<HaveException, ThrownException>();
      std::rethrow_exception(std::move(mPromise->mUncaught));
    }
    mPromise->mResultState.Transition<HaveVoidResult, ReturnedVoid>();
  }

  template <std::convertible_to<TResult> T = TResult>
    requires(!std::same_as<T, void>)
  T&& await_resume() noexcept {
    using enum TaskPromiseResultState;
    if (mPromise->mUncaught) {
      mPromise->mResultState.Transition<HaveException, ThrownException>();
      std::rethrow_exception(std::move(mPromise->mUncaught));
    }
    mPromise->mResultState.Transition<HaveResult, ReturnedResult>();
    return std::move(mPromise->mResult);
  }
};

template <class TResult>
struct [[nodiscard]] Task {
  using promise_t = TaskPromise<TResult>;
  using promise_ptr_t = TaskPromisePtr<promise_t>;

  using promise_type = promise_t;

  Task() = delete;
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&) = default;
  Task& operator=(Task&&) = default;

  Task(std::nullptr_t) = delete;
  Task(promise_t* promise) : mPromise(promise) {
    OPENKNEEBOARD_ASSERT(promise);
  }

  ~Task() {
    if (!mPromise) {
      // moved
      return;
    }
    using enum TaskPromiseResultState;
    const auto resultState = mPromise->mResultState.Get();
    switch (resultState) {
      case ThrownException:
      case ReturnedResult:
      case ReturnedVoid:
        break;
      default:
        fatal(
          mPromise->mContext.mCaller,
          "Invalid final result state: {:#} - did you `co_await` the task?",
          resultState);
    }
  }

  // just to give nice compiler error messages
  struct cannot_await_lvalue_use_std_move {
    void await_ready() {
    }
  };
  cannot_await_lvalue_use_std_move operator co_await() & = delete;

  auto operator co_await() && noexcept {
    // probably moved
    OPENKNEEBOARD_ASSERT(mPromise);
    return TaskAwaiter<TResult> {std::move(mPromise)};
  }

  promise_ptr_t mPromise;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {
/** A coroutine that:
 * - always returns to the same thread it was invoked from
 * - to implement that, requires that it is called from a thread with a COM
 * apartment
 * - calls fatal() if not awaited
 * - statically requires that the reuslt is discarded via [[nodiscard]]
 * - calls fatal() if there is an uncaught exception
 * - propagates uncaught exceptions back to the caller
 *
 * This is similar to wil::com_task(), except that it doesn't have as many
 * dependencies/unwanted interaections with various parts of Windows.h and
 * ole2.h
 *
 * If you want to store one, use an `std::optional<task<T>>`; to co_await the
 * stored task, use `co_await std::move(optional).value()`
 */
template <class TIgnoredDispatcherQueue, class T>
using basic_task = detail::Task<T>;
template <class T>
using task = basic_task<DispatcherQueue, T>;

}// namespace OpenKneeboard