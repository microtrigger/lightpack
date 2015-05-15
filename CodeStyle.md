# Оформление исходников #

Документ описывает правила оформления кода для сохранения единого стиля и повышения его читаемости.

  * Вместо табуляции для отступов в коде используются 4 пробела.

  * Конец строки задаётся в unix формате (LF).

  * В случае, если файлы содержат определение классов, они именуются следующим образом: `%ClassName%.hpp` `%ClassName%.cpp`. Иначе все буквы в имени файла должны быть строчными (маленькими):
```
SpeedTest.hpp
SpeedTest.cpp

version.hpp
```

  * Все имена переменных начинаются со строчной буквы:
```
QString filePath;
```

  * Все имена функций также начинаются со строчной буквы:
```
const QString & getFilePath();
```

  * Глобальные переменные программы именуются с префиксом `g%varName%`:
```
int gDebugLevel;
```

  * Переменные класса именуются с префиксом `_%varName%`:
```
uint8_t _buff;
```

  * Классы именуются с прописной (заглавной) буквы:
```
class Man
{
public:
    Man();

private:
    int _height;
    int _age;
}
```

  * Пространства имен -- строчными символами:
```
namespace lightpack
{
}
```

  * Константы именуются с прописной буквы с префиксом k:
```
const int kFirstLetterCapital;
```

  * Не рекомендуется использовать `#define` в C++ коде. В случае острой необходимости в имени все символы должны быть прописными:
```
#define ALL_CAPITAL_LETTERS
```

  * Пробелы после if, for, while, и других ключевых слов:
```
if (first == second)
{
    for (int i = 0; i < N; i++)
        doSomething();

    while (condition)
        doSomethingElse();

} else {
    findCallbackFunction();
}
```

  * В случае единственного оператора во всех ветках скобки можно опустить:
```
if (first == third)
    start();
else
    stop();
```

  * В заголовочных файлах для устранения коллизий используется директива контроля:
```
#pragma once
```

  * Пространства имен записываются без отступов:
```
namespace lightpack
{
namespace speedtests
{
    class AnotherMan
    {
    }
}
}
```

  * При использовании в коде комментариев с пометкой TODO желательно точно описывать, что именно автор планировал сделать:
```
int getAvgColor()
{
    // TODO: come up with a very fast algorithm for calculating the average color
    return -1;
}
```

# Пример #

Пример оформления кода взят из проекта и для удобства сокращен:

```
/*
 * SpeedTest.hpp
 *
 *  Created on: 22.04.11
 *      Author: Mike Shatohin (brunql)
 *     Project: Lightpack
 *
 *  Lightpack is very simple implementation of the backlight for a laptop
 *
 *  Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
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

#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QTime>

#include "Settings.hpp"
#include "ICaptureSource.hpp"

#include "capturemath.hpp"

using namespace lightpack::capture;
using namespace lightpack::capture::math;

namespace lightpack
{
namespace speedtests
{
    class SpeedTest
    {
    public:
        static void run();

    private:
        static void startTests();
        static void printHeader();
        static void printGrabPrecision(int column);
        static void printVersionOS(int column);
        static void printDateTime(int column);
        static void printSwVersion(int column);

    private:
        static const int kTestTimes = 20;
        static const int kLedsCount = 8;
        static const QString kFileName;

        static QTextStream _outStream;
        static QTime _timer;
    };
}
}


/*
 * SpeedTest.cpp
 *
 *  Created on: 22.04.2011
 *      Author: Mike Shatohin (brunql)
 *     Project: Lightpack
 *
 *  Lightpack is very simple implementation of the backlight for a laptop 
 *
 *  Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
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

#include <QApplication>
#include <QDesktopWidget>

#include <iostream>

#include "SpeedTest.hpp"
#include "Settings.hpp"
#include "version.hpp"

#include "capturemath.hpp"
#include "CaptureSourceQtGrabWindow.hpp"
#include "CaptureSourceWindowsWinApi.hpp"
#include "CaptureSourceWindowsDirect3D9.hpp"
#include "CaptureSourceWindowsDWM.hpp"

using namespace std;
using namespace lightpack::capture;
using namespace lightpack::capture::math;

namespace lightpack
{
namespace speedtests
{
    const QString SpeedTest::kFileName = "SpeedTest.csv";
    QTextStream   SpeedTest::_outStream;
    QTime         SpeedTest::_timer;

    void SpeedTest::run()
    {
        QString filePath = QApplication::applicationDirPath() + "/" + kFileName;

        QFile resultFile(filePath);

        bool isFileExists = false;

        if (resultFile.exists())
            isFileExists = true;

        if (resultFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            _outStream.setDevice(&resultFile);

            if (isFileExists == false)
                printHeader();

            startTests();

            _outStream.flush();

        } else {
            qWarning() << Q_FUNC_INFO << "Can't open file:" << filePath;
        }
    }

    void SpeedTest::printHeader()
    {
        for (int i = 0; i < m_columns.count(); i++)
            m_outStream << m_columns.at(i) << CsvSeparator;

        _outStream << endl;
    }

    void SpeedTest::startTests()
    {
        QList<CaptureListener *> captureListeners;

        for (int i = 0; i < kLedsCount; i++)
        {
            CaptureListener * grabMe = new CaptureListener;

            grabMe->setRect(QRect(
                    Settings::getDefaultPosition(i),
                    QSize(GrabWidgetWidth, GrabWidgetHeight)));

            captureListeners << grabMe;
        }

        QList <ICaptureSource *> captureSources;

        captureSources << (ICaptureSource *)(new CaptureSourceQtGrabWindow());
#       ifdef Q_WS_WIN
        captureSources << (ICaptureSource *)(new CaptureSourceWindowsWinApi());
        captureSources << (ICaptureSource *)(new CaptureSourceWindowsDirect3D9());
        captureSources << (ICaptureSource *)(new CaptureSourceWindowsDWM());
#       endif

        for (int i = 0; i < captureSources.count(); i++)
        {
            int column = 0;

            printDateTime(column++);

            outColumn(column++, captureSources[i]->name());
        }

        _outStream.flush();
    }

    void SpeedTest::printDateTime(int column)
    {
        outColumn(column,
                  QDateTime::currentDateTime().date().toString("yyyy.MM.dd") + " " +
                  QDateTime::currentDateTime().time().toString("hh:mm:ss"));
    }

    void SpeedTest::printSwVersion(int column)
    {
        outColumn(column, "sw" VERSION_STR);
    }
}
}

```