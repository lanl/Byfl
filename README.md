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

If you have reasonably recent versions of [GCC](http://gcc.gnu.org/), [LLVM](http://www.llvm.org/), [Clang](http://clang.llvm.org/), and [DragonEgg](http://dragonegg.llvm.org/), you should be able to perform the usual

    ./configure
    make
    make install

procedure.  See [INSTALL.md](https://github.com/losalamos/Byfl/blob/master/INSTALL.md) for a more complete explanation.

Usage
-----

### Basic usage

Byfl comes with a set of wrapper scripts that simplify instrumentation.  `bf-gcc`, `bf-g++`, `bf-gfortran`, and `bf-gccgo` wrap, respectively, the GNU C, C++, Fortran, and Go compilers.  `bf-mpicc`, `bf-mpicxx`, `bf-mpif90`, and `bf-mpif77` further wrap the similarly named [Open MPI](http://www.open-mpi.org/) and [MPICH](http://www.mpich.org/) wrapper scripts to use the Byfl compiler scripts instead of the default C, C++, and Fortran compilers.  Use any of these scripts as you would the underlying compiler.  When you run your code, Byfl will output a sequence of `BYFL`-prefixed lines to the standard output device and a superset of the data to a binary file (called *filename*`.byfl` by default):

    BYFL_INFO: Byfl command line:
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                     1,280 bytes (512 loaded + 768 stored)
    BYFL_SUMMARY:                        96 flops
    BYFL_SUMMARY:                       549 integer ops
    BYFL_SUMMARY:                        67 memory ops (2 loads + 65 stores)
    BYFL_SUMMARY:                        35 branch ops (1 unconditional and direct + 32 conditional or indirect + 2 function calls or returns + 0 other)
    BYFL_SUMMARY:                       747 TOTAL OPS
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    10,240 bits (4,096 loaded + 6,144 stored)
    BYFL_SUMMARY:                    12,288 flop bits
    BYFL_SUMMARY:                    94,880 op bits (excluding memory ops)
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    0.6667 bytes loaded per byte stored
    BYFL_SUMMARY:                  373.0000 ops per load instruction
    BYFL_SUMMARY:                  152.8358 bits loaded/stored per memory op
    BYFL_SUMMARY:                    3.0000 flops per conditional/indirect branch
    BYFL_SUMMARY:                   23.3125 ops per conditional/indirect branch
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                   13.3333 bytes per flop
    BYFL_SUMMARY:                    0.8333 bits per flop bit
    BYFL_SUMMARY:                    1.7158 bytes per op
    BYFL_SUMMARY:                    0.1079 bits per (non-memory) op bit
    BYFL_SUMMARY: -----------------------------------------------------------------

"Bits" are simply bytes*8.  "Flop bits" are the total number of bits in all inputs and outputs to each floating-point function.  As motivation, consider the operation `A = B + C`, where `A`, `B`, and `C` reside in memory.  This operation consumes 12 bytes per flop if the arguments are all single-precision but 24 bytes per flop if the arguments are all double-precision.  Similarly, `A = -B` consumes either 8 or 16 bytes per flop based on the argument type.  However, all of these examples consume one bit per flop bit regardless of numerical precision: every bit loaded or stored either enters or exits the floating-point unit.  Bit:flop-bit ratios above 1.0 imply that more memory is moved than fed into the floating-point unit; Bit:flop-bit ratios below 1.0 imply register reuse.

The Byfl wrapper scripts accept a number of options to provide more information about your program at a cost of increased execution times.  These can be specified either on the command line or within the `BF_OPTS` environment variable.  (The former takes precedence.)  See the `bf-gcc`, `bf-g++`, `bf-gfortran`, or `bf-gccgo` man page for a description of all of the information Byfl can report.

The following represents some sample output from a code instrumented with Byfl and most of the available options:
    BYFL_INFO: Byfl command line: -bf-unique-bytes -bf-vectors -bf-every-bb -bf-mem-footprint -bf-types -bf-inst-mix -bf-by-func -bf-call-stack
    BYFL_FUNC_HEADER:             LD_bytes             ST_bytes               LD_ops               ST_ops                Flops              FP_bits              Int_ops          Int_op_bits           Uniq_bytes             Cond_brs          Invocations Function
    BYFL_FUNC:                         512                  768                    2                   65                   96                12288                  549                94880                  768                   32                    1 main
    BYFL_CALLEE_HEADER:   Invocations Byfl Function
    BYFL_CALLEE:                    1 No   __printf_chk
    BYFL_VECTOR_HEADER:             Elements             Elt_bits Type                Tally Function
    BYFL_VECTOR:                          32                   64 FP                      1 main
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                     1,280 bytes (512 loaded + 768 stored)
    BYFL_SUMMARY:                       768 unique bytes
    BYFL_SUMMARY:                       512 addresses cover 50% of all dynamic loads and stores
    BYFL_SUMMARY:                        96 flops
    BYFL_SUMMARY:                       549 integer ops
    BYFL_SUMMARY:                        67 memory ops (2 loads + 65 stores)
    BYFL_SUMMARY:                        35 branch ops (1 unconditional and direct + 32 conditional or indirect + 2 function calls or returns + 0 other)
    BYFL_SUMMARY:                       747 TOTAL OPS
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    10,240 bits (4,096 loaded + 6,144 stored)
    BYFL_SUMMARY:                     6,144 unique bits
    BYFL_SUMMARY:                    12,288 flop bits
    BYFL_SUMMARY:                    94,880 op bits (excluding memory ops)
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                         1 vector operations (FP & int)
    BYFL_SUMMARY:                   32.0000 elements per vector
    BYFL_SUMMARY:                   64.0000 bits per element
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                         2 loads of vectors of 64-bit floating-point values
    BYFL_SUMMARY:                        64 stores of 64-bit floating-point values
    BYFL_SUMMARY:                         1 stores of vectors of 64-bit floating-point values
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                        96 Add            instructions executed
    BYFL_SUMMARY:                        67 BitCast        instructions executed
    BYFL_SUMMARY:                        65 Store          instructions executed
    BYFL_SUMMARY:                        64 GetElementPtr  instructions executed
    BYFL_SUMMARY:                        64 Trunc          instructions executed
    BYFL_SUMMARY:                        64 SRem           instructions executed
    BYFL_SUMMARY:                        64 SIToFP         instructions executed
    BYFL_SUMMARY:                        64 Mul            instructions executed
    BYFL_SUMMARY:                        33 Br             instructions executed
    BYFL_SUMMARY:                        32 PHI            instructions executed
    BYFL_SUMMARY:                        32 ICmp           instructions executed
    BYFL_SUMMARY:                        32 Shl            instructions executed
    BYFL_SUMMARY:                         3 Alloca         instructions executed
    BYFL_SUMMARY:                         2 ExtractElement instructions executed
    BYFL_SUMMARY:                         2 Call           instructions executed
    BYFL_SUMMARY:                         2 Load           instructions executed
    BYFL_SUMMARY:                         1 Ret            instructions executed
    BYFL_SUMMARY:                         1 FMul           instructions executed
    BYFL_SUMMARY:                       688 TOTAL          instructions executed
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                       512 bytes cover  80.0% of memory accesses
    BYFL_SUMMARY:                       768 bytes cover 100.0% of memory accesses
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    0.6667 bytes loaded per byte stored
    BYFL_SUMMARY:                  373.0000 ops per load instruction
    BYFL_SUMMARY:                  152.8358 bits loaded/stored per memory op
    BYFL_SUMMARY:                    3.0000 flops per conditional/indirect branch
    BYFL_SUMMARY:                   23.3125 ops per conditional/indirect branch
    BYFL_SUMMARY:                    0.0312 vector ops (FP & int) per conditional/indirect branch
    BYFL_SUMMARY:                    0.0104 vector ops (FP & int) per flop
    BYFL_SUMMARY:                    0.0013 vector ops (FP & int) per op
    BYFL_SUMMARY:                    1.0843 ops per instruction
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                   13.3333 bytes per flop
    BYFL_SUMMARY:                    0.8333 bits per flop bit
    BYFL_SUMMARY:                    1.7158 bytes per op
    BYFL_SUMMARY:                    0.1079 bits per (non-memory) op bit
    BYFL_SUMMARY: -----------------------------------------------------------------
    BYFL_SUMMARY:                    8.0000 unique bytes per flop
    BYFL_SUMMARY:                    0.5000 unique bits per flop bit
    BYFL_SUMMARY:                    1.0295 unique bytes per op
    BYFL_SUMMARY:                    0.0648 unique bits per (non-memory) op bit
    BYFL_SUMMARY:                    1.6667 bytes per unique byte
    BYFL_SUMMARY: -----------------------------------------------------------------

### Advanced usage

Under the covers, the Byfl wrapper scripts are using GCC to compile code to GCC IR and DragonEgg to convert GCC IR to LLVM IR, which is output in LLVM bitcode format.  The wrapper scripts then run LLVM's `opt` command, specifying Byfl's `bytesflops` plugin as an additional compiler pass.  The resulting instrumented bitcode is then converted to native machine code using Clang.  The following is an example of how to manually instrument `myprog.c` without using the wrapper scripts:

    $ gcc -g -fplugin=/usr/local/lib/dragonegg.so -fplugin-arg-dragonegg-emit-ir -O3 -Wall -Wextra -S myprog.c
    $ opt -O3 myprog.s -o myprog.opt.bc
    $ opt -load /usr/local/lib/bytesflops.so -bytesflops -bf-unique-bytes -bf-by-func -bf-call-stack -bf-vectors -bf-every-bb myprog.opt.bc -o myprog.inst.bc
    $ opt -O3 myprog.inst.bc -o myprog.bc
    $ clang myprog.bc -o myprog -L/usr/lib/gcc/x86_64-linux-gnu/4.7/ -L/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../x86_64-linux-gnu/ -L/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../lib/ -L/lib/x86_64-linux-gnu/ -L/lib/../lib/ -L/usr/lib/x86_64-linux-gnu/ -L/usr/lib/../lib/ -L/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../ -L/lib/ -L/usr/lib/ -L/usr/local/lib -Wl,--allow-multiple-definition -lm /usr/local/lib/libbyfl.bc -lstdc++ -lm

The `bf-inst` script makes these steps slightly simpler:

    $ gcc -g -fplugin=/usr/local/lib/dragonegg.so -fplugin-arg-dragonegg-emit-ir -O3 -Wall -Wextra -S myprog.c
    $ opt -O3 myprog.s -o myprog.bc
    $ bf-inst -bf-unique-bytes -bf-by-func -bf-call-stack -bf-vectors -bf-every-bb myprog.bc
    $ clang myprog.bc -o myprog -L/usr/lib/gcc/x86_64-linux-gnu/4.7/ -L/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../x86_64-linux-gnu/ -L/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../lib/ -L/lib/x86_64-linux-gnu/ -L/lib/../lib/ -L/usr/lib/x86_64-linux-gnu/ -L/usr/lib/../lib/ -L/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../ -L/lib/ -L/usr/lib/ -L/usr/local/lib -Wl,--allow-multiple-definition /usr/local/lib/libbyfl.bc -lstdc++ -lm

If GCC and DragonEgg are not required, Byfl instrumentation is even easier to apply manually:

    $ clang -O3 -c -emit-llvm myprog.c -o myprog.bc
    $ bf-inst -bf-unique-bytes -bf-by-func -bf-call-stack -bf-vectors -bf-every-bb myprog.bc
    $ clang myprog-bc -o myprog /usr/local/lib/libbyfl.bc -lstdc++ -lm

This basic approach can be useful for instrumenting code in languages other than C, C++, and Fortran.  For example, code compiled with any of the other [GCC frontends](http://gcc.gnu.org/frontends.html) can be instrumented as above.  Also, recent versions of the [Glasgow Haskell Compiler](http://www.haskell.org/ghc/) can compile directly to LLVM bitcode, although Byfl has not yet been successfully applied to GHC-generated code.

### Postprocessing Byfl output

Byfl provides a set of programs for converting Byfl binary output (`*.byfl` files) into various other formats.  Currently, these include the following:

<dl>
  <dt><code>bfbin2csv</code></dt>
  <dd>Output Byfl data in comma-separated value (CSV) format, suitable for processing with a scripting language (e.g., <a href="http://en.wikipedia.org/wiki/AWK">AWK</a> or <a href="https://www.perl.org/">Perl</a>)</dd>

  <dt><code>bfbin2xmlss</code></dt>
  <dd>Output Byfl data in XML Spreadsheet format, suitable for processing with many spreadsheet programs, including <a href="http://www.libreoffice.org/">LibreOffice</a>, <a href="http://products.office.com/en-us/excel">Microsoft Excel</a>, and <a href="https://www.apple.com/mac/numbers/">Numbers for Mac</a></dd>

  <dt><code>bfbin2sqlite3</code></dt>
  <dd>Output Byfl data as a <a href="http://www.sqlite.org/">SQLite3</a> database, suitable for processing with SQLite command-line tools or importing into other database management systems</dd>

  <dt><code>bfbin2hdf5</code></dt>
  <dd>Output Byfl data as a Hierarchical Data Format (<a href="http://www.hdfgroup.org/HDF5/">HDF5</a>) file for longer-term storage and processing (e.g.,&nbsp;with <a href="http://www.hdfgroup.org/products/java/hdfview/">HDFView</a>)</dd>
</dl>

Developers can write additional conversion tools using a simple API (installed as `bfbin.h` and `libbfbin.a`) that parses `.byfl` files into tables and invokes program-specified callback functions for each table component.

In addition to the above, Byfl installs two scripts to convert Byfl textual output (lines beginning with `BYFL`) into formats readable by various GUIs.  `bf2cgrind` converts Byfl output into [KCachegrind](http://kcachegrind.sourceforge.net/) input, and `bf2hpctk` converts Byfl output into [HPCToolkit](http://www.hpctoolkit.org/) input.  (The latter program is more robust and appears to be more actively maintained.)  Run each of those scripts with no arguments to see the usage text.  These scripts will eventually be replaced by programs based on the Byfl parsing API.

Copyright and license
---------------------

Los Alamos National Security, LLC (LANS) owns the copyright to Byfl, which it identifies internally as LA-CC-12-039.  The license is BSD-ish with a "modifications must be indicated" clause.  See [LICENSE.md](https://github.com/losalamos/Byfl/blob/master/LICENSE.md) for the full text.

Authors
-------

Scott Pakin, [_pakin@lanl.gov_](mailto:pakin@lanl.gov)<br />
Pat McCormick, [_pat@lanl.gov_](mailto:pat@lanl.gov)<br />
Eric Anger, [_eanger@gmail.com_](mailto:eanger@gmail.com)<br />
Christine Sweeney, [_cahrens@lanl.gov_](mailto:cahrens@lanl.gov)
