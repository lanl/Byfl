#! /usr/bin/env perl

##########################################
# Install the ParseGccOpts helper module #
# By Scott Pakin <pakin@lanl.gov>        #
##########################################

use ExtUtils::MakeMaker;

symlink "bf-gcc", "bf-g++";
symlink "bf-gcc", "bf-gfortran";
symlink "bf-gcc", "bf-gccgo";
my @exe_files = ("bf-gcc", "bf-g++", "bf-gfortran", "bf-gccgo", "bf-inst");
WriteMakefile(
    NAME            => "ParseGccOpts",
    VERSION_FROM    => "lib/ParseGccOpts.pm",
    EXE_FILES       => \@exe_files,
    MAN1PODS        => {"bf-inst" => "blib/man1/bf-inst.1",
                        "bf-gcc"  => "blib/man1/bf-gcc.1"}
);
