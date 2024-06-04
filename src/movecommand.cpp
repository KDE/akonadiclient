/*
    Copyright (C) 2013  Jonathan Marten <jjm@keelhaul.me.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "movecommand.h"

#include "errorreporter.h"

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("move", MoveCommand, kli18nc("info:shell", "Move collections or items into a new collection"));

MoveCommand::MoveCommand(QObject *parent)
    : CopyCommand(parent)
{
    mMoving = true;
}

void MoveCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("source", i18nc("@info:shell", "Existing collections or items to move"), i18n("source..."));
    parser->addPositionalArgument("destination", i18nc("@info:shell", "Destination collection to move into"));
}

void MoveCommand::start()
{
    if (!allowDangerousOperation()) {
        Q_EMIT finished(RuntimeError);
    }
    CopyCommand::start();
}
