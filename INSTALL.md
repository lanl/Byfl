Byfl installation
=================

Byfl relies on [LLVM](http://www.llvm.org/) and [Clang](http://clang.llvm.org/) and can take advantage of [DragonEgg](http://dragonegg.llvm.org/).  The `llvm-3.5` branch of Byfl is designed to work with LLVM/Clang/DragonEgg release 3.5.  The `llvm-3.6` branch of Byfl is designed to work with LLVM/Clang release 3.6.  The `master` branch of Byfl is designed to work with LLVM/Clang trunk (i.e., the post-3.6-release development code).

Normal installation
-------------------

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

from the top-level Byfl directory.

Run `../configure --help` for usage information.  The [FSF's generic installation instructions](http://git.savannah.gnu.org/cgit/automake.git/tree/INSTALL) provide substantially more detail on customizing the configuration.

Note that DragonEgg requires [GCC](http://gcc.gnu.org/) versions 4.5-4.8 and LLVM/Clang 3.5.

Automatically installing LLVM/Clang/DragonEgg trunk plus Byfl
-------------------------------------------------------------

For convenience on systems that lack a LLVM/Clang/DragonEgg 3.5+ installation, Byfl's [`build-llvm-byfl`](https://github.com/losalamos/Byfl/blob/master/build-llvm-byfl) script automatically downloads LLVM, Clang, DragonEgg, and Byfl, configures them, builds them, and installs the result into a directory you specify.  The script takes one required argument, which is the root of the installation directory (e.g., `/usr/local` or `/opt/byfl` or whatnot).  The following optional arguments can appear before the required argument:

<dl>
  <dt><code>-b</code> <em>build_dir</em></dt>
  <dd>Build Byfl and its dependencies in directory <em>build_dir</em>.<br/>
      <em>Default:</em> <code>./byfl-build.</code><em>random</em><code>/</code></dd>

  <dt><code>-j</code> <em>parallelism</em></dt>
  <dd>Specify the maximum number of processes to use for compilation, passed directly to <code>make -j</code>.<br/>
      <em>Default:</em> number of entries in <code>/proc/cpuinfo</code></dd>

  <dt><code>-d</code></dt>
  <dd>Download Byfl and its dependencies into <em>build_dir</em>, but don't configure, build, or install them.<br/>
      <em>Default:</em> off</dd>

  <dt><code>-c</code></dt>
  <dd>Configure, build, and install Byfl and its dependencies without re-downloading them into <em>build_dir</em>.<br/>
      <em>Default:</em> off</dd>

  <dt><code>-C</code></dt>
  <dd>Configure, build, and install Byfl and its dependencies without re-downloading them into <em>build_dir</em> and without re-running either the LLVM or the Byfl <code>configure</code> scripts.<br/>
      <em>Default:</em> off</dd>

  <dt><code>-D</code> <em>dir_prefix</em></dt>
  <dd>Prefix the installation directory with <em>dir_prefix</em> when building and installing.  The idea is for the script to install into a temporary location with the intention that a user will manually move the files into the final installation directory.  This is sometimes called a <a href="https://www.gnu.org/prep/standards/html_node/DESTDIR.html"><em>staged install</em></a>.<br/>
      <em>Default:</em> <code>""</code></dd>

  <dt><code>-t</code></dt>
  <dd>Display progress textually instead of with a GUI progress bar (<a href="https://help.gnome.org/users/zenity/stable/">Zenity</a>).<br/>
      <em>Default:</em> GUI display if available, textual display otherwise</dd>

  <dt><code>-x</code> <code>llvm</code>|<code>dragonegg</code>|<code>byfl</code></dt>
  <dd>Exclude one of LLVM+Clang, DragonEgg, or Byfl from downloading/compiling.  For example, specify <code>-x dragonegg</code> on platforms on which DragonEgg is known not to build.  Or specify something to exclude from compilation if it requires manual configuration to work on some platform.  This option may be specified multiple times.<br/>
      <em>Default:</em> nothing is excluded</dd>
</dl>

Installation on Mac OS X
------------------------

A few extra steps are needed to build Byfl on OS X:

1. Install [Xcode](https://developer.apple.com/xcode/), which provides various standard tools, header files, and libraries.

2. Install [Homebrew](http://brew.sh/), which is needed to further download various packages that OS X doesn't provide by default.

3. Use Homebrew to install Byfl:
```bash
brew install https://github.com/losalamos/Byfl/releases/download/v1.3-llvm-3.6.2/byfl13.rb
```

The preceding procedure installs Byfl 1.3 from the `llvm-3.6` branch of Byfl.  If you instead prefer to install a newer, pre-release version of Byfl (still from the `llvm-3.6` branch), you can use Homebrew to install Byfl's dependencies but download Byfl itself from GitHub.

1. Use Homebrew to install the [GNU Autotools](https://en.wikipedia.org/wiki/GNU_build_system):
```bash
brew install autoconf
brew install automake
brew install libtool
```

2. Use Homebrew to install [LLVM](http://www.llvm.org/) 3.6:
```bash
brew install llvm36
```

3. For a normal Byfl installation (see above) you'll need to point Byfl's `../configure` to the Homebrew-versioned `llvm-config` file:
```bash
../configure LLVM_CONFIG=llvm-config-3.6
```

4. For an automatic installation with the Byfl build script (see above) you'll need a newer version of Bash than what OS X currently provides.  You'll also need to point the build script to Xcode's `/usr/include` directory, as in the following:
```bash
brew install bash
env CPATH=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk/usr/include bash ./build-llvm-byfl ~/byfl
```

Once you've built and installed Byfl, you'll probably need to set the following environment variables to point Byfl's [compiler wrapper scripts](https://github.com/losalamos/Byfl/wiki) to the version of Clang that Byfl was built against:
```bash
export BF_CLANGXX=clang++-3.6
export BF_CLANG=clang-3.6
```
