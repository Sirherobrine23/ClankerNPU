/*
 * AN75XX NPU firmware - memory and string helpers
 *
 * The firmware links with -nostdlib, so it carries its own copies.
 */

#include "npu_internal.h"


/* ================================================================
 * Utility functions
 * ================================================================ */

void *npu_memset(void *dst, int c, u32 n)
{
	u8 *d = (u8 *)dst;

	while (n--)
		*d++ = (u8)c;
	return dst;
}

void *npu_memcpy(void *dst, const void *src, u32 n)
{
	u8 *d = (u8 *)dst;
	const u8 *s = (const u8 *)src;

#ifdef EN7523
	/* Packet buffers are word-aligned; avoid byte transactions to uncached DRAM. */
	if (!(((u32)d | (u32)s) & 3)) {
		u32 *dw = (u32 *)d;
		const u32 *sw = (const u32 *)s;

		while (n >= 16) {
			u32 v0 = sw[0], v1 = sw[1], v2 = sw[2], v3 = sw[3];

			dw[0] = v0;
			dw[1] = v1;
			dw[2] = v2;
			dw[3] = v3;
			sw += 4;
			dw += 4;
			n -= 16;
		}
		while (n >= 4) {
			*dw++ = *sw++;
			n -= 4;
		}
		d = (u8 *)dw;
		s = (const u8 *)sw;
	}
#endif
	while (n--)
		*d++ = *s++;
	return dst;
}

u32 npu_strlen(const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	return (u32)(p - s);
}

/* floor(sqrt(x)), bit by bit: no multiply or divide */
u32 npu_isqrt(u32 x)
{
	u32 r = 0, b = 1u << 30;

	while (b > x)
		b >>= 2;
	while (b != 0) {
		if (x >= r + b) {
			x -= r + b;
			r = (r >> 1) + b;
		} else {
			r >>= 1;
		}
		b >>= 2;
	}
	return r;
}

char get_core_char(void)
{
	u32 id = get_hartid();

	if (id <= 7)
		return (char)('0' + id);
	return 'X';
}

static u16 npu_htons(u16 x)
{
	return (u16)((x >> 8) | (x << 8));
}

static void wfi_idle(void)
{
	__asm__ volatile("wfi");
}
