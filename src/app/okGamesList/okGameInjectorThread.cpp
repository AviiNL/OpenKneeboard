#include "okGameInjectorThread.h"

#include <TlHelp32.h>
#include <detours.h>
#include <winrt/base.h>

#include <mutex>

#include "OpenKneeboard/dprint.h"
#include "RuntimeFiles.h"
#include "okEvents.h"

using namespace OpenKneeboard;

class okGameInjectorThread::Impl final {
 public:
  wxEvtHandler* receiver;
  std::vector<GameInstance> games;
  std::mutex gamesMutex;
};

okGameInjectorThread::okGameInjectorThread(
  wxEvtHandler* receiver,
  const std::vector<GameInstance>& games) {
  p.reset(new Impl {
    .receiver = receiver,
    .games = games,
  });
}

okGameInjectorThread::~okGameInjectorThread() {
}

void okGameInjectorThread::SetGameInstances(
  const std::vector<GameInstance>& games) {
  std::scoped_lock lock(p->gamesMutex);
  p->games = games;
}

namespace {
class DebugPrivileges final {
 private:
  winrt::handle mToken;
  LUID mLuid;

 public:
  DebugPrivileges() {
    if (!OpenProcessToken(
          GetCurrentProcess(),
          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
          mToken.put())) {
      dprint("Failed to open own process token");
      return;
    }

    LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &mLuid);

    TOKEN_PRIVILEGES privileges;
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = mLuid;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(
      mToken.get(), false, &privileges, sizeof(privileges), nullptr, nullptr);
  }

  ~DebugPrivileges() {
    if (!mToken) {
      return;
    }
    TOKEN_PRIVILEGES privileges;
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = mLuid;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;
    AdjustTokenPrivileges(
      mToken.get(), false, &privileges, sizeof(privileges), nullptr, nullptr);
  }
};

bool AlreadyInjected(DWORD processId, const std::filesystem::path& _dll) {
  auto dll = _dll.filename();
  winrt::handle snapshot {
    CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId)};
  MODULEENTRY32 module;
  module.dwSize = sizeof(module);

  if (!Module32First(snapshot.get(), &module)) {
    return false;
  }

  do {
    if (dll == module.szModule) {
      return true;
    }

  } while (Module32Next(snapshot.get(), &module));
  return false;
}

bool InjectDll(DWORD processId, const std::filesystem::path& _dll) {
  const auto dll = std::filesystem::canonical(_dll);
  if (AlreadyInjected(processId, dll)) {
    dprint("Asked to load a DLL that's already loaded");
    return false;
  }

  DebugPrivileges privileges;

  winrt::handle process {OpenProcess(PROCESS_ALL_ACCESS, false, processId)};

  if (!process) {
    dprint("Failed to open process");
    return false;
  }

  auto dllStr = dll.wstring();

  const auto dllStrByteLen = (dllStr.size() + 1) * sizeof(wchar_t);
  auto targetBuffer = VirtualAllocEx(
    process.get(), nullptr, dllStrByteLen, MEM_COMMIT, PAGE_READWRITE);
  if (!targetBuffer) {
    return false;
  }

  wxON_BLOCK_EXIT0([=]() { VirtualFree(targetBuffer, 0, MEM_RELEASE); });
  WriteProcessMemory(
    process.get(), targetBuffer, dllStr.c_str(), dllStrByteLen, nullptr);

  auto kernel32 = GetModuleHandleA("Kernel32");
  if (!kernel32) {
    dprint("Failed to open kernel32");
    return false;
  }

  auto loadLibraryW = reinterpret_cast<PTHREAD_START_ROUTINE>(
    GetProcAddress(kernel32, "LoadLibraryW"));
  if (!loadLibraryW) {
    dprint("Failed to find loadLibraryW");
    return false;
  }

  winrt::handle thread {CreateRemoteThread(
    process.get(), nullptr, 0, loadLibraryW, targetBuffer, 0, nullptr)};

  if (!thread) {
    dprint("Failed to create remote thread");
  }

  WaitForSingleObject(thread.get(), INFINITE);
  DWORD result = -1;
  GetExitCodeThread(thread.get(), &result);
  dprintf("Thread result: {}", result);

  return true;
}
}// namespace

wxThread::ExitCode okGameInjectorThread::Entry() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(NULL, buf, MAX_PATH);
  const auto executablePath
    = std::filesystem::canonical(std::filesystem::path(buf).parent_path());
  const auto injectedDll = executablePath / RuntimeFiles::AUTODETECT_DLL;

  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  MODULEENTRY32 module;
  module.dwSize = sizeof(module);
  dprint("Looking for game processes...");
  while (IsAlive()) {
    winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
      dprintf("CreateToolhelp32Snapshot failed in {}", __FILE__);
      return ExitCode(1);
    }

    if (!Process32First(snapshot.get(), &process)) {
      dprintf("Process32First failed: {}", GetLastError());
      return ExitCode(2);
    }

    std::filesystem::path currentPath;

    do {
      winrt::handle processHandle {
        OpenProcess(PROCESS_ALL_ACCESS, false, process.th32ProcessID)};
      if (!processHandle) {
        continue;
      }
      DWORD bufSize = sizeof(buf);
      QueryFullProcessImageNameW(processHandle.get(), 0, buf, &bufSize);
      const std::filesystem::path path(std::wstring(buf, bufSize));

      std::scoped_lock lock(p->gamesMutex);
      for (const auto& game: p->games) {
        if (path == game.Path) {
          const auto friendly = game.Game->GetUserFriendlyName(game.Path);
          if (AlreadyInjected(process.th32ProcessID, injectedDll)) {
            if (path != currentPath) {
              currentPath = path;
              wxCommandEvent ev(okEVT_GAME_CHANGED);
              ev.SetString(path.wstring());
              wxQueueEvent(p->receiver, ev.Clone());
            }
            break;
          }

          dprintf(
            "Found '{}' process to inject: {} {}",
            friendly.ToStdString(),
            process.th32ProcessID,
            path.string());

          if (!InjectDll(process.th32ProcessID, injectedDll)) {
            currentPath = path;
            dprintf("Failed to inject DLL: {}", GetLastError());
            break;
          }

          if (path == currentPath) {
            break;
          }

          currentPath = path;
          wxCommandEvent ev(okEVT_GAME_CHANGED);
          ev.SetString(path.wstring());
          wxQueueEvent(p->receiver, ev.Clone());
          break;
        }
      }
    } while (Process32Next(snapshot.get(), &process));

    wxThread::This()->Sleep(200);
  }
  return ExitCode(0);
}
