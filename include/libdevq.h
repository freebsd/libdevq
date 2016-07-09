/*
 * Copyright (c) 2014 Jean-Sebastien Pedron <dumbbell@FreeBSD.org>
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

#ifndef _LIBDEVQ_H_
#define _LIBDEVQ_H_

#define	DEVQ_MAX_DEVS	16

typedef enum {
	DEVQ_ATTACHED = 1U,
	DEVQ_DETACHED,
	DEVQ_NOTICE,
	DEVQ_UNKNOWN
} devq_event_t;

typedef enum {
	DEVQ_DEVICE_UNKNOWN = 1U,
	DEVQ_DEVICE_KEYBOARD,
	DEVQ_DEVICE_MOUSE,
	DEVQ_DEVICE_JOYSTICK,
	DEVQ_DEVICE_TOUCHPAD,
	DEVQ_DEVICE_TOUCHSCREEN
} devq_device_t;

typedef enum {
	DEVQ_CLASS_UNKNOWN = 1U,
	DEVQ_CLASS_INPUT
} devq_class_t;

struct devq_evmon;
struct devq_event;
struct devq_device;

int		devq_device_get_devpath_from_fd(int fd,
		    char *path, size_t *path_len);
int		devq_device_get_pciid_from_fd(int fd,
		    int *vendor_id, int *device_id);
int		devq_device_get_pciid_full_from_fd(int fd,
		    int *vendor_id, int *device_id,
		    int *subversion_id, int *subdevice_id,
		    int *revision_id);

int		devq_device_get_pcibusaddr(int fd,
		    int *domain, int *bus,
		    int *slot, int *function);

int		devq_device_drm_get_drvname_from_fd(int fd,
		    char *driver_name, size_t *driver_name_len);
devq_device_t	devq_device_get_type(struct devq_device *);
devq_class_t	devq_device_get_class(struct devq_device *);
const char *	devq_device_get_path(struct devq_device *);
const char *	devq_device_get_product(struct devq_device *);
const char *	devq_device_get_vendor(struct devq_device *);

struct devq_evmon *	devq_event_monitor_init(void);
void			devq_event_monitor_fini(struct devq_evmon *);
int			devq_event_monitor_get_fd(struct devq_evmon *);
int			devq_event_monitor_poll(struct devq_evmon *);
struct devq_event *	devq_event_monitor_read(struct devq_evmon *);
struct devq_device *	devq_event_get_device(struct devq_event *);
devq_event_t		devq_event_get_type(struct devq_event *);
const char *		devq_event_dump(struct devq_event *);
void			devq_event_free(struct devq_event *);

#endif /* _LIBDEVQ_H_ */
