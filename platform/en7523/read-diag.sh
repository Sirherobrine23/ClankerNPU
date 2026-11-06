#!/bin/sh
# Read only: the host must reserve 0x84000000..0x840fffff for the NPU.
set -eu
command -v devmem >/dev/null
for hart in 0 1 2 3; do
	base=$((0x840f0000 + hart * 64))
	echo "hart $hart"
	index=0
	for field in magic abi hart state heartbeat mcause mepc mtval mstatus mctren mpis sp requests c_result c_sp command; do
		printf '  %-10s ' "$field"
		devmem "$((base + index * 4))" 32
		index=$((index + 1))
	done
done
