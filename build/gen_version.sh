# Copyright (c) 2015 Intel Corporation. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#!/bin/sh

top_srcdir="$1"
srcdir="$2"

# git version
VERSION_FILE=".VERSION"
OLD_VERSION_FILE=$VERSION_FILE.old
NEW_VERSION_FILE=$VERSION_FILE.new
PKG_VERSION_FILE=$VERSION_FILE.pkg
HAVE_GIT=0;

check_git() {
    git --version 2>&1 /dev/null
    if [ $? -eq "0" ]; then
	HAVE_GIT=1
    else
	HAVE_GIT=0;
    fi
}

gen_version() {
    echo $VERSION > $NEW_VERSION_FILE
    if [ $HAVE_GIT -eq "1" ]; then
	[ -d $top_srcdir/.git ] && \
	    (cd $top_srcdir && git describe --tags) > $NEW_VERSION_FILE || :
    fi
    [ -f $srcdir/$PKG_VERSION_FILE ] && \
	cp -f $srcdir/$PKG_VERSION_FILE $NEW_VERSION_FILE || :
}

check_git;
gen_version;

OV=`[ -f $OLD_VERSION_FILE ] && cat $OLD_VERSION_FILE || :`;
NV=`cat $NEW_VERSION_FILE`;
if [ "$$OV" != "$$NV" -o ! -f intel_version.h ]; then
    cp -f $NEW_VERSION_FILE $OLD_VERSION_FILE;
    echo "Replace"
    sed -e "s|\@INTEL_DRIVER_GIT_VERSION\@|$NV|" \
	$srcdir/intel_version.h.in > $srcdir/intel_version.h;
fi
