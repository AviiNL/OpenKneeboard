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
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <queue>

namespace OpenKneeboard {

static uint64_t sNextUniqueID = 0x1234abcdui64 << 32;

uint64_t _UniqueIDImpl::GetAndIncrementNextValue() {
  return sNextUniqueID++;
}

EventReceiver::EventReceiver() {
}

EventReceiver::~EventReceiver() {
  if (!mSenders.empty()) {
    dprint(
      "I'm in danger! ~EventReceiver() called without "
      "RemoveAllEventListeners()");
    OPENKNEEBOARD_BREAK;
  }
  this->RemoveAllEventListeners();
}

void EventReceiver::RemoveAllEventListeners() {
  decltype(mSenders) senders;
  {
    std::unique_lock lock(mMutex);
    senders = mSenders;
    mSenders.clear();
  }

  for (auto& sender: senders) {
    sender->Invalidate();
  }
}

void EventReceiver::RemoveEventListener(EventHandlerToken token) {
  std::shared_ptr<EventConnectionBase> toInvalidate;
  {
    std::unique_lock lock(mMutex);
    for (auto it = mSenders.begin(); it != mSenders.end(); ++it) {
      auto& sender = *it;
      if (sender->mToken != token) {
        continue;
      }
      toInvalidate = sender;
      mSenders.erase(it);
      break;
    }
  }
  if (toInvalidate) {
    toInvalidate->Invalidate();
  }
}

namespace {
struct EmitterQueueItem {
  std::function<void()> mEmitter;
  std::source_location mEnqueuedFrom;
};
}// namespace

static thread_local bool gHandlerRunning = false;
static thread_local std::queue<EmitterQueueItem> gEmitterQueue;

static void FlushEmitterQueue() {
  while (!gEmitterQueue.empty()) {
    auto item = gEmitterQueue.front();
    gEmitterQueue.pop();
    item.mEmitter();
  }
}

void EventBase::InvokeOrEnqueue(
  std::function<void()> func,
  std::source_location location) {
  if (gHandlerRunning) {
    gEmitterQueue.push({func, location});
    return;
  }
  gHandlerRunning = true;
  scope_guard notRunning([&]() { gHandlerRunning = false; });
  func();
  FlushEmitterQueue();
}

EventDelay::EventDelay() {
  if (gHandlerRunning) {
    return;
  }
  gHandlerRunning = true;
  mOwner = true;
}

EventDelay::~EventDelay() {
  if (!mOwner) {
    return;
  }
  FlushEmitterQueue();
  gHandlerRunning = false;
}

}// namespace OpenKneeboard
