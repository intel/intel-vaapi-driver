






























































































































__INTER_START:
mov (16) r32.0<1>:UD 0x0:UD {align1};
mov (16) r34.0<1>:UD 0x0:UD {align1};
mov (16) r35.0<1>:UD 0x0:UD {align1};
shl (2) r32.0<1>:D r5.0<2,2,1>:UB 4:UW {align1};
add (1) r32.0<1>:D r32.0<0,1,0>:D -8:W {align1};
add (1) r32.4<1>:D r32.4<0,1,0>:D -1:W {align1};
mov (1) r32.8<1>:UD 0x0000001F {align1};
mov (1) r32.20<1>:UB r0.20<0,1,0>:UB {align1};
shl (2) r33.0<1>:D r5.0<2,2,1>:UB 4:UW {align1};
add (1) r33.0<1>:D r33.0<0,1,0>:D -4:W {align1};
mov (1) r33.8<1>:UD 0x000F0003 {align1};
mov (1) r33.20<1>:UB r0.20<0,1,0>:UB {align1};
shl (2) r34.8<1>:UW r5.0<2,2,1>:UB 4:UW {align1};
mov (1) r34.0<1>:W -16:W {align1} ;
mov (1) r34.2<1>:W -12:W {align1} ;
mov (1) r34.12<1>:UD 0x00000000 + 0x7e000000 + 0x00200000 + 0x00003000:UD {align1};
mov (1) r34.20<1>:UB r0.20<0,1,0>:UB {align1};
mov (1) r34.22<1>:UW 0x2830:UW {align1};
mov (1) r35.0<1>:UD 0x00000002:ud {align1} ;
mov (1) r35.4<1>:UD 0x40000000:UD {align1};
mov (1) r35.4<1>:UB r4.28<0,1,0>:UB {align1};
mov (1) r35.8<1>:UD 0x30000000 + 0x00003030:UD {align1};
mul (1) r36.8<1>:UD r5.2<0,1,0>:UW r5.1<0,1,0>:UB {align1};
add (1) r36.8<1>:UD r36.8<0,1,0>:UD r5.0<0,1,0>:UB {align1};
mul (1) r36.8<1>:UD r36.8<0,1,0>:UD 10:UD {align1};
mov (1) r36.20<1>:UB r0.20<0,1,0>:UB {align1};
__VME_LOOP:
mov (8) g64.0<1>:UD r32.0<8,8,1>:UD {align1};
send (8) 64 r18<1>:UB null read(4, 0, 0, 4) mlen 1 rlen 1 {align1};
mov (8) g64.0<1>:UD r33.0<8,8,1>:UD {align1};
send (8) 64 r20<1>:UB null read(4, 0, 0, 4) mlen 1 rlen 2 {align1};
mov (8) g64.0<1>:UD r34.0<8,8,1>:UD {align1};
mov (1) r35.28<1>:UW 0x0:UW {align1} ;
and.z.f0.0 (1) null<1>:UW r5.4<0,1,0>:UB 1:UW {align1};
(f0.0) mov (1) r35.28<1>:UB 0x2 {align1};
cmp.nz.f0.0 (1) null<1>:UW r5.0<0,1,0>:UB 0:UW {align1};
(f0.0) add (1) r35.29<1>:UB r35.29<0,1,0>:UB 0x60 {align1};
cmp.nz.f0.0 (1) null<1>:UW r5.1<0,1,0>:UB 0:UW {align1};
(f0.0) add (1) r35.29<1>:UB r35.29<0,1,0>:UB 0x10 {align1};
mul.nz.f0.0 (1) null<1>:UW r5.0<0,1,0>:UB r5.1<0,1,0>:UB {align1};
(f0.0) add (1) r35.29<1>:UB r35.29<0,1,0>:UB 0x4 {align1};
add (1) r41.0<1>:W r5.0<0,1,0>:UB 1:UW {align1};
add (1) r41.0<1>:W r5.2<0,1,0>:UW -r41.0<0,1,0>:W {align1};
mul.nz.f0.0 (1) null<1>:UD r41.0<0,1,0>:W r5.1<0,1,0>:UB {align1};
(f0.0) add (1) r35.29<1>:UB r35.29<0,1,0>:UB 0x8 {align1};
and.nz.f0.0 (1) null<1>:UW r5.4<0,1,0>:UB 2:UW {align1};
(f0.0) and (1) r35.29<1>:UB r35.29<0,1,0>:UB 0xE0 {align1};
mov (8) g65<1>:UD r35.0<8,8,1>:UD {align1};
mov (8) g66<1>:UD 0x0:UD {align1};
mov (1) r18.0<1>:UD 0x0:UD {align1};
and (1) r18.4<1>:UD r18.4<0,1,0>:UD 0xFF000000:UD {align1};
mov (8) g67<1>:UD r18.0<8,8,1>:UD {align1};
mov (8) g68<1>:UD 0x0 {align1};
mov (16) g68.0<1>:UB r20.3<32,8,4>:UB {align1};
mov (1) g68.16<1>:UD 0x11111111:UD {align1};
send (8)
        64
        r12
        null
        vme(
                0,
                0,
                0,
                3
        )
        mlen 5
        rlen 6
        {align1};
mov (8) g64.0<1>:UD r36.0<8,8,1>:UD {align1};
mov (8) r37.0<1>:ud r13.0<8,8,1>:ud {align1};
mov (8) r38.0<1>:ud r14.0<8,8,1>:ud {align1};
mov (8) r39.0<1>:ud r15.0<8,8,1>:ud {align1};
mov (8) r40.0<1>:ud r16.0<8,8,1>:ud {align1};
mov (8) g65.0<1>:UD r37.0<8,8,1>:UD {align1};
mov (8) g66.0<1>:UD r38.0<8,8,1>:UD {align1};
mov (8) g67.0<1>:UD r39.0<8,8,1>:UD {align1};
mov (8) g68.0<1>:UD r40.0<8,8,1>:UD {align1};
send (16)
        64
        null<1>:W
        null
        data_port(
                10,
                8,
                4,
                3,
                0,
                1
        )
        mlen 5
        rlen 0
        {align1};
add (1) g64.8<1>:UD r36.8<0,1,0>:UD 8:UD {align1} ;
and.z.f0.0 (1) null<1>:ud r12.0<0,1,0>:ud 0x00002000:ud {align1} ;
(-f0.0)jmpi (1) __INTRA_INFO ;
__INTER_INFO:
mov (1) r42.2<1>:uw 0:uw {align1} ;
mov (1) r42.4<1>:ud 0:ud {align1} ;
(f0.0)and (1) r42.2<1>:uw r12.2<0,1,0>:uw 0x0020:uw {align1} ;
(f0.0)shr (1) r42.2<1>:uw r42.2<1>:uw 5:uw {align1} ;
(f0.0)mul (1) r42.4<1>:ud r42.2<0,1,0>:uw 96:uw {align1} ;
(f0.0)add (1) r42.4<1>:ud r42.4<0,1,0>:ud 32:uw {align1} ;
(f0.0)shl (1) r42.2<1>:uw r42.2<0,1,0>:uw 5:uw {align1} ;
(f0.0)add (1) r42.2<1>:uw r42.2<0,1,0>:uw 0x0040:uw {align1} ;
add (1) r42.2<1>:uw r42.2<0,1,0>:uw 0x000E:uw {align1} ;
mov (1) g65.0<1>:uw r12.0<0,1,0>:uw {align1} ;
mov (1) g65.2<1>:uw r42.2<0,1,0>:uw {align1} ;
mov (1) g65.4<1>:UD r12.28<0,1,0>:UD {align1};
mov (1) g65.8<1>:ud r42.4<0,1,0>:ud {align1} ;
jmpi (1) __OUTPUT_INFO ;
__INTRA_INFO:
mov (1) g65.0<1>:UD r12.0<0,1,0>:UD {align1};
mov (1) g65.4<1>:UD r12.16<0,1,0>:UD {align1};
mov (1) g65.8<1>:UD r12.20<0,1,0>:UD {align1};
mov (1) g65.12<1>:UD r12.24<0,1,0>:UD {align1};
__OUTPUT_INFO:
send (16)
        64
        null<1>:W
        null
        data_port(
                10,
                8,
                0,
                3,
                0,
                1
        )
        mlen 2
        rlen 0
        {align1};
add (1) r5.0<1>:ub r5.0<0,1,0>:ub 1:uw {align1} ;
add (1) r34.8<1>:UW r34.8<0,1,0>:UW 16:UW {align1};
cmp.e.f0.0 (1) null<1>:uw r5.2<0,1,0>:uw r5.0<0,1,0>:ub {align1};
(f0.0)mov (1) r5.0<1>:ub 0:uw {align1} ;
(f0.0)mov (1) r34.8<1>:uw 0:uw {align1} ;
(f0.0)add (1) r34.10<1>:uw r34.10<0,1,0>:uw 16:uw {align1} ;
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 10:UW {align1} ;
add.z.f0.1 (1) r5.6<1>:w r5.6<0,1,0>:w -1:w {align1} ;
(-f0.1)jmpi (1) __VME_LOOP ;
__EXIT:
mov (8) g64<1>:UD r0<8,8,1>:UD {align1};
send (16) 64 acc0<1>UW null thread_spawner(0, 0, 1) mlen 1 rlen 0 {align1 EOT};
