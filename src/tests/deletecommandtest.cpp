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

#include "deletecommandtest.h"

#include "../collectionresolvejob.h"
#include "../deletecommand.h"

#include <AkonadiCore/control.h>
#include <AkonadiCore/itemfetchjob.h>
#include <akonadi/qtest_akonadi.h>

#include <QSignalSpy>
using namespace Akonadi;

QTEST_AKONADIMAIN(DeleteCommandTest, NoGUI);

Q_DECLARE_METATYPE(KJob*)

void DeleteCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void DeleteCommandTest::testDeleteItem()
{
    const char *args[4] = {
        "akonadiclient",
        "delete",
        "-i",
        "1"
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(4, args);

    DeleteCommand *command = new DeleteCommand();

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    Item item(1);

    ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
    QVERIFY(!fetchJob->exec());
}

void DeleteCommandTest::testDeleteCollection()
{
    qRegisterMetaType<KJob*>();

    const char *args[3] = {
        "akonadiclient",
        "delete",
        "/res3"
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(3, args);

    DeleteCommand *command = new DeleteCommand();

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    CollectionResolveJob *resolveJob = new CollectionResolveJob(args[2], this);

    QVERIFY(resolveJob->hasUsableInput());
    QVERIFY(!resolveJob->exec());
}
