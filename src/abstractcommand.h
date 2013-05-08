/*
    Copyright (C) 2012  Kevin Krammer <krammer@kde.org>

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

#ifndef ABSTRACTCOMMAND_H
#define ABSTRACTCOMMAND_H

#include <QObject>

class KCmdLineArgs;
class KCmdLineOptions;
class KLocalizedString;

class AbstractCommand : public QObject
{
  Q_OBJECT

  public:
    enum Errors {
      NoError = 0,
      InvalidUsage
    };

    explicit AbstractCommand( QObject *parent = 0 );
    ~AbstractCommand();

    int init( KCmdLineArgs *parsedArgs );

    QString shortHelp() const;
    virtual QString name() const = 0;

  public Q_SLOTS:
    virtual void start();

  Q_SIGNALS:
    void finished( int exitCode );
    void error( const QString &message );

  protected:
    QString mShortHelp;

  protected:
    virtual void setupCommandOptions( KCmdLineOptions &options );
    virtual int initCommand( KCmdLineArgs *parsedArgs ) = 0;
    void emitErrorSeeHelp( const KLocalizedString &msg );
};

#endif // ABSTRACTCOMMAND_H
