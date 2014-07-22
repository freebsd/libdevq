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
#include <ctype.h>

#include "libdevq.h"

static struct hw_type {
	const char *driver;
	devq_device_t type;
	devq_class_t class;
} hw_types[] = {
	{ "ukbd",  DEVQ_DEVICE_KEYBOARD,     DEVQ_CLASS_INPUT   },
	{ "atkbd", DEVQ_DEVICE_KEYBOARD,     DEVQ_CLASS_INPUT   },
	{ "ums",   DEVQ_DEVICE_MOUSE,        DEVQ_CLASS_INPUT   },
	{ "psm",   DEVQ_DEVICE_MOUSE,        DEVQ_CLASS_INPUT   },
	{ "uhid",  DEVQ_DEVICE_MOUSE,        DEVQ_CLASS_INPUT   },
	{ "joy",   DEVQ_DEVICE_JOYSTICK,     DEVQ_CLASS_INPUT   },
	{ "atp",   DEVQ_DEVICE_TOUCHPAD,     DEVQ_CLASS_INPUT   },
	{ "uep",   DEVQ_DEVICE_TOUCHSCREEN,  DEVQ_CLASS_INPUT   },
	{ NULL,	   DEVQ_DEVICE_UNKNOWN,      DEVQ_CLASS_UNKNOWN },
};

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

struct devq_device {
	devq_device_t type;
	devq_class_t class;
	char *path;
	char *driver;
	char *vendor;
	char *product;
	const char *vstr;
	const char *pstr;
};

struct devq_event {
	int type;
	struct devq_device *device;
	char *raw;
};

static ssize_t
socket_getline(struct devq_evmon *evmon)
{
       ssize_t ret, sz = 0;
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

	if (e == NULL)
		return (DEVQ_UNKNOWN);

	return (e->type);
}

static void
usb_vendor_product(struct devq_device *d)
{
	const char *usbids = PREFIX "/share/usbids/usb.ids";
	FILE *f;
	char *line = NULL;
	const char *walk;
	size_t linecap = 0;
	ssize_t linelen;

	if ((f = fopen(usbids, "r")) == NULL)
		return;

	while ((linelen = getline(&line, &linecap, f)) > 0) {
		if (d->vendor == NULL) {
			if (strncmp(line, d->vstr + 2, 4) == 0) {
				walk = line + 4;

				while (isspace(*walk))
					walk++;

				if (line[linelen -1] == '\n')
					line[linelen - 1 ] = '\0';

				d->vendor = strndup(walk, strlen(walk));
			}
		} else {
			walk = line;
			while (isspace(*walk))
				walk++;

			if (strncmp(walk, d->pstr + 2, 4) == 0) {
				walk += 4;

				while (isspace(*walk))
					walk++;

				if (line[linelen -1] == '\n')
					line[linelen - 1 ] = '\0';

				d->product = strndup(walk, strlen(walk));
			}
		}
		
	}
	fclose(f);
	free(line);

}

static void
device_vendor_product(struct devq_event *e)
{

	e->device->vstr = strstr(e->raw, "vendor=");
	if (e->device->vstr == NULL)
		return;

	e->device->vstr += 7;
	e->device->pstr = strstr(e->raw, "product=");
	e->device->pstr += 8;

	if (*e->device->driver == 'u')
		usb_vendor_product(e->device);
}

struct devq_device *
devq_event_get_device(struct devq_event *e)
{
	const char *line, *walk;
	int i;

	if (e == NULL)
		return (NULL);

	if (e->type != DEVQ_ATTACHED && e->type != DEVQ_DETACHED)
		return (NULL);

	if (e->device != NULL)
		return (e->device);

	e->device = calloc(1, sizeof(struct devq_device));
	if (e->device == NULL)
		return (NULL);

	e->device->type = DEVQ_DEVICE_UNKNOWN;
	e->device->class = DEVQ_CLASS_UNKNOWN;

	line = e->raw + 1;
	walk = line;
	while (!isspace(*walk))
		walk++;

	asprintf(&e->device->path, "/dev/%.*s", (int)(walk - line), line);

	for (i = 0; hw_types[i].driver != NULL; i++) {
		if (strncmp(line, hw_types[i].driver,
		            strlen(hw_types[i].driver)) == 0 &&
		    isdigit(*(line + strlen(hw_types[i].driver)))) {
			e->device->type = hw_types[i].type;
			e->device->class = hw_types[i].class;
			e->device->driver = strdup(hw_types[i].driver);
			break;
		}
	}
	printf("%s\n", e->device->path);

	if (e->device->driver == NULL) {
		walk--;
		while (isdigit(*walk))
			walk--;
		e->device->driver = strndup(line, walk - line);
	}

	device_vendor_product(e);

	return (e->device);
}

const char *
devq_event_dump(struct devq_event *e)
{
	return (e->raw);
}

void
devq_event_free(struct devq_event *e)
{
	if (e->device != NULL) {
		free(e->device->path);
		free(e->device->driver);
		free(e->device);
	}

	free(e->raw);
	free(e);
}

devq_device_t
devq_device_get_type(struct devq_device *d)
{

	if (d == NULL)
		return (DEVQ_DEVICE_UNKNOWN);

	return (d->type);
}

devq_class_t
devq_device_get_class(struct devq_device *d)
{

	if (d == NULL)
		return (DEVQ_CLASS_UNKNOWN);

	return (d->class);
}

const char *
devq_device_get_path(struct devq_device *d)
{

	if (d == NULL)
		return (NULL);

	return (d->path);
}

const char *
devq_device_get_product(struct devq_device *d)
{

	if (d == NULL)
		return (NULL);

	return (d->product);
}

const char *
devq_device_get_vendor(struct devq_device *d)
{

	if (d == NULL)
		return (NULL);

	return (d->vendor);
}
