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

#pragma once

#include <QObject>
#include <QCommandLineParser>


class KJob;
class CollectionResolveJob;


class AbstractCommand : public QObject
{
    Q_OBJECT

public:
    enum Errors {
        DefaultError = -1,
        NoError = 0,
        InvalidUsage = 1,
        RuntimeError = 2
    };

    explicit AbstractCommand(QObject *parent = nullptr);
    virtual ~AbstractCommand() = default;

    int init(const QStringList &parsedArgs, bool showHelp = false);

    virtual QString name() const = 0;

public Q_SLOTS:
    virtual void start() = 0;

Q_SIGNALS:
    void finished(AbstractCommand::Errors exitCode = AbstractCommand::DefaultError);
    void error(const QString &message);

protected:
    virtual void setupCommandOptions(QCommandLineParser *parser) = 0;
    // TODO: return type should be the enum
    virtual int initCommand(QCommandLineParser *parser) = 0;

    void emitErrorSeeHelp(const QString &msg);
    bool allowDangerousOperation() const;

    void addOptionsOption(QCommandLineParser *parser);
    void addCollectionItemOptions(QCommandLineParser *parser);
    void addDryRunOption(QCommandLineParser *parser);

    bool getCommonOptions(QCommandLineParser *parser);
    bool checkArgCount(const QStringList &args, int min, const QString &errorText);

    bool isDryRun() const			{ return (mDryRun); }
    bool wantCollection() const			{ return (mWantCollection); }
    bool wantItem() const			{ return (mWantItem); }

    bool getResolveJob(const QString &arg);
    CollectionResolveJob *resolveJob() const	{ Q_ASSERT(mResolveJob!=nullptr); return (mResolveJob); }

    /**
     * Check the result status of a KIO job.
     *
     * Call this first of all in a slot connected to
     * a job's @c result() signal.  If the job had an error then
     * the @c error() signal will be emitted, and then @c processNext()
     * called to process the next loop argument.  If the processing
     * loop is not being used then the @c finished() signal will
     * be emitted to end the command.
     *
     * @param job The job that has just run
     * @param message An error message, or a null string to use the
     * job's @c errorString() message.
     * @result @c true if the job completed without error,
     * @c false otherwise.
     **/
    bool checkJobResult(KJob *job, const QString &message = QString());

    /**
     * Prepare to run a loop to process multiple command arguments.
     *
     * @param args The list of arguments to be processed, usually
     * the @c positionalArguments() from the @c CommandParser after
     * removing any initial ones with special meaning.
     * @param finishedMessage A progress message to display when the
     * loop is finished.  A null string means to display nothing.
     **/
    void initProcessLoop(const QStringList &args, const QString &finishedMessage = QString());

    /**
     * Run a loop to process multiple command arguments.
     *
     * The @p slot function will be called to start processing.
     * While processing, the current argument can be accessed
     * by @c currentArg().  The processing may run any number of
     * asynchronous jobs or event loops, and when finished should
     * call @c processNext() to continue with the next command argument.
     * Any errors should be reported via the @c error() signal, which
     * will track the overall return code and report it when finished.
     *
     * @param slot The name of the processing slot to perform an
     * action for a single argument.
     **/
    void startProcessLoop(const char *slot);

    /**
     * Process the next command argument, if there are any remaining.
     *
     * If there are arguments remaining, then the @c slot specified
     * to @c startProcessLoop() will be called again.  If the arguments
     * are all processed then the @c finished() signal will be emitted
     * to indicate that the command is finished.
     **/
    void processNext();

    /**
     * Get the command argument currently being processed.
     *
     * @return the current argument.
     **/
    const QString &currentArg() const			{ return (mCurrentArg); };

    /**
     * Check whether the processing loop is finished.
     *
     * @return @c true if there are no more arguments to process,
     * @c false if there are any more remaining.
     **/
    bool isProcessLoopFinished() const			{ return (mProcessLoopArgs.isEmpty()); }

private:
    bool mDryRun;
    bool mWantCollection;
    bool mWantItem;

    bool mSetDryRun;
    bool mSetCollectionItem;

    CollectionResolveJob *mResolveJob = nullptr;

    QStringList mProcessLoopArgs;
    const char *mProcessLoopSlot;
    QString mCurrentArg;
    QString mFinishedLoopMessage;
};

