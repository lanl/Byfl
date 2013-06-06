#! /usr/bin/env perl

##########################################
# Install the ParseGccOpts helper module #
# By Scott Pakin <pakin@lanl.gov>        #
##########################################

use ExtUtils::MakeMaker;

symlink "bf-gcc", "bf-g++";
symlink "bf-gcc", "bf-gfortran";
symlink "bf-gcc", "bf-gccgo";
WriteMakefile(
    NAME            => "ParseGccOpts",
    VERSION_FROM    => "lib/ParseGccOpts.pm",
    EXE_FILES       => ["bf-gcc", "bf-g++", "bf-gfortran", "bf-gccgo", "bf-inst"],
    MAN1EXT         => "1"
);
