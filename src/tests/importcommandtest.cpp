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

#include "importcommandtest.h"
#include "../exportcommand.h"
#include "../importcommand.h"

#include <akonadi/control.h>
#include <akonadi/qtest_akonadi.h>

using namespace Akonadi;

QTEST_AKONADIMAIN(ImportCommandTest, NoGUI);

void ImportCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void ImportCommandTest::testImportCommand()
{
    QTemporaryFile exportFile;
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

    ExportCommand *exportCommand = new ExportCommand(this);
    int ret = exportCommand->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(exportCommand);
    QCOMPARE(ret, 0);

    args.clear();
    args << "akonadiclient" << "import" << "/res3" << exportFile.fileName().toLocal8Bit();

    for (int i=0; i < numArgs; i++) {
        testArgs[i] = args[i].data();
    }

    parsedArgs = getParsedArgs(numArgs, testArgs);

    ImportCommand *importCommand = new ImportCommand(this);
    ret = importCommand->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(importCommand);
    QCOMPARE(ret, 0);

    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res3/foo", this);
    QVERIFY(resolveJob->hasUsableInput());

    AKVERIFYEXEC(resolveJob);

    QVERIFY(resolveJob->collection().isValid());
}