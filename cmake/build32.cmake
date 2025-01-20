if(BUILD_IS_32BIT)
  add_custom_target(build32)
  return()
endif()

include(ExternalProject)

ExternalProject_Add(
  build32 
  SOURCE_DIR "${CMAKE_SOURCE_DIR}"
  STAMP_DIR "${CMAKE_BINARY_DIR}/stamp32"
  BINARY_DIR "${CMAKE_BINARY_DIR}/build32"
  INSTALL_DIR "${CMAKE_BINARY_DIR}/install32"
  BUILD_ALWAYS ON
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND
  "${CMAKE_COMMAND}"
  -S "<SOURCE_DIR>"
  -B "<BINARY_DIR>"
  -G "${CMAKE_GENERATOR}"
  "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  -A Win32
  "-DBUILD_OUT_PREFIX=${BUILD_OUT_PREFIX}"
  -DWITH_ASAN=${WITH_ASAN}
  BUILD_COMMAND
  "${CMAKE_COMMAND}"
  --build "<BINARY_DIR>"
  --config "$<CONFIG>"
  --target build32
  --parallel
  INSTALL_COMMAND
  ""
)