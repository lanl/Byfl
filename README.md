Byfl: Compiler-based Application Analysis
=========================================

Description
-----------

Byfl helps application developers understand code performance in a _hardware-independent_ way.  The idea is that it instruments your code at compile time then gathers and reports data at run time.  For example, suppose you wanted to know how many bytes are accessed by the following C code:

    double array[100000][100];
    volatile double sum = 0.0;

    for (int row=0; row<100000; row++)
      sum += array[row][0];

Reading the hardware performance counters (e.g., using [PAPI](http://icl.cs.utk.edu/papi/)) can be misleading.  The performance counters on most processors tally not the number of bytes but rather the number of cache-line accesses.  Because the array is stored in row-major order, each access to `array` will presumably reference a different cache line while each access to `sum` will presumably reference the same cache line.

Byfl does the equivalent of transforming the code into the following:

    unsigned long int bytes accessed = 0;
    double array[100000][100];
    volatile double sum = 0.0;

    for (int row=0; row<100000; row++) {
      sum += array[row][0];
      bytes_accessed += 3*sizeof(double);
    }

In the above, one can consider the `bytes_accessed` variable as a "software performance counter," as it is maintained entirely by software.

In practice, however, Byfl doesn't do source-to-source transformations (unlike, for example, [ROSE](http://www.rosecompiler.org/)) as implied by the preceding code sample.  Instead, it integrates into the [LLVM compiler infrastructure](http://www.llvm.org/) as an LLVM compiler pass.  If your application compiles with [LLVM's Clang](http://clang.llvm.org/) or any of the [GNU compilers](http://gcc.gnu.org/), you can instrument it with Byfl.

Because Byfl instruments code in LLVM's intermediate representation (IR), not native machine code, it outputs the same counter values regardless of target architecture.  In contrast, binary-instrumentation tools such as [Pin](http://www.pintool.org/) may tally operations differently on different platforms.

The name "Byfl" comes from "bytes/flops".  The very first version of the code counted only bytes and floating-point operations (flops).


Installation
------------

### Automatic installation

Byfl relies on [LLVM](http://www.llvm.org/), [Clang](http://clang.llvm.org/), and [DragonEgg](http://dragonegg.llvm.org/).  These are huge and must be built from trunk (i.e., the post-3.1-release development code).  My [`build-llvm-byfl`](https://github.com/downloads/losalamos/Byfl/build-llvm-byfl) script automatically downloads all of these plus Byfl into a temporary directory, configures them, builds them, and installs the result into a directory you specify.

Byfl also relies on [GCC](http://gcc.gnu.org/).  You should already have GCC installed and in your path before running [`build-llvm-byfl`](https://github.com/downloads/losalamos/Byfl/build-llvm-byfl).  The LLVM guys currently seem to do most of their testing with GCC 4.6 so that's your best bet for having everything work.


### Manual installation

Manual installation is good if you periodically want to update Byfl and its prerequisites without doing having to re-download everything every time.  Byfl depends on LLVM (the compiler infrastructure), Clang (an LLVM-based C/C++ compiler), and DragonEgg (a technically optional but strongly recommended tool for using GCC compilers as LLVM front ends).  See the following URLs for instructions on building each of these:

* **LLVM:** [http://llvm.org/docs/GettingStarted.html#checkout](http://llvm.org/docs/GettingStarted.html#checkout)

* **Clang:** [http://clang.llvm.org/get_started.html](http://clang.llvm.org/get_started.html)

* **DragonEgg:** [http://dragonegg.llvm.org/](http://dragonegg.llvm.org/)

I use the following `configure` line in my top-level LLVM directory:

    ./configure --enable-optimized --enable-debug-runtime --enable-debug-symbols --disable-assertions CC=gcc CXX=g++ REQUIRES_RTTI=1

Run `make` to build LLVM and Clang and `make install` to install them.  Then, with the LLVM `bin` directory in your path, run `make` in the DragonEgg directory.  Copy `dragonegg.so` to your LLVM `lib` directory.

The following steps can then be used to build and install Byfl:

    cd autoconf
    yes $HOME/llvm | ./AutoRegen.sh
    mkdir ../build
    cd ../build
    ../configure --disable-assertions --enable-optimized --enable-debug-runtime --enable-debug-symbols DRAGONEGG=/usr/local/lib/dragonegg.so CXX=g++ CXXFLAGS="-g -O2 -std=c++0x" --with-llvmsrc=$HOME/llvm --with-llvmobj=$HOME/llvm
    make
    make install

The `$HOME/llvm` lines in the above refer to your LLVM _source_ (not installation) directory.  Also, be sure to adjust the location of `dragonegg.so` as appropriate.


Usage
-----

### Basic usage

Byfl comes with a set of wrapper scripts that simplify instrumentation.  `bf-gcc`, `bf-g++`, and `bf-gfortran` wrap, respectively, the GNU C, C++, and Fortran compilers.  Use them as you would the underlying compiler.  When you run your code, Byfl will output a sequence of `BYFL`-prefixed lines to the standard output device:

    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                      1280 bytes (512 loaded + 768 stored)
    BYFL_SUMMARY:                        32 flops
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                     10240 bits (4096 loaded + 6144 stored)
    BYFL_SUMMARY:                      6144 flop bits
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    0.6667 bytes loaded per byte stored
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                   40.0000 bytes per flop
    BYFL_SUMMARY:                    1.6667 bits per flop bit
    BYFL_SUMMARY: -----------------------------------------------------------------

"Bits" are simply bytes*8.  "Flop bits" are the total number of bits in all inputs and outputs to each floating-point function.  As motivation, consider the operation `A = B + C`, where `A`, `B`, and `C` reside in memory.  This operation consumes 12 bytes per flop if the arguments are all single-precision but 24 bytes per flop if the arguments are all double-precision.  Similarly, `A = -B` consumes either 8 or 16 bytes per flop based on the argument type.  However, all of these examples consume one bit per flop bit regardless of numerical precision: every bit loaded or stored either enters or exits the floating-point unit.  Bit:flop-bit ratios above 1.0 imply that more memory is moved than fed into the floating-point unit; Bit:flop-bit ratios below 1.0 imply register reuse.

The Byfl wrapper scripts accept a number of options to provide more information about your program at a cost of increased execution times.  The following can be specified either on the command line or within the `BF_OPTS` environment variable.  (The former takes precedence.)

<dl>
<dt><code>-bf-all-ops</code></dt>
<dd>Tally <em>all</em> ALU operations, not just floating-point operations.</dd>

<dl>
<dt><code>-bf-types</code></dt>
<dd>Tally <em>type-specific</em> loads and stores of register friendly types.  The current set of included types are: single- and double-precision floating point values, 8-,16-, 32- and 64- integer values, and pointers. Remaining types will be categorized as <em>other types</em>.  Note that this flag will enable the <code>-bf-all-ops</code> option if not supplied on the command line.</dd>

<dt><code>-bf-every-bb</code></dt>
<dd>Output counters for every <a href="http://en.wikipedia.org/wiki/Basic_block">basic block</a> executed.

<dt><code>-bf-merge-bb=</code><i>number</i></dt>
<dd>When used with <code>-bf-every-bb</code>, merge every <i>number</i> basic-block readings into a single line of output.  (I typically specify <code>-bf-merge-bb=1000000</code>.)

<dt><code>-bf-by-func</code></dt>
<dd>Output counters for every function executed.</dd>

<dt><code>-bf-call-stack</code></dt>
<dd>When used with <code>-bf-by-func</code>, distinguish functions by call path.  That is, if function <code>f</code> calls functions <code>g</code> and <code>h</code>, <code>-bf-by-func</code> by itself will output counts for each of the three functions while including <code>-bf-call-stack</code> will output counts for the two call stacks <code>f</code>&rarr;<code>g</code> and <code>f</code>&rarr;<code>h</code>.</dd>

<dt><code>-bf-include=</code><i>function1</i>[,<i>function2</i>,&hellip;]</dt>
<dd>Instrument only the named functions.</dd>

<dt><code>-bf-exclude=</code><i>function1</i>[,<i>function2</i>,&hellip;]</dt>
<dd>Instrument all but the named functions.</dd>

<dt><code>-bf-thread-safe</code></dt>
<dd>Indicate that the application is multithreaded (e.g., with <a href="http://en.wikipedia.org/wiki/POSIX_Threads">Pthreads</a> or <a href="http://www.openmp.org/">OpenMP</a>) so Byfl should protect all counter updates.</dd>

<dt><code>-bf-unique-bytes</code></dt>
<dd>Keep track of <em>unique</em> memory locations accessed.  For example, if a program accesses 8 bytes at address <code>A</code>, then at <code>B</code>, thenat <code>A</code> again, Byfl will report this as 24 bytes but only 16 unique bytes.</dd>

<dt><code>-bf-vectors</code></dt>
<dd>Report statistics on vector operations (element sizes and number of elements).  Unfortunately, at the time of this writing (July 2012), LLVM's autovectorizer is extremely limited and is unable to manipulate arbitrary-length vectors&mdash;even though the IR supports them.</dd>
</dl>

The following represents some sample output from a code instrumented with Byfl and most of the preceding options:

    BYFL_BB_HEADER:             Bytes_LD             Bytes_ST               Ops_LD               Ops_ST                Flops              FP_bits              Ops_ALU          Op_ALU_bits
    BYFL_BB:                           0                    0                    0                    0                    0                    0                    0                    0
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                           0                   16                    0                    2                    0                    0                   20                 2065
    BYFL_BB:                         512                  256                    2                    1                   32                 6144                   35                 6336
    BYFL_FUNC_HEADER:             Bytes_LD             Bytes_ST               Ops_LD               Ops_ST                Flops              FP_bits              Ops_ALU          Op_ALU_bits           Uniq_bytes             Cond_brs          Invocations Function
    BYFL_FUNC:                         512                  768                    2                   65                   32                 6144                  675                72416                  768                   32                    1 main
    BYFL_CALLEE_HEADER:   Invocations Byfl Function
    BYFL_CALLEE:                    3 No   llvm.lifetime.end
    BYFL_CALLEE:                    3 No   llvm.lifetime.start
    BYFL_CALLEE:                    1 No   __printf_chk
    BYFL_VECTOR_HEADER:             Elements             Elt_bits Type                Tally Function
    BYFL_VECTOR:                          32                   64 FP                      1 main
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                        34 basic blocks
    BYFL_SUMMARY:                        32 conditional or indirect branches
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                      1280 bytes (512 loaded + 768 stored)
    BYFL_SUMMARY:                       768 unique bytes
    BYFL_SUMMARY:                        32 flops
    BYFL_SUMMARY:                       675 ALU ops
    BYFL_SUMMARY:                        67 memory ops (2 loads + 65 stores)
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                     10240 bits (4096 loaded + 6144 stored)
    BYFL_SUMMARY:                      6144 unique bits
    BYFL_SUMMARY:                      6144 flop bits
    BYFL_SUMMARY:                     72416 ALU op bits
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                         1 vector operations
    BYFL_SUMMARY:                   32.0000 elements per vector
    BYFL_SUMMARY:                   64.0000 bits per elements
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    0.6667 bytes loaded per byte stored
    BYFL_SUMMARY:                  337.5000 ALU ops per load instruction
    BYFL_SUMMARY:                  152.8358 bits loaded/stored per memory op
    BYFL_SUMMARY:                    1.0000 flops per conditional/indirect branch
    BYFL_SUMMARY:                   21.0938 ops per conditional/indirect branch
    BYFL_SUMMARY:                    0.0312 vector ops per conditional/indirect branch
    BYFL_SUMMARY:                    0.0312 vector operations (FP & int) per flop
    BYFL_SUMMARY:                    0.0015 vector operations per ALU op
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                   40.0000 bytes per flop
    BYFL_SUMMARY:                    1.6667 bits per flop bit
    BYFL_SUMMARY:                    1.8963 bytes per ALU op
    BYFL_SUMMARY:                    0.1414 bits per ALU op bit
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                   24.0000 unique bytes per flop
    BYFL_SUMMARY:                    1.0000 unique bits per flop bit
    BYFL_SUMMARY:                    1.1378 unique bytes per ALU op
    BYFL_SUMMARY:                    0.0848 unique bits per ALU op bit
    BYFL_SUMMARY:                    1.6667 bytes per unique byte
    BYFL_SUMMARY: -----------------------------------------------------------------

The Byfl options listed above are accepted directly by the Byfl compiler pass.  In addition, the Byfl wrapper scripts (but not the compiler pass) accept the following options:

<dl>
<dt><code>-bf-verbose</code></dt>
<dd>Output all helper commands executed by the wrapper script.</dd>

<dt><code>-bf-static</code></dt>
<dd>Instead of instrumenting the code, merely output counts of number of instructions of various types.</dd>
</dl>

Finally, as a useful convenience for performance comparisons and troubleshooting, the Byfl wrapper scripts look for a `BF_DISABLE` environment variable.  When set, this disables various pieces of the instrumentation process.  Hence, you can configure your application once to use `bf-gcc`, `bf-g++`, and/or `bf-gfortran`, and can revert to stock `gcc`, `g++`, and/or `gfortran` simply by altering the environment variable.  The following are acceptable values for `BF_DISABLE`:

<dl>
<dt><code>none</code> (default)</dt>
<dd>Don't disable anything; run with regular Byfl instrumentation.</dd>

<dt><code>byfl</code></dt>
<dd>Disable the Byfl compiler pass, but retain all of the internal manipulation of LLVM file types (i.e., bitcode).</dd>

<dt><code>bitcode</code></dt>
<dd>Process the code with LLVM and DragonEgg but disable LLVM bitcode
and use exclusively native object files.</dd>

<dt><code>dragonegg</code></dt>
<dd>Use a GNU compiler directly, disabling all wrapper-script functionality.</dd>
</dl>

### Advanced usage

Under the covers, the Byfl wrapper scripts are using GCC to compile code to GCC IR and DragonEgg to convert GCC IR to LLVM IR, which is output in LLVM bitcode format.  The wrapper scripts then run LLVM's `opt` command, specifying Byfl's `bytesflops` plugin as an additional compiler pass.  The resulting instrumented bitcode is then converted to native machine code using Clang.  The following is an example of how to manually instrument `myprog.c` without using the wrapper scripts:

    gcc -g -fplugin=/usr/local/lib/dragonegg.so -fplugin-arg-dragonegg-emit-ir -O3 -S myprog.c
    opt -load /usr/local/lib/bytesflops.so -bytesflops -bf-all-ops -bf-unique-bytes -bf-by-func -bf-call-stack -bf-vectors -bf-every-bb -std-compile-opts myprog.s -o myprog.bc
    clang myprog.bc -o myprog -L/usr/lib/x86_64-linux-gnu/gcc/x86_64-linux-gnu/4.5.2/ -L/usr/lib/x86_64-linux-gnu/gcc/x86_64-linux-gnu/4.5.2/../../../ -L/lib/ -L/usr/lib/ -L/usr/lib/x86_64-linux-gnu/ -L/usr/local/lib -L/usr/local/lib -Wl,--allow-multiple-definition -lm /usr/local/lib/libbyfl.bc -lstdc++ -lstdc++ -lm

This approach can be useful for instrumenting code in languages other than C, C++, and Fortran.  For example, code compiled with any of the other [GCC frontends](http://gcc.gnu.org/frontends.html) can be instrumented as above.  Also, recent versions of the [Glasgow Haskell Compiler](http://www.haskell.org/ghc/) can compile directly to LLVM bitcode.

### Postprocessing Byfl output

Byfl installs two scripts to convert Byfl output (lines beginning with `BYFL`) into formats readable by various GUIs.  `bf2cgrind` converts Byfl output into [KCachegrind](http://kcachegrind.sourceforge.net/) input, and `bf2hpctk` converts Byfl output into [HPCToolkit](http://www.hpctoolkit.org/) input.  (The latter program is more robust and appears to be more actively maintained.)  Run each of those scripts with no arguments to see the usage text.


License
-------

Los Alamos National Security, LLC (LANS) owns the copyright to Byfl, which it identifies internally as LA-CC-12-039.  The license is BSD-ish with a "modifications must be indicated" clause.  See [LICENSE.md](https://github.com/losalamos/Byfl/blob/master/LICENSE.md) for the full text.


Authors
-------
<dl>
<dd>Scott Pakin, [_pakin@lanl.gov_](mailto:pakin@lanl.gov)</dd>
<dd>Pat McCormick, [_pat@lanl.gov_](mailto:pat@lanl.gov)</dd>
</dl>
