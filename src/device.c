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
#include <sys/sysctl.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(HAVE_LIBPROCSTAT_H)
# include <sys/param.h>
# include <sys/queue.h>
# include <sys/socket.h>
# include <kvm.h>
# include <libprocstat.h>
#else
# include <sys/stat.h>
# include <dirent.h>
#endif

#if ! defined(HAVE_DEVSYSCTLS)
#include <sys/pciio.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#endif

#include "libdevq.h"

int
devq_device_get_devpath_from_fd(int fd,
    char *path, size_t *path_len)
{
#if defined(HAVE_LIBPROCSTAT_H)
	int ret;
	struct procstat *procstat;
	struct kinfo_proc *kip;
	struct filestat_list *head;
	struct filestat *fst;
	unsigned int count;
	size_t len;

	ret = 0;
	head = NULL;

	procstat = procstat_open_sysctl();
	if (procstat == NULL)
		return (-1);

	count = 0;
	kip = procstat_getprocs(procstat, KERN_PROC_PID, getpid(), &count);
	if (kip == NULL || count != 1) {
		ret = -1;
		goto out;
	}

	head = procstat_getfiles(procstat, kip, 0);
	if (head == NULL) {
		ret = -1;
		goto out;
	}

	STAILQ_FOREACH(fst, head, next) {
		if (fst->fs_uflags != 0 ||
		    fst->fs_type != PS_FST_TYPE_VNODE ||
		    fst->fs_fd != fd)
			continue;

		if (fst->fs_path == NULL) {
			errno = EBADF;
			ret = -1;
			break;
		}

		len = strlen(fst->fs_path);
		if (path) {
			if (*path_len < len) {
				*path_len = len;
				errno = ENOMEM;
				ret = -1;
				break;
			}

			memcpy(path, fst->fs_path, len);
		}
		*path_len = len;
		break;
	}

out:
	if (head != NULL)
		procstat_freefiles(procstat, head);
	if (kip != NULL)
		procstat_freeprocs(procstat, kip);
	procstat_close(procstat);

	return (ret);
#else /* !defined(HAVE_LIBPROCSTAT_H) */
	int ret, found;
	DIR *dir;
	struct stat st;
	struct dirent *dp;
	char tmp_path[256];
	size_t tmp_path_len;

	/*
	 * FIXME: This function is specific to DRM devices.
	 */
#define DEVQ_DRIDEV_DIR "/dev/dri"

	ret = fstat(fd, &st);
	if (ret != 0)
		return (-1);
	if (!S_ISCHR(st.st_mode)) {
		errno = EBADF;
		return (-1);
	}

	dir = opendir(DEVQ_DRIDEV_DIR);
	if (dir == NULL)
		return (-1);

	found = 0;
	while ((dp = readdir(dir)) != NULL) {
		struct stat tmp_st;

		if (dp->d_name[0] == '.')
			continue;

		tmp_path_len = strlen(DEVQ_DRIDEV_DIR);
		strcpy(tmp_path, DEVQ_DRIDEV_DIR);
		tmp_path[tmp_path_len++] = '/';
		tmp_path[tmp_path_len] = '\0';

		strcpy(tmp_path + tmp_path_len, dp->d_name);
		tmp_path_len += dp->d_namlen;
		tmp_path[tmp_path_len] = '\0';

		ret = stat(tmp_path, &tmp_st);
		if (ret != 0)
			continue;

		if (st.st_dev  == tmp_st.st_dev &&
		    st.st_ino  == tmp_st.st_ino) {
			found = 1;
			break;
		}
	}

	closedir(dir);

	if (!found) {
		errno = EBADF;
		return -(1);
	}

	if (path) {
		if (*path_len < tmp_path_len) {
			*path_len = tmp_path_len;
			errno = ENOMEM;
			return (-1);
		}

		memcpy(path, tmp_path, tmp_path_len);
	}
	if (path_len)
		*path_len = tmp_path_len;

	return (0);
#endif /* defined(HAVE_LIBPROCSTAT_H) */
}

int
devq_device_get_pciid_from_fd(int fd,
    int *vendor_id, int *device_id)
{
	int i, ret, dev, domain, bus, slot, function;
	char sysctl_name[32], sysctl_value[128];
	const char *busid_format;
	size_t sysctl_value_len;

#if ! defined(HAVE_DEVSYSCTLS)
	int pci;
	struct pci_conf_io pc;
	struct pci_conf conf;
	struct pci_match_conf drv_pattern;
#endif

	/*
	 * FIXME: This function is specific to DRM devices.
	 */

	/*
	 * We don't need the driver name, but this function already
	 * walks the hw.dri.* tree and returns the number in
	 * hw.dri.$number.
	 */
	dev = devq_device_drm_get_drvname_from_fd(fd, NULL, NULL);
	if (dev < 0)
		return (-1);

	/*
	 * Read the hw.dri.$n.busid sysctl to get the location of the
	 * device on the PCI bus. We can then use this location to find
	 * the corresponding dev.vgapci.$m tree.
	 */
	sprintf(sysctl_name, "hw.dri.%d.busid", dev);

	busid_format = "pci:%d:%d:%d.%d";
	sysctl_value_len = sizeof(sysctl_value);
	memset(sysctl_value, 0, sysctl_value_len);
	ret = sysctlbyname(sysctl_name, sysctl_value, &sysctl_value_len,
	    NULL, 0);

	if (ret != 0) {
		/*
		 * If hw.dri.$n.busid isn't available, fallback on
		 * hw.dri.$n.name.
		 */
		busid_format = "%*s %*s pci:%d:%d:%d.%d";
		sysctl_value_len = sizeof(sysctl_value);
		memset(sysctl_value, 0, sysctl_value_len);
		sprintf(sysctl_name, "hw.dri.%d.name", dev);
		ret = sysctlbyname(sysctl_name, sysctl_value, &sysctl_value_len,
		    NULL, 0);
	}

	if (ret != 0)
		return (-1);

#if defined(HAVE_DEVSYSCTLS)

	ret = sscanf(sysctl_value, busid_format,
	    &domain, &bus, &slot, &function);
	if (ret != 4) {
		errno = ENOENT;
		return (-1);
	}

	/*
	 * Now, look at all dev.vgapci.$m trees until we find the
	 * correct device. We specifically look at:
	 *     o  dev.vgapci.$m.%location
	 *     o  dev.vgapci.$m.%parent
	 */
	for (i = 0; i < DEVQ_MAX_DEVS; ++i) {
		sprintf(sysctl_name, "dev.vgapci.%d.%%location", i);

		sysctl_value_len = sizeof(sysctl_value);
		memset(sysctl_value, 0, sysctl_value_len);
		ret = sysctlbyname(sysctl_name, sysctl_value,
		    &sysctl_value_len, NULL, 0);
		if (ret != 0)
			continue;

		int tmp_slot, tmp_function;
		ret = sscanf(sysctl_value, "slot=%d function=%d %*s",
		    &tmp_slot, &tmp_function);
		if (ret != 2 ||
		    tmp_slot != slot || tmp_function != function)
			continue;

		sprintf(sysctl_name, "dev.vgapci.%d.%%parent", i);

		sysctl_value_len = sizeof(sysctl_value);
		memset(sysctl_value, 0, sysctl_value_len);
		ret = sysctlbyname(sysctl_name, sysctl_value,
		    &sysctl_value_len, NULL, 0);
		if (ret != 0)
			continue;

		int tmp_bus;
		ret = sscanf(sysctl_value, "pci%d",
		    &tmp_bus);
		if (ret != 1 || tmp_bus != bus)
			continue;

		break;
	}

	if (i == DEVQ_MAX_DEVS) {
		errno = ENOENT;
		return (-1);
	}

	/*
	 * Ok, we have the right tree. Let's read dev.vgapci.$m.%pnpinfo
	 * to gather the PCI ID.
	 */
	sprintf(sysctl_name, "dev.vgapci.%d.%%pnpinfo", i);

	sysctl_value_len = sizeof(sysctl_value);
	memset(sysctl_value, 0, sysctl_value_len);
	ret = sysctlbyname(sysctl_name, sysctl_value,
	    &sysctl_value_len, NULL, 0);
	if (ret != 0)
		return (-1);

	ret = sscanf(sysctl_value, "vendor=0x%04x device=0x%04x",
	    vendor_id, device_id);
	if (ret != 2) {
		errno = EINVAL;
		return (-1);
	}

#else
	memset(&pc, 0, sizeof(struct pci_conf_io));
	pc.matches = &conf;
	pc.match_buf_len = sizeof(conf);
	drv_pattern.flags = PCI_GETCONF_MATCH_DOMAIN | PCI_GETCONF_MATCH_BUS |
	                           PCI_GETCONF_MATCH_DEV | PCI_GETCONF_MATCH_FUNC;

	ret = sscanf(sysctl_value, busid_format,
	    &drv_pattern.pc_sel.pc_domain,
		&drv_pattern.pc_sel.pc_bus,
		&drv_pattern.pc_sel.pc_dev,
		&drv_pattern.pc_sel.pc_func);
	if (ret != 4) {
		errno = ENOENT;
		return (-1);
	}

	pc.patterns = &drv_pattern;
	pc.num_patterns = 1;
	pc.pat_buf_len = sizeof(struct pci_match_conf);

	pci = open("/dev/pci", O_RDONLY, 0);
	if (pci == -1) {
		return (-1);
	}

	if (ioctl(pci, PCIOCGETCONF, &pc) == -1) {
		close(pci);
		return (-1);
	}

	close(pci);

	if (pc.num_matches != 1) {
		errno = EINVAL;
		return (-1);
	}

	*vendor_id = conf.pc_vendor;
	*device_id = conf.pc_device;

#endif

	return (0);
}
