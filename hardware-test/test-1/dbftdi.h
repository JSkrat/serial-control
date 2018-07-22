#ifndef FTDI_H
#define FTDI_H

//#include <QObject>
//#include <QHash>
#include <map>
#include <list>
#include "dbftdievent.h"
#include <libftdi1/ftdi.h>

class Ftdi //: public QObject
{
    //Q_OBJECT
public:
    explicit Ftdi();
    ~Ftdi();
    int openAll();
    int doorAction(bool open);
    int write2(unsigned int pin, unsigned int value);

    void openDoor();
    void closeDoor();
//    int read();

//Q_SIGNALS:

//public Q_SLOTS:

private:
    struct ftdi_context *relay, *rfid;
    std::map <int,unsigned int> chipCodes;
    std::map <int,unsigned int> chipPins;
    struct ftdi_context* openSerial(const char *ft_serial);
};

#endif // FTDI_H
