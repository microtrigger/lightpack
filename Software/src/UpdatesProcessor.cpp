/*
 * UpdatesProcessor.cpp
 *
 *  Created on: 4/11/2014
 *     Project: Prismatik
 *
 *  Copyright (c) 2014 tim
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

#include "UpdatesProcessor.hpp"
#include <QXmlStreamReader>
#include <QNetworkReply>
#include "version.h"



const AppVersion kCurVersion(VERSION_STR);

UpdatesProcessor::UpdatesProcessor(QObject * parent)
    : QObject(parent)
    , _reply(NULL)
{
}

void UpdatesProcessor::requestUpdates()
{
    if(_reply != NULL) {
        _reply->disconnect();
        delete _reply;
        _reply = NULL;
    }

    QNetworkRequest request(QUrl("http://lightpack.tv/updates.xml"));
    _reply = _networkMan.get(request);
    connect(_reply, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
}

QList<UpdateInfo> UpdatesProcessor::readUpdates(uint lastReadId)
{
    QList<UpdateInfo> updates;

    QXmlStreamReader xmlReader(_reply->readAll());

    if (xmlReader.readNextStartElement()) {
        if (xmlReader.name() == "updates") {
            readUpdates(&updates, &xmlReader);
            QList<UpdateInfo>::iterator it = updates.begin();
            while (it != updates.end()) {
                UpdateInfo updateInfo = *it;
                if (!updateInfo.softwareVersion.isEmpty() && !isVersionMatches(updateInfo.softwareVersion, &kCurVersion)) {
                    updates.removeAll(updateInfo);
                } else if (updateInfo.id <= lastReadId) {
                    updates.removeAll(updateInfo);
                } else {
                    it++;
                }
            }
        }
    }

    return updates;
}

bool UpdatesProcessor::isVersionMatches(const QString &predicate, const AppVersion *version)
{
    enum Condition {
        LT,
        GT,
        EQ,
        LE,
        GE
    };

    Condition cond;
    QStringRef predicateRef(&predicate);

    QStringRef predicateVerStr = predicateRef.right(2).trimmed();

    QStringRef condStr = predicateRef.left(2);

    if (condStr == "lt") {
        cond = LT;
    } else if (condStr == "gt") {
        cond = GT;
    } else if (condStr == "eq") {
        cond = EQ;
    } else if (condStr == "le") {
        cond = LE;
    } else if (condStr == "ge") {
        cond = GE;
    } else {
        cond = EQ;
        predicateVerStr = predicateRef.trimmed();
    }

    AppVersion predicateVer(predicateVerStr);

    if (!version->isValid() || !predicateVer.isValid())
        return false;

    switch (cond) {
    case LT:
        return *version < predicateVer;
        break;
    case GT:
        return *version > predicateVer;
        break;
    case EQ:
        return *version == predicateVer;
        break;
    case LE:
        return *version <= predicateVer;
        break;
    case GE:
        return *version >= predicateVer;
        break;
    }
}

QList<UpdateInfo> * UpdatesProcessor::readUpdates(QList<UpdateInfo> *updates, QXmlStreamReader *xmlReader)
{
    if (xmlReader->readNextStartElement()) {
        while (xmlReader->name() == "update") {
            UpdateInfo updateInfo;
            while(xmlReader->readNextStartElement()) {
                if (xmlReader->name() == "id") {
                    xmlReader->readNext();
                    QStringRef id = xmlReader->text();
                    updateInfo.id = id.toUInt();
                } else if (xmlReader->name() == "url") {
                    xmlReader->readNext();
                    updateInfo.url = xmlReader->text().toString();
                } else if (xmlReader->name() == "title") {
                    xmlReader->readNext();
                    updateInfo.title = xmlReader->text().toString();
                } else if (xmlReader->name() == "text") {
                    xmlReader->readNext();
                    updateInfo.text = xmlReader->text().toString();
                } else if (xmlReader->name() == "softwareVersion") {
                    xmlReader->readNext();
                    updateInfo.softwareVersion = xmlReader->text().toString();
                } else if (xmlReader->name() == "firmwareVersion") {
                    xmlReader->readNext();
                    updateInfo.firmwareVersion = xmlReader->text().toString();
                } else if (xmlReader->name() == "update") {
                    break;
                } else {
                    xmlReader->skipCurrentElement();
                }
                xmlReader->skipCurrentElement();
            }
            // add updates with defined id only
            if(updateInfo.id > 0)
                updates->append(updateInfo);
        }
    }
    return updates;
}
