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

#ifndef UPDATECOMMAND_H
#define UPDATECOMMAND_H

#include "abstractcommand.h"

class KJob;
class QFile;

class UpdateCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit UpdateCommand(QObject *parent = nullptr);
    virtual ~UpdateCommand();

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    int initCommand(QCommandLineParser *parser) override;
    void setupCommandOptions(QCommandLineParser *parser) override;

private:
    QFile *mFile;
    QString mFileArg;
    QString mItemArg;

private Q_SLOTS:
    void onItemFetched(KJob *job);
    void onItemUpdated(KJob *job);
};

#endif // UPDATECOMMAND_H
