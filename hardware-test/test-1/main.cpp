#include <iostream>
#include "dbftdi.h"
#include <unistd.h>

using namespace std;

int main()
{
    Ftdi *driver = new Ftdi();
    driver->openAll();
    driver->openDoor();
    usleep(1000000);
    driver->closeDoor();
    return 0;
}
