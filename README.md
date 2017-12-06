Byfl: Compiler-based Application Analysis
=========================================

Description
-----------

Byfl helps application developers understand code performance in a _hardware-independent_ way.  The idea is that it instruments your code at compile time then gathers and reports data at run time.  For example, suppose you wanted to know how many bytes are accessed by the following C code:
```C
double array[100000][100];
volatile double sum = 0.0;

for (int row=0; row<100000; row++)
  sum += array[row][0];
```

Reading the hardware performance counters (e.g., using [PAPI](http://icl.cs.utk.edu/papi/)) can be misleading.  The performance counters on most processors tally not the number of bytes but rather the number of cache-line accesses.  Because the array is stored in row-major order, each access to `array` will presumably reference a different cache line while each access to `sum` will presumably reference the same cache line.

Byfl does the equivalent of transforming the code into the following:
```C
unsigned long int bytes accessed = 0;
double array[100000][100];
volatile double sum = 0.0;

for (int row=0; row<100000; row++) {
  sum += array[row][0];
  bytes_accessed += 3*sizeof(double);
}
```

In the above, one can consider the `bytes_accessed` variable as a "software performance counter," as it is maintained entirely by software.

In practice, however, Byfl doesn't do source-to-source transformations (unlike, for example, [ROSE](http://www.rosecompiler.org/)) as implied by the preceding code sample.  Instead, it integrates into the [LLVM compiler infrastructure](http://www.llvm.org/) as an LLVM compiler pass.  Because Byfl instruments code in LLVM's intermediate representation (IR), not native machine code, it outputs the same counter values regardless of target architecture.  In contrast, binary-instrumentation tools such as [Pin](https://software.intel.com/en-us/articles/pin-a-dynamic-binary-instrumentation-tool) may tally operations differently on different platforms.

The name "Byfl" comes from "bytes/flops".  The very first version of the code counted only bytes and floating-point operations (flops).

Installation
------------

There are eight "release" branches of Byfl on GitHub:

  * [`master`](https://github.com/lanl/Byfl), corresponding to LLVM trunk (often not up to date; best to avoid)
  * [`llvm-5.0`](https://github.com/lanl/Byfl/tree/llvm-5.0), corresponding to LLVM 5.0._x_ releases
  * [`llvm-4.0`](https://github.com/lanl/Byfl/tree/llvm-4.0), corresponding to LLVM 4.0._x_ releases
  * [`llvm-3.9`](https://github.com/lanl/Byfl/tree/llvm-3.9), corresponding to LLVM 3.9._x_ releases
  * [`llvm-3.8`](https://github.com/lanl/Byfl/tree/llvm-3.8), corresponding to LLVM 3.8._x_ releases
  * [`llvm-3.7`](https://github.com/lanl/Byfl/tree/llvm-3.7), corresponding to LLVM 3.7._x_ releases
  * [`llvm-3.6`](https://github.com/lanl/Byfl/tree/llvm-3.6), corresponding to LLVM 3.6._x_ releases
  * [`llvm-3.5`](https://github.com/lanl/Byfl/tree/llvm-3.5), corresponding to LLVM 3.5._x_ releases

Be sure to download the Byfl branch that corresponds to your installed LLVM version.  (Run `llvm-config --version` to check.)  Then, the usual
```bash
./configure
make
make install
```
procedure should work.  See [INSTALL.md](https://github.com/lanl/Byfl/blob/master/INSTALL.md) for a more complete explanation.

Documentation
-------------

Byfl documentation is maintained in the [Byfl wiki](https://github.com/lanl/Byfl/wiki) on GitHub.

Copyright and license
---------------------

Los Alamos National Security, LLC (LANS) owns the copyright to Byfl, which it identifies internally as LA-CC-12-039.  The license is BSD-ish with a "modifications must be indicated" clause.  See [LICENSE.md](https://github.com/lanl/Byfl/blob/master/LICENSE.md) for the full text.

Contact
-------

Scott Pakin, [_pakin@lanl.gov_](mailto:pakin@lanl.gov)

A list of [all contributors to Byfl](https://github.com/lanl/Byfl/wiki/contributors) is available online.
