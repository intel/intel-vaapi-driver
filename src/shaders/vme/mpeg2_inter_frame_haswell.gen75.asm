

































































































































































__INTRA_START:
mov (16) r32.0<1>:UD 0x0:UD {align1};
mov (16) r34.0<1>:UD 0x0:UD {align1};
mov (16) r36.0<1>:UD 0x0:UD {align1} ;
mov (16) r38.0<1>:UD 0x0:UD {align1} ;
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
mov (1) r34.20<1>:UB r0.20<0,1,0>:UB {align1};
mul (1) r36.8<1>:UD r5.2<0,1,0>:UW r5.1<0,1,0>:UB {align1};
add (1) r36.8<1>:UD r36.8<0,1,0>:UD r5.0<0,1,0>:UB {align1};
mul (1) r36.8<1>:UD r36.8<0,1,0>:UD 24:UD {align1};
mov (1) r36.20<1>:UB r0.20<0,1,0>:UB {align1};
mov (8) r64.0<1>:UD r32.0<8,8,1>:UD {align1};
send (8) 64 r28<1>:UB null read(4, 0, 0, 4) mlen 1 rlen 1 {align1};
mov (8) r64.0<1>:UD r33.0<8,8,1>:UD {align1};
send (8) 64 r29<1>:UB null read(4, 0, 0, 4) mlen 1 rlen 2 {align1};
shl (2) r32.0<1>:D r5.0<2,2,1>:UB 3:UW {align1};
mul (1) r32.0<1>:D r32.0<0,1,0>:D 2:W {align1};
add (1) r32.0<1>:D r32.0<0,1,0>:D -8:W {align1};
add (1) r32.4<1>:D r32.4<0,1,0>:D -1:W {align1};
mov (8) r64.0<1>:UD r32.0<8,8,1>:UD {align1};
send (8) 64 r48<1>:UB null read(6, 0, 0, 4) mlen 1 rlen 1 {align1};
shl (2) r33.0<1>:D r5.0<2,2,1>:UB 3:UW {align1};
mul (1) r33.0<1>:D r33.0<0,1,0>:D 2:W {align1};
add (1) r33.0<1>:D r33.0<0,1,0>:D -4:W {align1};
mov (1) r33.8<1>:UD 0x00070003 {align1};
mov (8) r64.0<1>:UD r33.0<8,8,1>:UD {align1};
send (8) 64 r49<1>:UB null read(6, 0, 0, 4) mlen 1 rlen 1 {align1};
mov (8) r43<1>:UD r1.0<8,8,1>:UD {align1};
mov (8) r66<1>:UD r43.0<8,8,1>:UD {align1};
mov (8) r67<1>:UD 0x0:UD {align1};
mov (1) r28.0<1>:UD 0x0:UD {align1};
and (1) r28.4<1>:UD r28.4<0,1,0>:UD 0xFF000000:UD {align1};
mov (8) r68<1>:UD r28.0<8,8,1>:UD {align1};
mov (8) r69<1>:UD 0x0:UD {align1};
mov (16) r69.0<1>:UB r29.3<32,8,4>:UB {align1};
mov (1) r69.16<1>:UD 0x11111111:UD {align1};
mov (1) r69.28<1>:UD 0x010101:UD {align1};
mov (1) r69.20<1>:UW r48.6<0,1,0>:UW {align1};
mov (4) r70.16<1>:UD r48.8<4,4,1>:UD {align1};
mov (8) r70.0<1>:UW r49.2<16,8,2>:UW {align1};
mov (8) r64.0<1>:UD r34.0<8,8,1>:UD {align1};
mov (1) r32.0<1>:UW 0x1:UW {align1};
mov (1) r68.5<1>:UB r32.0<0,1,0>:UB {align1};
mov (1) r35.28<1>:UW 0x0:UW {align1} ;
and.z.f0.0 (1) null<1>:UW r5.4<0,1,0>:UB 1:UW {align1};
(f0.0) mov (1) r35.28<1>:UB 0x2 {align1};
mov (1) r35.29<1>:UB r5.5<0,1,0>:UB {align1};
mov (1) r32.0<1>:UW 0x0020:UW {align1};
mov (1) r35.30<1>:UB r32.0<0,1,0>:UB {align1};
mov (1) r34.12<1>:UD 0x00800000:UD {align1};
mov (8) r64.0<1>:UD r34.0<8,8,1>:UD {align1};
mov (8) r65<1>:UD r35.0<8,8,1>:UD {align1};
send (8)
        64
        r12<1>:UD
        null
        cre(
                0,
                1
        )
        mlen 7
        rlen 7
        {align1};
mov (8) r64.0<1>:UD r36<8,8,1>:UD {align1};
mov (1) r65.0<1>:UD r12.0<0,1,0>:UD {align1};
mov (1) r65.4<1>:UD r12.16<0,1,0>:UD {align1};
mov (1) r65.8<1>:UD r12.20<0,1,0>:UD {align1};
mov (1) r65.12<1>:UD r12.24<0,1,0>:UD {align1};
mov (1) r65.16<1>:UW r12.12<0,1,0>:UW {align1};
mov (1) r65.20<1>:UD r12.8<0,1,0>:UD {align1};
mov (1) r65.24<1>:UD r12.28<0,1,0>:UD {align1};
mov (1) r65.28<1>:UD r36.8<0,1,0>:UD {align1};
send (16)
        64
        null<1>:W
        null
        data_port(
                10,
                8,
                2,
                3,
                0,
                1
        )
        mlen 2
        rlen 0
        {align1};
mov (1) r34.12<1>:UD 0x00000000 + 0x7e000000 + 0x00200000:UD {align1};
mov (1) r34.22<1>:UW 0x2830:UW {align1};
mov (1) r34.0<1>:UD r34.8<0,1,0>:UD {align1};
add (1) r34.0<1>:W r34.0<0,1,0>:W -16:W {align1};
add (1) r34.2<1>:W r34.2<0,1,0>:W -12:W {align1};
mov (1) r34.0<1>:W -16:W {align1};
mov (1) r34.2<1>:W -12:W {align1};
mov (1) r34.4<1>:UD r34.0<0,1,0>:UD {align1};
mov (8) r64.0<1>:UD r34.0<8,8,1>:UD {align1};
mov (1) r35.0<1>:UD 0x00000002:ud {align1} ;
mov (1) r35.4<1>:UB r4.28<0,1,0>:UB {align1};
mov (1) r35.8<1>:UD 0x30000000 + 0x00003030:UD {align1};
mov (8) r65.0<1>:UD r35.0<8,8,1>:UD {align1};
mov (8) r66<1>:UD r43.0<8,8,1>:UD {align1};
mov (1) r67.0<1>:UD 0x01010101:UD {align1};
mov (1) r67.4<1>:UD 0x10010101:UD {align1};
mov (1) r67.8<1>:UD 0x0F0F0F0F:UD {align1};
mov (1) r67.12<1>:UD 0x100F0F0F:UD {align1};
mov (1) r67.16<1>:UD 0x01010101:UD {align1};
mov (1) r67.20<1>:UD 0x10010101:UD {align1};
mov (1) r67.24<1>:UD 0x0F0F0F0F:UD {align1};
mov (1) r67.28<1>:UD 0x100F0F0F:UD {align1};
mov (1) r68.0<1>:UD 0x01010101:UD {align1};
mov (1) r68.4<1>:UD 0x10010101:UD {align1};
mov (1) r68.8<1>:UD 0x0F0F0F0F:UD {align1};
mov (1) r68.12<1>:UD 0x000F0F0F:UD {align1};
mov (4) r68.16<1>:UD 0x0:UD {align1};
send (8)
        64
        r12<1>:UD
        null
        vme(
                0,
                0,
                0,
                2
        )
        mlen 5
        rlen 7 {align1};
mov (1) r43.20<1>:UD 0x0:UD {align1};
mov (1) r43.21<1>:UB r12.25<0,1,0>:UB {align1};
mov (1) r43.22<1>:UB r12.26<0,1,0>:UB {align1};
and (1) r32.0<1>:UW r12.0<0,1,0>:UW 0x03:UW {align1};
mov (1) r43.20<1>:UB r32.0<0,1,0>:UB {align1};
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 0x02:UD {align1};
mov (8) r64.0<1>:UD r36<8,8,1>:UD {align1};
mov (1) r65.0<1>:UD r12.0<0,1,0>:UD {align1};
mov (1) r65.4<1>:UD r12.24<0,1,0>:UD {align1};
mov (1) r65.8<1>:UD r12.8<0,1,0>:UD {align1};
mov (1) r65.12<1>:UD r36.8<0,1,0>:UD {align1};
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
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 0x01:UD {align1};
mov (8) r64.0<1>:UD r36<8,8,1>:UD {align1};
mov (8) r65.0<1>:UD r13.0<8,8,1>:UD {align1};
mov (8) r66.0<1>:ud r14.0<8,8,1>:ud {align1};
mov (8) r67.0<1>:ud r15.0<8,8,1>:ud {align1};
mov (8) r68.0<1>:ud r16.0<8,8,1>:ud {align1};
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
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 0x08:UD {align1};
mov (8) r64.0<1>:UD r36<8,8,1>:UD {align1};
mov (8) r65.0<1>:UD r18.0<8,8,1>:UD {align1};
send (16)
        64
        null<1>:W
        null
        data_port(
                10,
                8,
                2,
                3,
                0,
                1
        )
        mlen 2
        rlen 0
        {align1};
mov (8) r67.0<1>:UD r13.0<8,8,1>:UD {align1};
mov (8) r68.0<1>:ud r14.0<8,8,1>:ud {align1};
mov (8) r69.0<1>:ud r15.0<8,8,1>:ud {align1};
mov (8) r70.0<1>:ud r16.0<8,8,1>:ud {align1};
mov (1) r34.12<1>:UD 0x00200000 + 0x00003000 + 0x00040000:UD {align1};
mov (8) r64.0<1>:UD r34.0<8,8,1>:UD {align1};
mov (8) r65.0<1>:UD r35.0<8,8,1>:UD {align1};
mov (8) r66.0<1>:UD r43.0<8,8,1>:UD {align1};
send (8)
        64
        r12<1>:UD
        null
        cre(
                0,
                3
        )
        mlen 7
        rlen 7
        {align1};
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 0x02:UD {align1};
mov (8) r64.0<1>:UD r36<8,8,1>:UD {align1};
mov (1) r65.0<1>:UD r12.0<0,1,0>:UD {align1};
mov (1) r65.4<1>:UD r12.24<0,1,0>:UD {align1};
mov (1) r65.8<1>:UD r12.8<0,1,0>:UD {align1};
mov (1) r65.12<1>:UD r43.20<0,1,0>:UD {align1};
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
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 0x01:UD {align1};
mov (8) r64.0<1>:UD r36.0<8,8,1>:UD {align1};
mov (8) r65.0<1>:UD r13.0<8,8,1>:UD {align1};
mov (8) r66.0<1>:ud r14.0<8,8,1>:ud {align1};
mov (8) r67.0<1>:ud r15.0<8,8,1>:ud {align1};
mov (8) r68.0<1>:ud r16.0<8,8,1>:ud {align1};
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
add (1) r36.8<1>:UD r36.8<0,1,0>:UD 0x08:UD {align1};
mov (8) r64.0<1>:UD r36<8,8,1>:UD {align1};
mov (8) r65.0<1>:UD r18.0<8,8,1>:UD {align1};
send (16)
        64
        null<1>:W
        null
        data_port(
                10,
                8,
                2,
                3,
                0,
                1
        )
        mlen 2
        rlen 0
        {align1};
__EXIT:
mov (8) r112<1>:UD r0<8,8,1>:UD {align1};
send (16) 112 acc0<1>UW null thread_spawner(0, 0, 1) mlen 1 rlen 0 {align1 EOT};
