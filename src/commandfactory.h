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

#ifndef COMMANDFACTORY_H
#define COMMANDFACTORY_H

#include <QObject>

#include <KLocalizedString>

class AbstractCommand;
struct CommandData;

class QString;

#define DEFINE_COMMAND(commandName, className, shortHelp)			\
    class className##Factory							\
    {										\
    public:									\
        className##Factory();							\
    };										\
    static className##Factory sFactory;						\
    static AbstractCommand *className##Creator(QObject *parent)			\
    {										\
        return (new className(parent));						\
    }										\
    className##Factory::className##Factory()					\
    {										\
        CommandFactory::registerCommand(QLatin1String(commandName),		\
                                        ki18nc("@info:shell", shortHelp),	\
                                        &className##Creator);			\
    }										\
    QString className::name() const						\
    {										\
        return (QLatin1String(commandName));					\
    }

class CommandFactory
{
public:
    explicit CommandFactory(const QStringList *parsedArgs);
    ~CommandFactory() = default;

    AbstractCommand *createCommand();

    typedef AbstractCommand *(*creatorFunction)(QObject *parent);
    static void registerCommand(const QString &name,
                                const KLocalizedString &shortHelp,
                                CommandFactory::creatorFunction creator);
private:
    const QStringList *mParsedArgs;

private:
    void checkAndHandleHelp();
    void printHelpAndExit(bool userRequestedHelp);
};

#endif // COMMANDFACTORY_H
