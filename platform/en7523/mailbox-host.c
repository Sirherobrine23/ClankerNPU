/* EN7523 diagnostic mailbox client. Requires the diagnostic-only loader mode. */
#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static void barrier(void)
{
	__sync_synchronize();
}

static void pause_poll(void)
{
	struct timespec delay = { .tv_nsec = 1000000 };
	nanosleep(&delay, NULL);
}

static uint64_t now_ms(void)
{
	struct timespec t;

	if (clock_gettime(CLOCK_MONOTONIC, &t))
		exit(1);
	return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int main(int argc, char **argv)
{
	volatile uint32_t *mb = MAP_FAILED, *packet = MAP_FAILED, *diag = MAP_FAILED;
	uint32_t heartbeat, command, sequence, reply, target = 0;
	char *end;
	unsigned long parsed;
	int fd = -1, lock = -1, rc = 1;
	uint64_t deadline;
	unsigned int i;

	if (argc == 3 && !strcmp(argv[1], "trap")) {
		errno = 0;
		parsed = strtoul(argv[2], &end, 10);
		if (errno || !*argv[2] || *end || parsed < 1 || parsed > 3) {
			fprintf(stderr, "trap target must be hart 1, 2 or 3\n");
			return 2;
		}
		target = parsed;
		command = 2;
	} else if (argc == 2 && (!strcmp(argv[1], "ping") || !strcmp(argv[1], "status"))) {
		command = !strcmp(argv[1], "status");
	} else {
		fprintf(stderr, "usage: %s ping|status|trap HART (1..3)\n", argv[0]);
		return 2;
	}
	lock = open("/var/lock/en7523-npu-diag.lock", O_CREAT | O_RDWR, 0600);
	if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) {
		perror("diagnostic mailbox lock");
		goto out;
	}
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("/dev/mem");
		goto out;
	}
	mb = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1ec0c000);
	packet = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x840f1000);
	diag = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0x840f0000);
	if (mb == MAP_FAILED || packet == MAP_FAILED || diag == MAP_FAILED) {
		perror("mmap");
		goto out;
	}
	for (i = 0; i < 4; i++) {
		if (diag[i * 16] != 0x454e5055 || (diag[i * 16 + 1] != 3 && diag[i * 16 + 1] != 4) ||
		    diag[i * 16 + 2] != i ||
		    (diag[i * 16 + 3] != 1 && (i == 0 || diag[i * 16 + 3] != 2))) {
			fprintf(stderr, "hart %u: runtime record ABI 3/4 is not ready\n", i);
			goto out;
		}
	}
	heartbeat = diag[4];
	pause_poll();
	barrier();
	if (heartbeat == diag[4]) {
		fprintf(stderr, "hart 0 heartbeat is not advancing\n");
		goto out;
	}
	if ((mb[15] & 3) == 1) {
		fprintf(stderr, "mailbox busy; reboot after an abandoned request\n");
		goto out;
	}
	sequence = (uint32_t)now_ms();
	for (i = 0; i < 16; i++)
		packet[i] = 0;
	packet[0] = 0x4e50444d;
	packet[1] = 1;
	packet[2] = sequence;
	packet[3] = command;
	packet[4] = 0xffffffff;
	packet[12] = target;
	barrier();
	mb[12] = 0x840f1000;
	mb[13] = 64;
	mb[14] = mb[14] + 1;
	barrier();
	mb[15] = (7 << 11) | 1;
	barrier();
	deadline = now_ms() + 500;
	do {
		reply = mb[15];
		if (reply & 2)
			break;
		pause_poll();
	} while (now_ms() < deadline);
	barrier();
	if (!(reply & 2) || ((reply >> 2) & 7) != 1 || packet[4] ||
	    packet[0] != 0x4e50444d || packet[1] != 1 || packet[2] != sequence) {
		fprintf(stderr, "mailbox failed: control=%#x status=%#x\n", reply, packet[4]);
		goto out;
	}
	printf("%s ABI %u.%u, cores=%u requests=%u\n",
	       diag[1] == 4 ? "ClankerNPU core" : "diagnostic", packet[5] >> 16, packet[5] & 0xffff, packet[6], packet[11]);
	for (i = 0; i < 4; i++)
		printf("hart%u: state=%u heartbeat=%#x\n", i, packet[7 + i], diag[i * 16 + 4]);
	for (i = 0; i < 4; i++)
		printf("hart%u: C=%#x sp=%#x mcause=%#x mepc=%#x\n", i,
		       diag[i * 16 + 13], diag[i * 16 + 14],
		       diag[i * 16 + 5], diag[i * 16 + 6]);
	if (command == 2)
		printf("trap requested on hart %u; use status to confirm cause 11; reboot restores it\n", target);
	rc = 0;
out:
	if (mb != MAP_FAILED)
		munmap((void *)mb, 4096);
	if (packet != MAP_FAILED)
		munmap((void *)packet, 4096);
	if (diag != MAP_FAILED)
		munmap((void *)diag, 4096);
	if (fd >= 0)
		close(fd);
	if (lock >= 0)
		close(lock);
	return rc;
}
