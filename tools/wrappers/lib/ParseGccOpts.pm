###################################
# Help the Byfl wrapper scripts   #
# parse GCC options               #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

=head1 NAME

ParseGccOpts - Parse GCC options into compiler and linker options

=head1 SYNOPSIS

  use ParseGccOpts;

  @parse_info = parse_gcc_options @ARGV;
  %build_type = %{$parse_info[0]};
  @target_filenames = @{$parse_info[1]};
  @compiler_opts = @{$parse_info[2]};
  @linker_opts = @{$parse_info[3]};
  @leftover_values = @{$parse_info[4]};

=head1 DESCRIPTION

This module is intended to be used internally by the Byfl wrapper
scripts (bf-gcc, bf-g++, and bf-gfortran).

=head1 AUTHOR

Scott Pakin <pakin@lanl.gov>

=cut

###########################################################################

package ParseGccOpts;

use Carp;
use File::Basename;
use Getopt::Long qw(GetOptionsFromArray);
use warnings;
use strict;

BEGIN {
    use Exporter   ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
    $VERSION     = 1.00;
    @ISA         = qw(Exporter);
    @EXPORT      = qw(parse_gcc_options);
    %EXPORT_TAGS = ();
    @EXPORT_OK   = ();
}
our @EXPORT_OK;

# Parse a list of options, typically @ARGV.
sub parse_gcc_options (@)
{
    my @arg_list = @_;      # Copy of argument list
    my @compiler_opts;      # Options for the compiler
    my @linker_opts;        # Options for the linker
    my @leftover_values;    # Leftover non-options
    my %build_type = ("link" => 1);  # Set of "compile" and "link"
    my @target_filenames;   # Name of the target filename(s)

    # First pass: Use Getopt::Long to do most of the work.
    my $for_compiler = sub ($$) {
        push @compiler_opts, [$_[0], $_[1]];
    };
    my $for_linker = sub ($$) {
        push @linker_opts, [$_[0], $_[1]];
    };
    my $for_linker_1 = sub ($$) {
        push @linker_opts, [$_[0], undef];
    };
    Getopt::Long::Configure("bundling", "bundling_override", "pass_through");
    GetOptionsFromArray(\@arg_list,
                        # Handle a few special cases.
                        "c" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ("compile" => 1);
                        },
                        "S" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ("compile" => 1);
                        },
                        "E" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ();
                        },
                        "help" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ();
                        },
                        "version" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ();
                        },
                        "dumpversion" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ();
                        },
                        "print-search-dirs" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ();
                        },
                        "print-multi-os-directory" => sub {
                            $for_compiler->($_[0], undef);
                            %build_type = ();
                        },
                        "print-prog-name=s" => sub {
                            $for_compiler->($_[0], $_[1]);
                            %build_type = ();
                        },
                        "o=s" => sub {
                            $for_compiler->($_[0], $_[1]);
                            $for_linker->($_[0], $_[1]);
                            @target_filenames = ($_[1]);
                        },
                        "cpp" => sub {
                            $for_compiler->($_[0], undef);
                        },
                        "nocpp" => sub {
                            $for_compiler->($_[0], undef);
                        },
                        "I=s" => sub {
                            $for_compiler->($_[0], $_[1]);
                        },
                        "J=s" => sub {
                            $for_compiler->($_[0], $_[1]);
                        },
                        "isystem=s" => sub {
                            $for_compiler->($_[0], $_[1]);
                        },
                        "pg" => sub {
                            $for_compiler->($_[0], undef);
                            $for_linker->($_[0], undef);
                        },
                        "O:i" => $for_compiler,
                        "x=s" => $for_compiler,

                        # Recognize preprocessor options as compiler options.
                        "MF=s" => $for_compiler,
                        "MT=s" => $for_compiler,
                        "MQ=s" => $for_compiler,

                        # Recognize linker options.
                        "L=s" => $for_linker,
                        "l=s" => $for_linker,
                        "nostartfiles" => $for_linker_1,
                        "nodefaultlibs" => $for_linker_1,
                        "nostdlib" => $for_linker_1,
                        "pie" => $for_linker_1,
                        "rdynamic" => $for_linker_1,
                        "s" => $for_linker_1,
                        "static" => $for_linker_1,
                        "shared" => $for_linker_1,
                        "shared-libgcc" => $for_linker_1,
                        "static-libgcc" => $for_linker_1,
                        "symbolic" => $for_linker_1,
                        "T=s" => $for_linker,
                        "Xlinker=s" => $for_linker,
                        "u=s" => $for_linker);

    # Second pass: Handle various special cases and also construct the
    # leftover list.
    foreach my $arg (@arg_list) {
        if ($arg eq "-static-libstdc++" || $arg =~ /^-Wl,/) {
            $for_linker_1->(substr($arg, 1), undef);
        }
        elsif ($arg =~ /^-+([^=]+)(=(.*))?$/) {
            $for_compiler->($1, $3);
        }
        else {
            push @leftover_values, $arg;
            $build_type{"compile"} = 1 if defined $build_type{"link"} && $arg !~ /\.([ao]|so)$/;
        }
    }

    # Define the target filename if not already defined.
    if (!@target_filenames) {
        if (defined $build_type{"link"}) {
            @target_filenames = ("a.out");
        }
        else {
            foreach my $srcname (@leftover_values) {
                my ($base, $dir, $suffix) = fileparse $srcname, ".[^.]+";
                next if $suffix =~ /^\.[ao]$/;
                push @target_filenames, $base . ".o";
            }
        }
    }

    # Return a list of the information we gathered.
    return (\%build_type,
            \@target_filenames,
            \@compiler_opts,
            \@linker_opts,
            \@leftover_values);
}

1;
