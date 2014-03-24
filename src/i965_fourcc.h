#ifndef _I965_FOURCC_H_
#define _I965_FOURCC_H_

#ifndef VA_FOURCC_YV16
#define VA_FOURCC_YV16 VA_FOURCC('Y','V','1','6')
#endif

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420 VA_FOURCC('I','4','2','0')
#endif

/*
 * VA_FOURCC_IA44 is an exception because the va.h already
 * defines the AI44 as VA_FOURCC('I', 'A', '4', '4').
 */
#ifndef VA_FOURCC_IA44
#define VA_FOURCC_IA44 VA_FOURCC('A','I','4','4')
#endif

#ifndef VA_FOURCC_IA88
#define VA_FOURCC_IA88 VA_FOURCC('I','A','8','8')
#endif

#ifndef VA_FOURCC_AI88
#define VA_FOURCC_AI88 VA_FOURCC('A','I','8','8')
#endif

#ifndef VA_FOURCC_IMC1
#define VA_FOURCC_IMC1 VA_FOURCC('I','M','C','1')
#endif

#ifndef VA_FOURCC_YVY2
#define VA_FOURCC_YVY2 VA_FOURCC('Y','V','Y','2')
#endif

#endif /* _I965_FOURCC_H_ */
