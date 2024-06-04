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

#pragma once

#include "abstractcommand.h"

class QTextStream;

class CommandShell : public AbstractCommand
{
    Q_OBJECT

public:
    explicit CommandShell(QObject *parent = nullptr);
    ~CommandShell() override;

    [[nodiscard]] QString name() const override;

public:
    static bool isActive()
    {
        return (sIsActive);
    }

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    int initCommand(QCommandLineParser *parser) override;

private:
    QTextStream *const mTextStream;
    static bool sIsActive;

private:
    bool enterCommandLoop();

private Q_SLOTS:
    void onCommandError(const QString &error);
};
