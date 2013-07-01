/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "biopropproxy.h"
#include <algorithm>
#include <QStandardItemModel>
#include <QApplication>
#include <QtDebug>

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		class ArtistImagesModel : public QStandardItemModel
		{
		public:
			enum Role
			{
				ThumbURL = Qt::UserRole + 1,
				FullURL,
				Title,
				Author,
				Date
			};

			ArtistImagesModel (QObject *parent)
			: QStandardItemModel (parent)
			{
				QHash<int, QByteArray> roleNames;
				roleNames [Role::ThumbURL] = "thumbURL";
				roleNames [Role::FullURL] = "fullURL";
				roleNames [Role::Title] = "title";
				roleNames [Role::Author] = "author";
				roleNames [Role::Date] = "date";
				setRoleNames (roleNames);
			}
		};
	}

	BioPropProxy::BioPropProxy (QObject *parent)
	: QObject (parent)
	, ArtistImages_ (new ArtistImagesModel (this))
	{
	}

	void BioPropProxy::SetBio (const Media::ArtistBio& bio)
	{
		Bio_ = bio;

		QStringList tags;
		std::transform (Bio_.BasicInfo_.Tags_.begin (), Bio_.BasicInfo_.Tags_.end (),
				std::back_inserter (tags),
				[] (decltype (Bio_.BasicInfo_.Tags_.front ()) item) { return item.Name_; });
		CachedTags_ = tags.join ("; ");

		CachedInfo_ = Bio_.BasicInfo_.FullDesc_.isEmpty () ?
				Bio_.BasicInfo_.ShortDesc_ :
				Bio_.BasicInfo_.FullDesc_;
		CachedInfo_.replace ("\n", "<br />");

		ArtistImages_->clear ();
		QList<QStandardItem*> rows;
		for (const auto& imageItem : bio.OtherImages_)
		{
			auto item = new QStandardItem ();
			item->setData (imageItem.Thumb_, ArtistImagesModel::Role::ThumbURL);
			item->setData (imageItem.Full_, ArtistImagesModel::Role::FullURL);
			item->setData (imageItem.Title_, ArtistImagesModel::Role::Title);
			item->setData (imageItem.Author_, ArtistImagesModel::Role::Author);
			item->setData (imageItem.Date_, ArtistImagesModel::Role::Date);
			rows << item;
		}
		if (!rows.isEmpty ())
			ArtistImages_->invisibleRootItem ()->appendRows (rows);

		emit artistNameChanged (GetArtistName ());
		emit artistImageURLChanged (GetArtistImageURL ());
		emit artistBigImageURLChanged (GetArtistBigImageURL ());
		emit artistTagsChanged (GetArtistTags ());
		emit artistInfoChanged (GetArtistInfo ());
		emit artistPageURLChanged (GetArtistPageURL ());
	}

	QString BioPropProxy::GetArtistName () const
	{
		return Bio_.BasicInfo_.Name_;
	}

	QUrl BioPropProxy::GetArtistImageURL () const
	{
		return Bio_.BasicInfo_.Image_;
	}

	QUrl BioPropProxy::GetArtistBigImageURL () const
	{
		return Bio_.BasicInfo_.LargeImage_;
	}

	QString BioPropProxy::GetArtistTags () const
	{
		return CachedTags_;
	}

	QString BioPropProxy::GetArtistInfo () const
	{
		return CachedInfo_;
	}

	QUrl BioPropProxy::GetArtistPageURL () const
	{
		return Bio_.BasicInfo_.Page_;
	}

	QObject* BioPropProxy::GetArtistImagesModel () const
	{
		return ArtistImages_;
	}
}
}
