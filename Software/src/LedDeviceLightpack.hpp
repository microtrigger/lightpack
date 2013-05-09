/*
 * LedDeviceLightpack.hpp
 *
 *  Created on: 26.07.2010
 *      Author: Mike Shatohin (brunql)
 *     Project: Lightpack
 *
 *  Lightpack is very simple implementation of the backlight for a laptop
 *
 *  Copyright (c) 2010, 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *  Lightpack is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Lightpack is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <QtGui>

#include "AbstractLedDevice.hpp"
#include "TimeEvaluations.hpp"
#include "LightpackMath.hpp"

#include <usb.h>
#include "../../CommonHeaders/USB_ID.h"     /* For device VID, PID, vendor name and product name */
#include "../../CommonHeaders/COMMANDS.h"   /* CMD defines */

// This defines using in all data transfers to determine indexes in write_buffer[]
// In device COMMAND have index 0, data 1 and so on, report id isn't using
#define WRITE_BUFFER_INDEX_COMMAND      0
#define WRITE_BUFFER_INDEX_DATA_START   1

class LedDeviceLightpack : public AbstractLedDevice
{
    Q_OBJECT
public:
    LedDeviceLightpack(QObject *parent = 0);
    ~LedDeviceLightpack();

public slots:
    void open();
    void setColors(const QList<QRgb> & colors);
    void switchOffLeds();
    void setRefreshDelay(int value);
    void setColorDepth(int value);
    void setSmoothSlowdown(int value);
    void setColorSequence(QString /*value*/);
    void requestFirmwareVersion();
    void updateDeviceSettings();
    int maxLedsCount() { return m_devices.size() * kLedsPerDevice;}

private: 
    bool readDataFromDevice(usb_dev_handle *phDev);
    bool writeBufferToDevice(int command, usb_dev_handle *phDev);
    bool tryToReopenDevice();
    bool readDataFromDeviceWithCheck();
    bool writeBufferToDeviceWithCheck(int command, usb_dev_handle *phDev);
    void resizeColorsBuffer(int buffSize);
    void closeDevices();

private slots:
    void restartPingDevice(bool isSuccess);
    void timerPingDeviceTimeout();

private:
    QList<usb_dev_handle*> m_devices;
//    hid_device *m_hidDevice;

    unsigned char m_readBuffer[61];    /* 0-ReportID, 1..65-data */
    unsigned char m_writeBuffer[61];   /* 0-ReportID, 1..65-data */

    QTimer *m_timerPingDevice;

    static const int PingDeviceInterval;
    static const int kLedsPerDevice;
};
