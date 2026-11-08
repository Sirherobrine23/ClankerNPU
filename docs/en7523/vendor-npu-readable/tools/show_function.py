#!/usr/bin/env python3
"""Inspect a recovered firmware address without opening a multi-megabyte file."""
import argparse,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'recovered'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('firmware',help='Canonical name or one of the deduplicated aliases')
    ap.add_argument('address',type=lambda x:int(x,0),help='Load address (e.g. 0x84003004)')
    ap.add_argument('--style',choices=['assembly','pseudo'],default='pseudo')
    ap.add_argument('--max-lines',type=int,default=320)
    args=ap.parse_args()
    inv=json.loads((OUT/'inventory.json').read_text())
    name=inv['aliases'].get(args.firmware,args.firmware)
    ext='annotated.S' if args.style=='assembly' else 'pseudo.c.txt'
    path=OUT/f'{name}.{ext}'
    if not path.exists():ap.error(f'Unknown firmware: {args.firmware}')
    lines=path.read_text().splitlines()
    addresses=[]
    for i,line in enumerate(lines):
        m=re.match(r'^([0-9a-f]{8}):',line) if args.style=='assembly' else re.search(r'/\* 0x([0-9a-f]{8}):',line)
        if m:addresses.append((int(m.group(1),16),i))
    choices=[(pc,index) for pc,index in addresses if pc>=args.address]
    if not choices:ap.error('Address is not within the decoded image')
    first_pc,start=choices[0]
    if first_pc!=args.address:
        print(f'# Starting at next available decoded instruction 0x{first_pc:08x}')
    print(f'# {name} {args.style} from 0x{first_pc:08x}')
    for line in lines[max(0,start-5):start+args.max_lines]:print(line)
if __name__=='__main__':main()
