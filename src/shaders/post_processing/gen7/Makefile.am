INTEL_PP_G7B =	 		\
	dndi.g7b		\
	avs.g7b

INTEL_PP_G4A =			\
	DI_Core.g4a	\
	DI_Save_NV12_16x4.g4a\
	DI_Save_PA_16x4.g4a\
	EOT.g4a\
	VP_Setup.g4a\
	Set_Layer_0.g4a\
	PA_AVS_Buf_0.g4a\
	PA_AVS_Buf_1.g4a\
	PA_AVS_Buf_2.g4a\
	PA_AVS_Buf_3.g4a\
	PL2_AVS_Buf_0.g4a\
	PL2_AVS_Buf_1.g4a\
	PL2_AVS_Buf_2.g4a\
	PL2_AVS_Buf_3.g4a\
	PL3_AVS_Buf_0.g4a\
	PL3_AVS_Buf_1.g4a \
	PL3_AVS_Buf_2.g4a\
	PL3_AVS_Buf_3.g4a\
	Set_AVS_Buf_0123_VYUA.g4a\
	Save_AVS_RGB.g4a\
	Save_AVS_PA.g4a  \
	Save_AVS_PL3.g4a\
	Save_AVS_NV12.g4a\
	Set_AVS_Buf_0123_BGRA.g4a\
	Set_AVS_Buf_0123_PL2.g4a\
	Set_AVS_Buf_0123_PL3.g4a\
	Set_AVS_Buf_0123_VUYA.g4a

EXTRA_DIST = 	\
	$(INTEL_PP_G7B)

if HAVE_GEN4ASM

$(INTEL_PP_G7B): $(INTEL_PP_G4A)
	@_PP_TARGET=$@; \
	cpp $${_PP_TARGET/.g7b/.asm} > _pp0.asm; \
	../../gpp.py _pp0.asm _pp1.asm;	\
	intel-gen4asm -a -o $@ -g 7 _pp1.asm; \
	rm _pp0.asm _pp1.asm

BUILT_SOURCES= $(INTEL_PP_G7B)

clean-local:
	-rm -f $(INTEL_PP_G7B)

endif    
