#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <kvm.h>
#include <libprocstat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "devq.h"

int
devq_device_get_devpath_from_fd(int fd,
    char *path, size_t *path_len)
{
	int ret;
	struct procstat *procstat;
	struct kinfo_proc *kip;
	struct filestat_list *head;
	struct filestat *fst;
	unsigned int count;
	size_t len;

	ret = 0;

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
}

int
devq_device_get_pciid_from_fd(int fd,
    unsigned int *vendor_id, unsigned int *device_id)
{
	int i, ret, dev, domain, bus, slot, function;
	char sysctl_name[32], sysctl_value[128];
	size_t sysctl_value_len;

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

	sysctl_value_len = sizeof(sysctl_value);
	memset(sysctl_value, 0, sysctl_value_len);
	ret = sysctlbyname(sysctl_name, sysctl_value, &sysctl_value_len,
	    NULL, 0);
	if (ret != 0)
		return (-1);

	ret = sscanf(sysctl_value, "pci:%d:%d:%d.%d",
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

	return (0);
}
