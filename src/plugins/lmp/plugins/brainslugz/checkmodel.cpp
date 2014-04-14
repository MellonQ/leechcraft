/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "checkmodel.h"
#include <QtDebug>
#include <interfaces/media/idiscographyprovider.h>

namespace LeechCraft
{
namespace LMP
{
namespace BrainSlugz
{
	namespace
	{
		class ReleasesSubmodel : public QStandardItemModel
		{
		public:
			enum Role
			{
				ReleaseName = Qt::UserRole + 1,
				ReleaseYear
			};

			ReleasesSubmodel (QObject *parent)
			: QStandardItemModel { parent }
			{
				QHash<int, QByteArray> roleNames;
				roleNames [ReleaseName] = "releaseName";
				roleNames [ReleaseYear] = "releaseYear";
				setRoleNames (roleNames);
			}
		};
	}

	CheckModel::CheckModel (const Collection::Artists_t& artists, QObject *parent)
	: QStandardItemModel { parent }
	{
		QHash<int, QByteArray> roleNames;
		roleNames [Role::ArtistId] = "artistId";
		roleNames [Role::ArtistName] = "artistName";
		roleNames [Role::ScheduledToCheck] = "scheduled";
		roleNames [Role::IsChecked] = "isChecked";
		roleNames [Role::Releases] = "releases";
		setRoleNames (roleNames);

		for (const auto& artist : artists)
		{
			auto item = new QStandardItem { artist.Name_ };
			item->setData (artist.ID_, Role::ArtistId);
			item->setData (artist.Name_, Role::ArtistName);
			item->setData (true, Role::ScheduledToCheck);
			item->setData (false, Role::IsChecked);

			const auto submodel = new ReleasesSubmodel { this };
			item->setData (QVariant::fromValue<QObject*> (submodel), Role::Releases);

			appendRow (item);

			Artist2Submodel_ [artist.ID_] = submodel;
			Artist2Item_ [artist.ID_] = item;

			Scheduled_ << artist.ID_;
		}
	}

	void CheckModel::SetMissingReleases (const QList<Media::ReleaseInfo>& releases,
			const Collection::Artist& artist)
	{
		qDebug () << Q_FUNC_INFO << artist.Name_ << releases.size ();

		const auto item = Artist2Item_.value (artist.ID_);
		if (!item)
		{
			qWarning () << Q_FUNC_INFO
					<< "no item for artist"
					<< artist.Name_;
			return;
		}

		const auto model = Artist2Submodel_.value (artist.ID_);
		for (const auto& release : releases)
		{
			auto item = new QStandardItem;
			item->setData (release.Name_, ReleasesSubmodel::ReleaseName);
			item->setData (release.Year_, ReleasesSubmodel::ReleaseYear);
			model->appendRow (item);
		}

		item->setData (true, Role::IsChecked);
	}

	void CheckModel::MarkNoNews (const Collection::Artist& artist)
	{
		const auto item = Artist2Item_.take (artist.ID_);
		if (!item)
		{
			qWarning () << Q_FUNC_INFO
					<< "no item for artist"
					<< artist.Name_;
			return;
		}

		removeRow (item->row ());
		Artist2Submodel_.take (artist.ID_)->deleteLater ();
	}

	void CheckModel::setArtistScheduled (int id, bool scheduled)
	{
		Artist2Item_ [id]->setData (true, Role::ScheduledToCheck);

		if (scheduled)
			Scheduled_ << id;
		else
			Scheduled_.remove (id);
	}
}
}
}
