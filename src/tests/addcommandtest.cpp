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

#include "addcommandtest.h"
#include "../addcommand.h"
#include <akonadi/control.h>
#include <akonadi/qtest_akonadi.h>
#include <QtDebug>

using namespace Akonadi;

QTEST_AKONADIMAIN(AddCommandTest, NoGUI);

void AddCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void AddCommandTest::testAddItem()
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    tempFile.open();
    tempFile.write("Hello World!");

    const char *args[6] = {
        "akonadiclient",
        "add",
        "/res3",
        tempFile.fileName().toLocal8Bit().data(),
        "--base",
        QDir::tempPath().toLocal8Bit().data()
    };

    tempFile.close();
    KCmdLineArgs *parsedArgs = getParsedArgs(6, args);

    AddCommand *command = new AddCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);
}
