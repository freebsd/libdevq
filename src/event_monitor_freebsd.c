/*
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/event.h>

#include <stdlib.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libdevq.h"

#define DEVD_SOCK_PATH "/var/run/devd.pipe"

#define DEVD_EVENT_ATTACH	'+'
#define DEVD_EVENT_DETTACH	'-'
#define DEVD_EVENT_NOTICE	'!'
#define DEVD_EVENT_UNKNOWN	'?'

struct devq_evmon {
	int fd;
	int kq;
	struct kevent ev;
	char *buf;
	size_t len;
};

struct devq_event {
	int type;
	char *raw;
};

static ssize_t
socket_getline(struct devq_evmon *evmon)
{
       ssize_t ret, cap, sz = 0;
       char c;

       for (;;) {
               ret = read(evmon->fd, &c, 1);
               if (ret < 1) {
                       return -1;
               }

               if (c == '\n')
                       break;

               if (sz + 1 >= evmon->len) {
                       evmon->len += 1024;
                       evmon->buf = reallocf(evmon->buf, evmon->len *sizeof(char));
               }
               evmon->buf[sz] = c;
               sz++;
       }

       evmon->buf[sz] = '\0';

       return (sz); /* number of bytes in the line, not counting the line break*/
}

struct devq_evmon *
devq_event_monitor_init(void)
{
	struct devq_evmon	*evm;
	struct sockaddr_un	 devd;
	struct kevent		ev;
       
	if ((evm = calloc(1, sizeof (struct devq_evmon))) == NULL)
		return (NULL);

	evm->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (evm->fd < 0) {
		free(evm);
		return (NULL);
	}

	devd.sun_family = AF_UNIX;
	strlcpy(devd.sun_path, DEVD_SOCK_PATH, sizeof(devd.sun_path));

	if (connect(evm->fd, (struct sockaddr *) &devd, sizeof(struct sockaddr_un)) < 0) {
		close(evm->fd);
		free(evm);
		return (NULL);
	}

	evm->kq = kqueue();
	if (evm->kq == -1) {
		close(evm->fd);
		free(evm);
	}

	EV_SET(&ev, evm->fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
	kevent(evm->kq, &ev, 1, NULL, 0, NULL);

	return (evm);
}

void
devq_event_monitor_fini(struct devq_evmon *evm)
{
	if (evm == NULL)
		return;

	close(evm->fd);
	free(evm->buf);
	free(evm);
}

int
devq_event_monitor_get_fd(struct devq_evmon *evm)
{
	if (evm == NULL)
		return (-1);

	return (evm->kq);
}

int
devq_event_monitor_poll(struct devq_evmon *evm)
{
	if (evm == NULL)
		return (0);

	if (kevent(evm->kq, NULL, 0, &evm->ev, 1, NULL) < 0)
		return (0);

	return (1);
}

struct devq_event *
devq_event_monitor_read(struct devq_evmon *evm)
{
	struct devq_event *e;
	size_t r=0;
	char *to;

	if (socket_getline(evm) < 0)
		return (NULL);

	/* XXX here may apply filters */
	e = calloc(1, sizeof(struct devq_event));
	if (e == NULL)
		return (NULL);

	e->raw = strdup(evm->buf);

	switch (*e->raw) {
	case DEVD_EVENT_ATTACH:
		e->type = DEVQ_ATTACHED;
		break;
	case DEVD_EVENT_DETTACH:
		e->type = DEVQ_DETACHED;
		break;
	case DEVD_EVENT_NOTICE:
		e->type = DEVQ_NOTICE;
		break;
	default:
		e->type = DEVQ_UNKNOWN;
		break;
	}

	return (e);
}

devq_event_t
devq_event_get_type(struct devq_event *e)
{
	return (e->type);
}

const char *
devq_event_dump(struct devq_event *e)
{
	return (e->raw);
}

void
devq_event_free(struct devq_event *e)
{
	free(e->raw);
	free(e);
}
