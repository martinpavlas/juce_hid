#include <usb.h>
#include <wchar.h>
#include "hidapi.h"

//static usb_dev_handle *libusb_teensy_handle = NULL;
static int usb_hid_initialized = 0;

struct hid_device_ {
  usb_dev_handle *device_handle;
	int blocking;
	int uses_numbered_reports;
	int disconnected;
  bool first_time;
};


static hid_device hid_dev;

int HID_API_EXPORT hid_init (void)
{
	if (!usb_hid_initialized)
	{
    /*
     * initialize HID device
     */
    hid_dev.device_handle = NULL;
    hid_dev.blocking = 1;
    hid_dev.uses_numbered_reports = 0;
    hid_dev.disconnected = 0;
    hid_dev.first_time = true;

		usb_init();
    //usb_set_debug (2);
		usb_hid_initialized = 1;

		return (1);
	}

	/* Already initialized. */
	return (0);
}

int HID_API_EXPORT hid_exit (void)
{
	if (usb_hid_initialized)
	{
    hid_dev.device_handle = NULL;
    hid_dev.disconnected = 1;
    usb_hid_initialized = 0;

		return (1);
	}

	return (0);
}

struct hid_device_info  HID_API_EXPORT *hid_enumerate (unsigned short vendor_id, unsigned short product_id)
{
  struct usb_bus *bus;
  struct usb_device *dev;
  usb_dev_handle *h;

  struct hid_device_info *root = NULL; /* return object */
  struct hid_device_info *cur_dev = NULL;
  int num_devices;
  int i;

  hid_init ();
  usb_find_busses();
  usb_find_devices();

  //printf ("\nSearching for USB device:\n");
	for (bus = usb_get_busses(); bus; bus = bus->next)
  {
		for (dev = bus->devices; dev; dev = dev->next)
    {
      /*
			printf ("bus \"%s\", device \"%s\" vid=%04X, pid=%04X\n",
				bus->dirname, dev->filename,
				dev->descriptor.idVendor,
				dev->descriptor.idProduct
			);
      */

      unsigned short dev_vid = dev->descriptor.idVendor ;
      unsigned short dev_pid = dev->descriptor.idProduct;

      /* Check the VID/PID against the arguments */
      if ((vendor_id == 0x0 || vendor_id == dev_vid) && (product_id == 0x0 || product_id == dev_pid))
      {
        struct hid_device_info *tmp;

        /* VID/PID match. Create the record. */
        tmp = (struct hid_device_info *) malloc (sizeof (struct hid_device_info));

        if (cur_dev)
        {
          cur_dev->next = tmp;
        }
        else
        {
          root = tmp;
        }
        cur_dev = tmp;

        /* Get the Usage Page and Usage for this device. */
        cur_dev->usage_page = 0x1111;
        cur_dev->usage = 0x9999;

        /* Fill out the record */
        cur_dev->next = NULL;

        /* Fill in the path (IOService plane) */
        cur_dev->path = strdup (dev->filename);

        /* Serial Number */

        wchar_t buf[100];
        mbstowcs (buf, "serial", 6);
        cur_dev->serial_number = wcsdup(buf);
        mbstowcs (buf, "Electra One", 11);
        cur_dev->manufacturer_string = wcsdup (buf);
        mbstowcs (buf, "Controller", 10);
        cur_dev->product_string = wcsdup (buf);

        /* VID/PID */
        cur_dev->vendor_id = dev_vid;
        cur_dev->product_id = dev_pid;

        /* Release Number */
        cur_dev->release_number = 213;

        /* Interface Number (Unsupported on Mac)*/
        cur_dev->interface_number = -1;
      }
    }
  }

  //printf ("ROOT: %x", root->vendor_id);
  return root;
}

void  HID_API_EXPORT hid_free_enumeration (struct hid_device_info *devs)
{
	/* This function is identical to the Linux version. Platform independent. */
	struct hid_device_info *d = devs;

	while (d)
  {
		struct hid_device_info *next = d->next;
		free(d->path);
		free(d->serial_number);
		free(d->manufacturer_string);
		free(d->product_string);
		free(d);
		d = next;
	}
}

usb_dev_handle * open_usb_device (int vid, int pid)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle *h;
	char buf[128];
	int r;

	printf ("\nSearching for USB device:\n");
	for (bus = usb_get_busses(); bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			printf ("bus \"%s\", device \"%s\" vid=%04X, pid=%04X\n",
				bus->dirname, dev->filename,
				dev->descriptor.idVendor,
				dev->descriptor.idProduct
			);

			if (dev->descriptor.idVendor != vid) continue;
			if (dev->descriptor.idProduct != pid) continue;
			h = usb_open(dev);
			if (!h) {
				printf("Found device but unable to open\n");
				continue;
			}
			#ifdef LIBUSB_HAS_GET_DRIVER_NP
			r = usb_get_driver_np(h, 0, buf, sizeof(buf));
			if (r >= 0) {
				r = usb_detach_kernel_driver_np(h, 0);
				if (r < 0) {
					usb_close(h);
					printf("Device is in use by \"%s\" driver\n", buf);
					continue;
				}
			}
			#endif
			// Mac OS-X - removing this call to usb_claim_interface() might allow
			// this to work, even though it is a clear misuse of the libusb API.
			// normally Apple's IOKit should be used on Mac OS-X

			r = usb_claim_interface(h, 0);
			if (r < 0) {
				usb_close(h);
				printf("Unable to claim interface, check USB permissions\n");
				continue;
			}

			return h;
		}
	}
	return NULL;
}

hid_device * HID_API_EXPORT hid_open (unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number)
{
  /* This function is identical to the Linux version. Platform independent. */
	struct hid_device_info *devs, *cur_dev;
	const char *path_to_open = NULL;
	hid_device * handle = NULL;

	devs = hid_enumerate(vendor_id, product_id);

	cur_dev = devs;

	while (cur_dev) {
		if (cur_dev->vendor_id == vendor_id &&
		    cur_dev->product_id == product_id) {
			if (serial_number) {
				if (wcscmp(serial_number, cur_dev->serial_number) == 0) {
					path_to_open = cur_dev->path;
					break;
				}
			}
			else {
				path_to_open = cur_dev->path;
				break;
			}
		}
		cur_dev = cur_dev->next;
	}

	if (path_to_open) {
		/* Open the device */
    handle->device_handle = open_usb_device (vendor_id, product_id);
	}

	hid_free_enumeration(devs);

  handle->first_time = true;

	return handle;
}

hid_device * HID_API_EXPORT hid_open_path (const char *path)
{
  printf ("opening by path\n");

	hid_dev.device_handle = open_usb_device (0x16c0, 0x0478);

	if (!hid_dev.device_handle)
	{
		return NULL;
	}

  printf ("Electra connected\n");

	return (&hid_dev);
}

int hid_report_write (usb_dev_handle *device_handle, const unsigned char *buf, int len, double timeout)
{
  int r;

  if (!device_handle)
  {
    return (-1);
  }

  while (timeout > 0)
  {
    r = usb_control_msg (device_handle, 0x21, 9, 0x0200, 0, (char *)(buf + 1), len - 1, (int)(timeout * 1000.0));

    if (r >= 0)
    {
      return (1);
    }

    usleep(10000);
    timeout -= 0.01;  // TODO: subtract actual elapsed time
  }

  return (-1);
}


int HID_API_EXPORT hid_write (hid_device *dev, const unsigned char *data, size_t length)
{
  int rc = hid_report_write (dev->device_handle, data, length, (dev->first_time == true) ? 5 : 0.5);

  return (rc);
}

int HID_API_EXPORT hid_read_timeout (hid_device *dev, unsigned char *data, size_t length, int milliseconds)
{
	int bytes_read = -1;

  return (bytes_read);
}

int HID_API_EXPORT hid_read (hid_device *dev, unsigned char *data, size_t length)
{
	return hid_read_timeout (dev, data, length, (dev->blocking)? -1: 0);
}

int HID_API_EXPORT hid_set_nonblocking(hid_device *dev, int nonblock)
{
	/* All Nonblocking operation is handled by the library. */
	dev->blocking = !nonblock;

	return 0;
}

int HID_API_EXPORT hid_send_feature_report(hid_device *dev, const unsigned char *data, size_t length)
{
	return (-1);
}

int HID_API_EXPORT hid_get_feature_report(hid_device *dev, unsigned char *data, size_t length)
{
		return -1;
}

void HID_API_EXPORT hid_close(hid_device *dev)
{
	if (!dev->device_handle)
  {
    return;
  }

	usb_release_interface (dev->device_handle, 0);
	usb_close(dev->device_handle);

	dev->device_handle = NULL;
}

int HID_API_EXPORT_CALL hid_get_manufacturer_string (hid_device *dev, wchar_t *string, size_t maxlen)
{
	return (1);
}

int HID_API_EXPORT_CALL hid_get_product_string (hid_device *dev, wchar_t *string, size_t maxlen)
{
	return (1);
}

int HID_API_EXPORT_CALL hid_get_serial_number_string (hid_device *dev, wchar_t *string, size_t maxlen)
{
	return (1);
}

int HID_API_EXPORT_CALL hid_get_indexed_string (hid_device *dev, int string_index, wchar_t *string, size_t maxlen)
{
	/* TODO: */

	return 0;
}


HID_API_EXPORT const wchar_t * HID_API_CALL  hid_error(hid_device *dev)
{
	/* TODO: */

	return NULL;
}
