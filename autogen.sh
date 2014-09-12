#! /bin/sh -x

# Generate all of the GNU Autotools helper files needed by Byfl.
libtoolize --copy || exit 1
aclocal || exit 1
automake --add-missing --copy --foreign || exit 1
autoconf || exit 1
