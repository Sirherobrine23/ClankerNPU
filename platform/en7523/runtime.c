/* EN7523 freestanding C entry and bounded stack smoke test. */
typedef unsigned int u32;

extern char __stack_base[];
/* Verify that the companion data image was actually loaded by the host. */
static volatile u32 runtime_seed = 0x13579bdf;

static __attribute__((noinline)) u32 stack_frame_test(u32 hart, u32 seed)
{
	volatile u32 frame[32];
	u32 result = 0;
	unsigned int i;

	for (i = 0; i < 32; i++)
		frame[i] = seed ^ (hart << 8) ^ i;
	for (i = 0; i < 32; i++)
		result += frame[i];
	return result;
}

u32 en7523_runtime_init(u32 hart, volatile u32 *record)
{
	volatile u32 *guard = (volatile u32 *)(__stack_base + (hart << 12));
	u32 sp, result, expected = 0;
	unsigned int i;

	asm volatile("mv %0, sp" : "=r" (sp));
	record[14] = sp;
	if (hart >= 4 || (sp & 15) || runtime_seed != 0x13579bdf)
		return 1;
	if (sp <= (u32)guard + 256 || sp >= (u32)guard + 4096)
		return 2;
	*guard = 0x5354414b ^ hart;
	result = stack_frame_test(hart, runtime_seed);
	for (i = 0; i < 32; i++)
		expected += 0x13579bdf ^ (hart << 8) ^ i;
	if (result != expected || *guard != (0x5354414b ^ hart))
		return 3;
	record[13] = 0x43525400 | hart;
	return 0;
}
