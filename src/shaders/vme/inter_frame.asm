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
__INTER_START:
mov  (16) tmp_reg0.0<1>:UD      0x0:UD {align1};
mov  (16) tmp_reg2.0<1>:UD      0x0:UD {align1};
mov  (16) tmp_reg3.0<1>:UD      0x0:UD {align1};

shl  (2) vme_m0.8<1>:UW         orig_xy_ub<2,2,1>:UB 4:UW {align1};    /* Source =  (x, y) * 16 */
        
#ifdef DEV_SNB
shl  (2) vme_m0.0<1>:UW         orig_xy_ub<2,2,1>:UB 4:UW {align1};	
add  (1) vme_m0.0<1>:W          vme_m0.0<0,1,0>:W -16:W {align1};		/* Reference = (x-16,y-12)-(x+32,y+24) */
add  (1) vme_m0.2<1>:W          vme_m0.2<0,1,0>:W -12:W {align1};
#else
mov  (1) vme_m0.0<1>:W          -16:W {align1} ;                /* Reference = (x-16,y-12)-(x+32,y+24) */
mov  (1) vme_m0.2<1>:W          -12:W {align1} ;
#endif
        
mov  (1) vme_m0.12<1>:UD        SEARCH_CTRL_DUAL_START + INTER_PART_MASK + INTER_SAD_HAAR + SUB_PEL_MODE_QUARTER:UD {align1};    /* 16x16 Source, 1/4 pixel, harr */
mov  (1) vme_m0.20<1>:UB        thread_id_ub {align1};                  /* dispatch id */
mov  (1) vme_m0.22<1>:UW        REF_REGION_SIZE {align1};               /* Reference Width&Height, 32x32 */

mov  (1) vme_m1.0<1>:UD         ADAPTIVE_SEARCH_ENABLE:ud {align1} ;
mov  (1) vme_m1.4<1>:UD         FB_PRUNING_ENABLE + MAX_NUM_MV:UD {align1};                                   /* Default value MAX 32 MVs */
mov  (1) vme_m1.8<1>:UD         START_CENTER + SEARCH_PATH_LEN:UD {align1};

mul  (1) obw_m0.8<1>:UD         w_in_mb_uw<0,1,0>:UW orig_y_ub<0,1,0>:UB {align1};
add  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD orig_x_ub<0,1,0>:UB {align1};
mul  (1) obw_m0.8<1>:UD         obw_m0.8<0,1,0>:UD INTER_VME_OUTPUT_IN_OWS:UD {align1};
mov  (1) obw_m0.20<1>:UB        thread_id_ub {align1};                  /* dispatch id */
        
/*
 * VME message
 */
/* m0 */
__VME_LOOP:     
mov  (8) vme_msg_0.0<1>:UD      vme_m0.0<8,8,1>:UD {align1};
        
/* m1 */
mov  (8) vme_msg_1<1>:UD        vme_m1.0<8,8,1>:UD {align1};
        
/* m2 */        
mov  (8) vme_msg_2<1>:UD        0x0:UD {align1};

/* m3 */        
mov  (8) vme_msg_3<1>:UD        0x0:UD {align1};

/* m4 */        
mov  (8) vme_msg_4<1>:UD        0x0:UD {align1};

send (8)
        vme_msg_ind
        vme_wb
        null
        vme(
                BIND_IDX_VME,
                0,
                0,
                VME_MESSAGE_TYPE_INTER
        )
        mlen vme_msg_length
        rlen vme_inter_wb_length
        {align1};
/*
 * Oword Block Write message
 */

/* MV pairs */        
mov  (8) msg_reg0.0<1>:UD       obw_m0.0<8,8,1>:UD {align1};

#ifdef DEV_SNB        
mov  (16) obw_m1.0<1>:UW        vme_wb1.0<16,16,1>:UB  {align1};
add  (8) obw_m1.0<2>:W          obw_m1.0<16,8,2>:W -64:W {align1};
add  (8) obw_m1.2<2>:W          obw_m1.2<16,8,2>:W -48:W {align1};
mov  (16) obw_m2.0<1>:UW        vme_wb1.16<16,16,1>:UB  {align1};
add  (8) obw_m2.0<2>:W          obw_m2.0<16,8,2>:W -64:W {align1};
add  (8) obw_m2.2<2>:W          obw_m2.2<16,8,2>:W -48:W {align1}; 
mov  (16) obw_m3.0<1>:UW        vme_wb2.0<16,16,1>:UB  {align1};
add  (8) obw_m3.0<2>:W          obw_m3.0<16,8,2>:W -64:W {align1};
add  (8) obw_m3.2<2>:W          obw_m3.2<16,8,2>:W -48:W {align1};
mov  (16) obw_m4.0<1>:UW        vme_wb2.16<16,16,1>:UB  {align1};
add  (8) obw_m4.0<2>:W          obw_m4.0<16,8,2>:W -64:W {align1};
add  (8) obw_m4.2<2>:W          obw_m4.2<16,8,2>:W -48:W {align1}; 
#else
mov  (16) obw_m1.0<1>:UW        vme_wb1.0<16,16,1>:B  {align1};
mov  (16) obw_m2.0<1>:UW        vme_wb1.16<16,16,1>:B  {align1};
mov  (16) obw_m3.0<1>:UW        vme_wb2.0<16,16,1>:B  {align1};
mov  (16) obw_m4.0<1>:UW        vme_wb2.16<16,16,1>:B  {align1};                
#endif       
        
mov  (8) msg_reg1.0<1>:UD       obw_m1.0<8,8,1>:UD   {align1};

mov  (8) msg_reg2.0<1>:UD       obw_m2.0<8,8,1>:UD   {align1};

mov  (8) msg_reg3.0<1>:UD       obw_m3.0<8,8,1>:UD   {align1};

mov  (8) msg_reg4.0<1>:UD       obw_m4.0<8,8,1>:UD   {align1};                

/* bind index 3, write 8 oword, msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_4,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 5
        rlen obw_wb_length
        {align1};

/* other info */        
add             (1)     msg_reg0.8<1>:UD        obw_m0.8<0,1,0>:UD      INTER_VME_OUTPUT_MV_IN_OWS:UD {align1} ;

mov             (1)     tmp_uw1<1>:uw           0:uw {align1} ;
mov             (1)     tmp_ud1<1>:ud           0:ud {align1} ;
and.z.f0.0      (1)     null<1>:ud              vme_wb0.0<0,1,0>:ud     INTRAMBFLAG_MASK:ud {align1} ;
(f0.0)and       (1)     tmp_uw1<1>:uw           vme_wb0.2<0,1,0>:uw     MV32_BIT_MASK:uw {align1} ;
(f0.0)shr       (1)     tmp_uw1<1>:uw           tmp_uw1<1>:uw           MV32_BIT_SHIFT:uw {align1} ;
(f0.0)mul       (1)     tmp_ud1<1>:ud           tmp_uw1<0,1,0>:uw       96:uw {align1} ;
(f0.0)add       (1)     tmp_ud1<1>:ud           tmp_ud1<0,1,0>:ud       32:uw {align1} ;
(f0.0)shl       (1)     tmp_uw1<1>:uw           tmp_uw1<0,1,0>:uw       MFC_MV32_BIT_SHIFT:uw {align1} ;
(f0.0)add       (1)     tmp_uw1<1>:uw           tmp_uw1<0,1,0>:uw       MVSIZE_UW_BASE:uw {align1} ;
add             (1)     tmp_uw1<1>:uw           tmp_uw1<0,1,0>:uw       CBP_DC_YUV_UW:uw {align1} ;

mov             (1)     msg_reg1.0<1>:uw        vme_wb0.0<0,1,0>:uw     {align1} ;
mov             (1)     msg_reg1.2<1>:uw        tmp_uw1<0,1,0>:uw       {align1} ;
mov             (1)     msg_reg1.4<1>:UD        vme_wb0.28<0,1,0>:UD    {align1};
mov             (1)     msg_reg1.8<1>:ud        tmp_ud1<0,1,0>:ud       {align1} ;
        
/* bind index 3, write 1 oword, msg type: 8(OWord Block Write) */
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

add             (1)     orig_x_ub<1>:ub         orig_x_ub<0,1,0>:ub             1:uw {align1} ;
add             (1)     vme_m0.8<1>:UW          vme_m0.8<0,1,0>:UW              16:UW {align1};    /* X += 16 */
#ifdef DEV_SNB        
add             (1)     vme_m0.0<1>:W           vme_m0.0<0,1,0>:W               16:W {align1};	   /* X += 16 */
#endif

cmp.e.f0.0      (1)     null<1>:uw              w_in_mb_uw<0,1,0>:uw            orig_x_ub<0,1,0>:ub {align1};
/* (0, y + 1) */        
(f0.0)mov       (1)     orig_x_ub<1>:ub         0:uw {align1} ;
(f0.0)mov       (1)     vme_m0.8<1>:uw          0:uw {align1} ;
(f0.0)add       (1)     vme_m0.10<1>:uw         vme_m0.10<0,1,0>:uw             16:uw {align1} ;
#ifdef DEV_SNB        
(f0.0)mov       (1)     vme_m0.0<1>:w           -16:W {align1};		        /* Reference = (x-16,y-12)-(x+32,y+24) */
(f0.0)add       (1)     vme_m0.2<1>:w           vme_m0.2<0,1,0>:w               16:w {align1};
#endif

add             (1)     obw_m0.8<1>:UD          obw_m0.8<0,1,0>:UD              INTER_VME_OUTPUT_IN_OWS:UW {align1} ;
        
add.z.f0.1      (1)     num_macroblocks<1>:w    num_macroblocks<0,1,0>:w        -1:w {align1} ;
(-f0.1)jmpi     (1)     __VME_LOOP ;
        
__EXIT: 
        
/*
 * kill thread
 */        
mov  (8) msg_reg0<1>:UD         r0<8,8,1>:UD {align1};
send (16) msg_ind acc0<1>UW null thread_spawner(0, 0, 1) mlen 1 rlen 0 {align1 EOT};
