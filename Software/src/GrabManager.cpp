/*
 * GrabManager.cpp
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

#include "GrabManager.hpp"
#include <QtCore/qmath.h>
#include "debug.h"
#include "PrismatikMath.hpp"
#include "LightpackApplication.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include "GrabberContext.hpp"
using namespace SettingsScope;

#if defined _MSC_VER
using PrismatikMath::round;
#endif

#ifdef D3D10_GRAB_SUPPORT
void *GetMainWindowHandle()
{
    return getLightpackApp()->getMainWindowHandle();
}
#endif

GrabManager::GrabManager(QWidget *parent) : QObject(parent)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    qRegisterMetaType<GrabResult>("GrabResult");

    m_parentWidget = parent;

    m_timerGrab = new QTimer(this);
    m_timeEval = new TimeEvaluations();

    m_fpsMs = 0;

    m_grabberContext = new GrabberContext();

    m_isSendDataOnlyIfColorsChanged = Settings::isSendDataOnlyIfColorsChanges();

//    m_grabbersThread = new QThread();
    initGrabbers();
    m_grabber = queryGrabber(Settings::getGrabberType());

    m_timerUpdateFPS = new QTimer(this);
    connect(m_timerUpdateFPS, SIGNAL(timeout()), this, SLOT(timeoutUpdateFPS()));
    m_timerUpdateFPS->setSingleShot(false);
    m_timerUpdateFPS->start(500);

    m_isPauseGrabWhileResizeOrMoving = false;
    m_isGrabWidgetsVisible = false;

    initColorLists(MaximumNumberOfLeds::Default);
    initLedWidgets(MaximumNumberOfLeds::Default);

//    connect(m_timerGrab, SIGNAL(timeout()), this, SLOT(handleGrabbedColors()));
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(scaleLedWidgets(int)));
    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), this, SLOT(onScreenCountChanged(int)));

    updateScreenGeometry();

    settingsProfileChanged(Settings::getCurrentProfileName());


    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "initialized";
}

GrabManager::~GrabManager()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    delete m_timerGrab;
    delete m_timeEval;
    m_grabber = NULL;

    for (int i = 0; i < m_ledWidgets.size(); i++)
    {
        delete m_ledWidgets[i];
    }

    m_ledWidgets.clear();

    for (int i = 0; i < m_grabbers.size(); i++)
        if (m_grabbers[i]){
            DEBUG_OUT << "deleting " << m_grabbers[i]->name();
            delete m_grabbers[i];
            m_grabbers[i] = NULL;
        }

    m_grabbers.clear();

#ifdef D3D10_GRAB_SUPPORT
    delete m_d3d10Grabber;
    m_d3d10Grabber = NULL;
#endif

    delete m_grabberContext;
}

void GrabManager::start(bool isGrabEnabled)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << isGrabEnabled;

    clearColorsNew();

    if (m_grabber != NULL) {
        if (isGrabEnabled) {
            m_timerUpdateFPS->start();
            m_grabber->startGrabbing();
        } else {
            clearColorsCurrent();
            m_timerUpdateFPS->stop();
            m_grabber->stopGrabbing();
            emit ambilightTimeOfUpdatingColors(0);
        }
    }
}

void GrabManager::onGrabberTypeChanged(const Grab::GrabberType grabberType)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << grabberType;

    bool isStartNeeded = false;
    if (m_grabber != NULL) {
        isStartNeeded = m_grabber->isGrabbingStarted();
#ifdef D3D10_GRAB_SUPPORT
        isStartNeeded = isStartNeeded || (m_d3d10Grabber != NULL && m_d3d10Grabber->isGrabbingStarted());
#endif
        m_grabber->stopGrabbing();
    }

    m_grabber = queryGrabber(grabberType);

    if (isStartNeeded) {
#ifdef D3D10_GRAB_SUPPORT
        if (Settings::isDx1011GrabberEnabled())
            m_d3d10Grabber->startGrabbing();
        else
            m_grabber->startGrabbing();
#else
        m_grabber->startGrabbing();
#endif
    }
}

void GrabManager::onGrabberStateChangeRequested(bool isStartRequested) {
#ifdef D3D10_GRAB_SUPPORT
    D3D10Grabber *grabber = static_cast<D3D10Grabber *>(sender());
    if (grabber != m_grabber) {
        if (isStartRequested) {
            if (Settings::isDx1011GrabberEnabled()) {
                m_grabber->stopGrabbing();
                grabber->startGrabbing();
            }
        } else {
            m_grabber->startGrabbing();
            grabber->stopGrabbing();
        }
    } else {
        qCritical() << Q_FUNC_INFO << " there is no grabber to take control by some reason";
    }
#else
    Q_UNUSED(isStartRequested)
#endif
}

void GrabManager::onGrabSlowdownChanged(int ms)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << ms;
    if (m_grabber)
        m_grabber->setGrabInterval(ms);
    else
        qWarning() << Q_FUNC_INFO << "trying to change grab slowdown while there is no grabber";
}

void GrabManager::onGrabAvgColorsEnabledChanged(bool state)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << state;
    m_avgColorsOnAllLeds = state;
}

void GrabManager::onSendDataOnlyIfColorsEnabledChanged(bool state)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << state;
    m_isSendDataOnlyIfColorsChanged = state;
}

void GrabManager::setNumberOfLeds(int numberOfLeds)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << numberOfLeds;

    initColorLists(numberOfLeds);
    initLedWidgets(numberOfLeds);

    for (int i = 0; i < m_ledWidgets.size(); i++)
    {
        m_ledWidgets[i]->settingsProfileChanged();
        m_ledWidgets[i]->setVisible(m_isGrabWidgetsVisible);
    }
}

void GrabManager::reset()
{
    clearColorsCurrent();
}

void GrabManager::settingsProfileChanged(const QString &profileName)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;
    Q_UNUSED(profileName)

    m_isSendDataOnlyIfColorsChanged = Settings::isSendDataOnlyIfColorsChanges();
    m_avgColorsOnAllLeds = Settings::isGrabAvgColorsEnabled();

    setNumberOfLeds(Settings::getNumberOfLeds(Settings::getConnectedDevice()));
}

void GrabManager::setVisibleLedWidgets(bool state)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << state;

    m_isGrabWidgetsVisible = state;

    for (int i = 0; i < m_ledWidgets.size(); i++)
    {
        if (state)
        {
            m_ledWidgets[i]->show();
        } else {
            m_ledWidgets[i]->hide();
        }
    }
}

void GrabManager::setColoredLedWidgets(bool state)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    // This slot is directly connected to radioButton toggled(bool) signal
    if (state)
    {
        for (int i = 0; i < m_ledWidgets.size(); i++)
            m_ledWidgets[i]->fillBackgroundColored();
    }
}

void GrabManager::setWhiteLedWidgets(bool state)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    // This slot is directly connected to radioButton toggled(bool) signal
    if (state)
    {
        for (int i = 0; i < m_ledWidgets.size(); i++)
            m_ledWidgets[i]->fillBackgroundWhite();
    }
}

void GrabManager::handleGrabbedColors()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    if (m_grabber == NULL)
    {
        qCritical() << Q_FUNC_INFO << "m_grabber == NULL";
        return;
    }

    // Temporary switch off updating colors
    // if one of LED widgets resizing or moving
    if (m_isPauseGrabWhileResizeOrMoving)
    {
        m_timerGrab->start(50); // check in 50 ms
        return;
    }    

    bool isColorsChanged = false;

    int avgR = 0, avgG = 0, avgB = 0;
    int countGrabEnabled = 0;

    if (m_avgColorsOnAllLeds)
    {
        for (int i = 0; i < m_ledWidgets.size(); i++)
        {
            if (m_ledWidgets[i]->isAreaEnabled())
            {
                    avgR += qRed(m_colorsNew[i]);
                    avgG += qGreen(m_colorsNew[i]);
                    avgB += qBlue(m_colorsNew[i]);
                    countGrabEnabled++;
            }
        }
        if (countGrabEnabled != 0)
        {
            avgR /= countGrabEnabled;
            avgG /= countGrabEnabled;
            avgB /= countGrabEnabled;
        }
        // Set one AVG color to all LEDs
        for (int ledIndex = 0; ledIndex < m_ledWidgets.size(); ledIndex++)
        {
            if (m_ledWidgets[ledIndex]->isAreaEnabled())
            {
                m_colorsNew[ledIndex] = qRgb(avgR, avgG, avgB);
            }
        }
    }

//    // White balance
//    for (int i = 0; i < m_ledWidgets.size(); i++)
//    {
//        QRgb rgb = m_colorsNew[i];

//        unsigned r = qRed(rgb)   * m_ledWidgets[i]->getCoefRed();
//        unsigned g = qGreen(rgb) * m_ledWidgets[i]->getCoefGreen();
//        unsigned b = qBlue(rgb)  * m_ledWidgets[i]->getCoefBlue();

//        if (r > 0xff) r = 0xff;
//        if (g > 0xff) g = 0xff;
//        if (b > 0xff) b = 0xff;

//        m_colorsNew[i] = qRgb(r, g, b);
//    }

    for (int i = 0; i < m_ledWidgets.size(); i++)
    {
        if (m_colorsCurrent[i] != m_colorsNew[i])
        {
            m_colorsCurrent[i] = m_colorsNew[i];
            isColorsChanged = true;
        }
    }

    if ((m_isSendDataOnlyIfColorsChanged == false) || isColorsChanged)
    {
        emit updateLedsColors(m_colorsCurrent);
    }

    m_fpsMs = m_timeEval->howLongItEnd();
    m_timeEval->howLongItStart();

}

void GrabManager::timeoutUpdateFPS()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;
    emit ambilightTimeOfUpdatingColors(m_fpsMs);
}

void GrabManager::pauseWhileResizeOrMoving()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;
    m_isPauseGrabWhileResizeOrMoving = true;
}

void GrabManager::resumeAfterResizeOrMoving()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;
    m_isPauseGrabWhileResizeOrMoving = false;
}

void GrabManager::updateScreenGeometry()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;
    m_lastScreenGeometry.clear();
    for (int i = 0; i < QApplication::desktop()->screenCount(); ++i) {
        m_lastScreenGeometry.append(QApplication::desktop()->screenGeometry(i));
    }
    emit changeScreen();
    if (m_grabber == NULL)
    {
        qCritical() << Q_FUNC_INFO << "m_grabber == NULL";
        return;
    }

}

void GrabManager::onScreenCountChanged(int)
{
    updateScreenGeometry();
}

void GrabManager::scaleLedWidgets(int screenIndexResized)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "screenIndexResized:" << screenIndexResized;

    QRect screenGeometry = QApplication::desktop()->screenGeometry(screenIndexResized);
    QRect lastScreenGeometry = m_lastScreenGeometry[screenIndexResized];

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "screen " << screenIndexResized << " is resized to " << screenGeometry;
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "was " << lastScreenGeometry;

    // Move LedWidgets
    int deltaX = lastScreenGeometry.x() - screenGeometry.x();
    int deltaY = lastScreenGeometry.y() - screenGeometry.y();

    double scaleX = (double) screenGeometry.width() / lastScreenGeometry.width();
    double scaleY = (double) screenGeometry.height() / lastScreenGeometry.height();

    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "deltaX =" << deltaX << "deltaY =" << deltaY;
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "scaleX =" << scaleX << "scaleY =" << scaleY;

    m_lastScreenGeometry[screenIndexResized] = screenGeometry;

    for(int i=0; i < m_ledWidgets.size(); i++){

        if (!lastScreenGeometry.contains(m_ledWidgets[i]->geometry().center()))
            continue;

        int width  = round(scaleX * m_ledWidgets[i]->width());
        int height = round(scaleY * m_ledWidgets[i]->height());

        int x = m_ledWidgets[i]->x();
        int y = m_ledWidgets[i]->y();

        x -= screenGeometry.x();
        y -= screenGeometry.y();

        x = round(scaleX * x);
        y = round(scaleY * y);

        x += screenGeometry.x();
        y += screenGeometry.y();

        x -= deltaX;
        y -= deltaY;

        m_ledWidgets[i]->move(x,y);
        m_ledWidgets[i]->resize(width, height);

        m_ledWidgets[i]->saveSizeAndPosition();

        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "new values [" << i << "]" << "x =" << x << "y =" << y << "w =" << width << "h =" << height;
    }

}

void GrabManager::initGrabbers()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    m_grabberContext->grabWidgets = &m_ledWidgets;
    m_grabberContext->grabResult = &m_colorsNew;

    for (int i = 0; i < Grab::GrabbersCount; i++)
        m_grabbers.append(NULL);

#ifdef WINAPI_GRAB_SUPPORT
    m_grabbers[Grab::GrabberTypeWinAPI] = initGrabber(new WinAPIGrabber(NULL, m_grabberContext));
#endif

#ifdef D3D9_GRAB_SUPPORT
    m_grabbers[Grab::GrabberTypeD3D9] = initGrabber(new D3D9Grabber(NULL, m_grabberContext));
#endif

#ifdef X11_GRAB_SUPPORT
    m_grabbers[Grab::GrabberTypeX11] = initGrabber(new X11Grabber(NULL, m_grabberContext));
#endif

#ifdef MAC_OS_CG_GRAB_SUPPORT
    m_grabbers[Grab::GrabberTypeMacCoreGraphics] = initGrabber(new MacOSGrabber(NULL, m_grabberContext));
#endif
#ifdef QT_GRAB_SUPPORT
    //TODO: migrate Qt grabbers to the new hierarchy
    m_grabbers[Grab::GrabberTypeQtEachWidget] = initGrabber(new QtGrabberEachWidget(NULL, m_grabberContext));
    m_grabbers[Grab::GrabberTypeQt] = initGrabber(new QtGrabber(NULL, m_grabberContext));
#endif
#ifdef WINAPI_EACH_GRAB_SUPPORT
    m_grabbers[Grab::GrabberTypeWinAPIEachWidget] = initGrabber(new WinAPIGrabberEachWidget(NULL, m_grabberContext));
#endif
#ifdef D3D10_GRAB_SUPPORT
    m_d3d10Grabber = static_cast<D3D10Grabber *>(initGrabber(new D3D10Grabber(NULL, m_grabberContext, &GetMainWindowHandle)));
    connect(m_d3d10Grabber, SIGNAL(grabberStateChangeRequested(bool)), SLOT(onGrabberStateChangeRequested(bool)));
    connect(getLightpackApp(), SIGNAL(postInitialization()), m_d3d10Grabber,  SLOT(init()));
#endif
}

GrabberBase *GrabManager::initGrabber(GrabberBase * grabber) {
    QMetaObject::invokeMethod(grabber, "setGrabInterval", Qt::QueuedConnection, Q_ARG(int, Settings::getGrabSlowdown()));
//    QMetaObject::invokeMethod(grabber, "startGrabbing", Qt::QueuedConnection);
    bool isConnected = connect(grabber, SIGNAL(frameGrabAttempted(GrabResult)), this, SLOT(onFrameGrabAttempted(GrabResult)), Qt::QueuedConnection);
    Q_ASSERT_X(isConnected, "connecting grabber to grabManager", "failed");

    return grabber;
}

GrabberBase *GrabManager::queryGrabber(Grab::GrabberType grabberType)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "grabberType:" << grabberType;
    GrabberBase *result;

    if (m_grabbers[grabberType] != NULL) {
        result = m_grabbers[grabberType];
    } else {
        qCritical() << Q_FUNC_INFO << "unsupported for the platform grabber type: " << grabberType << ", using QtGrabber";
        result = m_grabbers[Grab::GrabberTypeQt];
    }

    result->setGrabInterval(Settings::getGrabSlowdown());

    return result;
}

void GrabManager::onFrameGrabAttempted(GrabResult grabResult) {
    if (grabResult == GrabResultOk) {
        handleGrabbedColors();
    }
}

void GrabManager::initColorLists(int numberOfLeds)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << numberOfLeds;

    m_colorsCurrent.clear();
    m_colorsNew.clear();

    for (int i = 0; i < numberOfLeds; i++)
    {
        m_colorsCurrent << 0;
        m_colorsNew     << 0;
    }
}

void GrabManager::clearColorsNew()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    for (int i = 0; i < m_colorsNew.size(); i++)
    {
        m_colorsNew[i] = 0;
    }
}

void GrabManager::clearColorsCurrent()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;

    for (int i = 0; i < m_colorsCurrent.size(); i++)
    {
        m_colorsCurrent[i] = 0;
    }
}

void GrabManager::initLedWidgets(int numberOfLeds)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << numberOfLeds;

    if (m_ledWidgets.size() == 0)
    {
        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "First widget initialization";

        GrabWidget * ledWidget = new GrabWidget(m_ledWidgets.size(), m_parentWidget);

        connect(ledWidget, SIGNAL(resizeOrMoveStarted()), this, SLOT(pauseWhileResizeOrMoving()));
        connect(ledWidget, SIGNAL(resizeOrMoveCompleted(int)), this, SLOT(resumeAfterResizeOrMoving()));

// TODO: Check out this line!
//         First LED widget using to determine grabbing-monitor in WinAPI version of Grab
//        connect(ledWidget, SIGNAL(resizeOrMoveCompleted(int)), this, SLOT(firstWidgetPositionChanged()));

        m_ledWidgets << ledWidget;

//        firstWidgetPositionChanged();
    }

    int diff = numberOfLeds - m_ledWidgets.size();

    if (diff > 0)
    {
        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Append" << diff << "grab widgets";

        for (int i = 0; i < diff; i++)
        {
            GrabWidget * ledWidget = new GrabWidget(m_ledWidgets.size(), m_parentWidget);

            connect(ledWidget, SIGNAL(resizeOrMoveStarted()), this, SLOT(pauseWhileResizeOrMoving()));
            connect(ledWidget, SIGNAL(resizeOrMoveCompleted(int)), this, SLOT(resumeAfterResizeOrMoving()));

            m_ledWidgets << ledWidget;
        }
    } else {
        diff *= -1;
        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Remove last" << diff << "grab widgets";

        while (diff --> 0)
        {
            m_ledWidgets.last()->deleteLater();
            m_ledWidgets.removeLast();
        }
    }

    if (m_ledWidgets.size() != numberOfLeds)
        qCritical() << Q_FUNC_INFO << "Fail: m_ledWidgets.size()" << m_ledWidgets.size() << " != numberOfLeds" << numberOfLeds;
}
