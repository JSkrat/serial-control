#include <QCoreApplication>
#include "../../mmapGpioLib/dbftdi.h"
#include "../../mmapGpioLib/gpio.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    /// test gpio
    Ftdi ftdi;
    //ftdi.openAll();

    if (! ftdi.relayFound()) {
        printf("relay board not found\n");
        exit(1);
    }
    ftdi.openDoor(door1);
    sleep(1);
    ftdi.openDoor(door2);
    sleep(1);
    ftdi.openDoor(door3);
    sleep(1);
    ftdi.closeDoor(door1);
    sleep(1);
    ftdi.closeDoor(door2);
    sleep(1);
    ftdi.closeDoor(door3);

    /// test RFID
//    GPIO gpio;

//    return a.exec();
}
