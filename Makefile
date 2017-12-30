LDFLAGS = `libusb-config --libs`
CFLAGS = `libusb-config --cflags` -Wall

all:	synaptics-test

.PHONY: clean

clean:
	$(RM) -f *.o synaptics-test
