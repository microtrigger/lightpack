/*
 * calculations.cpp
 *
 *  Created on: 23.07.2012
 *     Project: Lightpack
 *
 *  Copyright (c) 2012 Timur Sattarov
 *
 *  Lightpack a USB content-driving ambient lighting system
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

#include "calculations.hpp"

namespace Grab {
    namespace Calculations {

#define _565R(b,x) (b[x+1] & 0xf8)
#define _565G(b,x) ((((b[x+1] << 3) & 0x38) | ((b[x] >> 5) & 0x07)) << 2)
#define _565B(b,x) ((b[x] & 0x1f) << 3)

        int calculateAvgColor(QRgb *result, unsigned char *buffer, BufferFormat bufferFormat, unsigned int pitch, const QRect &rect ) {

            Q_ASSERT_X(rect.width() % 4 == 0, "average color calculation", "rect width should be aligned by 4 bytes");

            int count = 0; // count the amount of pixels taken into account
            register unsigned int r=0, g=0, b=0;
            const char kUnrollRate = 4;

            switch(bufferFormat) {
            case BufferFormatArgb: {

                const char kBytesPerPixel = 4;
                for(int currentY = 0; currentY < rect.height(); currentY++) {
                    int index = pitch * (rect.y()+currentY) + rect.x()*kBytesPerPixel;
                    for(int currentX = 0; currentX < rect.width(); currentX += kUnrollRate) {
                        b += buffer[index]   + buffer[index + 4] + buffer[index + 8 ] + buffer[index + 12];
                        g += buffer[index+1] + buffer[index + 5] + buffer[index + 9 ] + buffer[index + 13];
                        r += buffer[index+2] + buffer[index + 6] + buffer[index + 10] + buffer[index + 14];
                        count += kUnrollRate;
                        index += kBytesPerPixel * kUnrollRate;
                    }

                }
                break;
            }

            case BufferFormatAbgr: {
                const char kBytesPerPixel = 4;
                for(int currentY = 0; currentY < rect.height(); currentY++) {
                    int index = pitch * (rect.y()+currentY) + rect.x()*kBytesPerPixel;
                    for(int currentX = 0; currentX < rect.width(); currentX += kUnrollRate) {
                        r += buffer[index]   + buffer[index + 4] + buffer[index + 8 ] + buffer[index + 12];
                        g += buffer[index+1] + buffer[index + 5] + buffer[index + 9 ] + buffer[index + 13];
                        b += buffer[index+2] + buffer[index + 6] + buffer[index + 10] + buffer[index + 14];
                        count += kUnrollRate;
                        index += kBytesPerPixel * kUnrollRate;
                    }
                }
                break;
            }

            case BufferFormatRgba: {
                const char kBytesPerPixel = 4;
                for(int currentY = 0; currentY < rect.height(); currentY++) {
                    int index = pitch * (rect.y()+currentY) + rect.x()*kBytesPerPixel;
                    for(int currentX = 0; currentX < rect.width(); currentX += kUnrollRate) {
                        b += buffer[index+1] + buffer[index + 5] + buffer[index + 9 ] + buffer[index + 13];
                        g += buffer[index+2] + buffer[index + 6] + buffer[index + 10] + buffer[index + 14];
                        r += buffer[index+3] + buffer[index + 7] + buffer[index + 11] + buffer[index + 15];
                        count += kUnrollRate;
                        index += kBytesPerPixel * kUnrollRate;
                    }

                }
                break;
            }
            case BufferFormatBgra: {
                const char kBytesPerPixel = 4;
                for(int currentY = 0; currentY < rect.height(); currentY++) {
                    int index = pitch * (rect.y()+currentY) + rect.x()*kBytesPerPixel;
                    for(int currentX = 0; currentX < rect.width(); currentX += kUnrollRate) {
                        r += buffer[index+1] + buffer[index + 5] + buffer[index + 9 ] + buffer[index + 13];
                        g += buffer[index+2] + buffer[index + 6] + buffer[index + 10] + buffer[index + 14];
                        b += buffer[index+3] + buffer[index + 7] + buffer[index + 11] + buffer[index + 15];
                        count += kUnrollRate;
                        index += kBytesPerPixel * kUnrollRate;
                    }

                }
                break;
            }

            case BufferFormatRgb565: {
                const char kBytesPerPixel = 2;
                for(int currentY = 0; currentY < rect.height(); currentY++) {
                    int index = pitch * (rect.y()+currentY) + rect.x()*kBytesPerPixel;
                    for(int currentX = 0; currentX < rect.width(); currentX += kUnrollRate) {
                        b += _565B(buffer,index) + _565B(buffer,index+2) + _565B(buffer,index+4) + _565B(buffer,index+6);
                        g += _565G(buffer,index) + _565G(buffer,index+2) + _565G(buffer,index+4) + _565G(buffer,index+6);
                        r += _565R(buffer,index) + _565R(buffer,index+2) + _565R(buffer,index+4) + _565R(buffer,index+6);
                        count += kUnrollRate;
                        index += kBytesPerPixel * kUnrollRate;
                    }

                }
                break;
            }
            default:
                return -1;
                break;
            }

            if ( count > 1 ) {
                r = int( r / (double)count) & 0xff;
                g = int( g / (double)count) & 0xff;
                b = int( b / (double)count) & 0xff;
            }

            *result = qRgb(r,g,b);
            return 0;
        }

        QRgb calculateAvgColor(QList<QRgb> *colors) {
            int r=0, g=0, b=0;
            for( int i=0; i<colors->size(); i++) {
               r += qRed(colors->at(i));
               g += qGreen(colors->at(i));
               b += qBlue(colors->at(i));
            }
            r = r / colors->size();
            g = g / colors->size();
            b = b / colors->size();
            return qRgb(r, g, b);
        }
    }
}
