###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#				              #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

dist_bin_SCRIPTS = bf2cgrind bf2hpctk bfmerge bf-parse-cache-dump

if NETCDF_AVAILABLE
bin_PROGRAMS = bfbin2netcdf
bfbin2netcdf_SOURCES = bfbin2netcdf.c
bfbin2netcdf_CPPFLAGS = -I$(top_srcdir)/include
bfbin2netcdf_LDADD = -lnetcdf
endif
