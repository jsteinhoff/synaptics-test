#include <stdio.h>
#include <string.h>
#include <usb.h>

#ifndef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
#warning : usb_detach_kernel_driver_np not available, you must do "rmmod usbhid" manually!
#endif
#ifndef LIBUSB_HAS_GET_DRIVER_NP
#warning : usb_detach_get_driver_np not available, you must do "rmmod usbhid" manually!
#endif

/* vendor and device IDs */
#define SYNAPTICS_VENDOR_ID	0x06cb
struct {
	int dev_id;
	char *dev_name;
} dev_names[] = {
	{ 0x0001, "Synaptics USB TouchPad" },
	{ 0x0002, "Synaptics Integrated USB TouchPad" },
	{ 0x0003, "Synaptics cPad" },
	{ 0x0006, "Synaptics TouchScreen" },
	{ 0x0007, "Synaptics USB Styk" },
	{ 0x0008, "Synaptics USB WheelPad" },
	{ 0x0009, "Synaptics Composite USB Device" },
	{ 0, NULL }
};

struct usb_dev_handle *usb_handle = NULL;
int endpoint;
char* data = NULL;
int max_data;
int claimed = 0;

int int_num = 0;
int alt = 0;
int newline = 1;
int skip = 0;
int decode = 0;

void leave(int i)
{
	if (data)
		free(data);
	if (claimed)
		if (usb_release_interface(usb_handle, int_num) != 0)
			perror("can not release inertface");
	if (usb_handle != NULL)
		if (usb_close(usb_handle) != 0)
			perror("can not close interface");
	exit(i);
}

char* get_dev_name(struct usb_device *dev)
{
	int i;

	for (i=0; dev_names[i].dev_name; i++) {
		if (dev->descriptor.idProduct == dev_names[i].dev_id)
			break;
	}

	return dev_names[i].dev_name ? dev_names[i].dev_name : "unknown";
}

void find_endpoint(struct usb_interface_descriptor* interface)
{
	struct usb_endpoint_descriptor* ep;
	int i;

	for (i=0; i<interface->bNumEndpoints; i++) {
		ep = &interface->endpoint[i];
		if ( (ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) &&
		     ((ep->bmAttributes & USB_ENDPOINT_TYPE_MASK) 
					== USB_ENDPOINT_TYPE_INTERRUPT) ) {
			endpoint = ep->bEndpointAddress;
			printf("found endpoint with address 0x%x\n", endpoint);
			max_data = ep->wMaxPacketSize;
			data = malloc(max_data);
			if (!data) {
				perror("can not allocate buffer");
				leave(-1);
			}
			return;
		}
	}

	fprintf(stderr, "no usable endpoint found\n");
	leave(-1);
}

void claim_device(struct usb_device *dev)
{
	int num_int;
	char mod_name[16];

	usb_handle = usb_open(dev);
	if (usb_handle == NULL) {
		perror("not able to claim the USB device");
		leave(-1);
	}

	if (dev->descriptor.bNumConfigurations != 1) {
		fprintf(stderr, "device has wrong number of configurations: %i\n",
			dev->descriptor.bNumConfigurations);
		leave(-1);
	}
	num_int = dev->config[0].bNumInterfaces;
	printf("number of interfaces: %i\n", num_int);
	if (int_num >= num_int) {
		fprintf(stderr, "requested interface %i not present\n", int_num);
		leave(-1);
	}

#if defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP) && defined(LIBUSB_HAS_GET_DRIVER_NP)
	if (usb_get_driver_np(usb_handle, int_num, mod_name, 16) == 0) {
		printf("interface %i is already attached to driver %s, detaching...\n",
			int_num, mod_name);
		if (usb_detach_kernel_driver_np(usb_handle, int_num) != 0) {
			perror("can not detach kernel driver");
			leave(-1);
		}
	}
#endif

	if (usb_claim_interface(usb_handle, int_num) != 0) {
		perror("can not claim interface");
		leave(-1);
	}
	claimed = 1;
	if (usb_set_altinterface(usb_handle, alt)) {
		perror("can not set alternate setting");
		leave(-1);
	}

	find_endpoint(&dev->config[0].interface[int_num].altsetting[alt]);
}

void init_device(void)
{
	struct usb_bus *usb_bus;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (usb_bus = usb_get_busses(); usb_bus; usb_bus = usb_bus->next) {
		for (dev = usb_bus->devices; dev;dev = dev->next) {
			if ((dev->descriptor.idVendor == SYNAPTICS_VENDOR_ID)) {
				printf("found device: %s\n", get_dev_name(dev));
				claim_device(dev);
				return;
			}
		}
	}

	fprintf(stderr, "no Synaptics device found\n");
	leave(-1);
}

#define PROGRAM_DESCRIPTION \
"synaptics-test - reads data from the first Synaptics USB device it finds\n\
\n\
Syntax:\n\
	%s [--interface <n>] [--abs] [--decode]\n\
		[--no-newline] [--skip <n>]\n\
\n\
Description:\n\
	--interface <n>: read data from the n-th interface\n\
	--abs: reads absolute data instead of relative\n\
		(this will switch the interface from alternate\n\
		 setting 0 to alternate setting 1)\n\
	--decoce: try to decode the data packets\n\
		(this will maybe only work for relative data or\n\
		 for alsolute data from a touchpad)\n\
		X: relative or absolute x position\n\
		Y: relative or absolute y position\n\
		B: buttons (hex)\n\
		P: pressure\n\
		W: value 4-15 is the finger width\n\
		   value 0 is for two finger tap\n\
  		   value 1 is for three finger tap\n\
		   value 2 is for pen detected\n\
	--no-newline: do not write a new packet to a new line\n\
	--skip <n>: skip n packets before showing the next\n\
\n"

void get_params(int argc, char **argv)
{
	int i;

	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i], "--abs")) {
			alt = 1;
		} else if (!strcmp(argv[i], "--no-newline")) {
			newline = 0;
		} else if (!strcmp(argv[i], "--decode")) {
			decode = 1;
		} else if ((!strcmp(argv[i], "--interface")) && (i<argc-1)) {
			if (sscanf(argv[++i], "%i", &int_num) != 1)
				goto error;
			if (int_num < 0)
				goto error;
		} else if ((!strcmp(argv[i], "--skip")) && (i<argc-1)) {
			if (sscanf(argv[++i], "%i", &skip) != 1)
				goto error;
			if (skip < 0)
				goto error;
		} else
			goto error;
	}

	return;
error:
	printf(PROGRAM_DESCRIPTION, argv[0]);
	leave(-2);
}

void decode_data()
{
	printf("    ");
	if (max_data < 8) {
		printf("X:%+-4hhi Y:%+-4hhi B:%02hhx", data[1], data[2], data[0]);
	} else {
		/* FIXME: is this still right for Trackpoint or Touchscreen ? */
		printf("X:%-5u Y:%-5u B:%02hhx P:%-3hhu W:%-2hhu",
			((int)data[2] << 8) + data[3],
			((int)data[4] << 8) + data[5],
			data[1], data[6], data[0] & 0x0f);
	}
}

int main(int argc, char **argv)
{
	int data_size;
	int i;
	int packet = 0;

	get_params(argc, argv);
	init_device();

	printf("reading %s data from interface %i, press <ctrl>-c to quit\n",
		alt ? "absolute" : "relative", int_num);

	while (1) {
		packet++;
		data_size = usb_interrupt_read(usb_handle, endpoint, data, max_data, 0);
		if (data_size < 0) {
			perror("can not read from endpoint");
			leave(-1);
		}

		if (packet%(skip+1))
			continue;

		printf("packet %5i: ", packet);
		for (i=0; i<data_size; i++)
			printf(" %02hhx", data[i]);
		if (decode)
			decode_data();
		if (newline) {
			printf("\n");
		} else {
			printf("\r");
			fflush(stdout);
		}
	}

	return 0;
}
