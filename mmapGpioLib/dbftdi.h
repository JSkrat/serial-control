#ifndef FTDI_H
#define FTDI_H

#include <QObject>
#include <QHash>
#include <map>
#include <list>
#include <ftdi.h>
#include <QTimer>

enum Doors {
    door1 = 0,
    door2 = 1,
    door3 = 2,
};

class Ftdi : public QObject
{
    Q_OBJECT
public:
    explicit Ftdi();
    ~Ftdi();
    int openAll();
    int doorAction(Doors door, bool open);
    int write2(unsigned int pin, unsigned int value);

    int write(QByteArray bytes);
    void start();

    void openDoor(Doors door);
    void closeDoor(Doors door);
//    int read();
    void writeCallback(struct libusb_transfer* transfer);
    void readCallback(struct libusb_transfer* transfer);
    QByteArray readAll();
    bool relayFound();
    bool rfidFound();

Q_SIGNALS:
    void bytesWritten(qint64 bytes);
    void readyRead(const QByteArray &payload);

public Q_SLOTS:
    void libusbDispatch();

private:
    struct ftdi_context *relay, *rfid;
    uint8_t relayState = 0;
    /*std::map <int,unsigned int> chipCodes;
    std::map <int,unsigned int> chipPins;*/
    struct ftdi_context* openSerial(const char *ft_serial);
    unsigned char *readBuffer, *readBufferRaw;
    int readDataSize;
    QTimer libusbDispatcher, readTimer;
    void startReadSequence();
};

#endif // FTDI_H
