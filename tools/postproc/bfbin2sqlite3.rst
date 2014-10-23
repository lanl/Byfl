=============
bfbin2sqlite3
=============

----------------------------------------------
convert Byfl output to a SQLite3 database file
----------------------------------------------

:Author:         Scott Pakin <pakin@lanl.gov>
:Date:           2014-10-22
:Manual section: 1
:Manual group:


Synopsis
========

**bfbin2sqlite3** <*input_file.byfl*> [<*output_file.db*>]


Description
===========

By default, applications instrumented with Byfl write measurement data
to a binary *.byfl* file.  **bfbin2sqlite3** converts such files to a
SQLite3 database file.


Options
=======

**bfbin2sqlite3** has no command-line options.  The program accepts
the name of a *.byfl* file to read and a SQLite3 database file to
create.  If not specified, the name of the database file will be the
same as that of the input file but with *.byfl* replaced with *.db*.
(If the file name does not end in *.byfl* then *.db* will be appended
to the file name.)


Examples
========

The simplest usage is just the following::

    $ bfbin2sqlite3 myprog.byfl

This produces *myprog.db*.

If the *.byfl* file is expected to be used only to produce the SQLite3
database and then deleted, one can save time and disk space by writing
Byfl binary output to a named pipe and running **bfbin2sqlite3** on
that named pipe::

    $ mkfifo myprog.pipe
    $ bfbin2sqlite3 myprog.pipe myprog.db &
    $ env BF_BINOUT=myprog.pipe ./myprog
    $ rm myprog.pipe


See also
========

bfbin2text(1), bfbin2hdf5(1),
bf-gcc(1), bf-g++(1), bf-gfortran(1), bf-gccgo(1),
the Byfl home page (https://github.com/losalamos/Byfl)
