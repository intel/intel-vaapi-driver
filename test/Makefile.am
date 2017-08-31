AUTOMAKE_OPTIONS = subdir-objects

AM_CXXFLAGS =								\
	-I$(top_srcdir)/test/gtest/include				\
	$(NULL)

AM_CPPFLAGS =								\
	-DGTEST_HAS_PTHREAD=1						\
	-DGTEST_USE_OWN_TR1_TUPLE=0					\
	-DGTEST_LANG_CXX11=1						\
	-DGTEST_HAS_TR1_TUPLE=1						\
	-DPTHREADS							\
	-DVA_DRIVERS_PATH="\"$(LIBVA_DRIVERS_PATH)\""			\
	-DTEST_VA_DRIVERS_PATH="\"$(abs_top_srcdir)/src/.libs\""	\
	-std=c++11							\
	$(NULL)

AM_LDFLAGS =								\
	-pthread							\
	$(NULL)

# libgtest
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

# test_i965_drv_video
noinst_PROGRAMS = test_i965_drv_video
noinst_HEADERS =							\
	i965_avce_test_common.h						\
	i965_config_test.h						\
	i965_internal_decl.h						\
	i965_jpeg_test_data.h						\
	i965_streamable.h						\
	i965_test_environment.h						\
	i965_test_fixture.h						\
	i965_test_image_utils.h						\
	test.h								\
	test_utils.h							\
	$(NULL)

test_i965_drv_video_SOURCES =						\
	i965_avcd_config_test.cpp					\
	i965_avce_config_test.cpp					\
	i965_avce_context_test.cpp					\
	i965_avce_test_common.cpp					\
	i965_chipset_test.cpp						\
	i965_config_test.cpp						\
	i965_initialize_test.cpp					\
	i965_jpeg_test_data.cpp						\
	i965_jpeg_decode_test.cpp					\
	i965_jpeg_encode_test.cpp					\
	i965_jpegd_config_test.cpp					\
	i965_jpege_config_test.cpp					\
	i965_surface_test.cpp						\
	i965_test_environment.cpp					\
	i965_test_fixture.cpp						\
	i965_test_image_utils.cpp					\
	object_heap_test.cpp						\
	test_main.cpp							\
	$(NULL)

test_i965_drv_video_LDFLAGS =						\
	$(DRM_LDFLAGS)							\
	$(LIBVA_DEPS_LDFLAGS)						\
	$(LIBVA_DRM_DEPS_LDFLAGS)					\
	$(AM_LDFLAGS)							\
	$(NULL)

test_i965_drv_video_LDADD =						\
	$(top_srcdir)/src/libi965_drv_video.la				\
	libgtest.la							\
	$(DRM_LIBS)							\
	$(LIBVA_DEPS_LIBS)						\
	$(LIBVA_DRM_DEPS_LIBS)						\
	-ldrm_intel -lm -ldl						\
	$(NULL)

test_i965_drv_video_CPPFLAGS =						\
	$(DRM_CFLAGS)							\
	$(LIBVA_DEPS_CFLAGS)						\
	$(LIBVA_DRM_DEPS_CFLAGS)					\
	$(AM_CPPFLAGS)							\
	$(NULL)

test_i965_drv_video_CXXFLAGS =						\
	-Wall -Werror							\
	$(AM_CXXFLAGS)							\
	$(NULL)

check-local: test_i965_drv_video
	$(builddir)/test_i965_drv_video
