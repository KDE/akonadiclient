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

#ifndef AGENTSCOMMAND_H
#define AGENTSCOMMAND_H

#include "abstractcommand.h"
#include <qstringlist.h>

namespace Akonadi {
    class AgentInstance;
};

class AgentsCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit AgentsCommand(QObject *parent = 0);
    ~AgentsCommand();

    QString name() const;

public Q_SLOTS:
    void start();

protected:
    int initCommand(KCmdLineArgs *parsedArgs);
    void setupCommandOptions(KCmdLineOptions &options);

private:
    void getState();
    void showInfo();
    void printAgentStatus(const QList<Akonadi::AgentInstance> &agents);
    void restartAgents();
    void setState();

private:
    bool mDryRun;
    enum Option {
        LIST = 0,
        SETSTATE = 1,
        GETSTATE = 2,
        INFO = 3,
        RESTART = 4
    };

    enum AgentState {
        OFFLINE = 0,
        ONLINE = 1
    };

    AgentState mStateArg;
    Option mOption;
    QStringList mArguments;
};

#endif // AGENTSCOMMAND_H