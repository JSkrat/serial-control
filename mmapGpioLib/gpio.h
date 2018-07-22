#ifndef GPIO_H
#define GPIO_H

/// experimentally discovered, that it is output number 1
#define DOOR_PIN 1

#include <QObject>
//#include <QSerialPort>
#include <QHash>
#include <QTimer>
#include <QProcess>

class Ftdi;

typedef enum {
    noCommand   = 0x0000,
    setDeviceID= 0x0102,
    getDeviceID = 0x0303,
    getHWVision = 0x0104,
    request     = 0x0201,
    setRFField  = 0x010C,
    setNFCField = 0x0D01,
    sendNFC     = 0x0E01,
    authenticate= 0x0203,
    readBlock   = 0x0208,
    anticollision=0x0202
} SL060Commands;

typedef struct {
    SL060Commands command;
    QByteArray data;
} SL060Command;

class GPIOTest;
class GPIO : public QObject
{
    friend GPIOTest;
    Q_OBJECT
    QProcess shell;
    //QSerialPort serialPort;
    Ftdi *serialPort;
    /// device ID, 0 is broadcast to receive the real device id
    uint16_t RFIDID = 0, newRFIDID;
    /// payload buffer
    QByteArray buffer;
    /// SL060 protocol parser states
    enum {
        error,
        start,
        prefix2,
        length1,
        length2,
        payload,
        checksum
    } bufferState = start;
    uint16_t bufferLength;
    /*SL060Command initSequence[2] = {
        {.command = initDeviceID, .data = QByteArrayLiteral("\x00\x01")},
        {.command = setRFField}
    };*/
    uint8_t bufferChecksum;
    qint64 bytesWritten;
    QTimer bufferTimeout, writeTimeout, readTimeout, scanTimer;

    const QHash<uint8_t, QString> RFIDStatuses = {
        {0x00, "Operation successes"},
        {0x01, "NFC connect fails"},
        {0x0A, "Operation fails"},
        {0x0B, "Command is not supported"},
        {0x0C, "Parameter is error"},
        {0x0D, "No cards"},
        {0x0E, "RF base station is damaged"},
        {0x14, "Searching card fails"},
        {0x15, "Reset card fails"},
        {0x16, "Verifying key fails"},
        {0x17, "Reading fails"},
        {0x18, "Writing fails"}
    };
#define RFIDStatusOk 0
#define RFIDStatusFailed 0x0A
#define RFIDStatusNoCard 0x14
    /// commands
    SL060Commands responseWaiting = noCommand;
    QList<SL060Command> sendQueue;
    /// current card uid, for filtering events
    QByteArray cardUID;
    // methods
    QByteArray makeCommand(uint16_t command, QByteArray &data);
    void writeNextCommand();

public:
    explicit GPIO(QObject *parent = nullptr);

    void writeCommand(SL060Commands command, QByteArray &data);
    void writeCommand(SL060Commands command);

    Ftdi * ftdi;

Q_SIGNALS:
    void cardDiscovered(QByteArray uid);
public Q_SLOTS:
private Q_SLOTS:
    //void handleError(QSerialPort::SerialPortError serialPortError);
    //void handleTimeout();
    void handleReadyRead(const QByteArray &chunk);
    void handleBytesWritten(qint64 bytes);
    void bufferTimeouted();
    void writeTimeouted();
    void readTimeouted();
    void scanTimeouted();
};

#endif // GPIO_H
