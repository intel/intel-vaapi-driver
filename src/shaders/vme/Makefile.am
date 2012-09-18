VME_CORE	= intra_frame.asm inter_frame.asm
VME75_CORE	= intra_frame_haswell.asm inter_frame_haswell.asm

INTEL_G6B	= intra_frame.g6b inter_frame.g6b
INTEL_G6A	= intra_frame.g6a inter_frame.g6a
INTEL_GEN6_INC	= gen6_vme_header.inc
INTEL_GEN6_ASM	= $(INTEL_G6A:%.g6a=%.gen6.asm)

INTEL_G7B	= intra_frame.g7b inter_frame.g7b
INTEL_G7A	= intra_frame.g7a inter_frame.g7a
INTEL_GEN7_INC	= gen7_vme_header.inc
INTEL_GEN7_ASM	= $(INTEL_G7A:%.g7a=%.gen7.asm)

INTEL_G75B	= intra_frame_haswell.g75b inter_frame_haswell.g75b
INTEL_G75A	= intra_frame_haswell.g75a inter_frame_haswell.g75a
INTEL_GEN75_INC	= vme75.inc
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
	$(AM_V_GEN)m4 $< > $@
.gen6.asm.g6b:
	$(AM_V_GEN)$(GEN4ASM) -g 6 -o $@ $<

$(INTEL_GEN7_ASM): $(VME_CORE) $(INTEL_GEN7_INC)
.g7a.gen7.asm:
	$(AM_V_GEN)m4 $< > $@
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
	$(INTEL_G7A)		\
	$(INTEL_G7B)		\
	$(INTEL_G75A)		\
	$(INTEL_G75B)		\
	$(INTEL_GEN6_INC)	\
	$(INTEL_GEN7_INC)	\
	$(INTEL_GEN75_INC)	\
	$(VME_CORE)		\
	$(VME75_CORE)		\
	$(NULL)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
