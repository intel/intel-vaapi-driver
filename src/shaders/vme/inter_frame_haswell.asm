/*
 * Copyright © <2010>, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Eclipse Public License (EPL), version 1.0.  The full text of the EPL is at
 * http://www.opensource.org/licenses/eclipse-1.0.php.
 *
 */
// Modual name: IntraFrame.asm
//
// Make intra predition estimation for Intra frame
//

//
//  Now, begin source code....
//

/*
 * __START
 */
__INTRA_START:
mov  (16) tmp_reg0.0<1>:UD      0x0:UD {align1};
mov  (16) tmp_reg2.0<1>:UD      0x0:UD {align1};
mov  (16) tmp_reg4.0<1>:UD      0x0:UD {align1} ;
mov  (16) tmp_reg6.0<1>:UD      0x0:UD {align1} ;

shl  (2) read0_header.0<1>:D    orig_xy_ub<2,2,1>:UB 4:UW {align1};    /* (x, y) * 16 */
add  (1) read0_header.0<1>:D    read0_header.0<0,1,0>:D -8:W {align1};     /* X offset */
add  (1) read0_header.4<1>:D    read0_header.4<0,1,0>:D -1:W {align1};     /* Y offset */ 
mov  (1) read0_header.8<1>:UD   BLOCK_32X1 {align1};
mov  (1) read0_header.20<1>:UB  thread_id_ub {align1};                  /* dispatch id */

shl  (2) read1_header.0<1>:D    orig_xy_ub<2,2,1>:UB 4:UW {align1};    /* (x, y) * 16 */
add  (1) read1_header.0<1>:D    read1_header.0<0,1,0>:D -4:W {align1};     /* X offset */
mov  (1) read1_header.8<1>:UD   BLOCK_4X16 {align1};
mov  (1) read1_header.20<1>:UB  thread_id_ub {align1};                  /* dispatch id */
        
shl  (2) vme_m0.8<1>:UW         orig_xy_ub<2,2,1>:UB 4:UW {align1};    /* (x, y) * 16 */
mov  (1) vme_m0.20<1>:UB        thread_id_ub {align1};                  /* dispatch id */

mul  (1) obw_m0.8<1>:UD         w_in_mb_uw<0,1,0>:UW orig_y_ub<0,1,0>:UB {align1};
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD orig_x_ub<0,1,0>:UB {align1};
mul  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 24:UD {align1};
mov  (1) obw_m0.20<1>:UB        thread_id_ub {align1};                  /* dispatch id */
        
/*
 * Media Read Message -- fetch Luma neighbor edge pixels 
 */
/* ROW */
mov  (8) msg_reg0.0<1>:UD       read0_header.0<8,8,1>:UD {align1};        
send (8) msg_ind INEP_ROW<1>:UB null read(BIND_IDX_INEP, 0, 0, 4) mlen 1 rlen 1 {align1};

/* COL */
mov  (8) msg_reg0.0<1>:UD       read1_header.0<8,8,1>:UD {align1};                
send (8) msg_ind INEP_COL0<1>:UB null read(BIND_IDX_INEP, 0, 0, 4) mlen 1 rlen 2 {align1};
        
/* m2, get the MV/Mb cost passed from constant buffer when
spawning thread by MEDIA_OBJECT */       
mov (8) vme_m2<1>:UD            r1.0<8,8,1>:UD {align1};

mov (8) vme_msg_2<1>:UD		vme_m2.0<8,8,1>:UD {align1};

/* m3 */
mov (8) vme_msg_3<1>:UD		0x0:UD {align1};	        

/* m4 */
mov  (1) INEP_ROW.0<1>:UD       0x0:UD {align1};
and  (1) INEP_ROW.4<1>:UD       INEP_ROW.4<0,1,0>:UD            0xFF000000:UD {align1};
mov  (8) vme_msg_4<1>:UD         INEP_ROW.0<8,8,1>:UD {align1};

/* m5 */        
mov  (8) vme_msg_5<1>:UD         0x0:UD {align1};
mov (16) vme_msg_5.0<1>:UB       INEP_COL0.3<32,8,4>:UB {align1};
mov  (1) vme_msg_5.16<1>:UD      INTRA_PREDICTORE_MODE {align1};

/* the penalty for Intra mode */
mov  (1) vme_msg_5.28<1>:UD	0x010101:UD {align1};


/* m6 */

mov (8) vme_msg_6<1>:UD		0x0:UD {align1};	        

/*
 * SIC VME message
 */
/* m0 */        
mov  (8) vme_msg_0.0<1>:UD      vme_m0.0<8,8,1>:UD {align1};
mov  (1) tmp_reg0.0<1>:UW	LUMA_INTRA_MODE:UW {align1};
/* Use the Luma mode */
mov  (1) vme_msg_4.5<1>:UB	tmp_reg0.0<0,1,0>:UB {align1};

/* m1 */
mov  (1) intra_flag<1>:UW       0x0:UW {align1}                     ;
and.z.f0.0 (1) null<1>:UW transform_8x8_ub<0,1,0>:UB 1:UW {align1};
(f0.0) mov  (1) intra_part_mask_ub<1>:UB  LUMA_INTRA_8x8_DISABLE {align1};

/* assign MB intra struct from the thread payload*/
mov (1) mb_intra_struct_ub<1>:UB input_mb_intra_ub<0,1,0>:UB {align1}; 

/* Disable DC HAAR component when calculating HARR SATD block */
mov  (1) tmp_reg0.0<1>:UW	DC_HARR_DISABLE:UW		{align1};
mov  (1) vme_m1.30<1>:UB	tmp_reg0.0<0,1,0>:UB  {align1};

mov  (1) vme_m0.12<1>:UD        INTRA_SAD_HAAR:UD {align1};    /* 16x16 Source, Intra_harr */
/* m0 */        
mov  (8) vme_msg_0.0<1>:UD      vme_m0.0<8,8,1>:UD {align1};
mov  (8) vme_msg_1<1>:UD        vme_m1.0<8,8,1>:UD {align1};

/* after verification it will be passed by using payload */
send (8)
        vme_msg_ind
        vme_wb<1>:UD
        null
        cre(
                BIND_IDX_VME,
                VME_SIC_MESSAGE_TYPE
        )
        mlen sic_vme_msg_length
        rlen vme_wb_length
        {align1};
/*
 * Oword Block Write message
 */
mov  (8) msg_reg0.0<1>:UD       obw_m0<8,8,1>:UD {align1};
        
mov  (1) msg_reg1.0<1>:UD       vme_wb.0<0,1,0>:UD      {align1};
mov  (1) msg_reg1.4<1>:UD       vme_wb.16<0,1,0>:UD     {align1};
mov  (1) msg_reg1.8<1>:UD       vme_wb.20<0,1,0>:UD     {align1};
mov  (1) msg_reg1.12<1>:UD      vme_wb.24<0,1,0>:UD     {align1};

/* Distortion, Intra (17-16), */
mov  (1) msg_reg1.16<1>:UW      vme_wb.12<0,1,0>:UW     {align1};

mov  (1) msg_reg1.20<1>:UD      vme_wb.8<0,1,0>:UD     {align1};
/* VME clock counts */
mov  (1) msg_reg1.24<1>:UD      vme_wb.28<0,1,0>:UD     {align1};

mov  (1) msg_reg1.28<1>:UD      obw_m0.8<0,1,0>:UD     {align1};

/* bind index 3, write 2 oword (32bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_2,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 2
        rlen obw_wb_length
        {align1};

/* IME search */
mov  (1) vme_m0.12<1>:UD        SEARCH_CTRL_SINGLE + INTER_PART_MASK + INTER_SAD_HAAR:UD {align1};    /* 16x16 Source, harr */
mov  (1) vme_m0.22<1>:UW        REF_REGION_SIZE {align1};         /* Reference Width&Height, 48x40 */

mov  (1) vme_m0.0<1>:UD		vme_m0.8<0,1,0>:UD      {align1};

add  (1) vme_m0.0<1>:W          vme_m0.0<0,1,0>:W -16:W {align1};		/* Reference = (x-16,y-12)-(x+32,y+28) */
add  (1) vme_m0.2<1>:W          vme_m0.2<0,1,0>:W -12:W {align1};

mov  (1) vme_m0.0<1>:W		-16:W			{align1};
mov  (1) vme_m0.2<1>:W		-12:W			{align1};

mov  (1) vme_m0.4<1>:UD		vme_m0.0<0,1,0>:UD	{align1};

mov  (8) vme_msg_0.0<1>:UD      vme_m0.0<8,8,1>:UD {align1};

mov  (1) vme_m1.0<1>:UD         ADAPTIVE_SEARCH_ENABLE:ud {align1} ;
mov  (1) vme_m1.4<1>:UD         MAX_NUM_MV:UD {align1};                                   /* Default value MAX 32 MVs */
mov  (1) vme_m1.8<1>:UD         START_CENTER + SEARCH_PATH_LEN:UD {align1};
mov  (8) vme_msg_1.0<1>:UD      vme_m1.0<8,8,1>:UD {align1};

mov (8) vme_msg_2<1>:UD		vme_m2.0<8,8,1>:UD {align1};
/* M3/M4 search path */

mov  (1) vme_msg_3.0<1>:UD	0x01010101:UD {align1};
mov  (1) vme_msg_3.4<1>:UD	0x10010101:UD {align1};
mov  (1) vme_msg_3.8<1>:UD	0x0F0F0F0F:UD {align1};
mov  (1) vme_msg_3.12<1>:UD	0x100F0F0F:UD {align1};
mov  (1) vme_msg_3.16<1>:UD	0x01010101:UD {align1};
mov  (1) vme_msg_3.20<1>:UD	0x10010101:UD {align1};
mov  (1) vme_msg_3.24<1>:UD	0x0F0F0F0F:UD {align1};
mov  (1) vme_msg_3.28<1>:UD	0x100F0F0F:UD {align1};

mov  (1) vme_msg_4.0<1>:UD	0x01010101:UD {align1};
mov  (1) vme_msg_4.4<1>:UD	0x10010101:UD {align1};
mov  (1) vme_msg_4.8<1>:UD	0x0F0F0F0F:UD {align1};
mov  (1) vme_msg_4.12<1>:UD	0x000F0F0F:UD {align1};

mov  (4) vme_msg_4.16<1>:UD	0x0:UD {align1};

send (8)
        vme_msg_ind
        vme_wb<1>:UD
        null
        vme(
                BIND_IDX_VME,
                0,
                0,
                VME_IME_MESSAGE_TYPE
        )
        mlen ime_vme_msg_length
        rlen vme_wb_length {align1};

/* Set Macroblock-shape/mode for FBR */

mov  (1) vme_m2.20<1>:UD	0x0:UD {align1};
mov  (1) vme_m2.21<1>:UB	vme_wb.25<0,1,0>:UB	{align1};
mov  (1) vme_m2.22<1>:UB	vme_wb.26<0,1,0>:UB	{align1};

and  (1) tmp_reg0.0<1>:UW	vme_wb.0<0,1,0>:UW	0x03:UW {align1};
mov  (1) vme_m2.20<1>:UB	tmp_reg0.0<0,1,0>:UB    {align1};

/* Write IME inter info */
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 0x02:UD {align1};
mov  (8) msg_reg0.0<1>:UD       obw_m0<8,8,1>:UD {align1};

mov  (1) msg_reg1.0<1>:UD       vme_wb.0<0,1,0>:UD      {align1};

mov  (1) msg_reg1.4<1>:UD       vme_wb.24<0,1,0>:UD     {align1};
/* Inter distortion of IME */
mov  (1) msg_reg1.8<1>:UD       vme_wb.8<0,1,0>:UD     {align1};

mov  (1) msg_reg1.12<1>:UD	obw_m0.8<0,1,0>:UD {align1};

/* bind index 3, write  oword (16bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_0,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 2
        rlen obw_wb_length
        {align1};

/* Write IME MV */
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 0x01:UD {align1};
mov  (8) msg_reg0.0<1>:UD       obw_m0<8,8,1>:UD {align1};

mov  (8) msg_reg1.0<1>:UD       vme_wb1.0<8,8,1>:UD {align1};
mov  (8) msg_reg2.0<1>:ud       vme_wb2.0<8,8,1>:ud {align1};
mov  (8) msg_reg3.0<1>:ud       vme_wb3.0<8,8,1>:ud {align1};
mov  (8) msg_reg4.0<1>:ud       vme_wb4.0<8,8,1>:ud {align1};                
/* bind index 3, write  8 oword (128 bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_8,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 5
        rlen obw_wb_length
        {align1};

/* Write IME RefID */
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 0x08:UD {align1};
mov  (8) msg_reg0.0<1>:UD       obw_m0<8,8,1>:UD {align1};

mov  (8) msg_reg1.0<1>:UD	vme_wb6.0<8,8,1>:UD {align1};

/* bind index 3, write 2 oword (32bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_2,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 2
        rlen obw_wb_length
        {align1};

/* Send FBR message into CRE */

mov  (8) vme_msg_3.0<1>:UD       vme_wb1.0<8,8,1>:UD {align1};
mov  (8) vme_msg_4.0<1>:ud       vme_wb2.0<8,8,1>:ud {align1};
mov  (8) vme_msg_5.0<1>:ud       vme_wb3.0<8,8,1>:ud {align1};
mov  (8) vme_msg_6.0<1>:ud       vme_wb4.0<8,8,1>:ud {align1};                

mov  (1) vme_m0.12<1>:UD	INTER_SAD_HAAR + SUB_PEL_MODE_QUARTER + FBR_BME_DISABLE:UD {align1};    /* 16x16 Source, 1/4 pixel, harr, BME disable */
mov  (8) vme_msg_0.0<1>:UD	vme_m0.0<8,8,1>:UD  {align1};
mov  (8) vme_msg_1.0<1>:UD	vme_m1.0<8,8,1>:UD  {align1};

mov  (8) vme_msg_2.0<1>:UD		vme_m2.0<8,8,1>:UD	{align1};

/* after verification it will be passed by using payload */
send (8)
        vme_msg_ind
        vme_wb<1>:UD
        null
        cre(
                BIND_IDX_VME,
                VME_FBR_MESSAGE_TYPE
        )
        mlen fbr_vme_msg_length
        rlen vme_wb_length
        {align1};

add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 0x02:UD {align1};
mov  (8) msg_reg0.0<1>:UD       obw_m0<8,8,1>:UD {align1};
/* write FME info */
mov  (1) msg_reg1.0<1>:UD       vme_wb.0<0,1,0>:UD      {align1};

mov  (1) msg_reg1.4<1>:UD       vme_wb.24<0,1,0>:UD     {align1};
/* Inter distortion of FME */
mov  (1) msg_reg1.8<1>:UD       vme_wb.8<0,1,0>:UD     {align1};

mov  (1) msg_reg1.12<1>:UD	vme_m2.20<0,1,0>:UD {align1};

/* bind index 3, write  oword (16bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_0,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 2
        rlen obw_wb_length
        {align1};

/* Write FME/BME MV */
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 0x01:UD {align1};
mov  (8) msg_reg0.0<1>:UD       obw_m0.0<8,8,1>:UD {align1};


mov  (8) msg_reg1.0<1>:UD       vme_wb1.0<8,8,1>:UD {align1};
mov  (8) msg_reg2.0<1>:ud       vme_wb2.0<8,8,1>:ud {align1};
mov  (8) msg_reg3.0<1>:ud       vme_wb3.0<8,8,1>:ud {align1};
mov  (8) msg_reg4.0<1>:ud       vme_wb4.0<8,8,1>:ud {align1};                
/* bind index 3, write  8 oword (128 bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_8,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 5
        rlen obw_wb_length
        {align1};

/* Write FME/BME RefID */
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD 0x08:UD {align1};
mov  (8) msg_reg0.0<1>:UD       obw_m0<8,8,1>:UD {align1};

mov  (8) msg_reg1.0<1>:UD	vme_wb6.0<8,8,1>:UD {align1};

/* bind index 3, write 2 oword (32bytes), msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_2,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 2
        rlen obw_wb_length
        {align1};

__EXIT: 
/*
 * kill thread
 */        
mov  (8) ts_msg_reg0<1>:UD         r0<8,8,1>:UD {align1};
send (16) ts_msg_ind acc0<1>UW null thread_spawner(0, 0, 1) mlen 1 rlen 0 {align1 EOT};
