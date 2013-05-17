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

/*
 * fbgrab - takes screenshots using the framebuffer.
 *
 * (C) Gunnar Monell <gmo@linux.nu> 2002
 *
 * This program is free Software, see the COPYING file
 * and is based on Stephan Beyer's <fbshot@s-beyer.de> FBShot
 * (C) 2000.
 *
 * For features and differences, read the manual page.
 *
 * This program has been checked with "splint +posixlib" without
 * warnings. Splint is available from http://www.splint.org/ .
 * Patches and enhancements of fbgrab have to fulfill this too.
 */

#ifdef FB_GRAB_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>

#include "debug.h"
#include <linux/fb.h> /* to handle framebuffer ioctls */

#define	DEFAULT_FB	"/dev/fb0"
#define MAX_LEN 512

void getFrameBufferData(int fd, struct fb_var_screeninfo *fb_varinfo_p)
{

    if (ioctl(fd, FBIOGET_VSCREENINFO, fb_varinfo_p) != 0)
        qCritical() << ("ioctl FBIOGET_VSCREENINFO");
}

inline size_t getBytesPerPixel(const int bbp)
{
   return (bbp + 7) >> 3;
}

inline size_t getBufSize(const struct fb_var_screeninfo *fb_info)
{
    return fb_info->xres * fb_info->yres * getBytesPerPixel(fb_info->bits_per_pixel);
}

inline int min(int a, int b)
{
    return a < b ? a : b;
}

inline int max(int a, int b)
{
    return a > b ? a : b;
}

void readFrameBuffer(const int fd, size_t bytes, unsigned char *buf_p)
{
    int res = read(fd, buf_p, bytes);
    if (res != (ssize_t) bytes)
        qCritical() << ("Error: Not enough memory or data: \n") << res << " < " << bytes;
}

void convert1555to32(int width, int height,
                unsigned char *inbuffer,
                unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width*2; i+=2)
    {
    /* BLUE  = 0 */
    outbuffer[(i<<1)+0] = (inbuffer[i+1] & 0x7C) << 1;
    /* GREEN = 1 */
        outbuffer[(i<<1)+1] = (((inbuffer[i+1] & 0x3) << 3) |
                 ((inbuffer[i] & 0xE0) >> 5)) << 3;
    /* RED   = 2 */
    outbuffer[(i<<1)+2] = (inbuffer[i] & 0x1f) << 3;
    /* ALPHA = 3 */
    outbuffer[(i<<1)+3] = '\0';
    }
}

#define _565R(b,x) (b[x+1] & 0xf8 << 3)
#define _565G(b,x) ((b[x+1] & 0x07 << 3) | (b[x] & 0xE0) >> 5) << 2
#define _565B(b,x) (b[x] & 0x1f << 3)

QRgb getColor(const unsigned char *buf, const struct fb_var_screeninfo &fb_info, const QRect &rect)
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
    if( rect.x() < 0 ) {
        width  += x;  /* reduce width  */
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

    unsigned count = 0; // count the amount of pixels taken into account
    unsigned bytes_per_pixel = getBytesPerPixel(fb_info.bits_per_pixel);
    unsigned endIndex = (screenWidth * (y + height) + x + width) * bytes_per_pixel;
    register unsigned index = (screenWidth * y + x) * bytes_per_pixel; // index of the selected pixel in pbPixelsBuff
    register unsigned r = 0, g = 0, b = 0;
    switch (fb_info.bits_per_pixel) {
        case 16:
            while (index < endIndex - width * bytes_per_pixel) {
                for(int i = 0; i < width; i += 4) {
                    b += _565B(buf,index) + _565B(buf,index+2) + _565B(buf,index+4) + _565B(buf,index+6);
                    g += _565G(buf,index) + _565G(buf,index+2) + _565G(buf,index+4) + _565G(buf,index+6);
                    r += _565R(buf,index) + _565R(buf,index+2) + _565R(buf,index+4) + _565R(buf,index+6);

                    count+=4;
                    index += bytes_per_pixel * 4;
                }

                index += (screenWidth - width) * bytes_per_pixel;
            }
            break;

        case 32:
            while (index < endIndex - width * bytes_per_pixel) {
                for(int i = 0; i < width; i += 4) {
                    b += buf[index]     + buf[index + 4] + buf[index + 8 ] + buf[index + 12];
                    g += buf[index + 1] + buf[index + 5] + buf[index + 9 ] + buf[index + 13];
                    r += buf[index + 2] + buf[index + 6] + buf[index + 10] + buf[index + 14];

                    count+=4;
                    index += bytes_per_pixel * 4;
                }

                index += (screenWidth - width) * bytes_per_pixel;
            }
            break;
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

FBGrabber::FBGrabber(QObject *parent, QList<QRgb> *grabResult, QList<GrabWidget *> *grabAreasGeometry) : TimeredGrabber(parent, grabResult, grabAreasGeometry)
{
    m_bufSize = 0;
    m_buf = 0;
    m_fd = 0;
    if (NULL == m_device) {
        m_device = getenv("FRAMEBUFFER");
        if (NULL == m_device) {
            m_device = DEFAULT_FB;
        }
    }
}

void FBGrabber::init()
{
    if(m_fd)
        return;
    TimeredGrabber::init();
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;
    /* now open framebuffer device */
    if(-1 == (m_fd=open(m_device, O_RDONLY)))
    {
        fprintf (stderr, "Error: Couldn't open %s.\n", m_device);
    }

//    getFrameBufferData(m_fd, &m_fb_info);

//    m_bufSize = getBufSize( m_fb_info);

//    m_buf = (unsigned char *)malloc(m_bufSize);

//    if(m_buf == NULL)
//        qCritical() << ("Not enough memory");

}

FBGrabber::~FBGrabber()
{
    if(m_fd)
        close(m_fd);
    if(m_buf)
        free(m_buf);
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
    getFrameBufferData(m_fd, &m_fb_info);

    DEBUG_HIGH_LEVEL << Q_FUNC_INFO << "bbp: " << m_fb_info.bits_per_pixel;
    if (m_bufSize != getBufSize(&m_fb_info)) {
        m_bufSize = getBufSize(&m_fb_info);
        if (m_buf) {
            free(m_buf);
            m_buf = NULL;
        }
        m_buf = (unsigned char *) malloc(m_bufSize);
        memset(m_buf, 0, m_bufSize);

        DEBUG_HIGH_LEVEL << "new buffer size: "<< m_bufSize;
    }

    readFrameBuffer(m_fd, m_bufSize, m_buf);

    m_grabResult->clear();
    foreach(GrabWidget * widget, *m_grabWidgets) {
        m_grabResult->append( widget->isAreaEnabled() ? getColor(m_buf,m_fb_info,widget->geometry()) : qRgb(0,0,0) );
    }

    return GrabResultOk;
}
#endif
