# Copyright (c) 2007 Intel Corporation. All Rights Reserved.
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

SUBDIRS		= shaders
DIST_SUBDIRS	= $(SUBDIRS)
EXTRA_DIST	=
BUILT_SOURCES	=

AM_CPPFLAGS = \
	-DPTHREADS		\
	$(DRM_CFLAGS)		\
	$(LIBVA_DEPS_CFLAGS)	\
	-DVA_DRIVERS_PATH="\"$(LIBVA_DRIVERS_PATH)\"" \
	$(NULL)

driver_cflags = \
	-Wall			\
	-fvisibility=hidden	\
	$(NULL)

driver_ldflags = \
	-avoid-version		\
	-no-undefined		\
	-Wl,--no-undefined	\
	$(NULL)

driver_libs = \
	-lpthread -lm -ldl	\
	$(DRM_LIBS) -ldrm_intel	\
	$(NULL)

include Makefile.sources

# convenience library that can be linked by driver and tests
noinst_LTLIBRARIES		= libi965_drv_video.la
libi965_drv_video_la_CFLAGS	= $(driver_cflags)
libi965_drv_video_la_LDFLAGS	= $(driver_ldflags)
libi965_drv_video_la_LIBADD	= $(driver_libs)
libi965_drv_video_la_SOURCES	= $(source_c)

# driver module
i965_drv_video_la_LTLIBRARIES	= i965_drv_video.la
i965_drv_video_ladir		= $(LIBVA_DRIVERS_PATH)
i965_drv_video_la_CFLAGS	= $(driver_cflags)
i965_drv_video_la_LDFLAGS	= -module $(driver_ldflags)
i965_drv_video_la_LIBADD	= libi965_drv_video.la $(driver_libs)
i965_drv_video_la_SOURCES	=

noinst_HEADERS			= $(source_h)

if USE_X11
source_c			+= i965_output_dri.c
source_h			+= i965_output_dri.h
endif

if USE_WAYLAND
source_c			+= i965_output_wayland.c
source_h			+= i965_output_wayland.h
source_h			+= $(protocol_source_h)
driver_cflags			+= $(WAYLAND_CFLAGS) $(WAYLAND_CLIENT_CFLAGS)
endif

# git version
VERSION_FILE			= .VERSION
OLD_VERSION_FILE		= $(VERSION_FILE).old
NEW_VERSION_FILE		= $(VERSION_FILE).new
PKG_VERSION_FILE		= $(VERSION_FILE).pkg

intel_version.h: gen-version
	$(AM_V_GEN)							 \
	OV=`[ -f $(OLD_VERSION_FILE) ] && cat $(OLD_VERSION_FILE) || :`; \
	NV=`cat $(NEW_VERSION_FILE)`;					 \
	if [ "$$OV" != "$$NV" -o ! -f intel_version.h ]; then		 \
		cp -f $(NEW_VERSION_FILE) $(OLD_VERSION_FILE);		 \
		$(SED) -e "s|\@INTEL_DRIVER_GIT_VERSION\@|$${NV}|"	 \
			$(srcdir)/intel_version.h.in > intel_version.h;	 \
	fi

gen-version:
	@echo $(VERSION) > $(NEW_VERSION_FILE)
if HAVE_GIT
	@[ -d $(top_srcdir)/.git ] && \
	(cd $(top_srcdir) && $(GIT) describe --tags) > $(NEW_VERSION_FILE) || :
endif
	@[ -f $(srcdir)/$(PKG_VERSION_FILE) ] && \
	cp -f $(srcdir)/$(PKG_VERSION_FILE) $(NEW_VERSION_FILE) || :

$(PKG_VERSION_FILE): $(NEW_VERSION_FILE)
	@cp -f $< $@

BUILT_SOURCES	+= intel_version.h
EXTRA_DIST	+= Android.mk intel_version.h.in $(PKG_VERSION_FILE)

# Wayland protocol
if USE_WAYLAND
protocol_source_h = wayland-drm-client-protocol.h
i965_output_wayland.c: $(protocol_source_h)
%-client-protocol.h : %.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@

BUILT_SOURCES += $(protocol_source_h)
endif

EXTRA_DIST += \
	wayland-drm.xml         \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in config.h.in
