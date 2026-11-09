# Execute default UART telemetry, rate limiting and runtime suppression.
mask_addr = 0x1e906844
mask = read(mask_addr)
assert mask & (1<<9), 'RX summary must be enabled by default'
original_gate = read(0x1ec0c194)

# Telemetry tests own their counters rather than relying on earlier traffic.
for reason in (0x0f, 0x15):
    addr = symbol('ppe_cpu_reasons') + reason * 4
    load(addr, (read(addr) + 1).to_bytes(4, 'little'))

def advance_log_time():
    load(symbol('timer_int_count'),
         (read(symbol('timer_int_count'))+500).to_bytes(4, 'little'))

# Clearing the summary bit suppresses the UART without disabling the datapath.
load(mask_addr, (mask & ~(1<<9)).to_bytes(4, 'little'))
load(0x1ec0c194, (0).to_bytes(4, 'little'))
before = len(uart)
advance_log_time()
tick(5000)
assert len(uart) == before
load(mask_addr, mask.to_bytes(4, 'little'))
for _ in range(150000):
    tick(1)
    report = bytes(uart[before:])
    if (b'[QDMA]' in report and b'last=' in report and
        b'host_drop=' in report and b'[PPE REASON] code=15' in report and
        report.endswith(b'\n\r')):
        break
else:
    raise AssertionError('periodic RX/QDMA UART report did not finish')
report = bytes(uart[before:])
assert report.count(b'[RX0]') == report.count(b'[RX1]') == report.count(b'[QDMA]') == 1
assert b'ppe_ret=' in report and b'wb_wait=' in report and b'full=' in report
assert b'[PPE REASON] code=0f' in report
assert b'[PPE REASON] code=15' in report
before = len(uart)
tick(5000)
assert len(uart) == before, 'summary spammed within its reporting period'
# The host UART gate has precedence over the default firmware log.
load(0x1ec0c194, (1).to_bytes(4, 'little'))
advance_log_time()
tick(5000)
assert len(uart) == before
load(0x1ec0c194, original_gate.to_bytes(4, 'little'))
print('PASS: default RX/QDMA UART summary, rate limit, runtime print mask and host UART gate')
