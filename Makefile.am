AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

if ENABLE_TESTS
SUBDIRS += test
endif


# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = \
	aclocal.m4 compile config.guess config.sub \
	configure depcomp install-sh ltmain.sh     \
	Makefile.in missing

EXTRA_DIST = Android.mk
