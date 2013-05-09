/*
 * LightpackDevice.cpp
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

#include "LedDeviceLightpack.hpp"
#include "LightpackApplication.hpp"

#include <unistd.h>

#include <QtDebug>
#include "debug.h"
#include "Settings.hpp"
#include "usb.h"

using namespace SettingsScope;

const int LedDeviceLightpack::PingDeviceInterval = 1000;
const int LedDeviceLightpack::kLedsPerDevice = 10;

LedDeviceLightpack::LedDeviceLightpack(QObject *parent) :
    AbstractLedDevice(parent)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "thread id: " << this->thread()->currentThreadId();

//    m_hidDevice = NULL;

    memset(m_writeBuffer, 0, sizeof(m_writeBuffer));
    memset(m_readBuffer, 0, sizeof(m_readBuffer));

    m_timerPingDevice = new QTimer(this);

    connect(m_timerPingDevice, SIGNAL(timeout()), this, SLOT(timerPingDeviceTimeout()));
    connect(this, SIGNAL(ioDeviceSuccess(bool)), this, SLOT(restartPingDevice(bool)));
    connect(this, SIGNAL(openDeviceSuccess(bool)), this, SLOT(restartPingDevice(bool)));

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "initialized";
}

LedDeviceLightpack::~LedDeviceLightpack()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "hid_close(...);";
    closeDevices();
}

void LedDeviceLightpack::setColors(const QList<QRgb> & colors)
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO << hex << (colors.isEmpty() ? -1 : colors.first());
#if 0
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "thread id: " << this->thread()->currentThreadId();
#endif
    if (colors.count() > maxLedsCount()) {
        qWarning() << Q_FUNC_INFO << "data size is greater than max leds count";

        // skip command with wrong data size
        return;
    }

    QMutexLocker locker(&getLightpackApp()->m_mutex);

    resizeColorsBuffer(colors.count());

    // Save colors for showing changes of the brightness
    m_colorsSaved = colors;

    applyColorModifications(colors, m_colorsBuffer);

    // First write_buffer[0] == 0x00 - ReportID, i have problems with using it
    // Second byte of usb buffer is command (write_buffer[1] == CMD_UPDATE_LEDS, see below)
    int buffIndex = WRITE_BUFFER_INDEX_DATA_START;

    bool ok = true;

    memset(m_writeBuffer, 0, sizeof(m_writeBuffer));
    for (int i = 0; i < m_colorsBuffer.count(); i++)
    {
        StructRgb color = m_colorsBuffer[i];

        // Send main 8 bits for compability with existing devices
        m_writeBuffer[buffIndex++] = (color.r & 0x0FF0) >> 4;
        m_writeBuffer[buffIndex++] = (color.g & 0x0FF0) >> 4;
        m_writeBuffer[buffIndex++] = (color.b & 0x0FF0) >> 4;

        // Send over 4 bits for devices revision >= 6
        // All existing devices ignore it
        m_writeBuffer[buffIndex++] = (color.r & 0x000F);
        m_writeBuffer[buffIndex++] = (color.g & 0x000F);
        m_writeBuffer[buffIndex++] = (color.b & 0x000F);

        if ((i+1) % kLedsPerDevice == 0 || i == m_colorsBuffer.size() - 1) {
            if (!writeBufferToDeviceWithCheck(CMD_UPDATE_LEDS, m_devices[(i+kLedsPerDevice)/kLedsPerDevice - 1])) {
                ok = false;
            }
            memset(m_writeBuffer, 0, sizeof(m_writeBuffer));
            buffIndex = WRITE_BUFFER_INDEX_DATA_START;
        }
    }

    locker.unlock();


    // WARNING: LedDeviceManager sends data only when the arrival of this signal
    emit commandCompleted(ok);
}

void LedDeviceLightpack::switchOffLeds()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    if (m_colorsSaved.count() == 0)
    {
        for (int i = 0; i < maxLedsCount(); i++)
            m_colorsSaved << 0;
    } else {
        for (int i = 0; i < m_colorsSaved.count(); i++)
            m_colorsSaved[i] = 0;
    }

    m_timerPingDevice->stop();

    memset(m_writeBuffer, 0, sizeof(m_writeBuffer));

    bool ok = true;
    for(int i = 0; i < m_devices.size(); i++) {
        if (!writeBufferToDeviceWithCheck(CMD_UPDATE_LEDS, m_devices[i]))
            ok = false;
    }


    emit commandCompleted(ok);
    // Stop ping device if switchOffLeds() signal comes
}

void LedDeviceLightpack::setRefreshDelay(int value)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << value;

    m_writeBuffer[WRITE_BUFFER_INDEX_DATA_START] = value & 0xff;
    m_writeBuffer[WRITE_BUFFER_INDEX_DATA_START+1] = (value >> 8);

    bool ok = true;
    for(int i = 0; i < m_devices.size(); i++) {
        if (!writeBufferToDeviceWithCheck(CMD_SET_TIMER_OPTIONS, m_devices[i]))
            ok = false;
    }
    emit commandCompleted(ok);
}

void LedDeviceLightpack::setColorDepth(int value)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << value;

    m_writeBuffer[WRITE_BUFFER_INDEX_DATA_START] = (unsigned char)value;

    bool ok = true;
    for(int i = 0; i < m_devices.size(); i++) {
        if (!writeBufferToDeviceWithCheck(CMD_SET_PWM_LEVEL_MAX_VALUE, m_devices[i]))
            ok = false;
    }
    emit commandCompleted(ok);
}

void LedDeviceLightpack::setSmoothSlowdown(int value)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << value;

    m_writeBuffer[WRITE_BUFFER_INDEX_DATA_START] = (unsigned char)value;

    bool ok = true;
    for(int i = 0; i < m_devices.size(); i++) {
        if (!writeBufferToDeviceWithCheck(CMD_SET_SMOOTH_SLOWDOWN, m_devices[i]))
            ok = false;
    }
    emit commandCompleted(ok);
}

void LedDeviceLightpack::setColorSequence(QString /*value*/)
{
    emit commandCompleted(true);
}

void LedDeviceLightpack::requestFirmwareVersion()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    if(m_devices.size() < 1) {
        emit commandCompleted(false);
        return;
    }

    QString fwVersion;

    bool ok = readDataFromDevice(m_devices[0]);

    // TODO: write command CMD_GET_VERSION to device
    if (ok)
    {
        int fw_major = m_readBuffer[INDEX_FW_VER_MAJOR];
        int fw_minor = m_readBuffer[INDEX_FW_VER_MINOR];
        fwVersion = QString::number(fw_major) + "." + QString::number(fw_minor);
    } else {
        fwVersion = QApplication::tr("read device fail");
    }

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Version:" << fwVersion;

    emit firmwareVersion(fwVersion);
    emit commandCompleted(ok);
}

void LedDeviceLightpack::updateDeviceSettings()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << sender();

    AbstractLedDevice::updateDeviceSettings();
    setRefreshDelay(Settings::getDeviceRefreshDelay());
    setColorDepth(Settings::getDeviceColorDepth());
    setSmoothSlowdown(Settings::getDeviceSmooth());

    requestFirmwareVersion();
}


void LedDeviceLightpack::open()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_devices.size() > 0) {
        return;
    }

    struct usb_bus *busses;
    struct usb_bus *bus;

    usb_init();

    usb_find_busses();
    usb_find_devices();

    busses = usb_get_busses();
    for (bus = busses; bus; bus = bus->next) {
        struct usb_device *dev;
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == USB_VENDOR_ID &&
                dev->descriptor.idProduct == USB_PRODUCT_ID) {
                if (usb_dev_handle *phDev = usb_open(dev)) {
                    usb_detach_kernel_driver_np(phDev, 0);
                    usb_claim_interface(phDev, 0);
                    m_devices.append(phDev);
                }
            }
        }
    }

    if (m_devices.size() == 0)
    {
        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Lightpack device not found";
        emit openDeviceSuccess(false);
        return;
    }

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Lightpack opened";

    updateDeviceSettings();

    emit openDeviceSuccess(true);
}

bool LedDeviceLightpack::readDataFromDevice(usb_dev_handle *phDev)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    int bytes_read = usb_control_msg(phDev,
                                     USB_ENDPOINT_IN | USB_TYPE_CLASS
                                     | USB_RECIP_INTERFACE,
                                     0x01, //REQ_GetReport
                                     (2 << 8),
                                     0x00,
                                     reinterpret_cast<char *>(m_readBuffer), sizeof(m_readBuffer), 500);
    if (bytes_read < 0) {
        qWarning() << "Error reading data:" << bytes_read;
        emit ioDeviceSuccess(false);
        return false;
    }
    emit ioDeviceSuccess(true);
    return true;
}

bool LedDeviceLightpack::writeBufferToDevice(int command, usb_dev_handle *phDev)
{    
    DEBUG_MID_LEVEL << Q_FUNC_INFO << command;
#if 0
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "thread id: " << this->thread()->currentThreadId();
#endif

    m_writeBuffer[WRITE_BUFFER_INDEX_COMMAND] = command;

    int result = usb_control_msg(phDev,
                                     USB_ENDPOINT_OUT | USB_TYPE_CLASS
                                     | USB_RECIP_INTERFACE,
                                     0x09, //REQ_SetReport
                                     (2 << 8),
                                     0x00,
                                     reinterpret_cast<char *>(m_writeBuffer), sizeof(m_writeBuffer), 500);
    emit ioDeviceSuccess(result > 0);
    return true;
}

bool LedDeviceLightpack::tryToReopenDevice()
{
    closeDevices();
    open();

    if (m_devices.size() == 0)
    {
        return false;
    }

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Reopen success";
    return true;
}

bool LedDeviceLightpack::readDataFromDeviceWithCheck()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_devices.size() > 0)
    {
        if (!readDataFromDevice(m_devices[0]))
        {
            if (tryToReopenDevice())
                return readDataFromDevice(m_devices[0]);
            else
                return false;
        }
        return true;
    } else {
        if (tryToReopenDevice())
            return readDataFromDevice(m_devices[0]);
        else
            return false;
    }
}

bool LedDeviceLightpack::writeBufferToDeviceWithCheck(int command, usb_dev_handle *phDev)
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (phDev != NULL)
    {
        if (!writeBufferToDevice(command, phDev))
        {
            if (!writeBufferToDevice(command, phDev))
            {
                if (tryToReopenDevice())
                    return writeBufferToDevice(command, phDev);
                else
                    return false;
            }
        }
        return true;
    } else {
        if (tryToReopenDevice())
            return writeBufferToDevice(command, phDev);
        else
            return false;
    }
}

void LedDeviceLightpack::resizeColorsBuffer(int buffSize)
{
    if (m_colorsBuffer.count() == buffSize)
        return;

    m_colorsBuffer.clear();

    if (buffSize > maxLedsCount())
    {
        qCritical() << Q_FUNC_INFO << "buffSize > MaximumLedsCount" << buffSize << ">" << maxLedsCount();

        buffSize = maxLedsCount();
    }

    for (int i = 0; i < buffSize; i++)
    {
        m_colorsBuffer << StructRgb();
    }
}

void LedDeviceLightpack::closeDevices()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;
    for(int i=0; i < m_devices.size(); i++) {
        usb_release_interface(m_devices[i],0);
        usb_close(m_devices[i]);
    }
    m_devices.clear();
}

void LedDeviceLightpack::restartPingDevice(bool isSuccess)
{
    Q_UNUSED(isSuccess);

    if (Settings::isBacklightEnabled() && Settings::isPingDeviceEverySecond())
    {
        // Start ping device with PingDeviceInterval ms after last data transfer complete
        m_timerPingDevice->start(PingDeviceInterval);
    } else {
        m_timerPingDevice->stop();
    }
}

void LedDeviceLightpack::timerPingDeviceTimeout()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;
/*
    if (m_hidDevice == NULL)
    {
        DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_open";
        m_hidDevice = hid_open(USB_VENDOR_ID, USB_PRODUCT_ID, NULL);

        if (m_hidDevice == NULL)
        {
            DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_open fail";
            emit openDeviceSuccess(false);
            return;
        }
        DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_open ok";

        emit openDeviceSuccess(true);
        closeDevices(); // device should be opened by open() function
        return;
    }

    DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_write";

    m_writeBuffer[WRITE_BUFFER_INDEX_REPORT_ID] = 0x00;
    m_writeBuffer[WRITE_BUFFER_INDEX_COMMAND] = CMD_NOP;
    int bytes = hid_write(m_hidDevice, m_writeBuffer, sizeof(m_writeBuffer));

    if (bytes < 0)
    {
        DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_write fail";
        closeDevices();
        emit ioDeviceSuccess(false);
        return;
    }

    DEBUG_MID_LEVEL << Q_FUNC_INFO << "hid_write ok";
*/
    emit ioDeviceSuccess(true);
}
