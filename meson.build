project(
  'intel-vaapi-driver', 'c',
  version : '2.2.1.1',
  meson_version : '>= 0.43.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

version_arr = meson.project_version().split('.')
intel_vaapi_driver_major_version = version_arr[0]
intel_vaapi_driver_minor_version = version_arr[1]
intel_vaapi_driver_micro_version = version_arr[2]
intel_vaapi_driver_version = '@0@.@1@.@2@'.format(intel_vaapi_driver_major_version,
						  intel_vaapi_driver_minor_version,
						  intel_vaapi_driver_micro_version)
if version_arr.length() == 4
  intel_vaapi_driver_pre_version = version_arr[3]
  intel_vaapi_driver_version = '@0@.pre@1@'.format(intel_vaapi_driver_version,
						   intel_vaapi_driver_pre_version)
endif

cc = meson.get_compiler('c')
dl_dep = cc.find_library('dl')
mathlib_dep = cc.find_library('m', required : false)

git = find_program('git', required : false)

thread_dep = dependency('threads')
libdrm_dep = dependency('libdrm', version : '>= 2.4.52')
libdrm_intel_dep = dependency('libdrm_intel')

libva_version = '>= 1.1.0'
libva_dep = dependency('libva', version : libva_version,
  fallback : [ 'libva', 'libva_dep' ])
if get_option ('enable_tests')
  libva_drm_dep = dependency('libva-drm', version : libva_version,
    fallback : [ 'libva', 'libva_drm_dep' ])
endif

va_api_major_version = '0'
va_api_minor_version = '33'
driverdir = get_option('driverdir')
if libva_dep.type_name() == 'pkgconfig'
  if driverdir == ''
    driverdir = libva_dep.get_pkgconfig_variable('driverdir')
  endif
  va_api_version_array = libva_dep.version().split('.')
  va_api_major_version = va_api_version_array[0]
  va_api_minor_version = va_api_version_array[1]
else
  libva = subproject('libva')
  if driverdir == ''
    driverdir = libva.get_variable('driverdir')
  endif
  va_api_major_version = libva.get_variable('va_api_major_version')
  va_api_minor_version = libva.get_variable('va_api_minor_version')
endif

if driverdir == ''
  driverdir = '@0@/@1@/@2@'.format(get_option('prefix'), get_option('libdir'), 'dri')
endif

va_driver_init_func = '__vaDriverInit_@0@_@1@'.format(va_api_major_version,
                                                      va_api_minor_version)

WITH_X11 = false
if get_option('with_x11') != 'no'
  libva_x11_dep = dependency(
    'libva-x11',
    version : libva_version,
    required : get_option('with_x11') == 'yes')

  WITH_X11 = libva_x11_dep.found()
endif

WITH_WAYLAND = false
if get_option('with_wayland') != 'no'
  wayland_client_dep = dependency(
    'wayland-client',
    version : '>= 1.11.0',
    required : get_option('with_wayland') == 'yes')

  if wayland_client_dep.found()
    prefix = wayland_client_dep.get_pkgconfig_variable('prefix')
    wl_scanner = find_program(
      'wayland-scanner',
      join_paths(prefix, '/bin/wayland-scanner'))
  endif

  libva_wayland_dep = dependency(
    'libva-wayland',
    version : libva_version,
    required : get_option('with_wayland') == 'yes')

  WITH_WAYLAND = wl_scanner.found() and libva_wayland_dep.found()
endif

subdir('src')

if get_option('enable_tests')
  srcdir = include_directories('src')
  subdir('test')
endif
