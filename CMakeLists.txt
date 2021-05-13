###################################
# Build all of Byfl               #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

# Declare our project information.
cmake_minimum_required(VERSION 3.12)
project(
  byfl
  DESCRIPTION "Program analysis tool based on software performance counters"
  HOMEPAGE_URL "https://github.com/lanl/Byfl"
  LANGUAGES C CXX
  )
set(PACKAGE_VERSION_MAJOR 1)
set(PACKAGE_VERSION_MINOR 8)
set(PACKAGE_VERSION_PATCH 0)
set(BYFL_PACKAGE_VERSION ${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH})

# HandleLLVMOptions *ought* to define these itself but as of LLVM 8 still
# neglects to do so.
set(LLVM_ENABLE_EH OFF CACHE BOOL "Enable Exception handling")
set(LLVM_ENABLE_RTTI OFF CACHE BOOL "Enable run time type information")
set(LLVM_EXPORT_SYMBOLS_FOR_PLUGINS OFF CACHE BOOL "Export symbols from LLVM tools so that plugins can import them")

# Load all the LLVM-specific CMake features we require.
find_package(LLVM CONFIG REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR} ${PROJECT_SOURCE_DIR}/cmake)
include(LLVMConfig)
include(AddLLVM)
include(HandleLLVMOptions)
mark_as_advanced(LLVM_DIR)

# Load other required CMake features.
include(CheckIncludeFileCXX)
include(CheckCXXSymbolExists)
include(GNUInstallDirs)
include(ByflUtilities)
include(ByflCPack)
enable_testing()

# Point the compiler to Byfl's top-level include directory and to LLVM's.  Also
# point the linker to LLVM's library directory.
include_directories(${PROJECT_SOURCE_DIR}/include ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

# Let the user choose between static and shared libraries.
option(BUILD_SHARED_LIBS "Build the Byfl run-time library as a shared library" OFF)

# We don't currently support different build types so hide from the user the
# option to select a build type.
mark_as_advanced(CMAKE_BUILD_TYPE)

# HandleLLVMOptions introduces a bunch of other cache variables the user
# probably doesn't care about.
foreach(_opt LLVM_ENABLE_EH LLVM_ENABLE_IDE LLVM_ENABLE_LTO LLVM_ENABLE_RTTI LLVM_EXPORT_SYMBOLS_FOR_PLUGINS LLVM_FORCE_ENABLE_STATS LLVM_PARALLEL_COMPILE_JOBS LLVM_PARALLEL_LINK_JOBS)
  mark_as_advanced(${_opt})
endforeach()

# Let the user override the Clang compilers we'll use for testing.
find_llvm_program(CLANG_EXECUTABLE "Clang C compiler" clang)
find_llvm_program(CLANGXX_EXECUTABLE "Clang C++ compiler" clang++)
find_llvm_program(OPT_EXECUTABLE "LLVM standalone optimizer" opt)
find_llvm_program(LLVM_AS_EXECUTABLE "LLVM assembler" llvm-as)
find_llvm_program(LLVM_DIS_EXECUTABLE "LLVM disassembler" llvm-dis)
find_llvm_program(LLVM_NM_EXECUTABLE "LLVM symbol-table reader" llvm-nm)

# Let the user choose where to install the Byfl plugin.  It defaults to
# libexec/byfl as opposed to LLVM's default of lib (or lib64) because a plugin
# is not a library to which an application explicitly links.
set(CMAKE_INSTALL_BYFL_PLUGIN_DIR ${CMAKE_INSTALL_LIBEXECDIR}/byfl CACHE PATH
  "Directory in which to install the Byfl LLVM plugin")
mark_as_advanced(CMAKE_INSTALL_BYFL_PLUGIN_DIR)
file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BYFL_PLUGIN_DIR}" CMAKE_INSTALL_FULL_BYFL_PLUGIN_DIR)

# Byfl requires C++14.
set(CMAKE_CXX_STANDARD 14)

# Specify various fixed strings to use in man pages.
include(FindGit)
if (GIT_FOUND)
  execute_process(COMMAND x${GIT_EXECUTABLE} rev-parse --short HEAD
    OUTPUT_VARIABLE _byfl_man_release
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif (GIT_FOUND)
if (NOT _have_byfl_git_sha1 EQUAL 0)
  set(_byfl_man_release "${BYFL_PACKAGE_VERSION}")
endif (NOT _have_byfl_git_sha1 EQUAL 0)
set(MAN_CATEGORY "Byfl tools" CACHE STRING "Category string to list on Byfl man pages")
mark_as_advanced(MAN_CATEGORY)
set(MAN_RELEASE ${_byfl_man_release} CACHE STRING "Version string to list on Byfl man pages")
mark_as_advanced(MAN_RELEASE)

# Extract the Git branch and commit hash for provenance purposes.
if (GIT_FOUND)
  # Git branch
  set_var_to_process_output(
    OUTPUT_VARIABLE BYFL_GIT_BRANCH
    MESSAGE "Detecting the active Byfl Git branch"
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    )

  # Git commit
  set_var_to_process_output(
    OUTPUT_VARIABLE BYFL_GIT_SHA1
    MESSAGE "Detecting the active Byfl Git commit"
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    )
endif (GIT_FOUND)

# Byfl's MPI wrappers require Perl.
include(FindPerl)
if (NOT PERL_FOUND)
  message(FATAL_ERROR "Byfl requires Perl to build.")
endif (NOT PERL_FOUND)
include(FindPerlLibs)
find_program(POD2MAN_EXECUTABLE pod2man${CMAKE_EXECUTABLE_SUFFIX}
  DOC "Program to convert Perl POD files to man pages")
mark_as_advanced(POD2MAN_EXECUTABLE)

# Store the closest thing we have to an ARCH-VENDOR-OS string.
set(TARGET_DESCRIPTION ${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM} CACHE STRING
  "Target system identifying information")
mark_as_advanced(TARGET_DESCRIPTION)

# Byfl supports Flang as the Fortran compiler but doesn't require it.
set(_flang_msg "Checking for a Clang-compatible Fortran compiler")
message(STATUS ${_flang_msg})
find_llvm_program(FLANG_EXECUTABLE "LLVM-based Fortran compiler" flang)
if (${FLANG_EXECUTABLE})
  message(STATUS "${_flang_msg} - ${FLANG_EXECUTABLE}")
else (${FLANG_EXECUTABLE})
  message(STATUS "${_flang_msg} - not found")
endif (${FLANG_EXECUTABLE})
mark_as_advanced(FLANG_EXECUTABLE)

# Byfl can give more informative warning messages if the system supports
# weak function aliases, which, as the time of this writing, OS X does not.
check_weak_aliases()

# OS X lacks a /proc/self/cmdline file, but it does provide a _NSGetArgv()
# function to read the caller's command line.
check_include_file_cxx(crt_externs.h HAVE_CRT_EXTERNS_H)
check_cxx_symbol_exists(_NSGetArgv crt_externs.h HAVE__NSGETARGV)

# The Byfl build process parses Instruction.def.  Ensure it exists.
check_include_file_cxx(llvm/IR/Instruction.def _have_inst_def)
if (NOT _have_inst_def)
  message(FATAL_ERROR "Byfl requires the llvm/IR/Instruction.def C++ header file")
endif (NOT _have_inst_def)

# Tally the number of LLVM opcodes.
tally_llvm_opcodes()

# The Byfl binary-output library uses asprintf for error reporting but can
# work around its absence with an unsafe alternative.
set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
check_cxx_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
if (NOT HAVE_ASPRINTF)
  message(WARNING "Using an unsafe alternative to asprintf, which wasn't found.")
endif (NOT HAVE_ASPRINTF)

# If getopt_long is available we can build bfbin2csv and bfbin2sqlite3.
check_cxx_symbol_exists(getopt_long getopt.h HAVE_GETOPT_LONG)

# If HDF5 is installed and seems to work we can build bfbin2hdf5.
find_package(HDF5 COMPONENTS CXX)
if (NOT HDF5_FOUND)
  message(WARNING "Not building the bfbin2hdf5 postprocessor because it requires a working HDF5.")
endif (NOT HDF5_FOUND)
mark_as_advanced(
  HDF5_CXX_LIBRARY_dl
  HDF5_CXX_LIBRARY_hdf5
  HDF5_CXX_LIBRARY_hdf5_cpp
  HDF5_CXX_LIBRARY_m
  HDF5_CXX_LIBRARY_pthread
  HDF5_CXX_LIBRARY_sz
  HDF5_CXX_LIBRARY_z
  )

# If SQLite3 is available we can build bfbin2sqlite3.
function (check_sqlite3)
  set(CMAKE_REQUIRED_LIBRARIES "sqlite3;${CMAKE_REQUIRED_LIBRARIES}")
  check_cxx_symbol_exists(sqlite3_errstr sqlite3.h SQLITE3_FOUND)
endfunction (check_sqlite3)
check_sqlite3()
if (NOT SQLITE3_FOUND)
  message(WARNING "Not building the bfbin2sqlite3 postprocessor because it requires SQLite3 (v3.7.15+).")
endif (NOT SQLITE3_FOUND)

# Generate a configuration file.
configure_file(config.h.in config.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Build all of our subdirectories.
add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(tools)
add_subdirectory(tests)
