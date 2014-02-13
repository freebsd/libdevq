#include <sys/types.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <devq.h>

#define	DRIDEV_DIR "/dev/dri"

int
print_drm_info(int fd)
{
	int ret;
	unsigned int vendor_id, device_id;
	char *device_path, *driver_name;
	size_t device_path_len, driver_name_len;

	ret = devq_device_get_devpath_from_fd(fd,
	    NULL, &device_path_len);
	if (ret < 0) {
		perror("devq_device_get_devpath_from_fd");
		return (-1);
	}

	device_path = malloc(device_path_len);
	ret = devq_device_get_devpath_from_fd(fd,
	    device_path, &device_path_len);
	if (ret < 0) {
		perror("devq_device_get_devpath_from_fd");
		return (-1);
	}

	printf("%.*s:\n", (int)device_path_len, device_path);
	free(device_path);
	device_path = NULL;

	ret = devq_device_drm_get_drvname_from_fd(fd, NULL, &driver_name_len);
	if (ret < 0) {
		perror("devq_device_drm_get_drvname_from_fd");
		return (-1);
	}

	driver_name = malloc(driver_name_len);
	ret = devq_device_drm_get_drvname_from_fd(fd,
	    driver_name, &driver_name_len);
	if (ret < 0) {
		perror("devq_device_drm_get_drvname_from_fd");
		return (-1);
	}

	printf("    Driver name:   %.*s\n", (int)driver_name_len, driver_name);
	free(driver_name);
	driver_name = NULL;

	ret = devq_device_get_pciid_from_fd(fd, &vendor_id, &device_id);
	if (ret < 0) {
		perror("devq_get_device_pciid_from_fd");
		return (-1);
	}

	printf("    PCI vendor ID: %04x\n", vendor_id);
	printf("    PCI device ID: %04x\n", device_id);

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
				perror(argv[i]);
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
				perror(path);
				continue;
			}

			print_drm_info(fd);
			close(fd);
		}

		closedir(dir);
	}

	return (0);
}
