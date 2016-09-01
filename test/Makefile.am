AUTOMAKE_OPTIONS = subdir-objects

AM_CXXFLAGS =								\
	-I$(top_srcdir)/test/gtest/include				\
	$(NULL)

AM_CPPFLAGS =								\
	-DGTEST_HAS_PTHREAD=1						\
	-DGTEST_USE_OWN_TR1_TUPLE=0					\
	-DGTEST_LANG_CXX11=1						\
	-DGTEST_HAS_TR1_TUPLE=1						\
	-std=c++11							\
	$(NULL)

AM_LDFLAGS =								\
	-pthread							\
	$(NULL)

noinst_LTLIBRARIES = libgtest.la

libgtest_la_SOURCES =							\
	gtest/src/gtest-all.cc						\
	gtest/src/gtest_main.cc						\
	$(NULL)

libgtest_la_CXXFLAGS =							\
	-I$(top_srcdir)/test/gtest					\
	$(AM_CXXFLAGS)							\
	$(NULL)

EXTRA_DIST =								\
	gtest/docs							\
	gtest/include							\
	gtest/src/*.cc							\
	gtest/src/*.h							\
	gtest/README.md							\
	gtest/LICENSE							\
	$(NULL)
