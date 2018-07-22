#include "gpio.h"
#include <QProcess>
#include <iostream>
#include <QObject>
#include <QException>
#include "dbftdi.h"
#include <assert.h>

#define HARD_DEBUG_OUTPUT false

QByteArray GPIO::makeCommand(uint16_t command, QByteArray &data)
{
    QByteArray ret;
    uint8_t csum = 0;
    // preamble
    ret.append(0xAA);
    ret.append(0xBB);
    // length (2 for device id, 2 for command, 1 for checksum)
    ret.append(data.length() + 2 + 2 + 1);
    ret.append(1, 0);
    // device id
    ret.append(RFIDID & 0xFF);
    ret.append(RFIDID >> 8);
    csum ^= (RFIDID & 0xFF) ^ (RFIDID >> 8);
    // command
    ret.append(command & 0xFF);
    ret.append(command >> 8);
    csum ^= (command & 0xFF) ^ (command >> 8);
    // data
    //ret.append(data);
    for (uint8_t i: data) {
        ret.append(i);
        csum ^= i;
        /**
         * If there is any byte equaling to AA occurs between Len and Checksum, one byte 00 will
         * be added after this byte to differentiate preamble. However, the Len keeps unchanged.
         **/
        if (0xAA == i) {
            ret.append(1, 0);
        }
    }
    // control sum
    ret.append(csum);
    return ret;
}

void GPIO::writeNextCommand()
{
    if (this->sendQueue.isEmpty()) return;
    SL060Commands command = this->sendQueue.last().command;
    QByteArray data = this->sendQueue.last().data;

    if (noCommand == command) return;

    QByteArray bytes = makeCommand(command, data);
    assert(0 != this->serialPort);
    bytesWritten = serialPort->write(bytes);
    #if HARD_DEBUG_OUTPUT == true
    QString output;
    for (uint8_t i: bytes) {
        output += QString("%1 ").arg(i, 2, 16);
    }
    std::cout << "command sent " << output.toStdString() << ", total bytes " << bytesWritten << std::endl;
    #endif
    if (bytesWritten == -1) {
        std::cerr << QObject::tr("GPIO::writeCommand bytesWritten == -1 port %1, error: %2")
                            /*.arg(serialPort.portName())
                            .arg(serialPort.errorString())*/
                            .toStdString()
                         << std::endl;
        throw new QException();
    } else if (bytesWritten != bytes.size()) {
        std::cerr << QObject::tr("GPIO::writeCommand bytesWritten != bytes.size() port %1, error: %2\nbytes.size %3, bytesWritten %4")
                            .arg(/*serialPort.portName()*/0)
                            .arg(/*serialPort.errorString()*/0)
                .arg(bytes.size())
                .arg(bytesWritten)
                            .toStdString()
                         << std::endl;
        throw new QException();
    }
    responseWaiting = command;
    writeTimeout.start();
}

void GPIO::writeCommand(SL060Commands command, QByteArray &data)
{
    if (! this->ftdi->rfidFound()) return;
    bool wasEmpty = this->sendQueue.isEmpty();
    this->sendQueue.push_front(SL060Command({
                                   .command = command,
                                   .data = data
            }));
    // if there was no commands in queue, execute immediately
    if (wasEmpty) this->writeNextCommand();
}

void GPIO::writeCommand(SL060Commands command)
{
    QByteArray empty;
    writeCommand(command, empty);
}

GPIO::GPIO(QObject *parent) : QObject(parent), ftdi(nullptr)
{
    RFIDID = 0;
    /*QString portname = "/dev/ttyUSB0";
    serialPort.setPortName(portname);
    serialPort.setBaudRate(QSerialPort::Baud9600);
    serialPort.setParity(QSerialPort::NoParity);
    serialPort.setStopBits(QSerialPort::OneStop);
    serialPort.setFlowControl(QSerialPort::NoFlowControl);
    if (!serialPort.open(QIODevice::ReadWrite)) {
        std::cerr << QObject::tr("Failed to open port %1, error: %2")
                          .arg(portname)
                          .arg(serialPort.errorString())
                          .toStdString()
                       << std::endl;
        //throw new QException();
    }*/
    this->ftdi = new Ftdi();
    this->serialPort = this->ftdi;
    connect(this->ftdi, &Ftdi::readyRead, this, &GPIO::handleReadyRead);
    connect(this->ftdi, &Ftdi::bytesWritten, this, &GPIO::handleBytesWritten);
    //connect(&serialPort, &QSerialPort::errorOccurred, this, &GPIO::handleError);
    /// timeout for unexpected response
    bufferTimeout.setSingleShot(true);
    bufferTimeout.setInterval(5000);
    connect(&bufferTimeout, &QTimer::timeout, this, &GPIO::bufferTimeouted);
    /// wait library to write the command
    writeTimeout.setSingleShot(true);
    writeTimeout.setInterval(2000);
    connect(&writeTimeout, &QTimer::timeout, this, &GPIO::writeTimeouted);
    /// wait for response from rfid controller
    readTimeout.setSingleShot(true);
    readTimeout.setInterval(2000);
    connect(&readTimeout, &QTimer::timeout, this, &GPIO::readTimeouted);
    /// poll for new rfid interval
    scanTimer.setInterval(300);
    scanTimer.setSingleShot(false);
    connect(&scanTimer, &QTimer::timeout, this, &GPIO::scanTimeouted);
    if (this->ftdi->rfidFound()) {
        scanTimer.start();
        /// setup the rfid id
        QByteArray payload = QByteArrayLiteral("\x34\x12");
        this->newRFIDID = 0x1234;
        writeCommand(setDeviceID, payload);
        /// turn on RF field
        payload = QByteArrayLiteral("\x01");
        writeCommand(setRFField, payload);
    }
}

void GPIO::handleReadyRead(const QByteArray &chunk)
{
    //QByteArray chunk(this->ftdi->readAll());
    if (chunk.isEmpty()) {
        // if no response try to send 0 bytes, maybe library will change it's mind
        // correction: most probably that cause very interesting results in ft232h chip, don't do this
        //QByteArray empty;
        //this->serialPort->write(empty);
        return;
    }
    for (uint8_t i: chunk) {
        switch (bufferState) {
        default:
        case error:
            // do nothing, wait timeout
            std::cerr << "GPIO::handleReadyRead parser in error state" << std::endl;
            if (! bufferTimeout.isActive()) bufferTimeout.start();
            [[fallthrough]];
        case start: {
#if HARD_DEBUG_OUTPUT == true
            std::cout << std::endl;
#endif
            if (0xAA == i) {
                bufferState = prefix2;
                bufferTimeout.stop();
            } else bufferState = error;
            break;
        }
        case prefix2: {
            if (0xBB == i) bufferState = length1;
            else bufferState = error;
            break;
        }
        case length1: {
            bufferLength = i;
            bufferState = length2;
            break;
        }
        case length2: {
            bufferLength |= i << 8;
            bufferState = payload;
            buffer.clear();
            bufferChecksum = 0;
            break;
        }
        case payload: {
            if (0 < buffer.length() && (char) 0xAA == buffer.at(buffer.length()-1) && 0x00 == i) break;
            buffer.append(i);
            bufferChecksum ^= i;
            // checksum is the part of the payload actually
            if (buffer.length() >= bufferLength-1) bufferState = checksum;
            break;
        }
        case checksum: {
#if HARD_DEBUG_OUTPUT == true
            std::cout << std::endl;
#endif
            //writeTimeout.stop();
            readTimeout.stop();
            if (bufferChecksum != i) {
                bufferState = error;
                std::cerr << "GPIO::handleReadyRead parse response: crc error "
                          << std::endl;
            } else {
                // read response
                uint8_t status = buffer[4];
                SL060Commands currentResponse = responseWaiting;
                responseWaiting = noCommand;
                switch (currentResponse) {
                default: {
                    QString sResponse;
                    for (uint8_t byte: buffer) {
                        sResponse += QString("%1 ").arg(byte, 2, 16);
                    }
                    std::cerr << "GPIO::handleReadyRead command switch: undefined command was sent <"
                              << currentResponse
                              << "> command "
                              << sResponse.toStdString()
                              << std::endl;
                    break;
                }
                case request: {
                    if (RFIDStatusOk == status) {
                        writeCommand(anticollision);
                    } else if (RFIDStatusNoCard == status) {
                        // erase buffer — so we can emit signal for the same card more, than once
                        cardUID = QByteArrayLiteral("");
                    } else {
                        QString sResponse;
                        for (uint8_t byte: buffer) {
                            sResponse += QString("%1 ").arg(byte, 2, 16);
                        }
                        std::cerr << QString("unknown response for initDeviceID <%1> status %2 command %3")
                                     .arg(currentResponse)
                                     .arg(status)
                                     .arg(sResponse)
                                     .toStdString()
                                  << std::endl;
                    }
                    break;
                }
                case anticollision: {
                    // DeviceID 2 bytes, command 0x0202, status 1 byte, uid 4-7 bytes
                    /// cut first 5 bytes from the buffer
                    QByteArray uid = buffer.right(buffer.length()-5);
//#if HARD_DEBUG_OUTPUT == true
                    QString output;
                    for (uint8_t i: uid) {
                        output += QString("%1 ").arg(i, 2, 16);
                    }
                    std::cout << "Key discovered <" << output.toStdString() << ">" << std::endl;
//#endif
                    if (uid != cardUID) {
                        cardUID = uid;
                        // if no card discovered, status will be RFIDStatusFailed
                        if (RFIDStatusOk == status) {
                            QString output;
                            for (uint8_t i: cardUID) {
                                output += QString("%1 ").arg(i, 2, 16);
                            }
                            std::cout << "EMIT Key discovered <" << output.toStdString() << ">" << std::endl;
                            Q_EMIT cardDiscovered(cardUID);
                        }
                    }
                    break;
                }
                case getDeviceID: {
                    if (RFIDStatusOk == status) {
                        RFIDID = buffer[5] | (buffer[6] << 8);
                        scanTimer.start();

                    } else {
                        std::cerr << "Achtung! Reader responded with not ok status <"
                                  << QString("%1").arg(status, 4, 16).toStdString()
                                  << "> to getDeviceID command. That was unexpected."
                                  << std::endl;
                    }
                    break;
                }
                case setDeviceID: {
                    if (RFIDStatusOk == status) {
                        // everything is fine, do nothing
                        this->RFIDID = this->newRFIDID;
                    } else {
                        std::cerr << QString("setDeviceID responded with status %1. Will use broadcast address 0x0000")
                                     .arg(status, 2, 16, QChar('0'))
                                     .toStdString()
                                  << std::endl;
                        this->RFIDID = 0;
                    }
                    break;
                }
                }
            }
            buffer.clear();
            bufferState = start;
            // the end of parsing response
            // execute next command from the queue
            if (! this->sendQueue.empty()) this->sendQueue.pop_back();
            this->writeNextCommand();
            break;
        }
        }
#if HARD_DEBUG_OUTPUT == true
        std::cout << QString("%1 ").arg(i, 2, 16).toStdString();
#endif
    }
}

void GPIO::handleBytesWritten(qint64 bytes)
{
#if HARD_DEBUG_OUTPUT == true
    std::cout << "bytes written " << bytes << std::endl;
#endif
    bytesWritten -= bytes;
    if (0 >= bytesWritten) {
        writeTimeout.stop();
        readTimeout.start();
    }
}

void GPIO::bufferTimeouted()
{
    std::cerr << "buffer timeouted" << std::endl;
    bufferState = start;
}

void GPIO::writeTimeouted()
{
    std::cerr << QObject::tr("GPIO::writeTimeouted port %1, error: %2\nCommand was <%3>")
                        .arg(/*serialPort.portName()*/0)
                        .arg(/*serialPort.errorString()*/0)
                        .arg(responseWaiting, 4, 16)
                        .toStdString()
              << std::endl;
    responseWaiting = noCommand;
}

void GPIO::readTimeouted()
{
    std::cerr << QObject::tr("GPIO::readTimeouted port %1, error: %2\nCommand was <%3>")
                        .arg(/*serialPort.portName()*/0)
                        .arg(/*serialPort.errorString()*/0)
                        .arg(responseWaiting, 4, 16)
                        .toStdString()
              << std::endl;
    // execute next command from the queue
    this->sendQueue.pop_back();
    this->writeNextCommand();

}

/**
 * @brief GPIO::scanTimeouted requests a list of RF cards in the field from the reader
 */
void GPIO::scanTimeouted()
{
    writeCommand(request);
}
