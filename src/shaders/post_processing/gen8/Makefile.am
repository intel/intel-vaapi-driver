INTEL_PP_G8B = \
	pl2_to_pl2.g8b		\
	pl2_to_pl3.g8b		\
	pl3_to_pl2.g8b		\
	pl3_to_pl3.g8b		\
	pl2_to_rgbx.g8b		\
	rgbx_to_nv12.g8b	\
	pl2_to_pa.g8b		\
	pl3_to_pa.g8b		\
	pa_to_pl2.g8b		\
	pa_to_pl3.g8b		\
	pa_to_pa.g8b		\
	$(NULL)

INTEL_PP_PRE_G8B =		\
	sharpening_h_blur.g8b	\
	sharpening_unmask.g8b	\
	sharpening_v_blur.g8b   \
	conv_nv12.g8b		\
	conv_8bit_420_rgb32.g8b

INTEL_PP2_G8B = 		\
	clear_bgrx.g8b		\
	clear_pl2_8bit.g8b	\
	clear_pl3_8bit.g8b	\
	clear_rgbx.g8b		\
	clear_yuy2.g8b		\
	clear_uyvy.g8b		\
	$(NULL)

INTEL_PP_G8A = \
	EOT.g8a				\
	PL2_AVS_Buf_0.g8a		\
	PL2_AVS_Buf_1.g8a		\
	PL2_AVS_Buf_2.g8a		\
	PL2_AVS_Buf_3.g8a		\
	PL2_media_read_buf0123.g8a	\
	PL3_AVS_Buf_0.g8a		\
	PL3_AVS_Buf_1.g8a		\
	PL3_AVS_Buf_2.g8a		\
	PL3_AVS_Buf_3.g8a		\
	PL3_media_read_buf0123.g8a	\
	PA_AVS_Buf_0.g8a		\
	PA_AVS_Buf_1.g8a		\
	PA_AVS_Buf_2.g8a		\
	PA_AVS_Buf_3.g8a		\
	Save_AVS_NV12.g8a		\
	Save_AVS_PL3.g8a		\
	Save_AVS_RGBX.g8a		\
	Save_AVS_PA.g8a			\
	Set_AVS_Buf_0123_PL2.g8a	\
	Set_AVS_Buf_0123_PL3.g8a	\
	Set_AVS_Buf_0123_BGRA.g8a	\
	Set_AVS_Buf_0123_VYUA.g8a	\
	YUV_to_RGB.g8a			\
	RGB_to_YUV.g8a			\
	Set_Layer_0.g8a			\
	VP_Setup.g8a			\
	$(NULL)

INTEL_PP_ASM = $(INTEL_PP_G8B:%.g8b=%.asm)
INTEL_PP_GEN8_ASM = $(INTEL_PP_G8B:%.g8b=%.g8s)

INTEL_PP2_GXA = $(INTEL_PP2_G8B:%.g8b=%.gxa)
INTEL_PP2_GXS = $(INTEL_PP2_G8B:%.gxa=%.gxs)

TARGETS  =
if HAVE_GEN4ASM
TARGETS += $(INTEL_PP_G8B) $(INTEL_PP2_G8B)
endif

all-local: $(TARGETS)

SUFFIXES = .g8b .g8s .asm

if HAVE_GEN4ASM
$(INTEL_PP_GEN8_ASM): $(INTEL_PP_ASM) $(INTEL_PP_G8A)
.asm.g8s:
	$(AM_V_GEN)cpp $< > _pp0.$@;		\
	$(PYTHON2) $(top_srcdir)/src/shaders/gpp.py _pp0.$@ $@;	\
	rm _pp0.$@
.g8s.g8b:
	$(AM_V_GEN)$(GEN4ASM) -a -o $@ -g 8 $<

.gxa.gxs:
	$(AM_V_GEN)cpp -P $< > _tmp.$@ &&	\
	m4 _tmp.$@ > $@	&&			\
	rm _tmp.$@
.gxs.g8b:
	$(AM_V_GEN)$(GEN4ASM) -o $@ -g 8 $<
endif

CLEANFILES = $(INTEL_PP_GEN7_ASM) $(INTEL_PP_GEN8_ASM) $(INTEL_PP2_GXS)

DISTCLEANFILES = $(TARGETS)

EXTRA_DIST = \
	$(INTEL_PP_ASM)	\
	$(INTEL_PP_G8A)	\
	$(INTEL_PP_G8B) \
	$(INTEL_PP_PRE_G8B) \
	$(INTEL_PP2_G8B) \
	$(NULL)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
