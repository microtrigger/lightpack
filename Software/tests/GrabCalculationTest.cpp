#include "GrabCalculationTest.hpp"
#include <stdio.h>

void GrabCalculationTest::testCase1()
{
    QRgb result;
    unsigned char buf[16];
    memset(buf, 0xfa, 16);
    QVERIFY2(Grab::Calculations::calculateAvgColor(&result, buf, BufferFormatArgb, 16, QRect(0,0,4,1)) == 0, "Failure. calculateAvgColor returned wrong errorcode");
    QCOMPARE(result, qRgb(0xfa,0xfa,0xfa));
}

void GrabCalculationTest::testCaseRgb565()
{
    QRgb result;
    unsigned char buf[16];
    memset(buf, 0xfa, 16);
    QVERIFY2(Grab::Calculations::calculateAvgColor(&result, buf, BufferFormatRgb565, 16, QRect(0,0,8,1)) == 0, "Failure. calculateAvgColor returned wrong errorcode");
    fprintf(stderr, "result is (%d,%d,%d)\n", qRed(result), qGreen(result), qBlue(result));
    QCOMPARE(result, qRgb(248,92,208));

}
