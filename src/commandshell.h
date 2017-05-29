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

#ifndef COMMANDSHELL_H
#define COMMANDSHELL_H

#include <QObject>
#include <QVarLengthArray>

#include "abstractcommand.h"

class KCmdLineArgs;
class QTextStream;

class CommandShell : public AbstractCommand
{
    Q_OBJECT

public:
    explicit CommandShell(QObject *parent = nullptr);
    ~CommandShell();

    QString name() const;

public:
    static bool isActive()
    {
        return (sIsActive);
    }

public Q_SLOTS:
    void start();

protected:
    int initCommand(KCmdLineArgs *parsedArgs);

private:
    AbstractCommand *mCommand;
    QTextStream *mTextStream;

    static bool sIsActive;

private:
    KCmdLineArgs *getParsedArgs(int argc, char **argv);
    void freeArguments(const QVarLengthArray<char *> &args);
    bool enterCommandLoop();

private Q_SLOTS:
    void onCommandFinished(int exitCode);
    void onCommandError(const QString &error);
};

#endif // COMMANDSHELL_H
