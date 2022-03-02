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

#include "copycommandtest.h"

#include "../abstractcommand.h"
#include "../commandrunner.h"
#include "../copycommand.h"
#include "../collectionresolvejob.h"

#include <Akonadi/Collection>
#include <Akonadi/Control>
#include <Akonadi/Item>
#include <akonadi/qtest_akonadi.h>

#include <QtDebug>

using namespace Akonadi;

QTEST_AKONADIMAIN(CopyCommandTest, NoGUI)

void CopyCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void CopyCommandTest::testCollectionCopy()
{
    const char *args[4] = {
        "akonadiclient",
        "copy",
        "/res1/foo",
        "/res2"
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(4, args);

    CopyCommand *command = new CopyCommand(this);
    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res2/foo", this);
    AKVERIFYEXEC(resolveJob);
    QCOMPARE(resolveJob->hasUsableInput(), 1);
    QCOMPARE(resolveJob->collection().isValid(), 1);
    delete resolveJob;
}

void CopyCommandTest::testItemCopy()
{
    const char *args[4] = {
        "akonadiclient",
        "copy",
        "1",
        "/res3/"
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(4, args);
    CopyCommand *command = new CopyCommand(this);
    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);

    QCOMPARE(ret, 0);
}
