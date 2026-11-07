# Executed by en7523-core.py before boot, using the linked RV32 image.

copy_read, copy_write = read, write
for src_offset in range(4):
    for dst_offset in range(4):
        for length in (*range(34), 63, 64, 65, 127, 3500, 4096):
            src = 0x87000020 + src_offset
            dst = 0x87010020 + dst_offset
            blob = bytes((i * 13 + 7) & 255 for i in range(length))
            load(0x87000000, b'\xa5' * (length + 80))
            load(0x87010000, b'\x5a' * (length + 80))
            load(src, blob)

            def read(addr, n=4):
                if 0x87000000 <= addr < 0x87002000:
                    assert src <= addr and addr + n <= src + length, 'copy overread'
                    assert not addr % n, 'unaligned copy load'
                return copy_read(addr, n)

            def write(addr, val, n=4):
                if 0x87010000 <= addr < 0x87012000:
                    assert dst <= addr and addr + n <= dst + length, 'copy overwrite'
                    assert not addr % n, 'unaligned copy store'
                copy_write(addr, val, n)

            h = Hart(0)
            h.pc = symbol('npu_memcpy')
            h.r[1] = 0xdead0000
            h.r[2] = 0x840d4000
            h.r[10:13] = [dst, src, length]
            for _ in range(100000):
                h.step()
                if h.pc == 0xdead0000:
                    break
            else:
                raise AssertionError('copy did not return')
            read, write = copy_read, copy_write
            assert h.r[10] == dst, 'copy return value'
            assert bytes(read(dst + i, 1) for i in range(length)) == blob
            assert bytes(read(src + i, 1) for i in range(length)) == blob
            assert all(read(dst - i, 1) == 0x5a for i in range(1, 17))
            assert all(read(dst + length + i, 1) == 0x5a for i in range(16))
print('PASS: RV32 memcpy, all pointer alignments, zero length, word/block tails and packet-size guards')
