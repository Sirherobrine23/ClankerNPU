/* SPDX-License-Identifier: GPL-2.0-only
 * Current host's 34-command ABI. No automatic guess of vendor compact IDs.
 */
#include "lab.h"
#include "npu_wifi.h"
volatile u32 lab_pause_request,lab_pause_ack[4];
volatile u32 lab_started_mask;
volatile u32 lab_hwnat_enabled;
static int pause_workers(void)
{
 u32 start=csr_read(mcycle);
 /* A new generation cannot reuse acknowledgements from the previous one. */
 while(lab_pause_ack[1] || lab_pause_ack[3]) {
  if((u32)(csr_read(mcycle)-start)>EN7523_CORE_CLK_MHZ*1000000u)return LAB_REJECT_PAUSE_IDLE;
 }
 lab_pause_request=1;lab_fence();
 while(!lab_pause_ack[1] || !lab_pause_ack[3]) {
  if((u32)(csr_read(mcycle)-start)>EN7523_CORE_CLK_MHZ*1000000u){lab_pause_request=0;lab_fence();return LAB_REJECT_PAUSE_ACK;}
 }
 return 0;
}
static void resume_workers(void){lab_fence();lab_pause_request=0;}
static int reject(u32 cmd,u32 arg,u32 reason,int status)
{
 lab_diag->last_reject=reason;
 lab_diag->reject_cmd=cmd;lab_diag->reject_arg=arg;
 lab_diag->reject_rxd0=rxd_2g_init_done;lab_diag->reject_rxd1=rxd_5g_init_done;
 lab_diag->reject_pause=lab_pause_request;
 lab_diag->reject_ack1=lab_pause_ack[1];lab_diag->reject_ack3=lab_pause_ack[3];
 npu_printf("[WLAB] reject cmd=%u arg=%x reason=%u rx=%x/%x pause=%x ack=%x/%x\n",
            cmd,arg,reason,lab_diag->reject_rxd0,lab_diag->reject_rxd1,
            lab_diag->reject_pause,lab_diag->reject_ack1,lab_diag->reject_ack3);
 return status;
}
static int set(u32 *m,u32 bytes)
{
 u32 cmd=m[1],band=m[0]&15,v,reason=LAB_REJECT_ARGUMENT;
 int err;
 if(bytes<12)return reject(cmd,0,LAB_REJECT_LENGTH,NPU_MBOX_EINVAL);
 v=m[2];
 if(cmd>=WLAN_FUNC_SET_WAIT_MAX)return reject(cmd,v,LAB_REJECT_CMD,NPU_MBOX_EOPNOTSUPP);
 /* Before the first START both data workers are already gated off. Do not
  * depend on a pause acknowledgement just to publish their initial setup.
  * Once any band starts, including while configuring the other band, the
  * existing pause/ack protocol is still required. */
 if(lab_any_started() && (err=pause_workers())!=0)
  return reject(cmd,v,(u32)err,NPU_MBOX_EINVAL);
 switch(cmd) {
 case WLAN_FUNC_SET_WAIT_PCIE_ADDR:
  if(band>1 || !v || v&3)goto bad;
  npu_set_pcie_base(v,band);break;
 case WLAN_FUNC_SET_WAIT_DESC:
  if(band>1 || v!=1536 || !wifi_pkt_buf_addr ||
     (band?rxd_5g_init_done:rxd_2g_init_done))goto bad;
  wifi_npu_init(band);npu_set_rxd_init(v,band);
  if(!(band?rxd_5g_init_done:rxd_2g_init_done))goto bad;
  lab_diag->state=3;break;
 case WLAN_FUNC_SET_WAIT_NPU_INIT_DONE:
  /* Current mt7915_npu_start/stop use command 2 with a boolean value.
   * Command 13 is only the PCIe link-state notification. */
  if(band>1 || v>1)goto bad;
  if(v) {
   if(!wifi_pkt_buf_addr || !(band?rxd_5g_init_done:rxd_2g_init_done))goto bad;
   npu_set_band_enable(band);
   lab_fence();lab_started_mask|=1u<<band;lab_fence();
  } else {
   lab_started_mask&=~(1u<<band);lab_fence();
   wifi_pcie_state[band]=0;
  }
  npu_printf("[WLAB] runner band=%u enable=%u mask=%x\n",band,v,lab_started_mask);
  break;
 case WLAN_FUNC_SET_WAIT_TRAN_TO_CPU:npu_set_wait_state(band,(u8)v);break;
 case WLAN_FUNC_SET_WAIT_BA_WIN_SIZE:
  if(band>1 || ((v>>3)&255)==0 || ((v>>3)&255)>(wifi_dbdc_mode?300u:150u) || !ba_table_a || !ba_table_b)goto bad;
  npu_set_ba_entry(band,v);break;
 case WLAN_FUNC_SET_WAIT_DRIVER_MODEL:
  if(v>3)goto bad;
  npu_set_driver_model(v);break;
 case WLAN_FUNC_SET_WAIT_DEL_STA:
  if(band>1 || !v || v>(wifi_dbdc_mode?300u:150u) || !ba_table_a || !ba_table_b)goto bad;
  wifi_reset_ba_entry(band,v);break;
 case WLAN_FUNC_SET_WAIT_DRAM_BA_NODE_ADDR:
  /* The internal primary BA pool is used; no unchecked host pool size. */
  wifi_dram_ba_node_addr=0;break;
 case WLAN_FUNC_SET_WAIT_PKT_BUF_ADDR:
  if(!v){reason=LAB_REJECT_POOL_ZERO;goto bad;}
  if(v&4095){reason=LAB_REJECT_POOL_ALIGN;goto bad;}
  if(rxd_2g_init_done || rxd_5g_init_done){reason=LAB_REJECT_POOL_BUSY;goto bad;}
  npu_set_pkt_buf_addr(v);break;
 case WLAN_FUNC_SET_WAIT_IS_TEST_NOBA:npu_set_no_ba_test(v);break;
 case WLAN_FUNC_SET_WAIT_FLUSHONE_TIMEOUT:npu_set_flushone_timeout(v);break;
 case WLAN_FUNC_SET_WAIT_FLUSHALL_TIMEOUT:npu_set_flushall_timeout(v);break;
 case WLAN_FUNC_SET_WAIT_IS_FORCE_TO_CPU:npu_set_force_to_cpu(v!=0);break;
 case WLAN_FUNC_SET_WAIT_PCIE_STATE:
  if(band>1)goto bad;
  npu_set_band_enable(band);break;
 case WLAN_FUNC_SET_WAIT_HWNAT_INIT:
  /* Linux owns PPE register initialization. This WLAN command controls
   * permission for NEW wireless packets to use the QDMA/PPE path. */
  if(band>1 || v>1)goto bad;
  lab_hwnat_enabled=v;lab_fence();
  npu_printf("[WLAB] hwnat_requested=%u\n",v);break;
 case WLAN_FUNC_SET_WAIT_PCIE_PORT_TYPE:npu_set_pcie_port_type(v);break;
 case WLAN_FUNC_SET_WAIT_ERROR_RETRY_TIMES:npu_set_retry_limit(v);break;
 case WLAN_FUNC_SET_WAIT_BAR_INFO:
  if(band>1 || ((v>>3)&255)==0 || ((v>>3)&255)>(wifi_dbdc_mode?300u:150u) || !ba_table_a || !ba_table_b)goto bad;
  npu_set_bar_info(band,v);break;
 case WLAN_FUNC_SET_WAIT_FAST_FLAG:wifi_debug_flags=v&~1u;break;
 case WLAN_FUNC_SET_WAIT_NPU_BAND0_ONCPU:break;
 default:resume_workers();return reject(cmd,v,LAB_REJECT_UNSUPPORTED,NPU_MBOX_EOPNOTSUPP);
 }
 resume_workers();return NPU_MBOX_SUCCESS;
bad:resume_workers();return reject(cmd,v,reason,NPU_MBOX_EINVAL);
}
int lab_wifi_dispatch(u32 base,u32 bytes)
{
 u32 *m=(u32 *)base,type,cmd,band;
 if(bytes<8 || (base&3))return NPU_MBOX_EINVAL;
 type=(m[0]>>4)&15;cmd=m[1];band=m[0]&15;
 lab_diag->mail_count++;lab_diag->last_cmd=(type<<16)|(band<<8)|cmd;
 lab_diag->last_arg=bytes>=12?m[2]:0;lab_diag->last_base=base;
 if(type==1)return set(m,bytes);
 if(type!=3)return NPU_MBOX_EOPNOTSUPP;
 if(bytes<12)return NPU_MBOX_EINVAL;
 switch(cmd) {
 case WLAN_FUNC_GET_WAIT_NPU_VERSION:m[2]=NPU_VERSION_CODE(NPU_VERSION_MAJOR,NPU_VERSION_MINOR,NPU_VERSION_PATCH);break;
 case WLAN_FUNC_GET_WAIT_RXDESC_BASE:if(band>1)return 2;m[2]=(band?rxd_base_5g:rxd_base_2g)&0x1fffffff;break;
 case WLAN_FUNC_GET_WAIT_DBG_COUNTER:if(band>1)return 2;m[2]=(band?counter_base_5g:counter_base_2g)&0x1fffffff;break;
 case WLAN_FUNC_GET_WAIT_WCID_DBG_COUNTER:if(band>1)return 2;m[2]=(band?wcid_counter_base_5g:wcid_counter_base_2g)&0x1fffffff;break;
 case WLAN_FUNC_GET_WAIT_NPU_INFO:m[2]=1111;break;
 case WLAN_FUNC_GET_WAIT_LAST_RATE:if(bytes<16)return 2;m[2]=0;m[3]=0;break;
 case WLAN_FUNC_GET_WAIT_NPU_SUPPORT_MAP:m[2]=3;break;
 case WLAN_FUNC_GET_WAIT_MDC_LOCK_ADDRESS:m[2]=0;break;
 default:return NPU_MBOX_EOPNOTSUPP;
 }
 return NPU_MBOX_SUCCESS;
}
