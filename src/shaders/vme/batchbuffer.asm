/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Xiang Haihao <haihao.xiang@intel.com>
 */
        
/*
 * __START
 */
__INTER_START:
        and.z.f0.1      (1)     remainder_cmds<1>:uw            total_mbs<0,1,0>:uw     0x0003:uw {align1};
        and.z.f0.0      (1)     total_mbs<1>:uw                 total_mbs<0,1,0>:uw     0xfffc:uw {align1};

        mov             (16)    tmp_reg0<1>:ud                  0x0:ud {align1} ;

        mov             (8)     media_object_ud<1>:ud           0x0:ud {align1} ;
        mov             (1)     media_object0_ud<1>:ud          CMD_MEDIA_OBJECT {align1} ;
        mov             (1)     media_object1_ud<1>:ud          mtype_ub<0,1,0>ub {align1};
        mov             (1)     media_object6_width<1>:uw       width_in_mb<0,1,0>:uw {align1};
        mov             (1)     media_object7_ud<1>:ud          transform_8x8_ub<0,1,0>ub {align1};

        mul             (1)     tmp_reg0.8<1>:ud                width_in_mb<0,1,0>:uw   mb_y<0,1,0>:ub {align1};
        add             (1)     tmp_reg0.8<1>:ud                tmp_reg0.8<0,1,0>:ud    mb_x<0,1,0>:ub {align1};
        mul             (1)     tmp_reg0.8<1>:ud                tmp_reg0.8<0,1,0>:ud    0x2:ud {align1} ;
        mov             (1)     tmp_reg0.20<1>:ub               thread_id_ub {align1};                  /* dispatch id */
        
        (f0.0)jmpi      (1)     __REMAINDER ;

__CMD_LOOP:
        mov             (8)     msg_reg0.0<1>:ud                tmp_reg0<8,8,1>:ud {align1};
        add             (1)     tmp_reg0.8<1>:ud                tmp_reg0.8<0,1,0>:ud       8:uw {align1} ;
        
        mov             (1)     media_object6_xy<1>:uw          mb_xy<1>:uw {align1} ;
        mov             (8)     msg_reg1<1>:ud                  media_object_ud<8,8,1>:ud {align1};
        add             (1)     mb_x<1>:ub                      mb_x<0,1,0>:ub          1:uw {align1};
        cmp.e.f0.0      (1)     null<1>:uw                      width_in_mb<0,1,0>:uw   mb_x<0,1,0>:ub {align1};
        (f0.0)mov       (1)     mb_x<1>:ub                      0:uw {align1} ;
        (f0.0)add       (1)     mb_y<1>:ub                      mb_y<0,1,0>:ub          1:uw {align1} ;
        
        mov             (1)     media_object6_xy<1>:uw          mb_xy<1>:uw {align1} ;
        mov             (8)     msg_reg2<1>:ud                  media_object_ud<8,8,1>:ud {align1};
        add             (1)     mb_x<1>:ub                      mb_x<0,1,0>:ub          1:uw {align1};
        cmp.e.f0.0      (1)     null<1>:uw                      width_in_mb<0,1,0>:uw   mb_x<0,1,0>:ub {align1};
        (f0.0)mov       (1)     mb_x<1>:ub                      0:uw {align1} ;
        (f0.0)add       (1)     mb_y<1>:ub                      mb_y<0,1,0>:ub          1:uw {align1} ;
        
        mov             (1)     media_object6_xy<1>:uw          mb_xy<1>:uw {align1} ;
        mov             (8)     msg_reg3<1>:ud                  media_object_ud<8,8,1>:ud {align1};
        add             (1)     mb_x<1>:ub                      mb_x<0,1,0>:ub          1:uw {align1};
        cmp.e.f0.0      (1)     null<1>:uw                      width_in_mb<0,1,0>:uw   mb_x<0,1,0>:ub {align1};
        (f0.0)mov       (1)     mb_x<1>:ub                      0:uw {align1} ;
        (f0.0)add       (1)     mb_y<1>:ub                      mb_y<0,1,0>:ub          1:uw {align1} ;
        
        mov             (1)     media_object6_xy<1>:uw          mb_xy<1>:uw {align1} ;
        mov             (8)     msg_reg4<1>:ud                  media_object_ud<8,8,1>:ud {align1};
        add             (1)     mb_x<1>:ub                      mb_x<0,1,0>:ub          1:uw {align1};
        cmp.e.f0.0      (1)     null<1>:uw                      width_in_mb<0,1,0>:uw   mb_x<0,1,0>:ub {align1};
        (f0.0)mov       (1)     mb_x<1>:ub                      0:uw {align1} ;
        (f0.0)add       (1)     mb_y<1>:ub                      mb_y<0,1,0>:ub          1:uw {align1} ;
        
/* bind index 5, write 8 oword, msg type: 8(OWord Block Write) */
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
        
        
        add.z.f0.0      (1)	total_mbs<1>:w                  total_mbs<0,1,0>:w      -4:w {align1};
        (-f0.0)jmpi     (1)     __CMD_LOOP ;
        
__REMAINDER:
        (f0.1)jmpi      (1)     __DONE ;        

__REMAINDER_LOOP:
        mov             (8)     msg_reg0.0<1>:ud                tmp_reg0<8,8,1>:ud {align1} ;
        add             (1)     tmp_reg0.8<1>:ud                tmp_reg0.8<0,1,0>:ud    2:uw {align1} ;
        
        mov             (1)     media_object6_xy<1>:uw          mb_xy<1>:uw {align1} ;
        mov             (8)     msg_reg1<1>:ud                  media_object_ud<8,8,1>:ud {align1};
        add             (1)     mb_x<1>:ub                      mb_x<0,1,0>:ub          1:uw {align1};
        cmp.e.f0.0      (1)     null<1>:uw                      width_in_mb<0,1,0>:uw   mb_x<0,1,0>:ub {align1};
        (f0.0)mov       (1)     mb_x<1>:ub                      0:uw {align1} ;
        (f0.0)add       (1)     mb_y<1>:ub                      mb_y<0,1,0>:ub          1:uw {align1} ;

/* bind index 5, write 2 oword, msg type: 8(OWord Block Write) */
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
        
	add.z.f0.1      (1)	remainder_cmds<1>:w             remainder_cmds<0,1,0>:w -1:w;
	(-f0.1)jmpi     (1)     __REMAINDER_LOOP                               ;
        
__DONE:

        cmp.e.f0.0      (1)     null<1>:uw                      last_object<0,1,0>:uw   1:uw {align1};
        (-f0.0)jmpi     (1)     __EXIT ;
        
/* bind index 5, write 1 oword, msg type: 8(OWord Block Write) */
        mov             (8)     msg_reg0.0<1>:ud                tmp_reg0<8,8,1>:ud {align1} ;
        mov             (4)     msg_reg1.0<1>:ud                0x0:ud {align1} ;
        mov             (1)     msg_reg1.4<1>:ud                MI_BATCH_BUFFER_END {align1} ;
        
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

__EXIT:
        mov             (8)     msg_reg0<1>:ud                  r0<8,8,1>:ud {align1} ;
        send            (16)    msg_ind acc0<1>ud null thread_spawner(0, 0, 1) mlen 1 rlen 0 {align1 EOT} ;
