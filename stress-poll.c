/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include "stress-ng.h"

#define MAX_PIPES	(5)
#define POLL_BUF	(4)

static inline void pipe_read(int fd, int n)
{
	char buf[POLL_BUF];
	ssize_t ret;

	ret = read(fd, buf, sizeof(buf));
	if (opt_flags & OPT_FLAGS_VERIFY) {
		if (ret < 0) {
			pr_fail(stderr, "pipe read error detected\n");
			return;
		}
		if (ret > 0) {
			ssize_t i;

			for (i = 0; i < ret; i++) {
				if (buf[i] != '0' + n) {
					pr_fail(stderr, "pipe read error, expecting different data on pipe\n");
					return;
				}
			}
		}
	}
}

/*
 *  stress_poll()
 *	stress system by rapid polling system calls
 */
int stress_poll(
	uint64_t *const counter,
	const uint32_t instance,
	const uint64_t max_ops,
	const char *name)
{
	int pipefds[MAX_PIPES][2];
	int i;
	pid_t pid;
	int ret = EXIT_SUCCESS;

	(void)instance;

	for (i = 0; i < MAX_PIPES; i++) {
		if (pipe(pipefds[i]) < 0) {
			pr_failed_dbg(name, "pipe");
			while (--i >= 0) {
				(void)close(pipefds[i][0]);
				(void)close(pipefds[i][1]);
			}
			return EXIT_FAILURE;
		}
	}

	pid = fork();
	if (pid < 0) {
		pr_failed_dbg(name, "fork");
		ret = EXIT_FAILURE;
		goto tidy;
	}
	else if (pid == 0) {
		/* Child writer */
		for (i = 0; i < MAX_PIPES; i++)
			(void)close(pipefds[i][0]);

		for (;;) {
			char buf[POLL_BUF];

			memset(buf, '0' + i, sizeof(buf));

			/* Write on a randomly chosen pipe */
			i = (mwc() >> 8) % MAX_PIPES;
			if (write(pipefds[i][1], buf, sizeof(buf)) < 0) {
				pr_failed_dbg(name, "write");
				goto abort;
			}
		}
abort:
		for (i = 0; i < MAX_PIPES; i++)
			(void)close(pipefds[i][1]);
		exit(EXIT_SUCCESS);
	} else {
		/* Parent read */

		int maxfd = 0;
		struct pollfd fds[MAX_PIPES];
		fd_set rfds;

		FD_ZERO(&rfds);
		for (i = 0; i < MAX_PIPES; i++) {
			fds[i].fd = pipefds[i][0];
			fds[i].events = POLLIN;
			fds[i].revents = 0;

			FD_SET(pipefds[i][0], &rfds);
			if (pipefds[i][0] > maxfd)
				maxfd = pipefds[i][0];
		}

		do {
			struct timeval tv;
			int ret;

			/* First, stress out poll */
			ret = poll(fds, MAX_PIPES, mwc() & 15);
			if ((opt_flags & OPT_FLAGS_VERIFY) &&
			    (ret < 0) && (errno != EINTR)) {
				pr_fail(stderr, "poll failed with error: %d (%s)\n",
				errno, strerror(errno));
			}
			if (ret > 0) {
				for (i = 0; i < MAX_PIPES; i++)
					if (fds[i].revents == POLLIN)
						pipe_read(fds[i].fd, i);
			}

			/* Second, stress out select */
			tv.tv_sec = 0;
			tv.tv_usec = mwc() & 1023;
			ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
			if ((opt_flags & OPT_FLAGS_VERIFY) &&
			    (ret < 0) && (errno != EINTR)) {
				pr_fail(stderr, "select failed with error: %d (%s)\n",
					errno, strerror(errno));
			}
			if (ret > 0) {
				for (i = 0; i < MAX_PIPES; i++) {
					if (FD_ISSET(pipefds[i][0], &rfds))
						pipe_read(pipefds[i][0], i);
					FD_SET(pipefds[i][0], &rfds);
				}
			}
			if (!opt_do_run)
				break;

			/* Third, stress zero sleep, this is like a select zero timeout */
			(void)sleep(0);

			(*counter)++;
		} while (opt_do_run && (!max_ops || *counter < max_ops));

		(void)kill(pid, SIGKILL);
	}

tidy:
	for (i = 0; i < MAX_PIPES; i++) {
		(void)close(pipefds[i][0]);
		(void)close(pipefds[i][1]);
	}

	return ret;
}
