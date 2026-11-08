# High-value manually identified firmware routines from the vendor EN7523 MT7915 image.
# Offsets and mnemonics are preserved from the static disassembly.
# Embedded data or call trampolines can complicate precise function boundaries.

# ======================================================================
# boot entry 0x84000108..0x84000174
84000108: 9a901197 auipc	gp, 0x9a901                                                          # FUNCTION START / Per-hart boot entry, BSS initialization and startup handoff
8400010c: 9d018193 addi	gp, gp, -0x630                                                       
84000110: f14022f3 csrrs	t0, mhartid, zero                                                   
84000114: 00000313 addi	t1, zero, 0x0                                                        
84000118: 02629c63 bne	t0, t1, 0x84000150 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x150>
8400011c: 1ec0c2b7 lui	t0, 0x1ec0c                                                           
84000120: 14028293 addi	t0, t0, 0x140                                                        
84000124: 0002a303 lw	t1, 0x0(t0)                                                            
84000128: fff00393 addi	t2, zero, -0x1                                                       
8400012c: 02730263 beq	t1, t2, 0x84000150 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x150>
84000130: 9a900297 auipc	t0, 0x9a900                                                         
84000134: 1a828293 addi	t0, t0, 0x1a8                                                        
84000138: 9a903317 auipc	t1, 0x9a903                                                         
8400013c: 96030313 addi	t1, t1, -0x6a0                                                       
84000140: 00430313 addi	t1, t1, 0x4                                                          
84000144: 0002a023 sw	zero, 0x0(t0)                                                          
84000148: 00428293 addi	t0, t0, 0x4                                                          
8400014c: fe629ce3 bne	t0, t1, 0x84000144 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x144>
84000150: 30004137 lui	sp, 0x30004                                                           
84000154: 00000097 auipc	ra, 0x0                                                             
84000158: ff010113 addi	sp, sp, -0x10                                                        
8400015c: 00112423 sw	ra, 0x8(sp)                                                            
84000160: 234040ef jal	ra, 0x84004394 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x4394> # [CALL] sub_84004394
84000164: 00000513 addi	a0, zero, 0x0                                                        
84000168: 00000593 addi	a1, zero, 0x0                                                        
8400016c: 2a05 c.jal	0x8400029c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x29c> # [CALL] vendor_core_dispatch
8400016e: 0000006f jal	zero, 0x8400016e <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x16e>
84000172: 0000 c.unimp                                                                   

# ======================================================================
# trap entry 0x84000174..0x8400029c
84000174: f8010113 addi	sp, sp, -0x80                                                         # FUNCTION START / Trap register save, trap dispatch and mret
84000178: 00112223 sw	ra, 0x4(sp)                                                            
8400017c: 00212423 sw	sp, 0x8(sp)                                                            
84000180: 00312623 sw	gp, 0xc(sp)                                                            
84000184: 00412823 sw	tp, 0x10(sp)                                                           
84000188: 00512a23 sw	t0, 0x14(sp)                                                           
8400018c: 00612c23 sw	t1, 0x18(sp)                                                           
84000190: 00712e23 sw	t2, 0x1c(sp)                                                           
84000194: 02812023 sw	s0, 0x20(sp)                                                           
84000198: 02912223 sw	s1, 0x24(sp)                                                           
8400019c: 02a12423 sw	a0, 0x28(sp)                                                           
840001a0: 02b12623 sw	a1, 0x2c(sp)                                                           
840001a4: 02c12823 sw	a2, 0x30(sp)                                                           
840001a8: 02d12a23 sw	a3, 0x34(sp)                                                           
840001ac: 02e12c23 sw	a4, 0x38(sp)                                                           
840001b0: 02f12e23 sw	a5, 0x3c(sp)                                                           
840001b4: 05012023 sw	a6, 0x40(sp)                                                           
840001b8: 05112223 sw	a7, 0x44(sp)                                                           
840001bc: 05212423 sw	s2, 0x48(sp)                                                           
840001c0: 05312623 sw	s3, 0x4c(sp)                                                           
840001c4: 05412823 sw	s4, 0x50(sp)                                                           
840001c8: 05512a23 sw	s5, 0x54(sp)                                                           
840001cc: 05612c23 sw	s6, 0x58(sp)                                                           
840001d0: 05712e23 sw	s7, 0x5c(sp)                                                           
840001d4: 07812023 sw	s8, 0x60(sp)                                                           
840001d8: 07912223 sw	s9, 0x64(sp)                                                           
840001dc: 07a12423 sw	s10, 0x68(sp)                                                          
840001e0: 07b12623 sw	s11, 0x6c(sp)                                                          
840001e4: 07c12823 sw	t3, 0x70(sp)                                                           
840001e8: 07d12a23 sw	t4, 0x74(sp)                                                           
840001ec: 07e12c23 sw	t5, 0x78(sp)                                                           
840001f0: 07f12e23 sw	t6, 0x7c(sp)                                                           
840001f4: 34202573 csrrs	a0, mcause, zero                                                    
840001f8: 341025f3 csrrs	a1, mepc, zero                                                      
840001fc: 00010613 addi	a2, sp, 0x0                                                          
84000200: 130040ef jal	ra, 0x84004330 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x4330> # [CALL] vendor_trap_handler
84000204: 34151073 csrrw	zero, mepc, a0                                                      
84000208: 000022b7 lui	t0, 0x2                                                               
8400020c: 80028293 addi	t0, t0, -0x800                                                       
84000210: 3002a073 csrrs	zero, mstatus, t0                                                   
84000214: 00412083 lw	ra, 0x4(sp)                                                            
84000218: 00812103 lw	sp, 0x8(sp)                                                            
8400021c: 00c12183 lw	gp, 0xc(sp)                                                            
84000220: 01012203 lw	tp, 0x10(sp)                                                           
84000224: 01412283 lw	t0, 0x14(sp)                                                           
84000228: 01812303 lw	t1, 0x18(sp)                                                           
8400022c: 01c12383 lw	t2, 0x1c(sp)                                                           
84000230: 02012403 lw	s0, 0x20(sp)                                                           
84000234: 02412483 lw	s1, 0x24(sp)                                                           
84000238: 02812503 lw	a0, 0x28(sp)                                                           
8400023c: 02c12583 lw	a1, 0x2c(sp)                                                           
84000240: 03012603 lw	a2, 0x30(sp)                                                           
84000244: 03412683 lw	a3, 0x34(sp)                                                           
84000248: 03812703 lw	a4, 0x38(sp)                                                           
8400024c: 03c12783 lw	a5, 0x3c(sp)                                                           
84000250: 04012803 lw	a6, 0x40(sp)                                                           
84000254: 04412883 lw	a7, 0x44(sp)                                                           
84000258: 04812903 lw	s2, 0x48(sp)                                                           
8400025c: 04c12983 lw	s3, 0x4c(sp)                                                           
84000260: 05012a03 lw	s4, 0x50(sp)                                                           
84000264: 05412a83 lw	s5, 0x54(sp)                                                           
84000268: 05812b03 lw	s6, 0x58(sp)                                                           
8400026c: 05c12b83 lw	s7, 0x5c(sp)                                                           
84000270: 06012c03 lw	s8, 0x60(sp)                                                           
84000274: 06412c83 lw	s9, 0x64(sp)                                                           
84000278: 06812d03 lw	s10, 0x68(sp)                                                          
8400027c: 06c12d83 lw	s11, 0x6c(sp)                                                          
84000280: 07012e03 lw	t3, 0x70(sp)                                                           
84000284: 07412e83 lw	t4, 0x74(sp)                                                           
84000288: 07812f03 lw	t5, 0x78(sp)                                                           
8400028c: 07c12f83 lw	t6, 0x7c(sp)                                                           
84000290: 08010113 addi	sp, sp, 0x80                                                         
84000294: 30200073 mret                                                                      
84000298: 0000006f jal	zero, 0x84000298 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x298>

# ======================================================================
# QDMA IRQ poll 0x84002ecc..0x84003004
84002ecc: 2b4032ef jal	t0, 0x84006180 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x6180> # FUNCTION START / QDMA IRQ ring poll (inferred from MMIO) ; [CALL] sub_84006180
84002ed0: 1fb54537 lui	a0, 0x1fb54                                                           
84002ed4: 45c1 c.li	a1, 0x10                                                             
84002ed6: 02050513 addi	a0, a0, 0x20                                                         
84002eda: 1e903cb7 lui	s9, 0x1e903                                                           
84002ede: c10c c.sw	a1, 0x0(a0)                                                          
84002ee0: 40000c37 lui	s8, 0x40000                                                           
84002ee4: a84ca503 lw	a0, -0x57c(s9)                                                         
84002ee8: 4481 c.li	s1, 0x0                                                              
84002eea: 59fd c.li	s3, -0x1                                                             
84002eec: 00070a37 lui	s4, 0x70                                                              
84002ef0: 494c c.lw	a1, 0x14(a0)                                                         
84002ef2: 3ff00a93 addi	s5, zero, 0x3ff                                                      
84002ef6: 00060b37 lui	s6, 0x60                                                              
84002efa: 4bb1 c.li	s7, 0xc                                                              
84002efc: 1c7d c.addi	s8, -0x1                                                           
84002efe: 0585 c.addi	a1, 0x1                                                            
84002f00: c94c c.sw	a1, 0x14(a0)                                                         
84002f02: 1e9035b7 lui	a1, 0x1e903                                                           
84002f06: 1e901537 lui	a0, 0x1e901                                                           
84002f0a: 8185a583 lw	a1, -0x7e8(a1)                                                         
84002f0e: edc52603 lw	a2, -0x124(a0)                                                         
84002f12: 20c59633 md.sh2add	a2, a1, a2                                                       # [CUSTOM] md32rv
84002f16: 420c c.lw	a1, 0x0(a2)                                                          
84002f18: 01f5a567 md.bnei	a1, -1, 0x84002f22                                                 # [CUSTOM] md32rv
84002f1c: 420c c.lw	a1, 0x0(a2)                                                          
84002f1e: 0df59a67 md.beqi	a1, -1, 0x84002ff2                                                 # [CUSTOM] md32rv
84002f22: 01362023 sw	s3, 0x0(a2)                                                            
84002f26: 0485 c.addi	s1, 0x1                                                            
84002f28: edc52603 lw	a2, -0x124(a0)                                                         
84002f2c: 00160693 addi	a3, a2, 0x1                                                          
84002f30: 08c5e633 md.maskl	a2, a1, 12                                                        # [CUSTOM] md32rv
84002f34: 3ff6f693 andi	a3, a3, 0x3ff                                                        
84002f38: ecd52e23 sw	a3, -0x124(a0)                                                         
84002f3c: 02caed63 bltu	s5, a2, 0x84002f76 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f76>
84002f40: 0145f533 and	a0, a1, s4                                                            
84002f44: 03651963 bne	a0, s6, 0x84002f76 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f76>
84002f48: 1e903537 lui	a0, 0x1e903                                                           
84002f4c: 037605b3 mul	a1, a2, s7                                                            
84002f50: 81c52503 lw	a0, -0x7e4(a0)                                                         
84002f54: 00b50433 add	s0, a0, a1                                                            
84002f58: 4048 c.lw	a0, 0x4(s0)                                                          
84002f5a: 00c51463 bne	a0, a2, 0x84002f62 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f62>
84002f5e: 4408 c.lw	a0, 0x8(s0)                                                          
84002f60: c50d c.beqz	a0, 0x84002f8a <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f8a>
84002f62: a84ca503 lw	a0, -0x57c(s9)                                                         
84002f66: 550c c.lw	a1, 0x28(a0)                                                         
84002f68: 0585 c.addi	a1, 0x1                                                            
84002f6a: d50c c.sw	a1, 0x28(a0)                                                         
84002f6c: 08000513 addi	a0, zero, 0x80                                                       
84002f70: f8a499e3 bne	s1, a0, 0x84002f02 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f02>
84002f74: a041 c.j	0x84002ff4 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2ff4>
84002f76: a84ca503 lw	a0, -0x57c(s9)                                                         
84002f7a: 514c c.lw	a1, 0x24(a0)                                                         
84002f7c: 0585 c.addi	a1, 0x1                                                            
84002f7e: d14c c.sw	a1, 0x24(a0)                                                         
84002f80: 08000513 addi	a0, zero, 0x80                                                       
84002f84: f6a49fe3 bne	s1, a0, 0x84002f02 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f02>
84002f88: a0b5 c.j	0x84002ff4 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2ff4>
84002f8a: 4008 c.lw	a0, 0x0(s0)                                                          
84002f8c: 4148 c.lw	a0, 0x4(a0)                                                          
84002f8e: 00ac6c63 bltu	s8, a0, 0x84002fa6 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2fa6>
84002f92: a84ca503 lw	a0, -0x57c(s9)                                                         
84002f96: 554c c.lw	a1, 0x2c(a0)                                                         
84002f98: 0585 c.addi	a1, 0x1                                                            
84002f9a: d54c c.sw	a1, 0x2c(a0)                                                         
84002f9c: 08000513 addi	a0, zero, 0x80                                                       
84002fa0: f6a491e3 bne	s1, a0, 0x84002f02 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f02>
84002fa4: a881 c.j	0x84002ff4 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2ff4>
84002fa6: 1e903537 lui	a0, 0x1e903                                                           
84002faa: 00840d13 addi	s10, s0, 0x8                                                         
84002fae: 82050913 addi	s2, a0, -0x7e0                                                       
84002fb2: 854a c.mv	a0, s2                                                               
84002fb4: 091020ef jal	ra, 0x84005844 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x5844> # [CALL] sub_84005844
84002fb8: 4408 c.lw	a0, 0x8(s0)                                                          
84002fba: c901 c.beqz	a0, 0x84002fca <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2fca>
84002fbc: 8400e537 lui	a0, 0x8400e                                                           
84002fc0: 3fc50513 addi	a0, a0, 0x3fc                                                         # [STR] 0x8400e3fc: 'The TX DSCP is not return from tx used pool'
84002fc4: 1d5010ef jal	ra, 0x84004998 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x4998> # [CALL] sub_84004998
84002fc8: a829 c.j	0x84002fe2 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2fe2>
84002fca: 1e903537 lui	a0, 0x1e903                                                           
84002fce: 81452583 lw	a1, -0x7ec(a0)                                                         
84002fd2: 4190 c.lw	a2, 0x0(a1)                                                          
84002fd4: c219 c.beqz	a2, 0x84002fda <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2fda>
84002fd6: 41cc c.lw	a1, 0x4(a1)                                                          
84002fd8: 05a1 c.addi	a1, 0x8                                                            
84002fda: c180 c.sw	s0, 0x0(a1)                                                          
84002fdc: 81452503 lw	a0, -0x7ec(a0)                                                         
84002fe0: c140 c.sw	s0, 0x4(a0)                                                          
84002fe2: 854a c.mv	a0, s2                                                               
84002fe4: 0d5020ef jal	ra, 0x840058b8 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x58b8> # [CALL] sub_840058b8
84002fe8: 08000513 addi	a0, zero, 0x80                                                       
84002fec: f0a49be3 bne	s1, a0, 0x84002f02 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2f02>
84002ff0: a011 c.j	0x84002ff4 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x2ff4>
84002ff2: 8526 c.mv	a0, s1                                                               
84002ff4: 1fb545b7 lui	a1, 0x1fb54                                                           
84002ff8: 06458593 addi	a1, a1, 0x64                                                          # [MMIO] QDMA_IRQ_CLEAR = 0x1fb54064
84002ffc: c188 c.sw	a0, 0x0(a1)                                                          
84002ffe: 1ee0306f jal	zero, 0x840061ec <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x61ec>
84003002: 0001 c.nop                                                                     

# ======================================================================
# PPE return poll 0x84003004..0x840030c0
84003004: 17c032ef jal	t0, 0x84006180 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x6180> # FUNCTION START / PPE return FIFO polling and completion; [CALL] sub_84006180
84003008: 1e903937 lui	s2, 0x1e903                                                           
8400300c: a8492503 lw	a0, -0x57c(s2)                                                         
84003010: 414c c.lw	a1, 0x4(a0)                                                          
84003012: 0585 c.addi	a1, 0x1                                                            
84003014: c14c c.sw	a1, 0x4(a0)                                                          
84003016: 1fb51537 lui	a0, 0x1fb51                                                           
8400301a: 1511 c.addi	a0, -0x1c                                                          
8400301c: 4108 c.lw	a0, 0x0(a0)                                                          
8400301e: 09056c33 md.zext.h	s8, a0                                                           # [CUSTOM] md32rv
84003022: 080c1d67 md.beqi	s8, 0, 0x840030bc                                                  # [CUSTOM] md32rv
84003026: 1fb519b7 lui	s3, 0x1fb51                                                           
8400302a: 6ac1 c.lui	s5, 0x10                                                            
8400302c: 8a4e c.mv	s4, s3                                                               
8400302e: 6b09 c.lui	s6, 0x2                                                             
84003030: 4401 c.li	s0, 0x0                                                              
84003032: 1981 c.addi	s3, -0x20                                                          
84003034: fdca0a13 addi	s4, s4, -0x24                                                        
84003038: c00a8a93 addi	s5, s5, -0x400                                                       
8400303c: 300b0b13 addi	s6, s6, 0x300                                                        
84003040: 40000bb7 lui	s7, 0x40000                                                           
84003044: 0009a483 lw	s1, 0x0(s3)                                                            
84003048: 0604ba67 md.blti	s1, 0, 0x840030bc                                                  # [CUSTOM] md32rv
8400304c: 0904d533 md.extui	a0, s1, 16                                                        # [CUSTOM] md32rv
84003050: ed29 c.bnez	a0, 0x840030aa <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x30aa>
84003052: 000a2683 lw	a3, 0x0(s4)                                                            
84003056: 1e902637 lui	a2, 0x1e902                                                           
8400305a: a8492503 lw	a0, -0x57c(s2)                                                         
8400305e: 08000793 addi	a5, zero, 0x80                                                       
84003062: 450c c.lw	a1, 0x8(a0)                                                          
84003064: 0585 c.addi	a1, 0x1                                                            
84003066: c50c c.sw	a1, 0x8(a0)                                                          
84003068: 01049513 slli	a0, s1, 0x10                                                         
8400306c: bec62583 lw	a1, -0x414(a2)                                                         
84003070: 8511 c.srai	a0, 0x4                                                            
84003072: 08f6e633 md.maskl	a2, a3, 15                                                        # [CUSTOM] md32rv
84003076: 95aa c.add	a1, a0                                                              
84003078: 07049533 md.sext	a0, s1, 16                                                         # [CUSTOM] md32rv
8400307c: 45d8 c.lw	a4, 0xc(a1)                                                          
8400307e: 8656a6b3 md.bfextr16	a3, a3, 5                                                      # [CUSTOM] md32rv
84003082: 0005d583 lhu	a1, 0x0(a1)                                                           
84003086: 01577733 and	a4, a4, s5                                                            
8400308a: 00eb3733 sltu	a4, s6, a4                                                           
8400308e: 882e c.mv	a6, a1                                                               
84003090: c3cfd0ef jal	ra, 0x840004cc <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x4cc> # [CALL] vendor_process_ppe_cpu_return
84003094: cd19 c.beqz	a0, 0x840030b2 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x30b2>
84003096: 0904e533 md.zext.h	a0, s1                                                           # [CUSTOM] md32rv
8400309a: 492020ef jal	ra, 0x8400552c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x552c> # [CALL] vendor_complete_ppe_packet
8400309e: a8492503 lw	a0, -0x57c(s2)                                                         
840030a2: 454c c.lw	a1, 0xc(a0)                                                          
840030a4: 0585 c.addi	a1, 0x1                                                            
840030a6: c54c c.sw	a1, 0xc(a0)                                                          
840030a8: a029 c.j	0x840030b2 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x30b2>
840030aa: 0904e533 md.zext.h	a0, s1                                                           # [CUSTOM] md32rv
840030ae: 47e020ef jal	ra, 0x8400552c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x552c> # [CALL] vendor_complete_ppe_packet
840030b2: 0405 c.addi	s0, 0x1                                                            
840030b4: 0179a023 sw	s7, 0x0(s3)                                                            
840030b8: f98466e3 bltu	s0, s8, 0x84003044 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3044>
840030bc: 1300306f jal	zero, 0x840061ec <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x61ec>

# ======================================================================
# QDMA TX submit 0x840030c0..0x84003180
840030c0: 0dc032ef jal	t0, 0x8400619c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x619c> # FUNCTION START / Dequeue bookkeeping and submit TX descriptor; [CALL] sub_8400619c
840030c4: 842a c.mv	s0, a0                                                               
840030c6: 1e902537 lui	a0, 0x1e902                                                           
840030ca: bec52a03 lw	s4, -0x414(a0)                                                         
840030ce: 8932 c.mv	s2, a2                                                               
840030d0: 84ae c.mv	s1, a1                                                               
840030d2: 03c00593 addi	a1, zero, 0x3c                                                       
840030d6: 0095e463 bltu	a1, s1, 0x840030de <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x30de>
840030da: 03c00493 addi	s1, zero, 0x3c                                                       
840030de: 1e903537 lui	a0, 0x1e903                                                           
840030e2: 82050993 addi	s3, a0, -0x7e0                                                       
840030e6: 854e c.mv	a0, s3                                                               
840030e8: 75c020ef jal	ra, 0x84005844 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x5844> # [CALL] sub_84005844
840030ec: 1e9035b7 lui	a1, 0x1e903                                                           
840030f0: 8145a603 lw	a2, -0x7ec(a1)                                                         
840030f4: 4208 c.lw	a0, 0x0(a2)                                                          
840030f6: 4254 c.lw	a3, 0x4(a2)                                                          
840030f8: 00d50763 beq	a0, a3, 0x84003106 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3106>
840030fc: 4514 c.lw	a3, 0x8(a0)                                                          
840030fe: c214 c.sw	a3, 0x0(a2)                                                          
84003100: 8145a583 lw	a1, -0x7ec(a1)                                                         
84003104: a801 c.j	0x84003114 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3114>
84003106: 00062023 sw	zero, 0x0(a2)                                                          
8400310a: 8145a583 lw	a1, -0x7ec(a1)                                                         
8400310e: 0005a223 sw	zero, 0x4(a1)                                                          
84003112: c939 c.beqz	a0, 0x84003168 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3168>
84003114: 00052423 sw	zero, 0x8(a0)                                                          
84003118: 800006b7 lui	a3, 0x80000                                                           
8400311c: 4590 c.lw	a2, 0x8(a1)                                                          
8400311e: 1e903737 lui	a4, 0x1e903                                                           
84003122: 770406b3 md.bfinsr14	a3, s0, 16                                                     # [CUSTOM] md32rv
84003126: 4210 c.lw	a2, 0x0(a2)                                                          
84003128: ca14 c.sw	a3, 0x10(a2)                                                         
8400312a: 4689 c.li	a3, 0x2                                                              
8400312c: 82872703 lw	a4, -0x7d8(a4)                                                         
84003130: 670406b3 md.bfinsr12	a3, s0, 16                                                     # [CUSTOM] md32rv
84003134: ca58 c.sw	a4, 0x14(a2)                                                         
84003136: 96ca c.add	a3, s2                                                              
84003138: 4158 c.lw	a4, 0x4(a0)                                                          
8400313a: 96d2 c.add	a3, s4                                                              
8400313c: c614 c.sw	a3, 0x8(a2)                                                          
8400313e: c244 c.sw	s1, 0x4(a2)                                                          
84003140: 08c766b3 md.maskl	a3, a4, 12                                                        # [CUSTOM] md32rv
84003144: c654 c.sw	a3, 0xc(a2)                                                          
84003146: c588 c.sw	a0, 0x8(a1)                                                          
84003148: 0000000f fence	0, 0                                                                
8400314c: 4148 c.lw	a0, 0x4(a0)                                                          
8400314e: 1fb545b7 lui	a1, 0x1fb54                                                           
84003152: 1c858593 addi	a1, a1, 0x1c8                                                         # [MMIO] QDMA_TX_CPU = 0x1fb541c8
84003156: 08c56533 md.maskl	a0, a0, 12                                                        # [CUSTOM] md32rv
8400315a: c188 c.sw	a0, 0x0(a1)                                                          
8400315c: 854e c.mv	a0, s3                                                               
8400315e: 75a020ef jal	ra, 0x840058b8 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x58b8> # [CALL] sub_840058b8
84003162: 4501 c.li	a0, 0x0                                                              
84003164: 09c0306f jal	zero, 0x84006200 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x6200>
84003168: 1e903537 lui	a0, 0x1e903                                                           
8400316c: a8452503 lw	a0, -0x57c(a0)                                                         
84003170: 410c c.lw	a1, 0x0(a0)                                                          
84003172: 0585 c.addi	a1, 0x1                                                            
84003174: c10c c.sw	a1, 0x0(a0)                                                          
84003176: 854e c.mv	a0, s3                                                               
84003178: 740020ef jal	ra, 0x840058b8 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x58b8> # [CALL] sub_840058b8
8400317c: 557d c.li	a0, -0x1                                                             
8400317e: b7dd c.j	0x84003164 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3164>

# ======================================================================
# QDMA TX init 0x84003180..0x84003252
84003180: 01c032ef jal	t0, 0x8400619c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x619c> # FUNCTION START / Build 1024 descriptors and bookkeeping nodes; [CALL] sub_8400619c
84003184: 8400e537 lui	a0, 0x8400e                                                           
84003188: 8400e5b7 lui	a1, 0x8400e                                                           
8400318c: 70150513 addi	a0, a0, 0x701                                                        
84003190: f4958593 addi	a1, a1, -0xb7                                                         # [STR] 0x8400df49: 'qdma_init'
84003194: 005010ef jal	ra, 0x84004998 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x4998> # [CALL] sub_84004998
84003198: 1fb545b7 lui	a1, 0x1fb54                                                           
8400319c: 1e903637 lui	a2, 0x1e903                                                           
840031a0: 1c058593 addi	a1, a1, 0x1c0                                                         # [MMIO] QDMA_TX_BASE = 0x1fb541c0
840031a4: 7f4086b7 lui	a3, 0x7f408                                                           
840031a8: 450d c.li	a0, 0x3                                                              
840031aa: 7ff68693 addi	a3, a3, 0x7ff                                                        
840031ae: 0005aa03 lw	s4, 0x0(a1)                                                            
840031b2: 82d62423 sw	a3, -0x7d8(a2)                                                         
840031b6: 103020ef jal	ra, 0x84005ab8 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x5ab8> # [CALL] sub_84005ab8
840031ba: 1e903937 lui	s2, 0x1e903                                                           
840031be: 85aa c.mv	a1, a0                                                               
840031c0: 80a92a23 sw	a0, -0x7ec(s2)                                                         
840031c4: 00052423 sw	zero, 0x8(a0)                                                          
840031c8: 450d c.li	a0, 0x3                                                              
840031ca: 0005a223 sw	zero, 0x4(a1)                                                          
840031ce: 0005a023 sw	zero, 0x0(a1)                                                          
840031d2: 0e7020ef jal	ra, 0x84005ab8 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x5ab8> # [CALL] sub_84005ab8
840031d6: 0531 c.addi	a0, 0xc                                                            
840031d8: 1e9039b7 lui	s3, 0x1e903                                                           
840031dc: 660d c.lui	a2, 0x3                                                             
840031de: 4581 c.li	a1, 0x0                                                              
840031e0: 80a9ae23 sw	a0, -0x7e4(s3)                                                         
840031e4: 098030ef jal	ra, 0x8400627c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x627c> # [CALL] sub_8400627c
840031e8: 4401 c.li	s0, 0x0                                                              
840031ea: 4481 c.li	s1, 0x0                                                              
840031ec: 40000a93 addi	s5, zero, 0x400                                                      
840031f0: 81c9a503 lw	a0, -0x7e4(s3)                                                         
840031f4: 9522 c.add	a0, s0                                                              
840031f6: 01452023 sw	s4, 0x0(a0)                                                            
840031fa: 00052423 sw	zero, 0x8(a0)                                                          
840031fe: 004a2583 lw	a1, 0x4(s4)                                                            
84003202: 09f5b5b3 md.setbi	a1, a1, 31                                                        # [CUSTOM] md32rv
84003206: 00ba2223 sw	a1, 0x4(s4)                                                            
8400320a: 410c c.lw	a1, 0x0(a0)                                                          
8400320c: 41d0 c.lw	a2, 0x4(a1)                                                          
8400320e: 09e60633 md.clri	a2, a2, 30                                                         # [CUSTOM] md32rv
84003212: c1d0 c.sw	a2, 0x4(a1)                                                          
84003214: 450c c.lw	a1, 0x8(a0)                                                          
84003216: c144 c.sw	s1, 0x4(a0)                                                          
84003218: c981 c.beqz	a1, 0x84003228 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3228>
8400321a: 8400e537 lui	a0, 0x8400e                                                           
8400321e: 3fc50513 addi	a0, a0, 0x3fc                                                         # [STR] 0x8400e3fc: 'The TX DSCP is not return from tx used pool'
84003222: 776010ef jal	ra, 0x84004998 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x4998> # [CALL] sub_84004998
84003226: a819 c.j	0x8400323c <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x323c>
84003228: 81492583 lw	a1, -0x7ec(s2)                                                         
8400322c: 4190 c.lw	a2, 0x0(a1)                                                          
8400322e: c219 c.beqz	a2, 0x84003234 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x3234>
84003230: 41cc c.lw	a1, 0x4(a1)                                                          
84003232: 05a1 c.addi	a1, 0x8                                                            
84003234: c188 c.sw	a0, 0x0(a1)                                                          
84003236: 81492583 lw	a1, -0x7ec(s2)                                                         
8400323a: c1c8 c.sw	a0, 0x4(a1)                                                          
8400323c: 0485 c.addi	s1, 0x1                                                            
8400323e: 0431 c.addi	s0, 0xc                                                            
84003240: 020a0a13 addi	s4, s4, 0x20                                                         
84003244: fb5496e3 bne	s1, s5, 0x840031f0 <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x31f0>
84003248: 81492583 lw	a1, -0x7ec(s2)                                                         
8400324c: 4188 c.lw	a0, 0x0(a1)                                                          
8400324e: 41d0 c.lw	a2, 0x4(a1)                                                          
84003250: 00c50763 beq	a0, a2, 0x8400325e <_binary__mnt_data_vendor_npu_readable_input_npu_bins_en7523_7915_npu_rv32_bin_start+0x325e>
