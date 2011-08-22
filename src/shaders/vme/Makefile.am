VME_CORE	= intra_frame.asm inter_frame.asm

INTEL_G6B	= intra_frame.g6b inter_frame.g6b
INTEL_G6A	= intra_frame.g6a inter_frame.g6a
INTEL_GEN6_INC	= gen6_vme_header.inc
INTEL_GEN6_ASM	= $(INTEL_G6A:%.g6a=%.gen6.asm)

INTEL_G7B	= intra_frame.g7b inter_frame.g7b
INTEL_G7A	= intra_frame.g7a inter_frame.g7a
INTEL_GEN7_INC	= gen7_vme_header.inc
INTEL_GEN7_ASM	= $(INTEL_G7A:%.g7a=%.gen7.asm)

TARGETS  =
if HAVE_GEN4ASM
TARGETS += $(INTEL_G6B)
TARGETS += $(INTEL_G7B)
endif

all-local: $(TARGETS)

SUFFIXES = .g6a .g6b .g7a .g7b .gen6.asm .gen7.asm

$(INTEL_GEN6_ASM): $(VME_CORE) $(INTEL_GEN6_INC)
.g6a.gen6.asm:
	m4 $< > $@
.gen6.asm.g6b:
	intel-gen4asm -g 6 -o $@ $<

$(INTEL_GEN7_ASM): $(VME_CORE) $(INTEL_GEN7_INC)
.g7a.gen7.asm:
	m4 $< > $@
.gen7.asm.g7b:
	intel-gen4asm -g 7 -o $@ $<

CLEANFILES = $(INTEL_GEN6_ASM) $(INTEL_GEN7_ASM)

EXTRA_DIST = \
	$(INTEL_G6A)		\
	$(INTEL_G6B)		\
	$(INTEL_G7A)		\
	$(INTEL_G7B)		\
	$(INTEL_GEN6_INC)	\
	$(INTEL_GEN7_INC)	\
	$(VME_CORE)		\
	$(NULL)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
