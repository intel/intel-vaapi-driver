VME_CORE	= batchbuffer.asm intra_frame.asm inter_frame.asm
VME7_CORE	= batchbuffer.asm intra_frame_ivb.asm inter_frame_ivb.asm inter_bframe_ivb.asm mpeg2_inter_ivb.asm
VME75_CORE	= batchbuffer.asm intra_frame_haswell.asm inter_frame_haswell.asm inter_bframe_haswell.asm mpeg2_inter_haswell.asm

INTEL_G6B	= batchbuffer.g6b intra_frame.g6b inter_frame.g6b
INTEL_G6A	= batchbuffer.g6a intra_frame.g6a inter_frame.g6a
INTEL_GEN6_INC	= batchbuffer.inc vme.inc
INTEL_GEN6_ASM	= $(INTEL_G6A:%.g6a=%.gen6.asm)

INTEL_G7B	= batchbuffer.g7b intra_frame.g7b inter_frame.g7b intra_frame_ivb.g7b inter_frame_ivb.g7b inter_bframe_ivb.g7b mpeg2_inter_ivb.g7b
INTEL_G7A	= batchbuffer.g7a intra_frame.g7a inter_frame.g7a intra_frame_ivb.g7a inter_frame_ivb.g7a inter_bframe_ivb.g7a mpeg2_inter_ivb.g7a
INTEL_GEN7_INC	= batchbuffer.inc vme.inc vme7_mpeg2.inc vme7.inc
INTEL_GEN7_ASM	= $(INTEL_G7A:%.g7a=%.gen7.asm)

INTEL_G75B	= batchbuffer.g75b intra_frame_haswell.g75b inter_frame_haswell.g75b inter_bframe_haswell.g75b mpeg2_inter_haswell.g75b
INTEL_G75A	= batchbuffer.g75a intra_frame_haswell.g75a inter_frame_haswell.g75a inter_bframe_haswell.g75a mpeg2_inter_haswell.g75a
INTEL_GEN75_INC	= batchbuffer.inc vme75.inc vme75_mpeg2.inc
INTEL_GEN75_ASM	= $(INTEL_G75A:%.g75a=%.gen75.asm)

TARGETS  =
if HAVE_GEN4ASM
TARGETS += $(INTEL_G6B)
TARGETS += $(INTEL_G7B)
TARGETS += $(INTEL_G75B)
endif

all-local: $(TARGETS)

SUFFIXES = .g6a .g6b .g7a .g7b .gen6.asm .gen7.asm .g75a .g75b .gen75.asm

if HAVE_GEN4ASM
$(INTEL_GEN6_ASM): $(VME_CORE) $(INTEL_GEN6_INC)
.g6a.gen6.asm:
	$(AM_V_GEN)cpp -P -DDEV_SNB $< > _vme0.$@ 	&& \
	m4 _vme0.$@ > $@				&& \
	rm _vme0.$@
.gen6.asm.g6b:
	$(AM_V_GEN)$(GEN4ASM) -g 6 -o $@ $<

$(INTEL_GEN7_ASM): $(VME_CORE) $(INTEL_GEN7_INC)
.g7a.gen7.asm:
	$(AM_V_GEN)cpp -P -DDEV_IVB $< > _vme0.$@ 	&& \
	m4 _vme0.$@ > $@				&& \
	rm _vme0.$@
.gen7.asm.g7b:
	$(AM_V_GEN)$(GEN4ASM) -g 7 -o $@ $<


$(INTEL_GEN75_ASM): $(VME75_CORE) $(INTEL_GEN75_INC)
.g75a.gen75.asm:
	$(AM_V_GEN)cpp -P $< > _vme0.$@ 	&& \
	m4 _vme0.$@ > $@				&& \
	rm _vme0.$@
.gen75.asm.g75b:
	$(AM_V_GEN)$(GEN4ASM) -g 7.5 -o $@ $<
endif

CLEANFILES = $(INTEL_GEN6_ASM) $(INTEL_GEN7_ASM) $(INTEL_GEN75_ASM)

EXTRA_DIST = \
	$(INTEL_G6A)		\
	$(INTEL_G6B)		\
	$(INTEL_G75A)		\
	$(INTEL_G75B)		\
	$(INTEL_G7A)		\
	$(INTEL_G7B)		\
	$(INTEL_GEN6_ASM)	\
	$(INTEL_GEN6_INC)	\
	$(INTEL_GEN75_ASM)	\
	$(INTEL_GEN75_INC)	\
	$(INTEL_GEN7_ASM)	\
	$(INTEL_GEN7_INC)	\
	$(VME75_CORE)		\
	$(VME_CORE)		\
	$(NULL)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
