#! /bin/sh

autoreconf -v --install

if test -z "$NOCONFIGURE"; then
    ./configure "$@"
fi
