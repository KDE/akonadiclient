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


#ifndef COLLECTIONRESOLVEJOB_H
#define COLLECTIONRESOLVEJOB_H

#include <Akonadi/Collection>
#include <KCompositeJob>

class CollectionResolveJob : public KCompositeJob
{
  Q_OBJECT

  public:
    explicit CollectionResolveJob( const QString &userInput, QObject *parent = 0 );
    ~CollectionResolveJob();

    void start();

    bool hasUsableInput();
    bool hadSlash() const			{ return (mHadSlash); }
    Akonadi::Collection collection() const	{ return (mCollection); }
    QString formattedCollectionName() const;

  protected Q_SLOTS:
    void slotResult( KJob *job );

  private:
    const QString mUserInput;
    Akonadi::Collection mCollection;
    bool mHadSlash;

  private:
    void fetchBase();
};

#endif // COLLECTIONRESOLVEJOB_H
