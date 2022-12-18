#!/bin/sh
set -e
rm -f aclocal.m4 libtool config.cache config.status
echo "Running autoreconf -i ..."
autoreconf -i
echo "Now you can run ./configure"
echo "(use --enable-maintainer-mode if you want Makefile.in automatically regenerated)"
