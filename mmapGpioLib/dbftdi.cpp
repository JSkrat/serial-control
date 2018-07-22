/************ installing *************/
/* libftdi-dev  */
/*************************************/
#include "dbftdi.h"
#include <stdio.h>
#include <stdlib.h>

#include <ftdi.h>
#include <libusb.h>
#include <unistd.h>
#include <cstring>
#include <QObject>
#include <iostream>
//#include <QThread>
//#include <QTimer>

#define DOOR_PORT   3
#define VID         0x0403
#define PID         0x6014

#define READ_BUFFER_SIZE 4096

#define DEBUG_MESSAGES false

extern "C" void CWriteCallback(struct libusb_transfer* transfer) {
#if DEBUG_MESSAGES == true
    std::cout << "CWriteCallback" << std::endl;
#endif
    reinterpret_cast<Ftdi*>(transfer->user_data)->writeCallback(transfer);
    if (NULL != transfer->buffer) {
        delete [] (transfer->buffer);
        transfer->buffer = NULL;
    }
    libusb_free_transfer(transfer);
}

extern "C" void CReadCallback(struct libusb_transfer* transfer) {
#if DEBUG_MESSAGES == true
    std::cout << "CReadCallback" << std::endl;
#endif
    reinterpret_cast<Ftdi*>(transfer->user_data)->readCallback(transfer);
    libusb_free_transfer(transfer);
}

Ftdi::Ftdi() : QObject()
{
    this->readBufferRaw = new unsigned char[READ_BUFFER_SIZE];
    ftdi_version_info info = ftdi_get_library_version();
    fprintf(stdout, "ftdi version info: major %d, micro %d, minor %d, snapshot %s, version %s\n",
            info.major,
            info.micro,
            info.minor,
            info.snapshot_str,
            info.version_str
            );
    if (EXIT_SUCCESS != this->openAll()) {
        //throw std::runtime_error("Ftdi::openAll failed");
        fprintf(stderr, "Ftdi::openAll failed\n");
    }
    this->libusbDispatcher.setInterval(10);
    this->libusbDispatcher.setSingleShot(false);
    connect(&this->libusbDispatcher, &QTimer::timeout, this, &Ftdi::libusbDispatch);
    this->libusbDispatcher.start();
    this->startReadSequence();
}

Ftdi::~Ftdi()
{
    if (this->relayFound()) {
        ftdi_usb_close(this->relay);
        ftdi_free(this->relay);
    }
    if (this->rfidFound()) {
        ftdi_usb_close(this->rfid);
        ftdi_free(this->rfid);
    }
    delete [] (this->readBufferRaw);
}

void Ftdi::openDoor(Doors door)
{
    doorAction(door, true);
}

void Ftdi::closeDoor(Doors door)
{
    doorAction(door, false);
}

void Ftdi::writeCallback(libusb_transfer *transfer)
{
    Q_EMIT bytesWritten(transfer->actual_length);
    //usleep(20000);
}

void Ftdi::readCallback(libusb_transfer *transfer)
{
    switch(transfer->status)
    {
    case LIBUSB_TRANSFER_COMPLETED: {
        // Success here, data transfered are inside
        // xfr-&gt;buffer
        // and the length is
        // xfr-&gt;actual_length
        // skip FTDI status bytes.
        this->readBuffer = transfer->buffer + 2;
        this->readDataSize = transfer->actual_length - 2;
        if (0 > this->readDataSize) this->readDataSize = 0;
        // this delay is essental for a correct reading from the device. somehow without it the libusb returns empty buffer instead of the data
//        usleep(20000);
        Q_EMIT readyRead(QByteArray(reinterpret_cast<char *>(this->readBuffer),
                                    this->readDataSize));
        break;
    }
    case LIBUSB_TRANSFER_CANCELLED:
    case LIBUSB_TRANSFER_NO_DEVICE:
    case LIBUSB_TRANSFER_TIMED_OUT:
    case LIBUSB_TRANSFER_ERROR:
    case LIBUSB_TRANSFER_STALL:
    case LIBUSB_TRANSFER_OVERFLOW:
        // Various type of errors here
        printf("Ftdi::readCallback: the libusb read transfer status is %d\n", transfer->status);
        break;
    }
    this->startReadSequence();
}


void Ftdi::libusbDispatch()
{
    struct timeval tv;
    // If a zero timeval is passed, this function will handle any already-pending
    // events and then immediately return in non-blocking style.
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    //printf("Ftdi::libusbDispatch executing\n");
    int status = libusb_handle_events_timeout_completed(this->rfid->usb_ctx, &tv, NULL);
    if (0 != status) {
        printf("Ftdi::libusbDispatch: libusb_handle_events_timeout_completed returned %d\n", status);
    }
}

QByteArray Ftdi::readAll()
{
    return QByteArray(reinterpret_cast<char*>(this->readBuffer), this->readDataSize);
}

bool Ftdi::relayFound()
{
    return (this->relay != 0);
}

bool Ftdi::rfidFound()
{
    return (this->rfid != 0);
}

ftdi_context *Ftdi::openSerial(const char *ft_serial)
{
    int retOk = true;
#if DEBUG_MESSAGES == true
    printf("Ftdi::openSerial(%s)\n", ft_serial);
#endif
    struct ftdi_context *ret = ftdi_new();
    if (0 == ret) {
        fprintf(stderr, "Ftdi::openSerial: ftdi_new failed\n");
        return 0;
    }
    ftdi_set_interface(ret, INTERFACE_ANY);
    try {
        struct ftdi_device_list *devlist, *curdev;
        int status;
        if ((status = ftdi_usb_find_all(ret, &devlist, VID, PID)) < 0) {
            fprintf(stderr, "Ftdi::openSerial: ftdi_usb_find_all failed: %s\n", ftdi_get_error_string(ret));
            throw 1;
        }
        curdev = devlist;
        bool found = false;
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
                found = true;
                printf("Ftdi::openSerial found an ft232h device: ret %d, manufacturer %s, description %s, serial [%s]\n",
                       status,
                       manufacturer,
                       description,
                       serial
                       );
                int status = ftdi_usb_open_dev(ret, curdev->dev);
                if (status < 0) {
                    fprintf(stderr, "Ftdi::openSerial: Unable to open device: %s",
                            ftdi_get_error_string(ret));
                    retOk = false;
                }
                break;
            } else {
#if DEBUG_MESSAGES == true
                printf("Ftdi::openSerial found not matching device with serial [%s] ",
                       serial
                       );
                for (unsigned int i = 0; i < strlen(serial); i++) {
                    printf("%02x ", serial[i]);
                }
                printf("\n");
#endif
            }
            curdev = curdev->next;
        }
        ftdi_list_free(&devlist);
        if (! retOk) {
            throw 1;
        }
        if (! found) {
#if DEBUG_MESSAGES == true
            printf("Ftdi::openSerial FT232H device with a requested serial not found\n");
#endif
            throw 2;
        }
    } catch (...) {
        ftdi_free(ret);
        ret = 0;
    }
    return ret;
}

void Ftdi::startReadSequence()
{
    if (! this->rfid) return;
    // initiate read sequence
    struct libusb_transfer *t = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(t,
                                 this->rfid->usb_dev,
                                 0x81, // IN endpoint
                                 this->readBufferRaw,
                                 READ_BUFFER_SIZE,
                                 CReadCallback,
                                 this,
                                 this->rfid->usb_read_timeout
                                 );
    int status = libusb_submit_transfer(t);
    if (0 != status) {
        printf("Ftdi::startReadSequence: libusb_submit_transfer returned %d\n", status);
        // do nothing, it shall fire read timeout upthere
    }
}

/**
 * @brief Ftdi::openAll search, initialize and open rfid and relay ft232h boards
 * @return EXIT_SUCCESS
 */
int Ftdi::openAll() {
    if (0 == (this->relay = this->openSerial("FT2T4GX2"))) {
        fprintf(stderr, "Ftdi::openAll ft232h board with serial FT2T4GX2 (relay) not found\n");
//        return EXIT_FAILURE;
    } else {
        printf("ftdi_set_bitmode to relay\n");
        int status = ftdi_set_bitmode(this->relay, 0x00, BITMODE_MPSSE);
        if (status < 0) {
            fprintf(stderr, "Unable to set bitmode to relay: %s\n",
                    ftdi_get_error_string(this->relay));
            //return EXIT_FAILURE;
        }
        usleep(20000);
    }

    if (0 == (this->rfid = this->openSerial("FT2INRWM"))) {
        fprintf(stderr, "Ftdi::openAll ft232h board with serial FT2INRWM (rfid) not found\n");
//        return EXIT_FAILURE;
    } else {
        try {
            int status = ftdi_set_bitmode(this->rfid, 0xFF, BITMODE_RESET);
            if (status < 0) {
                fprintf(stderr, "Unable to set bitmode to rfid: %s\n",
                        ftdi_get_error_string(this->rfid));
                throw 3;
            }
            usleep(20000);
            status = ftdi_set_baudrate(this->rfid, 9600);
            if (status < 0) {
                fprintf(stderr, "Ftdi::openAll: Unable to set baudrate to rfid: %s",
                        ftdi_get_error_string(this->rfid));
                throw 4;
            }
            status = ftdi_set_line_property(this->rfid, BITS_8, STOP_BIT_1, NONE);
            if (status < 0) {
                fprintf(stderr, "Ftdi::openAll: unable to set line parameters to rfid: %d (%s)\n",
                        status,
                        ftdi_get_error_string(this->rfid)
                        );
                throw 5;
            }
        } catch (...) {
            //return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int Ftdi::doorAction(Doors door, bool open) {
    return write2(door, open);
}

int Ftdi::write2(unsigned int pin, unsigned int value) {
    this->relayState &= ~(1 << pin);
    if (value) this->relayState |= (1 << pin);
    printf("Ftdi::write2 set relay value %d\n", value);
    unsigned char buf[3] = {0, 0, 0};
    int f = 0;

    if (nullptr == this->relay) {
        fprintf(stderr, "Ftdi::write2 relay device is not initialized\n");
        return EXIT_FAILURE;
    }
    // set output
    buf[0] = 0x82;
    // value
    buf[1] = this->relayState;
    // direction (probably)
    buf[2] = 0x07; // 0x03
    fprintf(stdout, "ftdi_write_data\n");
    f = ftdi_write_data(this->relay, buf, 3);
    if (f < 0) {
        fprintf(stderr,"Ftdi::write2 write failed for 0x%x, error %d (%s)\n", buf[0], f, ftdi_get_error_string(this->relay));
    }
    return EXIT_SUCCESS;
}

/*
 * queue write bytes to a rfid
 */
int Ftdi::write(QByteArray bytes)
{
    // if no rfid this will cause an exception in GPIO
    if (! this->rfid) return 0;
    struct libusb_transfer *t = libusb_alloc_transfer(0);
    char *buffer = new char[bytes.length()];
    memcpy(buffer, bytes.data(), bytes.length());
    libusb_fill_bulk_transfer(t,
                                 this->rfid->usb_dev,
                                 2, // OUT endpoint
                                 reinterpret_cast<unsigned char*>(buffer),
                                 bytes.length(),
                                 CWriteCallback,
                                 this,
                                 this->rfid->usb_write_timeout
                                 );
    int status = libusb_submit_transfer(t);
    if (0 != status) {
        printf("Ftdi::write libusb_submit_transfer returned %d\n", status);
        return 0;
    }
    return bytes.size();
}

