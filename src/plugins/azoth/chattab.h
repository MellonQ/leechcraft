/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef PLUGINS_AZOTH_CHATTAB_H
#define PLUGINS_AZOTH_CHATTAB_H
#include <QWidget>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QDateTime>
#include <interfaces/iinfo.h>
#include <interfaces/imultitabs.h>
#include "ui_chattab.h"

namespace LeechCraft
{
namespace Plugins
{
namespace Azoth
{
	namespace Plugins
	{
		struct EntryStatus;
		class ICLEntry;
		class IMUCEntry;
		class IMessage;
	}

	class ChatTab : public QWidget
				  , public IMultiTabsWidget
	{
		Q_OBJECT
		Q_INTERFACES (IMultiTabsWidget)

		static QObject *S_ParentMultiTabs_;

		Ui::ChatTab Ui_;

		QByteArray EntryID_;
		QString Variant_;

		QRegExp LinkRegexp_;
		QRegExp ImageRegexp_;

		QColor BgColor_;
		QList<QColor> NickColors_;

		QList<QString> MsgHistory_;
		int CurrentHistoryPosition_;
		QStringList AvailableNickList_;
		int CurrentNickIndex_;
		int LastSpacePosition_;
		QString NickFirstPart_;

		int NumUnreadMsgs_;

		QIcon TabIcon_;

		bool IsMUC_;

		bool HasBeenAppended_;
	public:
		static void SetParentMultiTabs (QObject*);

		ChatTab (const QByteArray&, const QString&, QWidget* = 0);
		/** Is called after the tab has been added to the tabwidget so
		 * that it could set its icon and various other stuff.
		 */
		void HasBeenAdded ();

		QList<QAction*> GetTabBarContextMenuActions () const;
		QObject* ParentMultiTabs () const;
		void NewTabRequested ();
		QToolBar* GetToolBar () const;
		void Remove ();
		void TabMadeCurrent ();
	private slots:
		void clearAvailableNick ();
		void messageSend ();
		void nickComplete ();
		void on_MsgEdit__textChanged ();
		void on_SubjectButton__toggled (bool);
		void on_SubjChange__released ();
		void handleEntryMessage (QObject*);
		void handleStatusChanged (const Plugins::EntryStatus&, const QString&);
		void handleViewLinkClicked (const QUrl&);
		void scrollToEnd ();
		void handleHistoryUp ();
		void handleHistoryDown ();
	private:
		template<typename T>
		T* GetEntry () const;
		void CheckMUC ();
		void HandleMUC ();
		QStringList GetMUCParticipants () const;

		void ReformatTitle ();

		/** Appends the message to the message view area.
		 */
		void AppendMessage (Plugins::IMessage*);

		QString FormatDate (QDateTime, Plugins::IMessage*);
		QString FormatNickname (QString, Plugins::IMessage*);
		QString FormatBody (QString, Plugins::IMessage*);

		/** Processes the outgoing messages, replacing /nick with calls
		 * to the entity to change nick, for example, etc.
		 *
		 * If this function returns true, processing (and sending) the
		 * message should be aborted.
		 *
		 * @return true if the processing should be aborted, false
		 * otherwise.
		 */
		bool ProcessOutgoingMsg (Plugins::ICLEntry*, QString&);

		void GenerateColors ();

		/** Updates the tab icon and other usages of state icon from the
		 * TabIcon_.
		 */
		void UpdateStateIcon ();
	signals:
		void changeTabName (QWidget*, const QString&);
		void changeTabIcon (QWidget*, const QIcon&);
		void needToClose (ChatTab*);
		void clearUnreadMsgCount (QObject*);

		// Hooks
		void hookFormatDateTime (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab,
				QDateTime dateTime,
				QObject *message);
		void hookFormatNickname (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab,
				QString nick,
				QObject *message);
		void hookFormatBodyBegin (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab,
				QString body,
				QObject *message);
		void hookFormatBodyEnd (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab,
				QString body,
				QObject *message);
		void hookMadeCurrent (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab);
		void hookMessageWillCreated (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab,
				int type,
				QString variant,
				QString text);
		void hookMessageCreated (LeechCraft::IHookProxy_ptr proxy,
				QObject *chatTab,
				QObject *message);
	};

	typedef QPointer<ChatTab> ChatTab_ptr;
}
}
}

#endif
