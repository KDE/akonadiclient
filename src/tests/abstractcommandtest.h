/*
 * Copyright (C) 2014  Bhaskar Kandiyal <bkandiyal@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef ABSTRACTCOMMANDTEST_H
#define ABSTRACTCOMMANDTEST_H

#include "../abstractcommand.h"

#include <QObject>

class KAboutData;
class QSignalSpy;

class AbstractCommandTest : public QObject
{
    Q_OBJECT

public:
    AbstractCommandTest();
    virtual ~AbstractCommandTest();
    KCmdLineArgs *getParsedArgs(int argc, const char **argv);
    int runCommand(AbstractCommand *command, int maxWaitTime = 10000);

protected:
    KAboutData *mAboutData;

private:
    QSignalSpy *m_finishedSpy;
    QSignalSpy *m_errorSpy;

private Q_SLOTS:
    virtual void initTestCase() = 0;
    void cleanup();
};

#endif // ABSTRACTCOMMANDTEST_H
