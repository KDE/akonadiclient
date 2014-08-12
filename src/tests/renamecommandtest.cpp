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

#include "renamecommandtest.h"

#include "../collectionresolvejob.h"
#include "../renamecommand.h"

#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/control.h>
#include <akonadi/qtest_akonadi.h>

using namespace Akonadi;

QTEST_AKONADIMAIN(RenameCommandTest, NoGUI);

void RenameCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void RenameCommandTest::testRenameCollection()
{
    const char *args[4] = {
        "akonadiclient",
        "rename",
        "/res1/foo",
        "foobar"
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(4, args);
    RenameCommand *command = new RenameCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res1/foobar", this);
    AKVERIFYEXEC(resolveJob);

    QCOMPARE(resolveJob->collection().name(), QString::fromLatin1(args[3]));
}