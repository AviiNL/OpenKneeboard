#pragma once

#include <chrono>
#include <memory>

namespace OpenKneeboard {

class ConsoleLoopCondition final {
 public:
  ConsoleLoopCondition();
  ~ConsoleLoopCondition();

  /// Returns false if sleep is interrupted
  bool Sleep(const std::chrono::steady_clock::duration& delay);

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard
