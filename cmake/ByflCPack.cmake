############################################################
# This file prepares Byfl for source and binary packaging. #
############################################################

# Define generic package metadata.
set(CPACK_PACKAGE_VERSION "${BYFL_PACKAGE_VERSION}")
set(CPACK_PACKAGE_VENDOR "Triad National Security, LLC")
set(CPACK_PACKAGE_CONTACT "Scott Pakin <pakin@lanl.gov>")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")

# Define generator-specific package metadata.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "clang-${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

# Default to building only .tgz files, but let the user override this decision.
set(CPACK_GENERATOR "TGZ" CACHE STRING "Semicolon-separated list of binary package types to generate")
mark_as_advanced(CPACK_GENERATOR)
set(CPACK_SOURCE_GENERATOR "TGZ" CACHE STRING "Semicolon-separated list of source package types to generate")
mark_as_advanced(CPACK_SOURCE_GENERATOR)

# At least as of CMake 3.13.4, CPack is too stupid to omit the build directory
# from source packages.  Here we help cure at least this aspect of its
# stupidity.  We even let the user override the default list.
string(REGEX REPLACE "([][+.*()^])" "\\\\\\1" _build_dir_regex "${CMAKE_BINARY_DIR}")
set(_ignore_files "${_build_dir_regex}")
list(APPEND _ignore_files [=[/\\.git;\\.swp\$;\\.#;/#]=])
set(
  CPACK_SOURCE_IGNORE_FILES "${_ignore_files}" CACHE STRING
  "Semicolon-separated regular expressions of files to exclude from source packages"
  )
mark_as_advanced(CPACK_SOURCE_IGNORE_FILES)

# Remove "-Source" from the source package name.
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}")

# Include the LLVM version number in the binary package name.
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-llvm-${LLVM_PACKAGE_VERSION}")

# Add support for "make package" and "make package_source".
include(CPack)
