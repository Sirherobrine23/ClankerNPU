# ASCII strings and verified address references — en7523_heimgard_cx30

Printable runs may be false positives inside instructions. A reference is reported only if the LUI+ADDI sequence resolves into the run; addresses may still be part of data rather than code.

| Address | Length | Xrefs | String (truncated) |
|---|---:|---|---|
| `0x84001fcb` | 7 | — | `P0zAao\`` |
| `0x84003a95` | 8 | — | `M}Uo@@h7` |
| `0x84003ed1` | 7 | — | `8Aao@@#` |
| `0x8400429f` | 13 | — | `;Aao0pf}UAao0` |
| `0x8400479e` | 11 | — | `#.%?9e#,D?}` |
| `0x840048c3` | 7 | — | `j}UAao0` |
| `0x84004965` | 7 | — | `\`}UAao0` |
| `0x84005ed8` | 7 | — | `o @$}q7` |
| `0x840097ec` | 7 | — | `}qLEPI"` |
| `0x84009e55` | 19 | — | `.#$A/#"Q/# a/#.q-#,` |
| `0x8400b8dd` | 20 | — | ` #&1!#$A!#"Q!# a!#.q` |
| `0x8400bd6f` | 7 | — | `BLBPF#&` |
| `0x8400bd7f` | 19 | — | `$#.1##*Q##(a##&q##$` |
| `0x8400c163` | 9 | — | `!#$Q!#&1!` |
| `0x8400c488` | 8 | — | `, cT !#.` |
| `0x8400d063` | 8 | — | `EXF# !+3` |
| `0x8400dc89` | 9 | — | `!#$q #&q!` |
| `0x8400ec86` | 26 | — | `# !5#.13#,A3#*Q3#(a3#&q3#"` |
| `0x8400ed20` | 10 | — | `#(q+# q-#,` |
| `0x8400ee30` | 10 | — | `#(q##*1##,` |
| `0x8400fa80` | 30 | `0x84005752` | `timer_isr  Store Value %x, val` |
| `0x8400faa8` | 49 | `0x840021bc`,`0x840022e2`,`0x84002328` | `[%s] 5G pcieBase %lx mem_base %lx, hw_rx_desc=%lx` |
| `0x8400fae1` | 51 | `0x8400233a`,`0x84002472`,`0x84002520` | `[%s] 2.4G pcieBase %lx mem_base %lx, hw_rx_desc=%lx` |
| `0x8400fb16` | 25 | `0x84002b08` | `piNode[%d]           =%x ` |
| `0x8400fb31` | 18 | `0x8400489e`,`0x84004a66` | `pkt err 1 in %s,%d` |
| `0x8400fb45` | 90 | `0x84005c78` | `Error: exceed max num!interfaceid=%u wifi_mail_data->funcType=%u wifi_mail_data->funcId=%u` |
| `0x8400fba1` | 11 | `0x84005da4` | `%-15s<0x%x>` |
| `0x8400fbae` | 31 | `0x84008e80` | `constraint handler: bad message` |
| `0x8400fbce` | 36 | `0x84004d3a`,`0x84004db6`,`0x84004ece`,`0x84004f26`,`0x84004f82`,`0x84004fce`,`0x8400506c`,`0x84005090` | `%s just return due to intSrc:%d > %d` |
| `0x8400fbf4` | 44 | `0x8400135c`,`0x8400135c` | `ERROR!!!!! [%s]wcid == 0 BEFORE GET BA ENTRY` |
| `0x8400fc22` | 12 | `0x840026a4`,`0x840027ba` | `ba_node_init` |
| `0x8400fc2f` | 32 | `0x8400490e`,`0x8400495a`,`0x84004a6a`,`0x84004a80`,`0x84004a96` | `npu_tunnel_offload_reassemble_v6` |
| `0x8400fc50` | 23 | `0x84004dba`,`0x84005298` | `PLIC_register_ISR_bySrc` |
| `0x8400fc68` | 23 | — | `set fragment mtu-%d: %d` |
| `0x8400fc81` | 10 | `0x84000364` | `core2_main` |
| `0x8400fc8c` | 11 | `0x84002578`,`0x840026a0`,`0x840027b6`,`0x8400286e` | `[NPU] %s...` |
| `0x8400fc99` | 26 | `0x84002ec4` | `del: %d : ipdst:%x name:%s` |
| `0x8400fcb5` | 19 | `0x84005a3c` | `npuMbox_notify_host` |
| `0x8400fcce` | 54 | `0x8400711e` | `wifi_mail_data->wifi_mail_private.npuPktBufAddr=%x, %x` |
| `0x8400fd06` | 29 | `0x84002aae` | `piNode[%d]->crsn         =%x ` |
| `0x8400fd39` | 32 | `0x8400472a`,`0x84004ade` | `error ether type=0x%04X in %s,%d` |
| `0x8400fd5b` | 32 | `0x84004ae2` | `npu_tunnel_offload_reassemble_op` |
| `0x8400fd7c` | 23 | `0x84004ed2` | `PLIC_get_priority_bySrc` |
| `0x8400fd94` | 23 | `0x84004f86` | `PLIC_set_trigType_bySrc` |
| `0x8400fdac` | 57 | `0x84006432` | `Error: OVER GLB_VAR_SRAM_SIZE. _data:0x%x, __bss_end:0x%x` |
| `0x8400fde7` | 21 | `0x8400664e` | `%s done via base:0x%x` |
| `0x8400fe06` | 14 | `0x840025ea`,`0x840029a0` | `pkt_queue_init` |
| `0x8400fe15` | 8 | `0x84002872` | `npu_init` |
| `0x8400fe1e` | 23 | `0x84004f2a` | `PLIC_set_priority_bySrc` |
| `0x8400fe36` | 40 | `0x84005936` | `%s cpu_tmr:%d is wrong, should be 0 or 1` |
| `0x8400fe60` | 46 | `0x840022ea` | `!!!!!!!!!!!! [%s] rxd init: alloc buffid fail!` |
| `0x8400fe90` | 35 | `0x84000a74`,`0x84000b1e` | `ba_indicate_reordering_mpdus_le_seq` |
| `0x8400feb4` | 26 | `0x84003592` | `npu bridge buf init sucess` |
| `0x8400fed0` | 29 | `0x84004b20` | `npu_gre_tunnel_tx_header_init` |
| `0x8400feee` | 28 | `0x84004fd2`,`0x84005070`,`0x84005094`,`0x840050bc`,`0x840050e4`,`0x8400510c`,`0x84005136`,`0x84005160` | `PLIC_enable_intSrc_toOneCore` |
| `0x8400ff0b` | 76 | `0x84000b1a` | `[%d,%s]!!ERROR:[%s] bufid is over: %d, or pkt len is msdu_blk->datalength=%d` |
| `0x8400ff59` | 45 | `0x8400565a` | `%s timer_no:%d is wrong, should be 0,1,2,or 3` |
| `0x8400ff88` | 37 | `0x84005c3e` | `wifi_mail_get_nowait_operation fail !` |
| `0x8400ffaf` | 14 | `0x84005e0c` | `Cyc_Cnt[63:32]` |
| `0x8400ffbe` | 23 | `0x8400742e` | `invalid idx %d in %s,%d` |
| `0x8400ffd7` | 32 | `0x84000a04`,`0x8400230c` | `[NPU] %s, addr=%lx, band_idx=%d ` |
| `0x8400fff9` | 32 | `0x84004776`,`0x8400488c`,`0x840048a2`,`0x840048b8` | `npu_tunnel_offload_reassemble_v4` |
| `0x8401001a` | 30 | `0x840046ee`,`0x8400472e` | `npu_tunnel_offload_fragment_op` |
| `0x84010039` | 43 | `0x840055d6` | `%s timer_no:%d is disabled, so just return!` |
| `0x84010066` | 32 | `0x84005b00` | `Error: core_id:%d != mbox_idx:%d` |
| `0x8401008f` | 17 | `0x84006b88` | `baNode_array_init` |
| `0x840100a3` | 23 | `0x84003c90` | `invalid udf %d in %s,%d` |
| `0x840100bc` | 26 | `0x840046ea` | `pkt_len=%d mtu=%d in %s,%d` |
| `0x840100d8` | 8 | `0x840055da` | `delay1ms` |
| `0x840100e1` | 23 | `0x8400886c` | `printf_s: %n disallowed` |
| `0x840100f9` | 19 | `0x840020c8` | `npu_npu_init_rxd_5G` |
| `0x8401010d` | 21 | `0x84003a8a` | `type is invalid in %s` |
| `0x84010124` | 28 | `0x84003c94` | `npu_tunnel_offload_common_op` |
| `0x84010141` | 31 | `0x84004956` | `IPv6 next header error in %s,%d` |
| `0x84010168` | 7 | `0x84005ea8` | `MSTATUS` |
| `0x84010180` | 19 | `0x84004888`,`0x84004a7c` | `pkt loss 2 in %s,%d` |
| `0x84010198` | 23 | `0x84006e56` | `Support NPU Wifi ffload` |
| `0x840101b5` | 10 | `0x8400059c` | `core7_main` |
| `0x840101c0` | 76 | `0x84000a70` | `[%d,%s]!![%s]ERROR: bufid is over: %d, or pkt len is mpdu_blk->datalength=%d` |
| `0x8401020e` | 25 | `0x84002b4a` | `piNode->foe          =%x ` |
| `0x84010229` | 32 | `0x84004b1c` | `%s L%d gre_tx_head_addr[%d]=0x%x` |
| `0x8401024b` | 61 | `0x84004d8e` | `(%s) implement code to clear intrSrc:%d then execute ISR here` |
| `0x8401028a` | 47 | `0x84004df2`,`0x840052d0` | `Error: src:%d already registered ISR, just exit` |
| `0x840102bb` | 44 | `0x84005a38` | `Error: %s timeout for core_id:%d, func_id:%d` |
| `0x840102e9` | 13 | `0x84005df2` | `Cyc_Cnt[31:0]` |
| `0x840102f7` | 35 | — | `Chip id(%x) support NPU Wifi ffload` |
| `0x8401031c` | 26 | `0x84008b32` | `printf_s: bad %ls argument` |
| `0x8401033b` | 24 | `0x8400359c` | `npu bridge buf init fail` |
| `0x84010355` | 14 | `0x8400593a` | `cpu_timer_init` |
| `0x84010364` | 43 | `0x84005bd2` | `Error(%s): physAddr:0x%x==0 or size:0x%x==0` |
| `0x84010391` | 46 | `0x8400700a` | `warning: length (%d) more than buffer len(%d)!` |
| `0x840103c1` | 16 | `0x840073d6` | `set vxlan mtu %d` |
| `0x840103d7` | 16 | `0x8400a054`,`0x8400f1bc` | `0123456789abcdef` |
| `0x840103e8` | 10 | `0x84000390` | `core3_main` |
| `0x840103f3` | 17 | `0x84002308` | `npu_set_pcie_base` |
| `0x84010405` | 34 | `0x84002e44` | `tunnel table full, cannot add!!!!!` |
| `0x84010429` | 49 | `0x84006058` | `UnKnow Interrupts (mcause:0x%x, RA:0x%x, SP:0x%x)` |
| `0x8401046b` | 26 | `0x8400166c` | `!!!!!!!!!!!! enqueu error!` |
| `0x84010486` | 47 | `0x84005b7a` | `Error: (core_id:%d >= %d) \|\| (func_id:%d >= %d)` |
| `0x840104b7` | 37 | `0x84005c16` | `wifi_mail_set_nowait_operation fail !` |
| `0x840104de` | 17 | `0x84007050` | `Ring Index error!` |
| `0x840104f1` | 16 | `0x8400257c`,`0x840027e0` | `queue_mutex_init` |
| `0x84010502` | 19 | `0x84004772`,`0x8400490a` | `pkt loss 1 in %s,%d` |
| `0x84010517` | 9 | `0x84004b68` | `0x%04x : ` |
| `0x84010527` | 47 | — | `Chip id(%x) does not support NPU Wifi ffload!!!` |
| `0x84010558` | 16 | `0x84009fac`,`0x8400f22a` | `0123456789ABCDEF` |
| `0x84010569` | 25 | `0x84002b5a` | `piNode->status       =%x ` |
| `0x84010586` | 29 | `0x840003b8` | `%s for npu tunnel offload v21` |
| `0x840105a5` | 46 | `0x84001170` | `npu_ba_reoder():!!!!!!!! wrong pkt ,wcid is 0 ` |
| `0x840105d5` | 51 | `0x840021e4` | `[NPU] Enter(2.4G) %s, band_idx=%d, rx_ring_size=%lu` |
| `0x8401060a` | 12 | `0x840025e6`,`0x8400299c` | `[NPU]  %s...` |
| `0x84010618` | 29 | `0x84002ac0` | `piNode[%d]->foe          =%x ` |
| `0x84010637` | 33 | `0x84003a8e` | `npu_bridge_egress_pkt_contiuously` |
| `0x84010659` | 14 | `0x84004d3e` | `call_isr_bySrc` |
| `0x84010668` | 25 | `0x84008cd6` | `printf_s: bad %s argument` |
| `0x84010682` | 25 | `0x84002b1a` | `piNode->bufid        =%x ` |
| `0x8401069d` | 29 | `0x84002f58` | `add: %d : ipdst:%x name:%s %s` |
| `0x840106bc` | 43 | `0x8400309c`,`0x840033e2` | `The TX DSCP is not return from tx used pool` |
| `0x840106e9` | 62 | `0x84003640`,`0x8400370c`,`0x8400376c`,`0x840037e2`,`0x84003878`,`0x840038dc`,`0x8400392a`,`0x8400397c` | `npu bridge egress fail,pkt_addr = %x,tx_msg0 = %x,tx_msg1 = %x` |
| `0x84010729` | 35 | `0x84005cb8` | `wifi_mail_get_wait_operation fail !` |
| `0x8401074e` | 7 | `0x84005ec2` | `MICAUSE` |
| `0x84010756` | 12 | `0x84006dea` | `counter_init` |
| `0x84010763` | 30 | `0x84007432` | `tunnel_mail_npu_store_srv6_hdr` |
| `0x84010782` | 29 | `0x84002ad2` | `piNode[%d]->status       =%x ` |
| `0x840107a1` | 34 | `0x840056ac`,`0x840057f0` | `%s: Timer clk is running at %d Mhz` |
| `0x840107c5` | 18 | `0x8400640c` | `core freq at %d Hz` |
| `0x840107d9` | 58 | `0x8400031c` | `(%s) just return, because QDMA driver for ARM is not ready` |
| `0x84010815` | 10 | `0x8400055c` | `core5_main` |
| `0x84010820` | 29 | `0x84002a9c` | `piNode[%d]->pkt_len      =%x ` |
| `0x8401083f` | 13 | `0x84006652` | `npu_uart_init` |
| `0x8401084d` | 21 | `0x840073ac` | `release hdr buf id %d` |
| `0x84010864` | 33 | `0x840021c4`,`0x840021c4` | `[%s] rxd init: alloc buffid fail!` |
| `0x84010887` | 16 | `0x840021e8` | `npu_npu_init_rxd` |
| `0x84010898` | 29 | `0x84002a8a` | `piNode[%d]->bufid        =%x ` |
| `0x840108b7` | 25 | `0x84002b2a` | `piNode->pkt_len      =%x ` |
| `0x840108d2` | 27 | `0x84005c88` | `not support unknow funcType` |
| `0x840108ef` | 89 | `0x8400748c` | `set srv6 my ipv6: %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X` |
| `0x8401094a` | 10 | `0x84000320` | `core0_main` |
| `0x84010955` | 10 | `0x8400053c` | `core4_main` |
| `0x84010960` | 10 | `0x8400057c` | `core6_main` |
| `0x8401096b` | 24 | `0x840020e6`,`0x84002206` | `ERROR! rx_ring_size = %d` |
| `0x84010985` | 18 | `0x840048b4`,`0x84004a92` | `pkt err 2 in %s,%d` |
| `0x84010999` | 10 | `0x8400565e`,`0x840056b0`,`0x840057f4` | `timer_init` |
| `0x840109a4` | 13 | `0x84005e26` | `Ins_Cnt[31:0]` |
| `0x840109b2` | 19 | `0x84000370` | `do npu_enq_deq_init` |
| `0x840109c7` | 30 | `0x840020fc` | `5G init second: do np_skb free` |
| `0x840109e7` | 29 | `0x84002a78` | `piNode[%d]               =%x ` |
| `0x84010a06` | 16 | `0x84004b4a` | `%s: %p, len = %d` |
| `0x84010a18` | 11 | `0x84004d92` | `default_isr` |
| `0x84010a24` | 16 | `0x84005bd6` | `npu_get_mboxBuff` |
| `0x84010a35` | 35 | `0x84005c66` | `wifi_mail_set_wait_operation fail !` |
| `0x84010a5e` | 10 | `0x84000340` | `core1_main` |
| `0x84010a69` | 25 | `0x84002b3a` | `piNode->crsn         =%x ` |
| `0x84010a84` | 21 | `0x840036b6` | `head buf is not ready` |
| `0x84010a9b` | 29 | `0x84005302`,`0x84005356` | `PLIC_DISABLE_intSrc_toOneCore` |
| `0x84010ab9` | 23 | `0x84008e14` | `printf: bad %n argument` |
| `0x84010ad6` | 45 | `0x840020c4` | `[NPU] Enter %s, band_idx=%d, rx_ring_size=%lu` |
| `0x84010b05` | 24 | `0x84002472`,`0x84002520` | `ERROR!!!!! [%s]wcid == 0` |
| `0x84010b1f` | 14 | `0x84005e40` | `Ins_Cnt[63:32]` |
| `0x84010b2e` | 12 | `0x84005ed0` | `Show CSR END` |
