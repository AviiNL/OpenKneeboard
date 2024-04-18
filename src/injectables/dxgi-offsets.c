#include "dxgi-offsets.h"

#include <dxgi.h>

void* VTable_Lookup_IDXGISwapChain_Present(struct IDXGISwapChain* swapchain) {
#ifdef CLANG_TIDY
  return nullptr;
#else
  return swapchain->lpVtbl->Present;
#endif
}

void* VTable_Lookup_IDXGISwapChain_ResizeBuffers(
  struct IDXGISwapChain* swapchain) {
#ifdef CLANG_TIDY
  return nullptr;
#else
  return swapchain->lpVtbl->ResizeBuffers;
#endif
}
