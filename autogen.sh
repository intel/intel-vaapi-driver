#! /bin/sh

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd "$srcdir"

# install pre-commit hook
SRC_PRE_COMMIT=hooks/pre-commit.hook
GIT_PRE_COMMIT=.git/hooks/pre-commit

if [ ! \( -x $GIT_PRE_COMMIT -a -L $GIT_PRE_COMMIT \) ]; then
    rm -f $GIT_PRE_COMMIT
    ln -s ../../$SRC_PRE_COMMIT $GIT_PRE_COMMIT
fi

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

if test -z "$NOCONFIGURE"; then
    "$srcdir"/configure "$@"
fi
