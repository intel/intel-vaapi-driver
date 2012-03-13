MFC_CORE	= \
	end_thread.asm				\
	mfc_batchbuffer_head.asm		\
	mfc_batchbuffer_tail.asm
MFC_CORE_AVC	= \
	mfc_batchbuffer_avc_intra.asm		\
	mfc_batchbuffer_avc_inter.asm

INTEL_G6B	= mfc_batchbuffer_avc_intra.g6b mfc_batchbuffer_avc_inter.g6b
INTEL_G6A	= mfc_batchbuffer_avc_intra.g6a mfc_batchbuffer_avc_inter.g6a
INTEL_GEN6_INC	= mfc_batchbuffer.inc
INTEL_GEN6_ASM	= $(INTEL_G6A:%.g6a=%.gen6.asm)

INTEL_G7B	= mfc_batchbuffer_avc_intra.g7b mfc_batchbuffer_avc_inter.g7b
INTEL_G7A	= mfc_batchbuffer_avc_intra.g7a mfc_batchbuffer_avc_inter.g7a
INTEL_GEN7_INC	= mfc_batchbuffer.inc
INTEL_GEN7_ASM	= $(INTEL_G7A:%.g7a=%.gen7.asm)

TARGETS  =
if HAVE_GEN4ASM
TARGETS += $(INTEL_G6B)
TARGETS += $(INTEL_G7B)
endif

all-local: $(TARGETS)

SUFFIXES = .g6a .g6b .g7a .g7b .gen6.asm .gen7.asm

if HAVE_GEN4ASM
$(INTEL_GEN6_ASM): $(MFC_CORE) $(MFC_CORE_AVC) $(INTEL_GEN6_INC)
.g6a.gen6.asm:
	$(AM_V_GEN)cpp -P -DDEV_SNB $< > _mfc0.$@ 	&& \
	m4 _mfc0.$@ > $@				&& \
	rm _mfc0.$@
.gen6.asm.g6b:
	$(AM_V_GEN)$(GEN4ASM) -g 6 -o $@ $<

$(INTEL_GEN7_ASM): $(MFC_CORE) $(MFC_CORE_AVC) $(INTEL_GEN7_INC)
.g7a.gen7.asm:
	$(AM_V_GEN)cpp -P -DDEV_IVB $< > _mfc0.$@ 	&& \
	m4 _mfc0.$@ > $@				&& \
	rm _mfc0.$@
.gen7.asm.g7b:
	$(AM_V_GEN)$(GEN4ASM) -g 7 -o $@ $<
endif

CLEANFILES = $(INTEL_GEN6_ASM) $(INTEL_GEN7_ASM)

EXTRA_DIST = \
	$(INTEL_G6A)		\
	$(INTEL_G6B)		\
	$(INTEL_G7A)		\
	$(INTEL_G7B)		\
	$(INTEL_GEN6_INC)	\
	$(INTEL_GEN7_INC)	\
	$(MFC_CORE)		\
	$(MFC_CORE_AVC)		\
	$(NULL)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
