/***************************************************************************
 *   Copyright (C) 2009 - 2013 by Artem 'DOOMer' Galichkin                        *
 *   doomer3d@gmail.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "extedit.h"
#include "core/core.h"

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDebug>

const QByteArray _globalAppListPath_c = "/usr/share/applications/";

ExtEdit::ExtEdit(QObject *parent) :
    QObject(parent), _watcherEditedFile(new QFileSystemWatcher(this))
{
    createAppList();
    _fileIsCnaged = false;
    connect(_watcherEditedFile, SIGNAL(fileChanged(QString)), this, SLOT(editedFileChanged(QString)));
}

QStringList ExtEdit::listAppNames()
{
    QStringList list;

    for (int i =0; i < _appList.count(); ++i)
    {
        list << _appList.at(i).name;
    }

    return list;
}

void ExtEdit::addAppAction(QAction* act)
{
    _actionList.append(act);
}


void ExtEdit::runExternalEditor()
{
    qDebug() << "recevier " << sender()->objectName();
    QAction* selectedAction = qobject_cast<QAction*>(sender());
    int selectedIndex = _actionList.indexOf(selectedAction);

    ExtApp_t selectedApp = _appList.at(selectedIndex);
    QString exec = selectedApp.exec.split(" ").first();

    Core *core = Core::instance();
    QString format = "png"; //core->conf->getSaveFormat();
    _editFilename = core->getTempFilename(format);
    core->writeScreen(_editFilename, format , true);
    QStringList args;
    args << _editFilename;

    QProcess *execProcess = new QProcess(this);
    connect(execProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(closedExternalEditor(int,QProcess::ExitStatus)));
    execProcess->start(exec, args);
    _watcherEditedFile->addPath(_editFilename);
}

void ExtEdit::closedExternalEditor(int exitCode, QProcess::ExitStatus exitStatus)
{
    Core *core = Core::instance();

    if (_fileIsCnaged == true)
    {
        core->updatePixmap();
    }

    _fileIsCnaged = false;
    _watcherEditedFile->removePath(_editFilename);

    sender()->deleteLater();
    core->killTempFile();
    _editFilename.clear();
}

void ExtEdit::editedFileChanged(const QString& path)
{
    _fileIsCnaged = true;
}


void ExtEdit::createAppList()
{
    QByteArray globalMimeTypesList = _globalAppListPath_c + "mimeinfo.cache";
    QByteArray localMimeTypesPath = qgetenv("XDG_DATA_HOME");

    if (localMimeTypesPath.isEmpty() == true)
    {
        localMimeTypesPath = qgetenv("HOME") + "/.local/share";
    }
    localMimeTypesPath += "/applications/";
    QByteArray localMimeTypesList = localMimeTypesPath + "mimeinfo.cache";

    QVector<QByteArray> pathList;
    pathList << _globalAppListPath_c << localMimeTypesPath;

    QVector<QByteArray> fileList;
    fileList << globalMimeTypesList << localMimeTypesList;

    QFile file;

    for (int f = 0; f < fileList.count(); ++f)
    {
        file.setFileName(fileList.value(f));
        if (file.open(QIODevice::ReadOnly) == true)
        {
            QString inLine;
            QString mimetype;
            QStringList desktopFiles;

            QTextStream in(&file);
            while(in.atEnd() == false)
            {
                inLine = in.readLine();
                if (inLine.split("=").count() > 1)
                {
                    mimetype = inLine.split("=").at(0);
                    if (mimetype == "image/png")
                    {
                        desktopFiles = inLine.split("=").at(1).split(";");
                        if (desktopFiles.count() != 0)
                        {
                            for (int i = 0; i < desktopFiles.count(); ++i)
                            {
                                if (desktopFiles.at(i).isEmpty() == false)
                                {
                                    _appList.append(readDesktopFile(desktopFiles.at(i), pathList.at(f)));
                                }
                            }
                        }
                    }
                }
            }
        }
        file.close();
    }
}

ExtApp_t ExtEdit::readDesktopFile(QString filename, QByteArray path)
{
    ExtApp_t entry;

    if (filename.startsWith("kde4-") == true)
    {
        filename = "kde4/" + filename.remove("kde4-");
    }
    filename = path + filename;

    QFile file(filename);
    if (file.open(QIODevice::ReadOnly) == true)
    {
        QTextStream in(&file);
        QString inLine;
        while(in.atEnd() == false)
        {
            inLine = in.readLine();
            if (inLine.split("=").count() != 1)
            {
                if (inLine.split("=").at(0) == "Name")
                {
                    entry.name = inLine.split("=").at(1);
                }
                if (inLine.split("=").at(0) == "Exec")
                {
                    entry.exec = inLine.split("=").at(1).toLatin1();
                }
            }
        }
    }

    return entry;
}
