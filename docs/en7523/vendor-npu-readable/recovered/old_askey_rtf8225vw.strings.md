# ASCII strings and verified address references — old_askey_rtf8225vw

Printable runs may be false positives inside instructions. A reference is reported only if the LUI+ADDI sequence resolves into the run; addresses may still be part of data rather than code.

| Address | Length | Xrefs | String (truncated) |
|---|---:|---|---|
| `0x84000a73` | 9 | — | `@p6oP\`/7%` |
| `0x84000a83` | 7 | — | `@p5oP\`.` |
| `0x84000e6e` | 7 | — | `Eao@Po3` |
| `0x8400185f` | 8 | — | `WAao@ P&` |
| `0x84001e0e` | 8 | — | `Aao0Pu}T` |
| `0x84002297` | 7 | — | `0 4o0p.` |
| `0x840073b8` | 7 | — | `}qLEPI"` |
| `0x84007a35` | 19 | — | `.#$A/#"Q/# a/#.q-#,` |
| `0x840094bd` | 20 | — | ` #&1!#$A!#"Q!# a!#.q` |
| `0x8400994f` | 7 | — | `BLBPF#&` |
| `0x8400995f` | 19 | — | `$#.1##*Q##(a##&q##$` |
| `0x84009d43` | 9 | — | `!#$Q!#&1!` |
| `0x8400a068` | 8 | — | `, cT !#.` |
| `0x8400ac43` | 8 | — | `EXF# !+3` |
| `0x8400b869` | 9 | — | `!#$q #&q!` |
| `0x8400c866` | 26 | — | `# !5#.13#,A3#*Q3#(a3#&q3#"` |
| `0x8400c900` | 10 | — | `#(q+# q-#,` |
| `0x8400ca10` | 10 | — | `#(q##*1##,` |
| `0x8400d660` | 30 | `0x840037ba` | `timer_isr  Store Value %x, val` |
| `0x8400d682` | 9 | `0x840057fc` | `sram_init` |
| `0x8400d692` | 49 | `0x84002090` | `[%s] 5G pcieBase %lx mem_base %lx, hw_rx_desc=%lx` |
| `0x8400d6cb` | 51 | `0x840020a2` | `[%s] 2.4G pcieBase %lx mem_base %lx, hw_rx_desc=%lx` |
| `0x8400d700` | 25 | `0x840029d4` | `piNode[%d]           =%x ` |
| `0x8400d71b` | 90 | `0x84003d56` | `Error: exceed max num!interfaceid=%u wifi_mail_data->funcType=%u wifi_mail_data->funcId=%u` |
| `0x8400d777` | 11 | `0x84003e9c` | `%-15s<0x%x>` |
| `0x8400d784` | 31 | `0x84006a4c` | `constraint handler: bad message` |
| `0x8400d7a4` | 12 | `0x8400254c`,`0x840025e8` | `ba_node_init` |
| `0x8400d7b1` | 10 | `0x8400036c` | `core2_main` |
| `0x8400d7bc` | 43 | `0x84000edc` | `npu_ba_reorder():!!!!!!wrong pkt, wcid is 0` |
| `0x8400d7e9` | 11 | `0x84002420`,`0x84002548`,`0x840026a2` | `[NPU] %s...` |
| `0x8400d7f6` | 15 | `0x840026f4` | `driverModel =%s` |
| `0x8400d807` | 19 | `0x84003b10` | `npuMbox_notify_host` |
| `0x8400d820` | 54 | `0x84005aea` | `wifi_mail_data->wifi_mail_private.npuPktBufAddr=%x, %x` |
| `0x8400d858` | 29 | `0x8400297a` | `piNode[%d]->crsn         =%x ` |
| `0x8400d88b` | 57 | `0x84004382` | `Error: OVER GLB_VAR_SRAM_SIZE. _data:0x%x, __bss_end:0x%x` |
| `0x8400d8c6` | 21 | `0x84004596` | `%s done via base:0x%x` |
| `0x8400d8e5` | 14 | `0x84002492`,`0x84002818` | `pkt_queue_init` |
| `0x8400d8f4` | 8 | `0x840026a6` | `npu_init` |
| `0x8400d8fd` | 11 | `0x840026be` | `Sep 22 2025` |
| `0x8400d909` | 7 | `0x840026f8` | `MT7915D` |
| `0x8400d911` | 40 | `0x84003a0a` | `%s cpu_tmr:%d is wrong, should be 0 or 1` |
| `0x8400d943` | 46 | `0x84002052`,`0x84002052` | `!!!!!!!!!!!! [%s] rxd init: alloc buffid fail!` |
| `0x8400d973` | 35 | `0x84000982`,`0x84000a36` | `ba_indicate_reordering_mpdus_le_seq` |
| `0x8400d997` | 76 | `0x84000a32` | `[%d,%s]!!ERROR:[%s] bufid is over: %d, or pkt len is msdu_blk->datalength=%d` |
| `0x8400d9e5` | 45 | `0x840036aa` | `%s timer_no:%d is wrong, should be 0,1,2,or 3` |
| `0x8400da14` | 37 | `0x84003d18` | `wifi_mail_get_nowait_operation fail !` |
| `0x8400da3b` | 14 | `0x84003f04` | `Cyc_Cnt[63:32]` |
| `0x8400da4a` | 32 | `0x84002074` | `[NPU] %s, addr=%lx, band_idx=%d ` |
| `0x8400da6c` | 16 | `0x84002210` | `npu_set_ba_entry` |
| `0x8400da7d` | 9 | `0x84002e7c` | `qdma_init` |
| `0x8400da87` | 26 | `0x84002192` | `ERROR!!!!! %s[%s]wcid == 0` |
| `0x8400daa3` | 43 | `0x84003628` | `%s timer_no:%d is disabled, so just return!` |
| `0x8400dad0` | 32 | `0x84003bd4` | `Error: core_id:%d != mbox_idx:%d` |
| `0x8400daf9` | 17 | `0x8400517c` | `baNode_array_init` |
| `0x8400db0d` | 8 | `0x840035fa`,`0x8400362c` | `delay1ms` |
| `0x8400db16` | 23 | `0x84006438` | `printf_s: %n disallowed` |
| `0x8400db2e` | 19 | `0x84001e30` | `npu_npu_init_rxd_5G` |
| `0x8400db48` | 7 | `0x84003f9c` | `MSTATUS` |
| `0x8400db63` | 31 | `0x84004060` | `UnKnow Interrupts (mcause:0x%x)` |
| `0x8400db84` | 23 | `0x840058d6` | `Support NPU Wifi ffload` |
| `0x8400db9d` | 24 | `0x84005b5e` | `%s: pcie port%d link up ` |
| `0x8400dbbb` | 10 | `0x84000420` | `core7_main` |
| `0x8400dbc6` | 16 | `0x8400237a` | `npu_set_bar_info` |
| `0x8400dbd7` | 76 | `0x8400097e` | `[%d,%s]!![%s]ERROR: bufid is over: %d, or pkt len is mpdu_blk->datalength=%d` |
| `0x8400dc25` | 25 | `0x84002a16` | `piNode->foe          =%x ` |
| `0x8400dc40` | 61 | `0x8400303a` | `(%s) implement code to clear intrSrc:%d then execute ISR here` |
| `0x8400dc7f` | 47 | `0x840035f6`,`0x84003724`,`0x8400387c`,`0x840038e0`,`0x84003954`,`0x840039a6` | `%s: ERROR: trying to get time before init timer` |
| `0x8400dcb0` | 44 | `0x84003b0c` | `Error: %s timeout for core_id:%d, func_id:%d` |
| `0x8400dcde` | 13 | `0x84003eea` | `Cyc_Cnt[31:0]` |
| `0x8400dcec` | 35 | — | `Chip id(%x) support NPU Wifi ffload` |
| `0x8400dd11` | 26 | `0x840066fe` | `printf_s: bad %ls argument` |
| `0x8400dd30` | 30 | `0x84000f50`,`0x8400220c`,`0x84002376` | `ERROR!!!!! %s[%s]wcid == %d %d` |
| `0x8400dd50` | 14 | `0x84003a0e` | `cpu_timer_init` |
| `0x8400dd5f` | 43 | `0x840008f0`,`0x84003ca6` | `Error(%s): physAddr:0x%x==0 or size:0x%x==0` |
| `0x8400dd8c` | 46 | `0x8400577e` | `warning: length (%d) more than buffer len(%d)!` |
| `0x8400ddc0` | 16 | `0x84007c34`,`0x8400cd9c` | `0123456789abcdef` |
| `0x8400ddd1` | 26 | `0x8400030c` | `(%s) NPU Version: 20231025` |
| `0x8400dded` | 10 | `0x84000390` | `core3_main` |
| `0x8400ddf8` | 17 | `0x84002070` | `npu_set_pcie_base` |
| `0x8400de11` | 26 | `0x840013ce` | `!!!!!!!!!!!! enqueu error!` |
| `0x8400de2c` | 47 | `0x84003c4e` | `Error: (core_id:%d >= %d) \|\| (func_id:%d >= %d)` |
| `0x8400de5d` | 37 | `0x84003cee` | `wifi_mail_set_nowait_operation fail !` |
| `0x8400de84` | 17 | `0x84000f00`,`0x840022d0`,`0x840057c4` | `Ring Index error!` |
| `0x8400de97` | 16 | `0x84002424`,`0x84002614` | `queue_mutex_init` |
| `0x8400dea8` | 47 | — | `Chip id(%x) does not support NPU Wifi ffload!!!` |
| `0x8400ded9` | 16 | `0x84007b8c`,`0x8400ce0a` | `0123456789ABCDEF` |
| `0x8400deea` | 25 | `0x84002a26` | `piNode->status       =%x ` |
| `0x8400df05` | 36 | `0x84003d8e` | `enter wifi_mail_get_wait_operation !` |
| `0x8400df2b` | 51 | `0x84001f4c` | `[NPU] Enter(2.4G) %s, band_idx=%d, rx_ring_size=%lu` |
| `0x8400df60` | 12 | `0x8400248e`,`0x84002814` | `[NPU]  %s...` |
| `0x8400df74` | 29 | `0x8400298c` | `piNode[%d]->foe          =%x ` |
| `0x8400df93` | 25 | `0x840068a2` | `printf_s: bad %s argument` |
| `0x8400dfad` | 25 | `0x840029e6` | `piNode->bufid        =%x ` |
| `0x8400dfc8` | 43 | `0x84002cb0`,`0x84002f0a` | `The TX DSCP is not return from tx used pool` |
| `0x8400dff5` | 35 | `0x84003dac` | `wifi_mail_get_wait_operation fail !` |
| `0x8400e01a` | 7 | `0x84003fb4` | `MICAUSE` |
| `0x8400e022` | 12 | `0x84005872` | `counter_init` |
| `0x8400e02f` | 29 | `0x8400299e` | `piNode[%d]->status       =%x ` |
| `0x8400e04e` | 34 | `0x840036fc`,`0x84003858` | `%s: Timer clk is running at %d Mhz` |
| `0x8400e072` | 21 | `0x84003728`,`0x84003880`,`0x840038e4`,`0x84003958`,`0x840039aa` | `get_tmrTime_by_msTime` |
| `0x8400e088` | 18 | `0x84004358` | `core freq at %d Hz` |
| `0x8400e09c` | 58 | `0x84000328` | `(%s) just return, because QDMA driver for ARM is not ready` |
| `0x8400e0d8` | 10 | `0x840003e0` | `core5_main` |
| `0x8400e0e3` | 29 | `0x84002968` | `piNode[%d]->pkt_len      =%x ` |
| `0x8400e102` | 13 | `0x8400459a` | `npu_uart_init` |
| `0x8400e110` | 33 | `0x84001f2c`,`0x84001f2c` | `[%s] rxd init: alloc buffid fail!` |
| `0x8400e133` | 16 | `0x84001f50` | `npu_npu_init_rxd` |
| `0x8400e144` | 29 | `0x84002956` | `piNode[%d]->bufid        =%x ` |
| `0x8400e163` | 25 | `0x840029f6` | `piNode->pkt_len      =%x ` |
| `0x8400e17e` | 27 | `0x84003d66` | `not support unknow funcType` |
| `0x8400e19b` | 10 | `0x84000308` | `core0_main` |
| `0x8400e1a6` | 10 | `0x840003c0` | `core4_main` |
| `0x8400e1b1` | 10 | `0x84000400` | `core6_main` |
| `0x8400e1bc` | 24 | `0x84001e4e`,`0x84001f6e` | `ERROR! rx_ring_size = %d` |
| `0x8400e1d6` | 10 | `0x840036ae`,`0x84003700`,`0x8400385c` | `timer_init` |
| `0x8400e1e1` | 13 | `0x84003f1e` | `Ins_Cnt[31:0]` |
| `0x8400e1ef` | 11 | `0x84005320` | `np_skb_init` |
| `0x8400e1fb` | 19 | `0x8400039c` | `do npu_enq_deq_init` |
| `0x8400e210` | 30 | `0x84001e64` | `5G init second: do np_skb free` |
| `0x8400e230` | 22 | `0x840026ba` | `NPU Version: %s_NPU_%s` |
| `0x8400e248` | 13 | `0x84000f54` | `GET_BA_ENTRY_` |
| `0x8400e256` | 29 | `0x84002944` | `piNode[%d]               =%x ` |
| `0x8400e275` | 11 | `0x8400303e` | `default_isr` |
| `0x8400e281` | 16 | `0x84003caa` | `npu_get_mboxBuff` |
| `0x8400e292` | 35 | `0x84003d42` | `wifi_mail_set_wait_operation fail !` |
| `0x8400e2b7` | 39 | `0x84005b3a` | `%x= isforTestNoBA=%x isCounterEnable=%d` |
| `0x8400e2e0` | 29 | `0x84005b62` | `wifi_mail_set_wait_PCIE_state` |
| `0x8400e302` | 10 | `0x84000348` | `core1_main` |
| `0x8400e30d` | 18 | `0x84002196` | `npu_reset_ba_entry` |
| `0x8400e320` | 25 | `0x84002a06` | `piNode->crsn         =%x ` |
| `0x8400e33b` | 23 | `0x840069e0` | `printf: bad %n argument` |
| `0x8400e353` | 45 | `0x84001e2c` | `[NPU] Enter %s, band_idx=%d, rx_ring_size=%lu` |
| `0x8400e387` | 14 | `0x84003f38` | `Ins_Cnt[63:32]` |
| `0x8400e396` | 12 | `0x84003fc0` | `Show CSR END` |
