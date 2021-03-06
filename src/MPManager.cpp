/******************************************************************************
 **  Copyright (c) Raoul Hecky. All Rights Reserved.
 **
 **  Moolticute is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Moolticute is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#include "MPManager.h"
#include "AppDaemon.h"

#if defined(Q_OS_WIN)
#include "UsbMonitor_win.h"
#elif defined(Q_OS_LINUX)
#include "UsbMonitor_linux.h"
#elif defined(Q_OS_MAC)
#include "UsbMonitor_mac.h"
#endif

MPManager::MPManager():
    QObject(nullptr)
{
}

bool MPManager::initialize()
{
    qInfo() << "Starting MPManager";

    if (!AppDaemon::isEmulationMode())
    {
#if defined(Q_OS_WIN)
        connect(UsbMonitor_win::Instance(), SIGNAL(usbDeviceAdded(QString)), this, SLOT(usbDeviceAdded(QString)));
        connect(UsbMonitor_win::Instance(), SIGNAL(usbDeviceRemoved(QString)), this, SLOT(usbDeviceRemoved(QString)));

#elif defined(Q_OS_MAC)
        connect(UsbMonitor_mac::Instance(), SIGNAL(usbDeviceAdded(QString)), this, SLOT(usbDeviceAdded(QString)));
        connect(UsbMonitor_mac::Instance(), SIGNAL(usbDeviceRemoved(QString)), this, SLOT(usbDeviceRemoved(QString)));

#elif defined(Q_OS_LINUX)
        connect(UsbMonitor_linux::Instance(), SIGNAL(usbDeviceAdded(QString, bool, bool)), this, SLOT(usbDeviceAdded(QString, bool, bool)), Qt::QueuedConnection);
        connect(UsbMonitor_linux::Instance(), SIGNAL(usbDeviceRemoved(QString)), this, SLOT(usbDeviceRemoved(QString)), Qt::QueuedConnection);
#endif
    }

    bool startUsbCheck = true;
#if defined(Q_OS_MAC)
    //This is not needed on osx, the list is updated at startup by UsbMonitor
    //except the emulation mode is enabled
    if (!AppDaemon::isEmulationMode())
        startUsbCheck = false;
#endif
    if (startUsbCheck)
        checkUsbDevices();

    return true;
}

MPManager::~MPManager()
{
#if defined(Q_OS_LINUX)
    stop();
#endif
}

void MPManager::stop()
{
    //Clear all devices
    auto it = devices.begin();
    while (it != devices.end())
    {
        emit mpDisconnected(it.value());
        delete it.value();
        devices.remove(it.key());
        it = devices.begin();
    }
}

void MPManager::usbDeviceAdded()
{
    checkUsbDevices();
}

void MPManager::usbDeviceRemoved()
{
    checkUsbDevices();
}

void MPManager::usbDeviceAdded(QString path)
{
#if defined(Q_OS_LINUX)
    Q_UNUSED(path);
    return;
#endif
    if (devices.empty())
    {
        MPDevice *device = nullptr;
#if defined(Q_OS_WIN)
        //Create our platform device object
        bool isBLE = false;
        bool isBluetooth = false;
        if (!MPDevice_win::checkDevice(path, isBLE, isBluetooth))
        {
            qDebug() << "Not a mooltipass device: " << path;
            return;
        }

        if (isBluetooth && isBLEConnectedWithUsb())
        {
            qDebug() << "BLE is already connected with usb";
            return;
        }

        device = new MPDevice_win(this, MPDevice_win::getPlatDef(path, isBLE, isBluetooth));
#elif defined(Q_OS_MAC)
        device = new MPDevice_mac(this, MPDevice_mac::getPlatDef(path));
#endif
        devices[path] = device;
        emit mpConnected(device);
    }
    else
    {
        qDebug() << "Device is already added: " << path;
    }
}

#if defined(Q_OS_LINUX)
void MPManager::usbDeviceAdded(QString path, bool isBLE, bool isBT)
{
    if (devices.empty())
    {
        if (isBLE && isBLEConnectedWithUsb())
        {
            qDebug() << "BLE is already connected with usb";
            return;
        }
        MPDevice *device = nullptr;
        //Create our platform device object
        MPPlatformDef def;
        def.path = path;
        def.id = path;
        def.isBLE = isBLE;
        def.isBluetooth = isBT;
        device = new MPDevice_linux(this, def);

        devices[path] = device;
        emit mpConnected(device);
    }
    else
    {
        qDebug() << "Device is already added: " << path;
    }
}
#endif

void MPManager::usbDeviceRemoved(QString path)
{
    auto it = devices.find(path);
    if (it != devices.end())
    {
        qDebug() << "Disconnecting: " << path;
        emit mpDisconnected(it.value());
        delete it.value();
        devices.remove(path);
    }
    else
    {
        qDebug() << path << " is not connected.";
    }
    checkUsbDevices();
}

MPDevice* MPManager::getDevice(int at)
{
    if (at < 0 || at >= devices.count())
    {
        qCritical() << "Invalid device index: " << at;
        return nullptr;
    }
    return devices.values().at(at);
}

void MPManager::checkUsbDevices()
{
    // discover devices
    QList<MPPlatformDef> devlist;
    QList<QString> detectedDevs;

    qDebug() << "List usb devices...";

    if (!AppDaemon::isEmulationMode())
    {
#if defined(Q_OS_WIN)
        devlist = MPDevice_win::enumerateDevices();
#elif defined(Q_OS_MAC)
        devlist = MPDevice_mac::enumerateDevices();
#elif defined(Q_OS_LINUX)
        devlist = MPDevice_linux::enumerateDevices();
#endif
    }

    if (devlist.isEmpty() && !AppDaemon::isEmulationMode())
    {
        //No USB devices found, means all MPs are gone disconnected
        qDebug() << "Disconnecting devices";
        auto it = devices.begin();
        while (it != devices.end())
        {
            emit mpDisconnected(it.value());
            delete it.value();
            it++;
        }
        devices.clear();
        return;
    }

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    //Remove bt connection if usb is connected too
    if (devlist.size() > 1)
    {
        auto it = std::remove_if(devlist.begin(), devlist.end(),
                               [](MPPlatformDef def){return def.isBluetooth;});
        if (it != devlist.end())
        {
            devlist.erase(it);
        }
    }
#endif

    if (AppDaemon::isEmulationMode())
    {
        MPDevice *device;
        device = new MPDevice_emul(this);
        detectedDevs.append("EMULDEVICE_ID");
        devices["EMULDEVICE_ID"] = device;
        emit mpConnected(device);
    }
    else
    {
        for (const MPPlatformDef &def : devlist)
        {
            //This is a new connected mooltipass
            if (!devices.contains(def.id))
            {
                MPDevice *device;

                //Create our platform device object
#if defined(Q_OS_WIN)
                device = new MPDevice_win(this, def);
#elif defined(Q_OS_MAC)
                device = new MPDevice_mac(this, def);
#elif defined(Q_OS_LINUX)
                device = new MPDevice_linux(this, def);
#endif

                devices[def.id] = device;
                emit mpConnected(device);
            }
            else
            {
                qDebug() << "Device is already connected: " << def.id;
            }
            detectedDevs.append(def.id);
        }
    }
    //Clear disconnected devices
    auto it = devices.begin();
    while (it != devices.end())
    {
        if (!detectedDevs.contains(it.key()))
        {
            emit mpDisconnected(it.value());
            delete it.value();
            devices.remove(it.key());
            it = devices.begin();
        }
        else
            it++;
    }
}

bool MPManager::isBLEConnectedWithUsb()
{
    return std::find_if(devices.begin(), devices.end(),
              [](MPDevice * dev){ return dev->isBLE();})
            != devices.end();
}
