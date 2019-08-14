Byfl installation
=================

Byfl relies on [LLVM](http://www.llvm.org/) and [Clang](http://clang.llvm.org/) and can take advantage of [Flang](https://github.com/flang-compiler/flang) (not yet thoroughly tested).  The [`env` section of Byfl's Travis CI configuration file](https://github.com/lanl/Byfl/blob/master/.travis.yml#L5-L8) indicates the LLVM/Clang versions that are currently being used for regression testing and can therefore be considered the most stable.

Basic installation
------------------

Once you've downloaded Byfl, follow the usual [CMake](https://cmake.org/) build procedure:
```bash
cd Byfl
mkdir build
cd build
cmake ..
make
make install
```

You may also want to run `make test` to verify the build.

Some commonly used [`cmake`](https://cmake.org/cmake/help/latest/manual/cmake.1.html) options include [`-DCMAKE_INSTALL_PREFIX`](https://cmake.org/cmake/help/latest/variable/CMAKE_INSTALL_PREFIX.html)=〈*directory*〉 to specify the top-level installation directory (default: `/usr/local`) and [`-DCMAKE_C_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html)=〈*flags*〉 (and respectively, [`-DCMAKE_CXX_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html) and [`-DCMAKE_Fortran_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html)), which may be needed to point the compiler to the LLVM `include` directory, as in `-DCMAKE_C_FLAGS="-I/usr/include/llvm-8"`.

You may want to use CMake's graphical [`cmake-gui`](https://cmake.org/cmake/help/latest/manual/cmake-gui.1.html) or [curses](https://en.wikipedia.org/wiki/Curses_(programming_library))-based [`ccmake`](https://cmake.org/cmake/help/latest/manual/ccmake.1.html) front ends instead of `cmake` to configure Byfl and generate [`Makefile`](https://en.wikipedia.org/wiki/Makefile)s.  Enable advanced mode to see the complete list of user-configurable parameters.

Installation on Mac OS X
------------------------

A few extra steps are needed to build Byfl on OS X:

1. Install [Xcode](https://developer.apple.com/xcode/), which provides various standard tools, header files, and libraries.

2. Install the Xcode command-line tools with `xcode-select --install`.

3. To avoid having to manually specify long directory names in which to search for standard headers and libraries, install the `/Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg` package (or the corresponding package for your OS X version).

You'll also need to install CMake, LLVM, and Clang.  (The version of LLVM/Clang installed as part of Xcode lacks the CMake support files Byfl relies on.)  My preferred approach is to use the [Homebrew](http://brew.sh/) package manager:

4. Follow the instructions on http://brew.sh/ to install Homebrew.

5. Install CMake, LLVM, and Clang with `brew install cmake llvm`.  (The Homebrew `llvm` package includes Clang; there's not a separate package for it.)

Other information
-----------------

Previously, the Byfl repository provided a separate branch for each supported LLVM version due to significant API differences across even minor versions.  Because LLVM's APIs have more-or-less stabilized, the corresponding branches have since been removed and versions of LLVM prior to 6.0 are no longer supported.  The old Byfl branches were first snapshotted as the tags [`llvm-3.5-final`](https://github.com/lanl/Byfl/tree/llvm-3.5-final), [`llvm-3.6-final`](https://github.com/lanl/Byfl/tree/llvm-3.6-final), [`llvm-3.7-final`](https://github.com/lanl/Byfl/tree/llvm-3.7-final), [`llvm-3.8-final`](https://github.com/lanl/Byfl/tree/llvm-3.8-final), [`llvm-3.9-final`](https://github.com/lanl/Byfl/tree/llvm-3.9-final), [`llvm-4.0-final`](https://github.com/lanl/Byfl/tree/llvm-4.0-final), [`llvm-5.0-final`](https://github.com/lanl/Byfl/tree/llvm-5.0-final), [`llvm-6.0-final`](https://github.com/lanl/Byfl/tree/llvm-6.0-final), and [`llvm-7.0-final`](https://github.com/lanl/Byfl/tree/llvm-7.0-final) for posterity.
