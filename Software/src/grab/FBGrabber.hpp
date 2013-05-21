/*
 * FBGrabber.hpp
 *
 *  Created on: 5/16/2013
 *     Project: Prismatik 
 *
 *  Copyright (c) 2013 Timur Sattarov
 *
 *  Lightpack is an open-source, USB content-driving ambient lighting
 *  hardware.
 *
 *  Prismatik is a free, open-source software: you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as published 
 *  by the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Prismatik and Lightpack files is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../common/defs.h"
#ifdef FB_GRAB_SUPPORT
#ifndef FBGRABBER_HPP
#define FBGRABBER_HPP

#include "TimeredGrabber.hpp"
#include <linux/fb.h>

#define	DEFAULT_FB	"/dev/fb0\0"

class FBGrabberDataProviderTrait
{
public:
    virtual ~FBGrabberDataProviderTrait(){}
    virtual int openDevice() = 0;
    virtual fb_var_screeninfo * readFbScreenInfo(fb_var_screeninfo *pFbScreenInfo) = 0;
    virtual unsigned char * readFbData(unsigned char *pBuf, size_t bytesToRead) = 0;
    virtual void closeDevice() = 0;
};

class FBGrabberDataProvider : public FBGrabberDataProviderTrait
{
public:
    FBGrabberDataProvider():m_fd(0), m_deviceFileName(DEFAULT_FB){}
    FBGrabberDataProvider(const char *deviceFileName);
    ~FBGrabberDataProvider(){}

    virtual int openDevice();
    virtual fb_var_screeninfo * readFbScreenInfo(fb_var_screeninfo *pFbScreenInfo);
    virtual unsigned char * readFbData(unsigned char *pBuf, size_t bytesToRead);
    virtual void closeDevice();
private:
    char *m_deviceFileName;
    int m_fd;
};

class FBGrabber : public TimeredGrabber
{
public:
    FBGrabber(QObject *parent, QList<QRgb> *grabResult, QList<GrabWidget *> *grabAreasGeometry, FBGrabberDataProviderTrait *dataProvider);
    ~FBGrabber();
    virtual void init();
    virtual const char * getName();
    virtual void updateGrabMonitor( QWidget * widget );

protected:
    virtual GrabResult _grab();

private:
    FBGrabberDataProviderTrait *m_dataProvider;
    unsigned char *m_buf;
    size_t m_bufSize;


};

#endif // FBGRABBER_HPP
#endif
