add_languages('cpp')

gtest_sources = [
  'gtest/src/gtest-all.cc',
  'gtest/src/gtest_main.cc',
]

libgtest_includes = [
  include_directories('gtest'),
  include_directories('gtest/include'),
]

libgtest_cppflags = [
  '-DGTEST_HAS_PTHREAD=' + thread_dep.found().to_string('1', '0'),
  '-DGTEST_LANG_CXX11=1',
  '-DGTEST_USE_OWN_TR1_TUPLE=0',
  '-DGTEST_HAS_TR1_TUPLE=1',
]

libgtest = static_library(
  'gtest',
  gtest_sources,
  cpp_args : libgtest_cppflags,
  override_options : [ 'cpp_std=c++11' ],
  dependencies : [ thread_dep ],
  include_directories : libgtest_includes)

test_i965_headers = [
  'i965_avce_test_common.h',
  'i965_config_test.h',
  'i965_internal_decl.h',
  'i965_jpeg_test_data.h',
  'i965_streamable.h',
  'i965_test_environment.h',
  'i965_test_fixture.h',
  'i965_test_image_utils.h',
  'test.h',
  'test_utils.h',
]

test_i965_sources = [
  'i965_avcd_config_test.cpp',
  'i965_avce_config_test.cpp',
  'i965_avce_context_test.cpp',
  'i965_avce_test_common.cpp',
  'i965_chipset_test.cpp',
  'i965_config_test.cpp',
  'i965_initialize_test.cpp',
  'i965_jpeg_test_data.cpp',
  'i965_jpeg_decode_test.cpp',
  'i965_jpeg_encode_test.cpp',
  'i965_jpegd_config_test.cpp',
  'i965_jpege_config_test.cpp',
  'i965_surface_test.cpp',
  'i965_test_environment.cpp',
  'i965_test_fixture.cpp',
  'i965_test_image_utils.cpp',
  'object_heap_test.cpp',
  'test_main.cpp',
]

gtest_includes = [
  srcdir,
  include_directories('gtest/include'),
]

test_driverdir = join_paths(meson.build_root(), 'src')
test_cppflags = [
  '-DTEST_VA_DRIVERS_PATH="' + test_driverdir + '"',
  '-DHAVE_CONFIG_H'
]
if thread_dep.found()
  test_cppflags += [ '-DPTHREADS' ]
endif

test_i965_drv_video = executable(
  'test_i965_drv_video',
  [ test_i965_headers, test_i965_sources ],
  dependencies : [ shared_deps, libdrm_dep, libva_drm_dep ],
  link_with : [ libgtest, libi965_drv_video ],
  include_directories : gtest_includes,
  cpp_args : [ test_cppflags, libgtest_cppflags ],
  override_options : [ 'cpp_std=c++11' ])

# 3 minutes for timeout
test('driver test', test_i965_drv_video, timeout : 3 * 60)
