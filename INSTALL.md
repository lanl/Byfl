Byfl installation
=================

Byfl relies on [LLVM](http://www.llvm.org/), [Clang](http://clang.llvm.org/), and [DragonEgg](http://dragonegg.llvm.org/).  The `llvm-3.5` branch of Byfl has been tested with LLVM/Clang/DragonEgg release 3.5.  The `master` branch of Byfl is tested with LLVM/Clang/DragonEgg trunk (i.e., the post-3.5-release development code).

Normal installation
-------------------

As long as LLVM's `llvm-config` program is in your path, the Free Software Foundation's canonical installation procedure should work:

    ./configure
    make
    make install

Run `./configure --help` for usage information.  The [FSF's generic installation instructions](http://git.savannah.gnu.org/cgit/automake.git/tree/INSTALL) provide substantially more detail on customizing the configuration.

Note that DragonEgg requires [GCC](http://gcc.gnu.org/) version 4.5 or newer.  (Plugin support was introduced with version 4.5.)  Technically, Byfl can build without DragonEgg support, but all of the Byfl wrapper scripts will fail to run; Byfl instrumentation will have to be applied manually, which is inconvenient.

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
