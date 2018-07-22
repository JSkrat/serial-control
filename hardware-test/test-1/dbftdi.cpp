/************ installing *************/
/* libftdi-dev  */
/*************************************/
#include "dbftdi.h"
#include <stdio.h>
#include <stdlib.h>
#include <libftdi1/ftdi.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <cstring>
//#include <QThread>
//#include <QTimer>

#define DOOR_PORT   3
#define VID         0x0403
#define PID         0x6014

Ftdi::Ftdi() //: QObject(parent)
{
    chipCodes[1] = 128;
    chipPins[1] = 0;
    ftdi_version_info info = ftdi_get_library_version();
    fprintf(stdout, "ftdi version info: major %d, micro %d, minor %d, snapshot %s, version %s\n",
            info.major,
            info.micro,
            info.minor,
            info.snapshot_str,
            info.version_str
            );
}

Ftdi::~Ftdi()
{
    if (0 != this->relay) {
        ftdi_usb_close(this->relay);
        ftdi_free(this->relay);
    }
    if (0 != this->rfid) {
        ftdi_usb_close(this->rfid);
        ftdi_free(this->rfid);
    }
}

void Ftdi::openDoor()
{
    doorAction(true);
}

void Ftdi::closeDoor()
{
    doorAction(false);
}

ftdi_context *Ftdi::openSerial(const char *ft_serial)
{
    int retOk = true;
    struct ftdi_context *ret = ftdi_new();
    if (0 == ret) {
        fprintf(stderr, "ftdi_new failed\n");
        return 0;
    }
    ftdi_set_interface(ret, INTERFACE_ANY);
    try {
        struct ftdi_device_list *devlist, *curdev;
        int status;
        if ((status = ftdi_usb_find_all(ret, &devlist, VID, PID)) < 0) {
            fprintf(stderr, "ftdi_usb_find_all failed: %s\n", ftdi_get_error_string(ret));
            throw 1;
        }
        curdev = devlist;
        while (curdev != NULL) {
            char manufacturer[128], description[128], serial[128];
            int status = ftdi_usb_get_strings(
                        ret,
                        curdev->dev,
                        manufacturer,
                        sizeof(manufacturer),
                        description,
                        sizeof(description),
                        serial,
                        sizeof(serial)
                        );
            int cmp = strcmp(ft_serial, serial);
            if (0 == cmp) {
                printf("ret %d, manufacturer %s, description %s, serial [%s], is relay %d\n",
                       status,
                       manufacturer,
                       description,
                       serial,
                       cmp
                       );
                int status = ftdi_usb_open_dev(ret, curdev->dev);
                if (status < 0) {
                    fprintf(stderr, "Unable to open device: %s",
                            ftdi_get_error_string(ret));
                    retOk = false;
                }
                break;
            }
            curdev = curdev->next;
        }
        ftdi_list_free(&devlist);
        if (! retOk) {
            throw 1;
        }
    } catch (...) {
        ftdi_free(ret);
        ret = 0;
    }
    return ret;
}

int Ftdi::openAll() {
    this->relay = this->openSerial("FT2T4GX2");
    this->rfid = this->openSerial("FT2INRWM");
}

int Ftdi::doorAction(bool open) {
//    write(3, 0, open);
    write2(0, open);
}

int Ftdi::write2(unsigned int pin, unsigned int value) {
    value = value > 0 ? 1 : 0;
    printf("set value %d\n", value);
    unsigned char buf[3] = {0, 0, 0};
    int f = 0;

    if (nullptr == this->relay) {
        fprintf(stderr, "relay device not initialized\n");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "ftdi_set_bitmode\n");
    f = ftdi_set_bitmode(this->relay, 0x00, BITMODE_MPSSE);
    if (f < 0) {
        fprintf(stderr, "Unable to set bitmode: %s",
                ftdi_get_error_string(this->relay));
        return EXIT_FAILURE;
    }
    usleep(20000);

    buf[0] = 0x82;
    buf[1] = value << pin;
    buf[2] = 0x03;
    fprintf(stdout, "ftdi_write_data\n");
    f = ftdi_write_data(this->relay, buf, 3);
    if (f < 0) {
        fprintf(stderr,"write failed for 0x%x, error %d (%s)\n", buf[0], f, ftdi_get_error_string(this->relay));
    }
    return EXIT_SUCCESS;
}

