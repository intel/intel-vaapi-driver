#ifndef __I965_ENCODER_UTILS_H__
#define __I965_ENCODER_UTILS_H__

int 
build_avc_slice_header(VAEncSequenceParameterBufferH264 *sps_param,
                       VAEncPictureParameterBufferH264 *pic_param,
                       VAEncSliceParameterBufferH264 *slice_param,
                       VAEncH264DecRefPicMarkingBuffer *dec_ref_pic_marking_param,
                       unsigned char **slice_header_buffer);

#endif /* __I965_ENCODER_UTILS_H__ */
