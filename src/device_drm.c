#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libdevq.h"

int
devq_device_drm_get_drvname_from_fd(int fd,
    char *driver_name, size_t *driver_name_len)
{
	int ret, i;
	struct stat st;
	char sysctl_name[32], sysctl_value[128];
	size_t sysctl_value_len, name_len;
	long dev;

	ret = fstat(fd, &st);
	if (ret != 0)
		return (-1);
	if (!S_ISCHR(st.st_mode)) {
		errno = EBADF;
		return (-1);
	}

	/*
	 * Walk all the hw.dri.$n tree and compare the number stored at
	 * the end of hw.dri.$n.name (eg. "radeon 0x9b") to the value in
	 * stat.st_rdev.
	 */
	for (i = 0; i < DEVQ_MAX_DEVS; i++) {
		sprintf(sysctl_name, "hw.dri.%d.name", i);

		sysctl_value_len = sizeof(sysctl_value);
		ret = sysctlbyname(sysctl_name, sysctl_value,
		    &sysctl_value_len, NULL, 0);
		if (ret != 0)
			continue;

		for (name_len = 0;
		    name_len < sysctl_value_len &&
		    sysctl_value[name_len] != ' ';
		    ++name_len)
			;
		if (driver_name != NULL) {
			if (*driver_name_len < name_len) {
				*driver_name_len = name_len;
				errno = ENOMEM;
				return (-1);
			}

			memcpy(driver_name, sysctl_value, name_len);
		}
		if (driver_name_len)
			*driver_name_len = name_len;

		/*
		 * Now that we found the correct entry, return its
		 * number; this could be useful to others.
		 */
		dev = strtol(sysctl_value + name_len, NULL, 16);
		if (dev == (long)st.st_rdev)
			return (i);
	}

	errno = ENOENT;
	return (-1);
}
