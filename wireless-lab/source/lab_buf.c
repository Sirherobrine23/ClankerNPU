/* SPDX-License-Identifier: GPL-2.0-only */
#include "lab.h"
#include "npu_wifi.h"
static u16 *free_ring;
static u8 *allocated;
static u32 rd, wr, count;
static u32 id_lock[2] = {3, 0};
void lab_buf_init(void)
{
 free_ring = (u16 *)sram_buf_alloc(138);
 allocated = (u8 *)sram_buf_alloc(139);
 if (!free_ring || !allocated) { lab_fault(2,0,0,0); return; }
 for (u32 i=0;i<LAB_IDS;i++) { free_ring[i]=i; allocated[i]=0; }
 rd=wr=0; count=LAB_IDS; lab_diag->free_ids=count;
}
s32 lab_buf_alloc(void)
{
 u32 irq=irq_save(); s32 id=-1;
 hw_mutex_lock(id_lock);
 if (count) {
  id=free_ring[rd]; rd=rd+1==LAB_IDS?0:rd+1; count--;
  if ((u32)id>=LAB_IDS || allocated[id]) { lab_diag->bad_id++; lab_fault(3,0,0,id); id=-1; }
  else allocated[id]=1;
 }
 lab_diag->free_ids=count;
 hw_mutex_unlock(id_lock); irq_restore(irq); return id;
}
void lab_buf_free(u32 id)
{
 u32 irq=irq_save(); hw_mutex_lock(id_lock);
 if (id>=LAB_IDS || !allocated[id] || count==LAB_IDS) lab_diag->bad_id++;
 else {
  allocated[id]=0; free_ring[wr]=id; wr=wr+1==LAB_IDS?0:wr+1; count++;
 }
 lab_diag->free_ids=count;
 hw_mutex_unlock(id_lock); irq_restore(irq);
}
void buf_mgr_init(void) { lab_buf_init(); }
s32 buf_mgr_alloc(void) { return lab_buf_alloc(); }
void buf_mgr_free(u32 id) { lab_buf_free(id); }
