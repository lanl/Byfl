# Declare our project information.
cmake_minimum_required(VERSION 3.12)
project(Byfl)
set(PACKAGE_VERSION "1.6")

# Load all the LLVM-specific CMake features we require.
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

# Specify various fixed strings to use in man pages.
include(FindGit)
if (GIT_FOUND)
  execute_process(COMMAND x${GIT_EXECUTABLE} rev-parse --short HEAD
    OUTPUT_VARIABLE _byfl_man_release
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif (GIT_FOUND)
if (NOT _have_byfl_git_sha1 EQUAL 0)
  set(_byfl_man_release "${PACKAGE_VERSION}")
endif (NOT _have_byfl_git_sha1 EQUAL 0)  
set(MAN_CATEGORY "Byfl tools" CACHE STRING "Category string to list on Byfl man pages")
mark_as_advanced(MAN_CATEGORY)
set(MAN_RELEASE ${_byfl_man_release} CACHE STRING "Version string to list on Byfl man pages")
mark_as_advanced(MAN_RELEASE)

# Define the version of Byfl we're currently configuring.
if (GIT_FOUND)
  message(STATUS "Detecting the active Byfl Git commit")
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    RESULT_VARIABLE _have_byfl_git_sha1
    OUTPUT_VARIABLE _byfl_git_sha1
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if (_have_byfl_git_sha1 EQUAL 0)
    message(STATUS "Detecting the active Byfl Git commit - ${_byfl_git_sha1}")
    set(BYFL_GIT_SHA1 ${_byfl_git_sha1})
  else (_have_byfl_git_sha1 EQUAL 0)
    message(STATUS "Detecting the active Byfl Git commit - failed")
  endif (_have_byfl_git_sha1 EQUAL 0)
endif (GIT_FOUND)

# Byfl's MPI wrappers require Perl.
include(FindPerl)
if (NOT PERL_FOUND)
  message(FATAL_ERROR "Byfl requires Perl.")
endif (NOT PERL_FOUND)

# Generate a configuration file.
configure_file(config.h.in config.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Build all of our subdirectories.
#add_subdirectory(lib)
