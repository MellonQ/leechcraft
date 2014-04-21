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

#pragma once

#include <QStandardItemModel>
#include <QHash>
#include <QSet>
#include <interfaces/lmp/collectiontypes.h>
#include <interfaces/lmp/ilmpplugin.h>
#include <interfaces/core/icoreproxy.h>

namespace Media
{
struct ReleaseInfo;

class IAlbumArtProvider;
class IArtistBioFetcher;
}

namespace LeechCraft
{
namespace LMP
{
namespace BrainSlugz
{
	class CheckModel : public QStandardItemModel
	{
		Q_OBJECT

		QHash<int, QStandardItem*> Artist2Item_;
		QHash<int, QStandardItemModel*> Artist2Submodel_;

		QSet<int> Scheduled_;

		const Collection::Artists_t AllArtists_;
		const ILMPProxy_ptr Proxy_;
		const QString DefaultAlbumIcon_;
		const QString DefaultArtistIcon_;

		Media::IAlbumArtProvider * const AAProv_;
		Media::IArtistBioFetcher * const BioProv_;
	public:
		enum Role
		{
			ArtistId = Qt::UserRole + 1,
			ArtistName,
			ScheduledToCheck,
			IsChecked,
			ArtistImage,
			Releases,
			MissingCount,
			PresentCount
		};

		CheckModel (const Collection::Artists_t&,
				const ICoreProxy_ptr&, const ILMPProxy_ptr&, QObject*);

		Collection::Artists_t GetSelectedArtists () const;

		void SetMissingReleases (const QList<Media::ReleaseInfo>&, const Collection::Artist&);
		void MarkNoNews (const Collection::Artist&);

		void RemoveUnscheduled ();
	public slots:
		void selectAll ();
		void selectNone ();
		void setArtistScheduled (int, bool);
	};
}
}
}
