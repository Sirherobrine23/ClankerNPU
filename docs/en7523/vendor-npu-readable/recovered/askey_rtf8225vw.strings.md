# ASCII strings and verified address references — askey_rtf8225vw

Printable runs may be false positives inside instructions. A reference is reported only if the LUI+ADDI sequence resolves into the run; addresses may still be part of data rather than code.

| Address | Length | Xrefs | String (truncated) |
|---|---:|---|---|
| `0x84000a73` | 9 | — | `@p-oP\`&7%` |
| `0x84000a83` | 7 | — | `@p,oP\`%` |
| `0x8400180b` | 8 | — | `TAao@\`L&` |
| `0x840018e0` | 8 | — | `Aao@ ?}T` |
| `0x8400205f` | 7 | — | ` \`[o00J` |
| `0x84007328` | 7 | — | `}qLEPI"` |
| `0x840079a5` | 19 | — | `.#$A/#"Q/# a/#.q-#,` |
| `0x8400942d` | 20 | — | ` #&1!#$A!#"Q!# a!#.q` |
| `0x840098bf` | 7 | — | `BLBPF#&` |
| `0x840098cf` | 19 | — | `$#.1##*Q##(a##&q##$` |
| `0x84009cb3` | 9 | — | `!#$Q!#&1!` |
| `0x84009fd8` | 8 | — | `, cT !#.` |
| `0x8400abb3` | 8 | — | `EXF# !+3` |
| `0x8400b7d9` | 9 | — | `!#$q #&q!` |
| `0x8400c7d6` | 26 | — | `# !5#.13#,A3#*Q3#(a3#&q3#"` |
| `0x8400c870` | 10 | — | `#(q+# q-#,` |
| `0x8400c980` | 10 | — | `#(q##*1##,` |
| `0x8400d5c0` | 30 | `0x8400372a` | `timer_isr  Store Value %x, val` |
| `0x8400d5e2` | 9 | `0x8400576c` | `sram_init` |
| `0x8400d5f2` | 49 | `0x8400203c` | `[%s] 5G pcieBase %lx mem_base %lx, hw_rx_desc=%lx` |
| `0x8400d62b` | 51 | `0x8400204e` | `[%s] 2.4G pcieBase %lx mem_base %lx, hw_rx_desc=%lx` |
| `0x8400d660` | 25 | `0x84002944` | `piNode[%d]           =%x ` |
| `0x8400d67b` | 90 | `0x84003cc6` | `Error: exceed max num!interfaceid=%u wifi_mail_data->funcType=%u wifi_mail_data->funcId=%u` |
| `0x8400d6d7` | 11 | `0x84003e0c` | `%-15s<0x%x>` |
| `0x8400d6e4` | 31 | `0x840069bc` | `constraint handler: bad message` |
| `0x8400d704` | 12 | `0x840024e0`,`0x84002576` | `ba_node_init` |
| `0x8400d711` | 10 | `0x8400036c` | `core2_main` |
| `0x8400d71c` | 43 | `0x84000eda` | `npu_ba_reorder():!!!!!!wrong pkt, wcid is 0` |
| `0x8400d749` | 11 | `0x840023b4`,`0x840024dc`,`0x84002570`,`0x8400262e` | `[NPU] %s...` |
| `0x8400d756` | 15 | `0x84002664` | `driverModel =%s` |
| `0x8400d767` | 19 | `0x84003a80` | `npuMbox_notify_host` |
| `0x8400d780` | 54 | `0x84005a5a` | `wifi_mail_data->wifi_mail_private.npuPktBufAddr=%x, %x` |
| `0x8400d7b8` | 29 | `0x840028ea` | `piNode[%d]->crsn         =%x ` |
| `0x8400d7eb` | 57 | `0x840042f2` | `Error: OVER GLB_VAR_SRAM_SIZE. _data:0x%x, __bss_end:0x%x` |
| `0x8400d826` | 21 | `0x84004506` | `%s done via base:0x%x` |
| `0x8400d845` | 14 | `0x84002426`,`0x84002788` | `pkt_queue_init` |
| `0x8400d854` | 8 | `0x84002632` | `npu_init` |
| `0x8400d85d` | 7 | `0x84002668` | `MT7915D` |
| `0x8400d865` | 40 | `0x8400397a` | `%s cpu_tmr:%d is wrong, should be 0 or 1` |
| `0x8400d897` | 46 | `0x84001ffe`,`0x84001ffe` | `!!!!!!!!!!!! [%s] rxd init: alloc buffid fail!` |
| `0x8400d8c7` | 35 | `0x84000982`,`0x84000a36` | `ba_indicate_reordering_mpdus_le_seq` |
| `0x8400d8eb` | 76 | `0x84000a32` | `[%d,%s]!!ERROR:[%s] bufid is over: %d, or pkt len is msdu_blk->datalength=%d` |
| `0x8400d939` | 45 | `0x8400361a` | `%s timer_no:%d is wrong, should be 0,1,2,or 3` |
| `0x8400d968` | 37 | `0x84003c88` | `wifi_mail_get_nowait_operation fail !` |
| `0x8400d98f` | 14 | `0x84003e74` | `Cyc_Cnt[63:32]` |
| `0x8400d99e` | 32 | `0x84002020` | `[NPU] %s, addr=%lx, band_idx=%d ` |
| `0x8400d9c0` | 9 | `0x84002dec` | `qdma_init` |
| `0x8400d9ca` | 43 | `0x84003598` | `%s timer_no:%d is disabled, so just return!` |
| `0x8400d9f7` | 32 | `0x84003b44` | `Error: core_id:%d != mbox_idx:%d` |
| `0x8400da20` | 17 | `0x840050ec` | `baNode_array_init` |
| `0x8400da34` | 8 | `0x8400356a`,`0x8400359c` | `delay1ms` |
| `0x8400da3d` | 23 | `0x840063a8` | `printf_s: %n disallowed` |
| `0x8400da55` | 19 | `0x84001ddc` | `npu_npu_init_rxd_5G` |
| `0x8400da6f` | 7 | `0x84003f0c` | `MSTATUS` |
| `0x8400da8a` | 31 | `0x84003fd0` | `UnKnow Interrupts (mcause:0x%x)` |
| `0x8400daab` | 23 | `0x84005846` | `Support NPU Wifi ffload` |
| `0x8400dac4` | 24 | `0x84005ace` | `%s: pcie port%d link up ` |
| `0x8400dae2` | 10 | `0x84000420` | `core7_main` |
| `0x8400daed` | 76 | `0x8400097e` | `[%d,%s]!![%s]ERROR: bufid is over: %d, or pkt len is mpdu_blk->datalength=%d` |
| `0x8400db3b` | 25 | `0x84002986` | `piNode->foe          =%x ` |
| `0x8400db56` | 61 | `0x84002faa` | `(%s) implement code to clear intrSrc:%d then execute ISR here` |
| `0x8400db95` | 47 | `0x84003566`,`0x84003694`,`0x840037ec`,`0x84003850`,`0x840038c4`,`0x84003916` | `%s: ERROR: trying to get time before init timer` |
| `0x8400dbc6` | 44 | `0x84003a7c` | `Error: %s timeout for core_id:%d, func_id:%d` |
| `0x8400dbf4` | 13 | `0x84003e5a` | `Cyc_Cnt[31:0]` |
| `0x8400dc02` | 35 | — | `Chip id(%x) support NPU Wifi ffload` |
| `0x8400dc27` | 26 | `0x8400666e` | `printf_s: bad %ls argument` |
| `0x8400dc46` | 14 | `0x8400397e` | `cpu_timer_init` |
| `0x8400dc55` | 43 | `0x84003c16` | `Error(%s): physAddr:0x%x==0 or size:0x%x==0` |
| `0x8400dc82` | 46 | `0x840056ee` | `warning: length (%d) more than buffer len(%d)!` |
| `0x8400dcb6` | 16 | `0x84007ba4`,`0x8400cd0c` | `0123456789abcdef` |
| `0x8400dcc7` | 26 | `0x8400030c` | `(%s) NPU Version: 20231025` |
| `0x8400dce3` | 10 | `0x84000390` | `core3_main` |
| `0x8400dcee` | 17 | `0x8400201c` | `npu_set_pcie_base` |
| `0x8400dd07` | 26 | `0x84001382` | `!!!!!!!!!!!! enqueu error!` |
| `0x8400dd22` | 47 | `0x84003bbe` | `Error: (core_id:%d >= %d) \|\| (func_id:%d >= %d)` |
| `0x8400dd53` | 37 | `0x84003c5e` | `wifi_mail_set_nowait_operation fail !` |
| `0x8400dd7a` | 17 | `0x840008f0`,`0x84005734` | `Ring Index error!` |
| `0x8400dd8d` | 16 | `0x840023b8`,`0x840025a0` | `queue_mutex_init` |
| `0x8400dd9e` | 47 | — | `Chip id(%x) does not support NPU Wifi ffload!!!` |
| `0x8400ddcf` | 16 | `0x84007afc`,`0x8400cd7a` | `0123456789ABCDEF` |
| `0x8400dde0` | 25 | `0x84002996` | `piNode->status       =%x ` |
| `0x8400ddfb` | 36 | `0x84003cfe` | `enter wifi_mail_get_wait_operation !` |
| `0x8400de21` | 51 | `0x84001ef8` | `[NPU] Enter(2.4G) %s, band_idx=%d, rx_ring_size=%lu` |
| `0x8400de56` | 12 | `0x84002422`,`0x84002784` | `[NPU]  %s...` |
| `0x8400de64` | 29 | `0x840028fc` | `piNode[%d]->foe          =%x ` |
| `0x8400de83` | 25 | `0x84006812` | `printf_s: bad %s argument` |
| `0x8400de9d` | 25 | `0x84002956` | `piNode->bufid        =%x ` |
| `0x8400deb8` | 43 | `0x84002c20`,`0x84002e7a` | `The TX DSCP is not return from tx used pool` |
| `0x8400dee5` | 35 | `0x84003d1c` | `wifi_mail_get_wait_operation fail !` |
| `0x8400df0a` | 7 | `0x84003f24` | `MICAUSE` |
| `0x8400df12` | 12 | `0x840057e2` | `counter_init` |
| `0x8400df1f` | 29 | `0x8400290e` | `piNode[%d]->status       =%x ` |
| `0x8400df3e` | 34 | `0x8400366c`,`0x840037c8` | `%s: Timer clk is running at %d Mhz` |
| `0x8400df62` | 21 | `0x84003698`,`0x840037f0`,`0x84003854`,`0x840038c8`,`0x8400391a` | `get_tmrTime_by_msTime` |
| `0x8400df78` | 18 | `0x840042c8` | `core freq at %d Hz` |
| `0x8400df8c` | 58 | `0x84000328` | `(%s) just return, because QDMA driver for ARM is not ready` |
| `0x8400dfc8` | 10 | `0x840003e0` | `core5_main` |
| `0x8400dfd3` | 29 | `0x840028d8` | `piNode[%d]->pkt_len      =%x ` |
| `0x8400dff2` | 13 | `0x8400450a` | `npu_uart_init` |
| `0x8400e000` | 33 | `0x84001ed8`,`0x84001ed8` | `[%s] rxd init: alloc buffid fail!` |
| `0x8400e023` | 16 | `0x84001efc` | `npu_npu_init_rxd` |
| `0x8400e034` | 29 | `0x840028c6` | `piNode[%d]->bufid        =%x ` |
| `0x8400e053` | 25 | `0x84002966` | `piNode->pkt_len      =%x ` |
| `0x8400e06e` | 27 | `0x84003cd6` | `not support unknow funcType` |
| `0x8400e08b` | 10 | `0x84000308` | `core0_main` |
| `0x8400e096` | 10 | `0x840003c0` | `core4_main` |
| `0x8400e0a1` | 10 | `0x84000400` | `core6_main` |
| `0x8400e0ac` | 24 | `0x84001dfa`,`0x84001f1a` | `ERROR! rx_ring_size = %d` |
| `0x8400e0c6` | 10 | `0x8400361e`,`0x84003670`,`0x840037cc` | `timer_init` |
| `0x8400e0d1` | 13 | `0x84003e8e` | `Ins_Cnt[31:0]` |
| `0x8400e0df` | 11 | `0x84005290` | `np_skb_init` |
| `0x8400e0eb` | 19 | `0x8400039c` | `do npu_enq_deq_init` |
| `0x8400e100` | 30 | `0x84001e10` | `5G init second: do np_skb free` |
| `0x8400e120` | 29 | `0x840028b4` | `piNode[%d]               =%x ` |
| `0x8400e13f` | 11 | `0x84002fae` | `default_isr` |
| `0x8400e14b` | 16 | `0x84003c1a` | `npu_get_mboxBuff` |
| `0x8400e15c` | 35 | `0x84003cb2` | `wifi_mail_set_wait_operation fail !` |
| `0x8400e181` | 39 | `0x84005aaa` | `%x= isforTestNoBA=%x isCounterEnable=%d` |
| `0x8400e1aa` | 29 | `0x84005ad2` | `wifi_mail_set_wait_PCIE_state` |
| `0x8400e1cc` | 10 | `0x84000348` | `core1_main` |
| `0x8400e1d7` | 25 | `0x84002976` | `piNode->crsn         =%x ` |
| `0x8400e1f2` | 23 | `0x84006950` | `printf: bad %n argument` |
| `0x8400e20a` | 45 | `0x84001dd8` | `[NPU] Enter %s, band_idx=%d, rx_ring_size=%lu` |
| `0x8400e23e` | 24 | `0x840020e4`,`0x840021d0`,`0x84002270` | `ERROR!!!!! [%s]wcid == 0` |
| `0x8400e258` | 14 | `0x84003ea8` | `Ins_Cnt[63:32]` |
| `0x8400e267` | 12 | `0x84003f30` | `Show CSR END` |
