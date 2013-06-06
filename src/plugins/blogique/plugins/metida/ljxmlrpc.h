/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include <functional>
#include <QObject>
#include <QQueue>
#include <QPair>
#include <QDomElement>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "core.h"
#include "profiletypes.h"
#include "ljaccount.h"

namespace LeechCraft
{
namespace Blogique
{
namespace Metida
{
	class LJFriendEntry;

	class LJXmlRPC : public QObject
	{
		Q_OBJECT

		LJAccount *Account_;
		QQueue<std::function<void (const QString&)>> ApiCallQueue_;

		const int BitMaskForFriendsOnlyComments_;
		const int MaxGetEventsCount_;

		QHash<QNetworkReply*, int> Reply2Skip_;

		enum class RequestType
		{
			Update,
			Post,
			RecentComments,
			Tags
		};
		QHash<QNetworkReply*, RequestType> Reply2RequestType_;
		QMap<QPair<int, int>, LJCommentEntry> Id2CommentEntry_;

	public:
		LJXmlRPC (LJAccount *acc, QObject *parent = 0);

		void Validate (const QString& login, const QString& pass);

		void AddNewFriend (const QString& username,
				const QString& bgcolor, const QString& fgcolor, uint groupId);
		void DeleteFriend (const QString& username);

		void AddGroup (const QString& name, bool isPublic, int id);
		void DeleteGroup (int id);

		void UpdateProfileInfo ();

		void Submit (const LJEvent& event);
		void BackupEvents ();
		void GetLastEvents (int count);
		void GetChangedEvents (const QDateTime& dt);
		void GetEventsByDate (const QDate& date);

		void RemoveEvent (const LJEvent& event);
		void UpdateEvent (const LJEvent& event);

		void RequestStatistics ();

		void RequestLastInbox ();
		void RequestRecentCommments ();

		void RequestTags ();
	private:
		void GenerateChallenge () const;
		void ValidateAccountData (const QString& login,
				const QString& pass, const QString& challenge);
		void RequestFriendsInfo (const QString& login,
				const QString& pass, const QString& challenge);
		void AddNewFriendRequest (const QString& username,
				const QString& bgcolor, const QString& fgcolor,
				int groupId, const QString& challenge);
		void DeleteFriendRequest (const QString& usernames,
				const QString& challenge);

		void AddGroupRequest (const QString& name, bool isPublic, int id,
				const QString& challenge);
		void DeleteGroupRequest (int id, const QString& challenge);

		void PostEventRequest (const LJEvent& event, const QString& challenge);
		void RemoveEventRequest (const LJEvent& event, const QString& challenge);
		void UpdateEventRequest (const LJEvent& event, const QString& challenge);

		void BackupEventsRequest (int skip, const QString& challenge);

		void GetLastEventsRequest (int count, const QString& challenge);
		void GetChangedEventsRequest (const QDateTime& dt, const QString& challenge);
		void GetEventsByDateRequest (const QDate& date, const QString& challenge);
		void GetParticularEventRequest (int id, RequestType prt,
				const QString& challenge);
		void GetMultipleEventsRequest (const QStringList& ids, RequestType rt,
				const QString& challenge);

		void BlogStatisticsRequest (const QString& challenge);

		void InboxRequest (const QString& challenge);
		void RecentCommentsRequest (const QString& challenge);

		void GetUserTagsRequest (const QString& challenge);

		void ParseForError (const QByteArray& content);
		void ParseFriends (const QDomDocument& doc);

		QList<LJEvent> ParseFullEvents (const QDomDocument& doc);

		QMap<QDate, int> ParseStatistics (const QDomDocument& doc);

	private slots:
		void handleChallengeReplyFinished ();
		void handleValidateReplyFinished ();
		void handleRequestFriendsInfoFinished ();
		void handleAddNewFriendReplyFinished ();
		void handleReplyWithProfileUpdate ();
		void handlePostEventReplyFinished ();
		void handleBackupEventsReplyFinished ();
		void handleGotEventsReplyFinished ();
		void handleRemoveEventReplyFinished ();
		void handleUpdateEventReplyFinished ();
		void handleGetParticularEventReplyFinished ();
		void handleGetMultipleEventsReplyFinished ();
		void handleBlogStatisticsReplyFinished ();
		void handleInboxReplyFinished ();
		void handleRecentCommentsReplyFinished ();
		void handleGetUserTagsReplyFinished ();

		void handleNetworkError (QNetworkReply::NetworkError error);

	signals:
		void validatingFinished (bool success);
		void profileUpdated (const LJProfileData& profile);
		void error (int code, const QString& msg,
				const QString& localizedMsg);
		void networkError (int code, const QString& msg);

		void eventPosted (const QList<LJEvent>& events);
		void eventUpdated (const QList<LJEvent>& events);
		void eventRemoved (int itemId);

		void gotEvents2Backup (const QList<LJEvent>& events);
		void gettingEvents2BackupFinished ();

		void gotEvents (const QList<LJEvent>& events);

		void gotStatistics (const QMap<QDate, int>& statistics);

		void unreadMessagesExist (bool exists);
		void gotRecentComments (const QList<LJCommentEntry>& comments);
		void gotTags (const QHash<QString, int>& tags);
	};
}
}
}
