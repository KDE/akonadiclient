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

#include "exportcommandtest.h"
#include "../exportcommand.h"

#include <akonadi/control.h>
#include <akonadi/xml/xmlwritejob.h>
#include <akonadi/qtest_akonadi.h>

using namespace Akonadi;

QTEST_AKONADIMAIN(ExportCommandTest, NoGUI);

void ExportCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void ExportCommandTest::testExportCollection()
{
    QTemporaryFile exportFile;
    exportFile.setAutoRemove(false);
    QVERIFY(exportFile.open());
    exportFile.close();

    const int numArgs = 4;
    QVector<QByteArray> args;
    args.reserve(numArgs);
    args << "akonadiclient" << "export" << "/res1/foo" << exportFile.fileName().toLocal8Bit();

    const char *testArgs[numArgs] = {
        args[0].data(),
        args[1].data(),
        args[2].data(),
        args[3].data()
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(numArgs, testArgs);

    ExportCommand *command = new ExportCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    exportFile.open();
    QByteArray exported = exportFile.readAll();

    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res1/foo", this);
    QVERIFY(resolveJob->hasUsableInput());
    resolveJob->start();
    AKVERIFYEXEC(resolveJob);

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);

    QVERIFY(tmpFile.open());
    tmpFile.close();

    XmlWriteJob *writeJob = new XmlWriteJob(resolveJob->collection(), tmpFile.fileName(), this);
    AKVERIFYEXEC(writeJob);

    tmpFile.open();
    QVERIFY(tmpFile.readAll() == exported);

    exportFile.close();
    tmpFile.close();
}
