/* Model of the 0x4000-spaced alias suggested by the v19 board capture.
 * Uses the real struct ndbg, counter enum and NDBG_SET macro from the repo.
 * A model demonstrates the collision; it is not a hardware capacity probe. */
#define _GNU_SOURCE
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../../npu_internal.h"
int main(void)
{
 int fd=memfd_create("en7523-local-alias-model",0);
 assert(fd>=0 && ftruncate(fd,0x4000)==0);
 for(uintptr_t addr=0x1e900000;addr<0x1e908000;addr+=0x4000)
  assert(mmap((void *)addr,0x4000,PROT_READ|PROT_WRITE,
              MAP_SHARED|MAP_FIXED_NOREPLACE,fd,0)!=MAP_FAILED);
 assert(mmap((void *)(uintptr_t)0x1e85e000,0x2000,PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED);
 /* Exact v19 symbol, obtained from its linked map. */
 volatile u32 *rx5=(volatile u32 *)(uintptr_t)0x1e902b04;
 *rx5=0;
 assert((uintptr_t)&ndbg->cnt[NC_SRAM_USED]-NDBG_BASE==0x304);
 NDBG_SET(NC_SRAM_USED,0x38060);
#ifdef LAB_EN7523
 assert(NDBG_BASE==0x1e85f000);
 assert(*rx5==0);
 assert(ndbg->cnt[NC_SRAM_USED]==0x38060);
 puts("debug alias model: relocated NDBG preserves RX flag and SRAM counter");
#else
 assert(NDBG_BASE==0x1e906800);
 assert(*rx5==0x38060);
 puts("debug alias model: v19 NDBG write reproduces rx5=0x38060");
#endif
 close(fd);
 return 0;
}
