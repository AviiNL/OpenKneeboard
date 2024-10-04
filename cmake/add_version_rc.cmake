include_guard(GLOBAL)

include(disable_clang_tidy)

function(create_version_rc RC_TARGET ORIGINAL_TARGET ORIGINAL_FILENAME OUTPUT_RC_FILE)
  add_custom_target(
    "${RC_TARGET}"
    COMMAND
    ${CMAKE_COMMAND}
    -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
    -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
    "-DACTIVE_BUILD_MODE=$<CONFIG>"
    "-DINPUT_RC_FILE=${CMAKE_SOURCE_DIR}/src/version.rc.in"
    "-DOUTPUT_RC_FILE=${OUTPUT_RC_FILE}"
    "-DFILE_DESCRIPTION=${ORIGINAL_TARGET}"
    "-DORIGINAL_FILENAME=${ORIGINAL_FILENAME}"
    "-P${CMAKE_SOURCE_DIR}/src/version.cmake"
    DEPENDS
    "${CMAKE_SOURCE_DIR}/src/version.cmake"
    BYPRODUCTS "${OUTPUT_RC_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  disable_clang_tidy("${RC_TARGET}")
endfunction()

function(add_version_rc TARGET)
  set(VERSION_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.version.rc")
  create_version_rc(
    "${TARGET}-version-rc"
    "$<TARGET_FILE_BASE_NAME:${TARGET}>"
    "$<TARGET_FILE_NAME:${TARGET}>"
    "${VERSION_RC_FILE}"
  )
  target_sources("${TARGET}" PRIVATE "${VERSION_RC_FILE}")
endfunction()