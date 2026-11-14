#!/usr/bin/env python3
"""Read UART [PROF] records from Wireless Lab v23. Python standard library only."""
import argparse,re,sys
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',help='UART capture, or - for stdin')
p.add_argument('--mhz',type=float,default=333,help='NPU mcycle frequency (default: 333 MHz)')
a=p.parse_args()
if a.mhz<=0:p.error('--mhz must be positive')
text=sys.stdin.read() if a.log=='-' else open(a.log,encoding='utf-8',errors='replace').read()
records={}
for line in text.splitlines():
 if '[PROF]' not in line:continue
 fields=dict(re.findall(r'(\w+)=([^\s]+)',line.split('[PROF]',1)[1]))
 if 'id' not in fields:continue
 rec=records.setdefault(fields['id'],{'stages':{}})
 if 'stage' in fields:rec['stages'][fields['stage']]=fields
 elif 'dt' in fields:rec['summary']=fields
 elif 'qpeak' in fields:rec['pressure']=fields
if not records:sys.exit('No [PROF] records found. Capture the NPU UART with LAB_PROFILE=1.')
for ident,rec in records.items():
 s=rec.get('summary')
 if not s or any(k not in s or not s[k].isdigit() for k in ['dt','rx','submit','free','punt','ids']):
  print(f'window {ident}: summary missing (incomplete UART capture)');continue
 dt=int(s['dt']);seconds=dt/(a.mhz*1e6)
 print(f"window {ident}: ~{seconds:.3f}s, RX={s['rx']}, submit={s['submit']}, free={s['free']}, punt={s['punt']}, IDs={s['ids']}")
 print('stage       calls   samples   avg cycles   busy cyc/work   estimated stage %*')
 for name in ['qdone','ppe','rx','class','submit','alloc']:
  f=rec['stages'].get(name)
  if not f or any(k not in f or not f[k].isdigit() for k in ['calls','n','cyc','work','bcyc']):
   print(f'{name:8} missing/incomplete');continue
  calls,n,cycles,work,bcyc=[int(f[k]) for k in ['calls','n','cyc','work','bcyc']]
  avg=cycles/n if n else None
  cost=bcyc/work if work else None
  estimate=avg*calls/dt*100 if avg is not None and dt else None
  def fmt(v):return 'n/a' if v is None else f'{v:.1f}'
  print(f'{name:8} {calls:9} {n:9} {fmt(avg):>12} {fmt(cost):>15} {fmt(estimate):>20}')
 if 'pressure' in rec:
  print('pressure:', ' '.join(k+'='+v for k,v in rec['pressure'].items() if k!='id'))
 print('* Sampling estimate, not measured hart utilization. RX includes class/submit/alloc; do not add nested rows.')
 print('  qpeak/irqpeak/fifopeak/peek and max are boot-lifetime peaks. Other printed counters are window deltas.')
