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

/*
 * VME message
 */
/* m0 */        
mul  (2) tmp_reg0.8<1>:UW       orig_xy_ub<2,2,1>:UB 16:UW {align1};    /* Source =  (x, y) * 16 */
mul  (2) tmp_reg0.0<1>:UW       orig_xy_ub<2,2,1>:UB 16:UW {align1};	
add  (1) tmp_reg0.0<1>:W        tmp_reg0.0<2,2,1>:W -16:W {align1};		/* Reference = (x-16,y-12)-(x+32,y+24) */
add  (1) tmp_reg0.2<1>:W        tmp_reg0.2<2,2,1>:W -12:W {align1};
mov  (1) tmp_reg0.12<1>:UD      INTER_PART_MASK + INTER_SAD_HAAR + SUB_PEL_MODE_QUARTER:UD {align1};    /* 16x16 Source, 1/4 pixel, harr */

mov  (1) tmp_reg0.20<1>:UB      thread_id_ub {align1};                  /* dispatch id */
mov  (1) tmp_reg0.22<1>:UW      REF_REGION_SIZE {align1};               /* Reference Width&Height, 32x32 */
mov  (8) vme_msg_0.0<1>:UD      tmp_reg0.0<8,8,1>:UD {align1};
        
/* m1 */
mov  (1) tmp_reg1.0<1>:UD       0x00002803:UD {align1};     /*Enable adapitive search and skip enable*/
mov  (1) tmp_reg1.4<1>:UD       0x20100010:UD {align1};     /*16 MVs */
mov  (1) tmp_reg1.8<1>:UD       0x00003F3F:UD {align1};     /*Max searhc path length is 63*/
mov  (1) tmp_reg1.12<1>:UD      0x00000000:UD {align1};     
mov  (1) tmp_reg1.16<1>:UD      0x00300040:UD {align1};
mov  (1) tmp_reg1.20<1>:UD      0x00300040:UD {align1};
mov  (1) tmp_reg1.24<1>:UD      0x00003040:UD {align1};
mov  (1) tmp_reg1.28<1>:UD      0x00000060:UD {align1};


mov  (8) vme_msg_1<1>:UD        tmp_reg1.0<8,8,1>:UD {align1};
        
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
mul  (1) tmp_reg3.8<1>:UD       w_in_mb_uw<0,1,0>:UW orig_y_ub<0,1,0>:UB {align1};
add  (1) tmp_reg3.8<1>:UD       tmp_reg3.8<0,1,0>:UD orig_x_ub<0,1,0>:UB {align1};
mul  (1) tmp_reg3.8<1>:UD       tmp_reg3.8<0,1,0>:UD 0x4:UD {align1};
mov  (1) tmp_reg3.20<1>:UB      thread_id_ub {align1};                  /* dispatch id */
mov  (8) msg_reg0.0<1>:UD       tmp_reg3.0<8,8,1>:UD {align1};

mov  (16) tmp_reg3.0<1>:UW      vme_wb1.0<32,32,1>:UB  {align1};
add  (8) tmp_reg3.0<2>:W        tmp_reg3.0<16,8,2>:W -64:W {align1};
add  (8) tmp_reg3.2<2>:W        tmp_reg3.2<16,8,2>:W -48:W {align1}; 
mov  (4) tmp_reg3.4<2>:UD       0x00000000:UD {align1};

/* Detecting zero MV issue */
mov  (1) tmp_reg1.0<1>:UD       0x00000000:UD {align1};
add  (1) tmp_reg1.0<1>:UW       vme_wb0.14<0,1,0>:UW 0x1000 {align1};
shr  (1) tmp_reg1.0<1>:UW       tmp_reg1.0<0,1,0>:UW 12  {align1};
mul  (2) tmp_reg3.0<1>:UW       tmp_reg3.0<0,2,1>:UW tmp_reg1.0<0,1,0>:UB {align1};
mul  (2) tmp_reg3.8<1>:UW       tmp_reg3.8<0,2,1>:UW tmp_reg1.0<0,1,0>:UB {align1};
mul  (2) tmp_reg3.16<1>:UW      tmp_reg3.16<0,2,1>:UW tmp_reg1.0<0,1,0>:UB {align1};
mul  (2) tmp_reg3.24<1>:UW      tmp_reg3.24<0,2,1>:UW tmp_reg1.0<0,1,0>:UB {align1};


mov  (8) msg_reg1.0<1>:UD       tmp_reg3.0<8,8,1>:UD   {align1};
mov  (8) msg_reg2.0<1>:UD       vme_wb0.0<8,8,1>:UD   {align1};

/* bind index 3, write 4 oword, msg type: 8(OWord Block Write) */
send (16)
        msg_ind
        obw_wb
        null
        data_port(
                OBW_CACHE_TYPE,
                OBW_MESSAGE_TYPE,
                OBW_CONTROL_3,
                OBW_BIND_IDX,
                OBW_WRITE_COMMIT_CATEGORY,
                OBW_HEADER_PRESENT
        )
        mlen 3
        rlen obw_wb_length
        {align1};
        
/*
 * kill thread
 */        
mov  (8) msg_reg0<1>:UD         r0<8,8,1>:UD {align1};
send (16) msg_ind acc0<1>UW null thread_spawner(0, 0, 1) mlen 1 rlen 0 {align1 EOT};
