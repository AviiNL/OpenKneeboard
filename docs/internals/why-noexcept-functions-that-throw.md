---
title: Throwing from `noexcept`
parent: Internals
---

# Why are some functions that throw marked `noexcept`?

WinRT ABI boundaries must not throw; if they do, C++/WinRT will catch, and
throw a different exception with a different trace, and much less information.

For this reason, WinRT functions, properties, and and event handlers in OpenKneeboard
should be marked as `noexcept`; this means that uncaught exceptions will invoke the
`std::terminate` handler, which will lead to a useful trace, exception, and dump.

Exceptions should be used for 'exceptional' behavior; if one is expected, prefer
not to use exceptions if possible.

Do not add catch-all try/catch statements: these just hide errors. If an exception
is unavoidable but should be caught, catch the specific exception.

For more information, see https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/error-handling
