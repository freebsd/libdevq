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

#include <sys/types.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libdevq.h>

#define	DRIDEV_DIR "/dev/dri"

int
print_drm_info(int fd)
{
	int ret;
	int vendor_id, device_id, subvendor_id, subdevice_id, revision_id;
	char *device_path, *driver_name;
	size_t device_path_len, driver_name_len;

	ret = devq_device_get_devpath_from_fd(fd,
	    NULL, &device_path_len);
	if (ret < 0) {
		fprintf(stderr, "Warning: unable to device path\n");
		return (-1);
	}

	device_path = malloc(device_path_len);
	ret = devq_device_get_devpath_from_fd(fd,
	    device_path, &device_path_len);
	if (ret < 0) {
		fprintf(stderr, "Warning: Unable to device path\n");
		return (-1);
	}

	printf("%.*s:\n", (int)device_path_len, device_path);
	free(device_path);
	device_path = NULL;

	ret = devq_device_drm_get_drvname_from_fd(fd, NULL, &driver_name_len);
	if (ret < 0) {
		printf("    Driver name:   Unknown\n");
		return (-1);
	}

	driver_name = malloc(driver_name_len);
	ret = devq_device_drm_get_drvname_from_fd(fd,
	    driver_name, &driver_name_len);
	if (ret < 0) {
		fprintf(stderr, "Warning: Unable to determine driver name\n");
		return (-1);
	}

	printf("    Driver name:   %.*s\n", (int)driver_name_len, driver_name);
	free(driver_name);
	driver_name = NULL;

	ret = devq_device_get_pciid_full_from_fd(fd, &vendor_id, &device_id,
				&subvendor_id, &subdevice_id, &revision_id);
	if (ret < 0) {
		fprintf(stderr, "Warning: Unable to determine vendor and device ID\n");
		return (-1);
	}

	printf("    PCI vendor ID: 0x%04x subvendor ID: 0x%04x\n", vendor_id, subvendor_id);
	printf("    PCI device ID: 0x%04x subdevice ID: 0x%04x\n", device_id, subdevice_id);
	printf("    PCI revision ID: 0x%04x\n", revision_id);

	return (0);
}

int
main(int argc, char *argv[])
{
	int fd;

	if (argc >= 2) {
		for (int i = 1; i < argc; ++i) {
			fd = open(argv[i], O_RDWR);
			if (fd < 0) {
				fprintf(stderr, "%s\n", argv[i]);
				continue;
			}

			print_drm_info(fd);
			close(fd);
		}
	} else {
		DIR *dir;
		struct dirent *dp;
		char path[256];

		dir = opendir(DRIDEV_DIR);
		if (dir == NULL)
			return (-1);

		while ((dp = readdir(dir)) != NULL) {
			if (dp->d_name[0] == '.')
				continue;

			path[0] = '\0';
			strcpy(path, DRIDEV_DIR);
			strcat(path, "/");
			strcat(path, dp->d_name);

			fd = open(path, O_RDWR);
			if (fd < 0) {
				fprintf(stderr, "%s\n", path);
				continue;
			}

			print_drm_info(fd);
			close(fd);
		}

		closedir(dir);
	}

	return (0);
}
