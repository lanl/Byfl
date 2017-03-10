Byfl installation
=================

Byfl relies on [LLVM](http://www.llvm.org/) and [Clang](http://clang.llvm.org/) and can take advantage of [DragonEgg](http://dragonegg.llvm.org/).  The `llvm-3.5` branch of Byfl is designed to work with LLVM/Clang/DragonEgg release 3.5.*x*.  The `llvm-3.6` branch of Byfl is designed to work with LLVM/Clang release 3.6.*x*.  The `llvm-3.7` branch of Byfl is designed to work with LLVM/Clang release 3.7.*x*.  The `llvm-3.8` branch of Byfl is designed to work with LLVM/Clang release 3.8.*x*.  The `master` branch of Byfl is designed to work with LLVM/Clang trunk (i.e., the post-3.8-release development code).

Basic installation
------------------

As long as LLVM's `llvm-config` program is in your path, the Free Software Foundation's canonical installation procedure for an out-of-source build should work:
```bash
mkdir build
cd build
../configure
make
make install
```

Note that `../configure` is included in Byfl releases but does not exist in the Git repository.  To create it, run
```bash
autoreconf -f -i
```

from the top-level Byfl directory.  It's been reported (in [issue #17](https://github.com/lanl/Byfl/issues/17)) that on some systems, the `autoreconf` line must be preceded by
```bash
libtoolize
```

Run `../configure --help` for usage information.  The [FSF's generic installation instructions](http://git.savannah.gnu.org/cgit/automake.git/tree/INSTALL) provide substantially more detail on customizing the configuration.

Note that DragonEgg requires [GCC](http://gcc.gnu.org/) versions 4.5-4.8 and LLVM/Clang 3.5.

Installation on Mac OS X
------------------------

A few extra steps are needed to build Byfl on OS X:

1. Install [Xcode](https://developer.apple.com/xcode/), which provides various standard tools, header files, and libraries.

2. Install [Homebrew](http://brew.sh/), which is needed to further download various packages that OS X doesn't provide by default.

3. Use Homebrew to install Byfl:
```bash
brew install https://github.com/losalamos/Byfl/releases/download/v1.5-llvm-3.8.0/byfl15.rb
```

The preceding procedure installs Byfl 1.5 from the `llvm-3.8` branch of Byfl.  If you instead prefer to install a newer, pre-release version of Byfl (still from the `llvm-3.8` branch), you can use Homebrew to install Byfl's dependencies but download Byfl itself from GitHub.

1. Use Homebrew to install the [GNU Autotools](https://en.wikipedia.org/wiki/GNU_build_system):
```bash
brew install autoconf
brew install automake
brew install libtool
```

2. Use Homebrew to install [LLVM](http://www.llvm.org/) 3.8:
```bash
brew install llvm38
```

3. For a basic Byfl installation (see above) you'll need to point Byfl's `../configure` to the Homebrew-versioned `llvm-config` file:
```bash
../configure LLVM_CONFIG=llvm-config-3.8
```

Once you've built and installed Byfl, you'll probably need to set the following environment variables to point Byfl's [compiler wrapper scripts](https://github.com/losalamos/Byfl/wiki) to the version of Clang that Byfl was built against:
```bash
export BF_CLANGXX=clang++-3.8
export BF_CLANG=clang-3.8
```
