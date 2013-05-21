/*
 * FBGrabber.cpp
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

#include "FBGrabber.hpp"

#ifdef FB_GRAB_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>

#include "debug.h"
#include <linux/fb.h> /* to handle framebuffer ioctls */

#define MAX_LEN 512

inline size_t getBytesPerPixel(const int bbp)
{
   return (bbp + 7) >> 3;
}

inline size_t getBufSize(const struct fb_var_screeninfo *fb_info)
{
    return fb_info->xres * fb_info->yres * getBytesPerPixel(fb_info->bits_per_pixel);
}

#define _565R(b,x) (b[x+1] & 0xf8)
#define _565G(b,x) ((((b[x+1] << 3)&0x38) | ((b[x] >> 5) & 0x07))<<2)
#define _565B(b,x) ((b[x] & 0x1f) << 3)

QRgb getColor(const unsigned char *buf, struct fb_var_screeninfo &fb_info, const QRect &rect)
{

    DEBUG_HIGH_LEVEL << Q_FUNC_INFO
                     << "x y w h:" << rect.x() << rect.y() << rect.width() << rect.height();
    int x = rect.x();
    int y = rect.y();
    int width = rect.width();
    int height = rect.height();

    if (buf == NULL)
    {
        qCritical() << Q_FUNC_INFO << "m_buf == NULL";
        return 0;
    }

    // Ignore part of LED widget which out of screen
    if( x < 0 ) {
        width += x;  /* reduce width  */
        x = 0;
    }
    if( y < 0 ) {
        height += y;  /* reduce height */
        y = 0;
    }
    unsigned screenWidth = fb_info.xres;
    unsigned screenHeight = fb_info.yres;

    if( x + width  > screenWidth  ) width  -= (x + width ) - screenWidth;
    if( y + height > screenHeight ) height -= (y + height) - screenHeight;

    //calculate aligned width (align by 4 pixels)
    width = width - (width % 4);

    if(width < 0 || height < 0){
        qWarning() << Q_FUNC_INFO << "width < 0 || height < 0:" << width << height;

        // width and height can't be negative
        return 0x000000;
    }


    unsigned long count = 0; // count the amount of pixels taken into account
    const unsigned char bytes_per_pixel = getBytesPerPixel(fb_info.bits_per_pixel);
    unsigned long r = 0, g = 0, b = 0;
//    qWarning() << "bbp = " << fb_info.bits_per_pixel << " bytesPerPixel = " << bytes_per_pixel;
    switch (fb_info.bits_per_pixel) {
        case 16:
            for (int k = y; k < y + height; k++) {
                register unsigned long index = (screenWidth * k + x) * bytes_per_pixel; // index of the selected pixel in pbPixelsBuff
                for(int i = 0; i < width; i += 4) {
                    b += _565B(buf,index) + _565B(buf,index+2) + _565B(buf,index+4) + _565B(buf,index+6);
                    g += _565G(buf,index) + _565G(buf,index+2) + _565G(buf,index+4) + _565G(buf,index+6);
                    r += _565R(buf,index) + _565R(buf,index+2) + _565R(buf,index+4) + _565R(buf,index+6);

                    count += 4;
                    index += bytes_per_pixel * 4;
                }
            }
            break;

/*        case 32:
            for (int k = y; k < y + height; k++) {
                register unsigned long index = (screenWidth * k + x) * bytes_per_pixel; // index of the selected pixel in pbPixelsBuff
                for(int i = 0; i < width; i += 4) {
                    b += buf[index]     + buf[index + 4] + buf[index + 8 ] + buf[index + 12];
                    g += buf[index + 1] + buf[index + 5] + buf[index + 9 ] + buf[index + 13];
                    r += buf[index + 2] + buf[index + 6] + buf[index + 10] + buf[index + 14];

                    count+=4;
                    index += bytes_per_pixel * 4;
                }
            }
            break;
*/
        default:
            qCritical() << "unsupported bitsPerPixel amount: " << fb_info.bits_per_pixel;
    }

    if( count != 0 ){
        r = (unsigned)round((double) r / count) & 0xff;
        g = (unsigned)round((double) g / count) & 0xff;
        b = (unsigned)round((double) b / count) & 0xff;
    }

    QRgb result = qRgb(r, g, b);

    DEBUG_HIGH_LEVEL << Q_FUNC_INFO << "QRgb result =" << hex << result;

    return result;
}

FBGrabberDataProvider::FBGrabberDataProvider(const char *deviceFileName) : FBGrabberDataProviderTrait()
{
    m_deviceFileName = const_cast<char *>(deviceFileName);
    m_fd = 0;
}

int FBGrabberDataProvider::openDevice()
{
    if(m_fd != 0) {
        return -2;
    }
    if(-1 == (m_fd=open(m_deviceFileName, O_RDONLY))) {
        fprintf (stderr, "Error: Couldn't open %s.\n", m_deviceFileName);
        return -1;
    }
    return 0;
}

void FBGrabberDataProvider::closeDevice()
{
    if(m_fd) {
        close(m_fd);
        m_fd=0;
    }
}

fb_var_screeninfo * FBGrabberDataProvider::readFbScreenInfo(fb_var_screeninfo *pFbScreenInfo)
{

    if (ioctl(m_fd, FBIOGET_VSCREENINFO, pFbScreenInfo) != 0) {
        return NULL;
        qCritical() << ("ioctl FBIOGET_VSCREENINFO failed");
    }

    DEBUG_HIGH_LEVEL << Q_FUNC_INFO << "bbp: " << pFbScreenInfo->bits_per_pixel;
    DEBUG_HIGH_LEVEL << Q_FUNC_INFO << "resolution: " << pFbScreenInfo->xres << "x" << pFbScreenInfo->yres;
    DEBUG_HIGH_LEVEL << Q_FUNC_INFO << "virtual resolution: " << pFbScreenInfo->xres_virtual << "x" << pFbScreenInfo->yres_virtual;

    return pFbScreenInfo;
}

unsigned char * FBGrabberDataProvider::readFbData(unsigned char *pBuf, size_t bytesToRead)
{
    int res = read(m_fd, pBuf, bytesToRead);
    if (res != (ssize_t) bytesToRead) {
        qCritical() << ("Error: Not enough memory or data: \n") << res << " < " << bytesToRead;
        return NULL;
    }
    return pBuf;
}

FBGrabber::FBGrabber(QObject *parent, QList<QRgb> *grabResult, QList<GrabWidget *> *grabAreasGeometry, FBGrabberDataProviderTrait *dataProvider) : TimeredGrabber(parent, grabResult, grabAreasGeometry)
{
    m_dataProvider = dataProvider;
    m_bufSize = 0;
    m_buf = 0;
}

void FBGrabber::init()
{
    TimeredGrabber::init();
}

FBGrabber::~FBGrabber()
{
    if(m_buf)
        free(m_buf);
    if(m_dataProvider)
        delete m_dataProvider;
}

const char* FBGrabber::getName()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;
    return "FBGrabber";
}

void FBGrabber::updateGrabMonitor(QWidget *widget)
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO;
}

GrabResult FBGrabber::_grab()
{
    DEBUG_HIGH_LEVEL << Q_FUNC_INFO;

    GrabResult result = GrabResultError;

    /* now open framebuffer device */
    if(m_dataProvider->openDevice() == 0)
    {

        fb_var_screeninfo fb_info;
        if (m_dataProvider->readFbScreenInfo(&fb_info)) {
            if (m_bufSize != getBufSize(&fb_info)) {
                m_bufSize = getBufSize(&fb_info);
                if (m_buf) {
                    free(m_buf);
                    m_buf = NULL;
                }
                m_buf = (unsigned char *) malloc(m_bufSize);
                memset(m_buf, 0, m_bufSize);

                DEBUG_MID_LEVEL << "new buffer size: "<< m_bufSize;
            }

            if(m_dataProvider->readFbData(m_buf, m_bufSize)) {
                fprintf(stderr, "got fb: 0x%x 0x%x...\n", m_buf[0], m_buf[1]);
                m_grabResult->clear();
                foreach(GrabWidget * widget, *m_grabWidgets) {
                    m_grabResult->append( widget->isAreaEnabled() ? getColor(m_buf, fb_info, widget->geometry()) : qRgb(0,0,0) );
                }
                result = GrabResultOk;
            }

        }
    }

    m_dataProvider->closeDevice();

    return result;
}
#endif
