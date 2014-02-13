#define	DEVQ_MAX_DEVS	16

int	devq_device_get_devpath_from_fd(int fd,
	    char *path, size_t *path_len);
int	devq_device_get_pciid_from_fd(int fd,
	    int *vendor_id, int *device_id);

int	devq_device_drm_get_drvname_from_fd(int fd,
	    char *driver_name, size_t *driver_name_len);
