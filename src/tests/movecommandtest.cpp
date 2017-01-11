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

#include "movecommandtest.h"

#include "../abstractcommand.h"
#include "../movecommand.h"
#include "../collectionresolvejob.h"

#include <AkonadiCore/control.h>
#include <akonadi/qtest_akonadi.h>

#include <QTest>
#include <QtDebug>

QTEST_AKONADIMAIN(MoveCommandTest, NoGUI);

using namespace Akonadi;

void MoveCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void MoveCommandTest::testMoveCollection()
{
    const char *args[4] = {
        "akonadiclient",
        "move",
        "/res1/foo",
        "/res3/"
    };

    setenv("AKONADICLIENT_DANGEROUS", "enabled", 0);
    KCmdLineArgs *parsedArgs = getParsedArgs(4, args);

    MoveCommand *command = new MoveCommand(this);
    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    CollectionResolveJob *destResolveJob = new CollectionResolveJob("/res3/foo", this);
    AKVERIFYEXEC(destResolveJob);
    QCOMPARE(destResolveJob->hasUsableInput(), 1);
    QCOMPARE(destResolveJob->collection().isValid(), 1);

    delete destResolveJob;
}
