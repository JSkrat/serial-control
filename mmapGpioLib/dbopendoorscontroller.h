#ifndef DBOPENDOORSCONTROLLER_H
#define DBOPENDOORSCONTROLLER_H

#include <QObject>
#include "mmapGpioLib/dbftdievent.h"

class Ftdi;

class DBOpenDoorsController : public QObject
{
    Q_OBJECT
public:
    DBOpenDoorsController(QList<DBFtdiEvent> &openDoorQueue, Ftdi * ftdi, QObject *parent = 0);

    void setOpenDoorQueue(QList<DBFtdiEvent> &openDoorQueue);
    void startQueue();


public Q_SLOTS:
    void handleEvent(const DBFtdiEvent & event);


private:
    QList<DBFtdiEvent> openDoorQueue;
    Ftdi * ftdi;
};

#endif // DBOPENDOORSCONTROLLER_H
