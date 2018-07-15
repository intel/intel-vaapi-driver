config_cfg = configuration_data()
config_cfg.set('VERSION', intel_vaapi_driver_version)
config_cfg.set('VA_DRIVER_INIT_FUNC', va_driver_init_func)
config_cfg.set('INTEL_DRIVER_MAJOR_VERSION', intel_vaapi_driver_major_version)
config_cfg.set('INTEL_DRIVER_MINOR_VERSION', intel_vaapi_driver_minor_version)
config_cfg.set('INTEL_DRIVER_MICRO_VERSION', intel_vaapi_driver_micro_version)
config_cfg.set('INTEL_DRIVER_PRE_VERSION', intel_vaapi_driver_pre_version)
config_cfg.set10('HAVE_HYBRID_CODEC', get_option('enable_hybrid_codec'))
if WITH_X11
  config_cfg.set10('HAVE_VA_X11', 1)
endif
if WITH_WAYLAND
  config_cfg.set10('HAVE_VA_WAYLAND', 1)
endif
if cc.has_function('log2f')
  config_cfg.set('HAVE_LOG2F', 1)
endif

config_file = configure_file(
  output : 'config.h',
  configuration : config_cfg)

if git.found()
  git_version = run_command(
    git, '--git-dir', join_paths(meson.source_root(), '.git'),
    'describe', '--tags')
  intel_driver_git_version = git_version.stdout().strip()
else
  intel_driver_git_version = intel_vaapi_driver_version
endif

version_cfg = configuration_data()
version_cfg.set('INTEL_DRIVER_GIT_VERSION', intel_driver_git_version)

version_file = configure_file(
  input : 'intel_version.h.in',
  output : 'intel_version.h',
  configuration : version_cfg)

sources = [
  'dso_utils.c',
  'gen6_mfc.c',
  'gen6_mfc_common.c',
  'gen6_mfd.c',
  'gen6_vme.c',
  'gen7_vme.c',
  'gen7_mfc.c',
  'gen7_mfd.c',
  'gen75_mfd.c',
  'gen75_mfc.c',
  'gen8_encoder_vp8.c',
  'gen8_mfc.c',
  'gen8_mfd.c',
  'gen8_vme.c',
  'gen9_encoder_vp8.c',
  'gen9_vme.c',
  'gen9_mfc.c',
  'gen9_mfc_hevc.c',
  'gen9_mfd.c',
  'gen9_vdenc.c',
  'gen75_picture_process.c',
  'gen75_vme.c',
  'gen75_vpp_gpe.c',
  'gen75_vpp_vebox.c',
  'gen9_post_processing.c',
  'i965_avc_bsd.c',
  'i965_avc_hw_scoreboard.c',
  'i965_avc_ildb.c',
  'i965_decoder_utils.c',
  'i965_device_info.c',
  'i965_drv_video.c',
  'i965_encoder.c',
  'i965_encoder_utils.c',
  'i965_encoder_vp8.c',
  'i965_media.c',
  'i965_media_h264.c',
  'i965_media_mpeg2.c',
  'i965_gpe_utils.c',
  'i965_post_processing.c',
  'i965_yuv_coefs.c',
  'gen8_post_processing.c',
  'i965_render.c',
  'i965_vpp_avs.c',
  'gen8_render.c',
  'gen9_render.c',
  'intel_batchbuffer.c',
  'intel_batchbuffer_dump.c',
  'intel_driver.c',
  'intel_memman.c',
  'object_heap.c',
  'intel_media_common.c',
  'vp8_probs.c',
  'vp9_probs.c',
  'vpx_quant.c',
  'gen9_vp9_encoder_kernels.c',
  'gen9_vp9_const_def.c',
  'gen9_vp9_encoder.c',
  'intel_common_vpp_internal.c',
  'i965_encoder_const_def.c',
  'i965_avc_const_def.c',
  'i965_avc_encoder_kernels.c',
  'i965_avc_encoder_common.c',
  'i965_avc_encoder.c',
  'gen9_hevc_enc_kernels_binary.c',
  'gen9_hevc_encoder.c',
  'gen9_hevc_enc_utils.c',
  'gen10_encoder_vp8.c',
  'gen10_hcp_common.c',
  'gen10_hevc_enc_kernels_binary.c',
  'gen10_hevc_enc_common.c',
  'gen10_hevc_encoder.c',
  'gen10_huc_common.c',
  'gen10_vdenc_common.c',
  'gen10_vdenc_vp9.c',
]

headers = [
  'dso_utils.h',
  'gen6_mfc.h',
  'gen6_mfd.h',
  'gen6_vme.h',
  'gen7_mfd.h',
  'gen75_picture_process.h',
  'gen75_vpp_gpe.h',
  'gen75_vpp_vebox.h',
  'gen8_post_processing.h',
  'gen9_mfd.h',
  'gen9_mfc.h',
  'gen9_vdenc.h',
  'i965_avc_bsd.h',
  'i965_avc_hw_scoreboard.h',
  'i965_avc_ildb.h',
  'i965_decoder.h',
  'i965_decoder_utils.h',
  'i965_defines.h',
  'i965_drv_video.h',
  'i965_encoder.h',
  'i965_encoder_utils.h',
  'i965_encoder_vp8.h',
  'i965_media.h',
  'i965_media_h264.h',
  'i965_media_mpeg2.h',
  'i965_mutext.h',
  'i965_gpe_utils.h',
  'i965_pciids.h',
  'i965_post_processing.h',
  'i965_render.h',
  'i965_structs.h',
  'i965_vpp_avs.h',
  'i965_yuv_coefs.h',
  'intel_batchbuffer.h',
  'intel_batchbuffer_dump.h',
  'intel_compiler.h',
  'intel_driver.h',
  'intel_media.h',
  'intel_memman.h',
  'object_heap.h',
  'vp8_probs.h',
  'vp9_probs.h',
  'vpx_quant.h',
  'sysdeps.h',
  'va_backend_compat.h',
  'i965_fourcc.h',
  'gen9_vp9_encoder.h',
  'gen9_vp9_encapi.h',
  'gen9_vp9_const_def.h',
  'gen9_vp9_encoder_kernels.h',
  'intel_gen_vppapi.h',
  'intel_common_vpp_internal.h',
  'i965_encoder_common.h',
  'i965_encoder_api.h',
  'i965_avc_const_def.h',
  'i965_avc_encoder_kernels.h',
  'i965_avc_encoder.h',
  'i965_avc_encoder_common.h',
  'gen9_hevc_enc_const_def.h',
  'gen9_hevc_enc_kernels.h',
  'gen9_hevc_enc_kernels_binary.h',
  'gen9_hevc_enc_utils.h',
  'gen9_hevc_encoder.h',
  'gen10_hcp_common.h',
  'gen10_hevc_enc_kernels_binary.h',
  'gen10_hevc_enc_common.h',
  'gen10_hevc_encoder.h',
  'gen10_hevc_enc_const_def.h',
  'gen10_huc_common.h',
  'gen10_vdenc_common.h',
  'gen10_vdenc_vp9.h',
]

if WITH_X11
  sources += 'i965_output_dri.c'
  headers += 'i965_output_dri.h'
endif

if WITH_WAYLAND
  protocol_header = custom_target(
      'wayland-drm-client-protocol_h',
      output : 'wayland-drm-client-protocol.h',
      input : 'wayland-drm.xml',
      command : [ wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ])

  sources += 'i965_output_wayland.c'
  headers += [ 'i965_output_wayland.h',
               protocol_header ]
endif

cflags = [
  '-DVA_DRIVERS_PATH="' + driverdir + '"',
  '-DHAVE_CONFIG_H'
]

if thread_dep.found()
  cflags += [ '-DPTHREADS' ]
endif

shared_deps = [
  dl_dep,
  mathlib_dep,
  thread_dep,
  libdrm_dep,
  libdrm_intel_dep,
  libva_dep,
]

if WITH_X11
  shared_deps += [ libva_x11_dep ]
endif

if WITH_WAYLAND
  shared_deps += [ wayland_client_dep, libva_wayland_dep ]
endif

shared_sources = [
  sources,
  headers,
  config_file,
  version_file,
]

libi965_drv_video = static_library(
  'i965_drv_video',
  c_args : cflags,
  sources : shared_sources,
  dependencies : shared_deps)

i965_drv_video = shared_module(
  'i965_drv_video',
  name_prefix : '',
  install : true,
  install_dir : driverdir,
  link_whole : libi965_drv_video,
  dependencies : shared_deps)
