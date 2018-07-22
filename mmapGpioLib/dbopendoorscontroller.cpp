#include "dbopendoorscontroller.h"
#include "dbftdi.h"

DBOpenDoorsController::DBOpenDoorsController(QList<DBFtdiEvent> &_openDoorQueue, Ftdi * _ftdi, QObject *parent) : QObject(parent),
    openDoorQueue(_openDoorQueue), ftdi(_ftdi)
{

}

void DBOpenDoorsController::setOpenDoorQueue(QList<DBFtdiEvent> &_openDoorQueue) {
    openDoorQueue = _openDoorQueue;
}

void DBOpenDoorsController::startQueue() {
    for (DBFtdiEvent & event: openDoorQueue) {
        QTimer::singleShot(event.timeInterval * 1000, this, [this, event] () {
            this->handleEvent(event);
        });
    }
}

void DBOpenDoorsController::handleEvent(const DBFtdiEvent & event) {
    if (event.open) {
        ftdi->openDoor((Doors)event.door);
    }
    else {
        ftdi->closeDoor((Doors)event.door);
    }
}
