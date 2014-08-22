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

#include "standardstylesource.h"
#include <QTextDocument>
#include <QWebElement>
#include <QWebFrame>
#include <QApplication>
#include <QtDebug>
#include <util/sys/resourceloader.h>
#include <util/util.h>
#include <interfaces/azoth/imessage.h>
#include <interfaces/azoth/iadvancedmessage.h>
#include <interfaces/azoth/irichtextmessage.h>
#include <interfaces/azoth/iaccount.h>
#include <interfaces/azoth/imucentry.h>
#include <interfaces/azoth/iproxyobject.h>

namespace LeechCraft
{
namespace Azoth
{
namespace StandardStyles
{
	StandardStyleSource::StandardStyleSource (IProxyObject *proxy, QObject *parent)
	: QObject (parent)
	, StylesLoader_ (new Util::ResourceLoader ("azoth/styles/standard/", this))
	, Proxy_ (proxy)
	{
		StylesLoader_->AddGlobalPrefix ();
		StylesLoader_->AddLocalPrefix ();

		StylesLoader_->SetCacheParams (256, 0);
	}

	QAbstractItemModel* StandardStyleSource::GetOptionsModel() const
	{
		return StylesLoader_->GetSubElemModel ();
	}

	QUrl StandardStyleSource::GetBaseURL (const QString& pack) const
	{
		const auto& path = StylesLoader_->GetPath (QStringList (pack + "/viewcontents.html"));
		if (path.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "empty base URL for"
					<< pack;
			return QUrl ();
		}

		return QUrl::fromLocalFile (QFileInfo (path).absolutePath ());
	}

	QString StandardStyleSource::GetHTMLTemplate (const QString& pack,
			const QString&, QObject *entryObj, QWebFrame*) const
	{
		Coloring2Colors_.clear ();
		if (pack != LastPack_)
		{
			LastPack_ = pack;
			StylesLoader_->FlushCache ();
		}

		ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);

		Util::QIODevice_ptr dev;
		if (entry && entry->GetEntryType () == ICLEntry::EntryType::MUC)
			dev = StylesLoader_->Load (QStringList (pack + "/viewcontents.muc.html"));
		if (!dev)
			dev = StylesLoader_->Load (QStringList (pack + "/viewcontents.html"));

		if (!dev)
		{
			qWarning () << Q_FUNC_INFO
					<< "could not load HTML template for pack"
					<< pack;
			return QString ();
		}

		if (!dev->open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open source file for"
					<< pack + "/viewcontents.html"
					<< dev->errorString ();
			return QString ();
		}

		QString data = QString::fromUtf8 (dev->readAll ());
		data.replace ("BACKGROUNDCOLOR",
				QApplication::palette ().color (QPalette::Base).name ());
		data.replace ("FOREGROUNDCOLOR",
				QApplication::palette ().color (QPalette::Text).name ());
		data.replace ("LINKCOLOR",
				QApplication::palette ().color (QPalette::Link).name ());
		data.replace ("HIGHLIGHTCOLOR",
				Proxy_->GetSettingsManager ()->
						property ("HighlightColor").toString ());
		return data;
	}

	namespace
	{
		QString WrapNickPart (const QString& part,
				const QString& color, IMessage::Type type)
		{
			const QString& pre = type == IMessage::Type::MUCMessage ?
					"<span class='nickname' style='color: " + color + "'>" :
					"<span class='nickname'>";
			return pre +
#if QT_VERSION < 0x050000
					Qt::escape (part) +
#else
					part.toHtmlEscaped () +
#endif
					"</span>";
		}
	}

	bool StandardStyleSource::AppendMessage (QWebFrame *frame,
			QObject *msgObj, const ChatMsgAppendInfo& info)
	{
		QObject *azothSettings = Proxy_->GetSettingsManager ();
		const auto& colors = CreateColors (frame->metaData ().value ("coloring"), frame);

		const bool isHighlightMsg = info.IsHighlightMsg_;
		const bool isActiveChat = info.IsActiveChat_;

		const QString& msgId = GetMessageID (msgObj);

		IMessage *msg = qobject_cast<IMessage*> (msgObj);
		ICLEntry *other = qobject_cast<ICLEntry*> (msg->OtherPart ());
		QString entryName = other ?
#if QT_VERSION < 0x050000
				Qt::escape (other->GetEntryName ()) :
#else
				other->GetEntryName ().toHtmlEscaped () :
#endif
				QString ();
		if (msg->GetMessageType () == IMessage::Type::ChatMessage &&
				Proxy_->GetSettingsManager ()->property ("ShowNormalChatResources").toBool () &&
				!msg->GetOtherVariant ().isEmpty ())
			entryName += '/' + msg->GetOtherVariant ();

		connect (msgObj,
				SIGNAL (destroyed ()),
				this,
				SLOT (handleMessageDestroyed ()),
				Qt::UniqueConnection);

		IAdvancedMessage *advMsg = qobject_cast<IAdvancedMessage*> (msgObj);
		if (msg->GetDirection () == IMessage::Direction::Out &&
				advMsg &&
				!advMsg->IsDelivered ())
		{
			connect (msgObj,
					SIGNAL (messageDelivered ()),
					this,
					SLOT (handleMessageDelivered ()));
			connect (frame,
					SIGNAL (destroyed (QObject*)),
					this,
					SLOT (handleFrameDestroyed ()),
					Qt::UniqueConnection);
			Msg2Frame_ [msgObj] = frame;
		}

		const QString& nickColor = Proxy_->GetNickColor (entryName, colors);

		IRichTextMessage *richMsg = qobject_cast<IRichTextMessage*> (msgObj);
		QString body;
		if (richMsg && info.UseRichTextBody_)
			body = richMsg->GetRichBody ();
		if (body.isEmpty ())
			body = msg->GetEscapedBody ();

		body = Proxy_->FormatBody (body, msg->GetQObject ());

		const QString dateBegin ("<span class='datetime'>");
		const QString dateEnd ("</span>");

		const QString& preNick =
				WrapNickPart (azothSettings->property ("PreNickText").toString (),
						nickColor, msg->GetMessageType ());
		const QString& postNick =
				WrapNickPart (azothSettings->property ("PostNickText").toString (),
						nickColor, msg->GetMessageType ());

		QString divClass;
		QString statusIconName;

		QString string = dateBegin + '[' +
				Proxy_->FormatDate (msg->GetDateTime (), msg->GetQObject ()) +
				']' + dateEnd;
		string.append (' ');
		switch (msg->GetDirection ())
		{
		case IMessage::Direction::In:
		{
			switch (msg->GetMessageType ())
			{
			case IMessage::Type::ChatMessage:
				statusIconName = "notification_chat_receive";
				divClass = msg->GetDirection () == IMessage::Direction::In ?
					"msgin" :
					"msgout";
			case IMessage::Type::MUCMessage:
			{
				statusIconName = "notification_chat_receive";

				if (body.startsWith ("/me "))
				{
					body = body.mid (3);
					string.append ("* ");
					string.append (Proxy_->FormatNickname (entryName, msg->GetQObject (), nickColor));
					string.append (' ');
					divClass = "slashmechatmsg";
				}
				else
				{
					string.append (preNick);
					string.append (Proxy_->FormatNickname (entryName, msg->GetQObject (), nickColor));
					string.append (postNick);
					string.append (' ');
					if (divClass.isEmpty ())
						divClass = isHighlightMsg ?
								"highlightchatmsg" :
								"chatmsg";
				}
				break;
			}
			case IMessage::Type::EventMessage:
				statusIconName = "notification_chat_info";
				string.append ("! ");
				divClass = "eventmsg";
				break;
			case IMessage::Type::StatusMessage:
				statusIconName = "notification_chat_info";
				string.append ("* ");
				divClass = "statusmsg";
				break;
			case IMessage::Type::ServiceMessage:
				statusIconName = "notification_chat_info";
				string.append ("* ");
				divClass = "servicemsg";
				break;
			}
			break;
		}
		case IMessage::Direction::Out:
		{
			statusIconName = "notification_chat_send";
			if (advMsg && advMsg->IsDelivered ())
				statusIconName = "notification_chat_delivery_ok";

			const auto entry = other ?
					qobject_cast<IMUCEntry*> (other->GetParentCLEntry ()) :
					nullptr;
			const auto acc = other ?
					qobject_cast<IAccount*> (other->GetParentAccount ()) :
					nullptr;
			const QString& nick = entry ?
					entry->GetNick () :
					acc->GetOurNick ();
			if (body.startsWith ("/leechcraft "))
			{
				body = body.mid (12);
				string.append ("* ");
			}
			else if (body.startsWith ("/me ") &&
					msg->GetMessageType () != IMessage::Type::MUCMessage)
			{
				body = body.mid (3);
				string.append ("* ");
				string.append (Proxy_->FormatNickname (nick, msg->GetQObject (), nickColor));
				string.append (' ');
				divClass = "slashmechatmsg";
			}
			else
			{
				string.append (preNick);
				string.append (Proxy_->FormatNickname (nick, msg->GetQObject (), nickColor));
				string.append (postNick);
				string.append (' ');
			}
			if (divClass.isEmpty ())
				divClass = "msgout";
			break;
		}
		}

		if (!statusIconName.isEmpty ())
			string.prepend (QString ("<img src='%1' style='max-width: 1em; max-height: 1em;' id='%2' class='deliveryStatusIcon' />")
					.arg (GetStatusImage (statusIconName))
					.arg (msgId));
		string.append (body);

		QWebElement elem = frame->findFirstElement ("body");

		if (msg->GetMessageType () == IMessage::Type::ChatMessage ||
			msg->GetMessageType () == IMessage::Type::MUCMessage)
		{
			const auto isRead = Proxy_->IsMessageRead (msgObj);
			if (!isActiveChat &&
					!isRead && IsLastMsgRead_.value (frame, false))
			{
				auto hr = elem.findFirst ("hr[class=\"lastSeparator\"]");
				if (!hr.isNull ())
					hr.removeFromDocument ();
				elem.appendInside ("<hr class=\"lastSeparator\" />");
			}
			IsLastMsgRead_ [frame] = isRead;
		}

		elem.appendInside (QString ("<div class='%1' style='word-wrap: break-word;'>%2</div>")
					.arg (divClass)
					.arg (string));
		return true;
	}

	void StandardStyleSource::FrameFocused (QWebFrame *frame)
	{
		IsLastMsgRead_ [frame] = true;
	}

	QStringList StandardStyleSource::GetVariantsForPack (const QString&)
	{
		return QStringList ();
	}

	QList<QColor> StandardStyleSource::CreateColors (const QString& scheme, QWebFrame *frame)
	{
		QColor bgColor;

		const auto js = "window.getComputedStyle(document.body) ['background-color']";
		auto res = frame->evaluateJavaScript (js).toString ();
		res.remove (" ");
		res.remove ("rgb(");
		res.remove (")");
		const auto& vals = res.split (',', QString::SkipEmptyParts);

		if (vals.size () == 3)
			bgColor.setRgb (vals.value (0).toInt (),
					vals.value (1).toInt (), vals.value (2).toInt ());

		const auto& mangledScheme = scheme + bgColor.name ();

		if (!Coloring2Colors_.contains (mangledScheme))
			Coloring2Colors_ [mangledScheme] = Proxy_->GenerateColors (scheme, bgColor);

		return Coloring2Colors_ [mangledScheme];
	}

	QString StandardStyleSource::GetMessageID (QObject *msgObj)
	{
		return QString::number (reinterpret_cast<uintptr_t> (msgObj));
	}

	QString StandardStyleSource::GetStatusImage (const QString& statusIconName)
	{
		const QString& fullName = Proxy_->GetSettingsManager ()->
				property ("SystemIcons").toString () + '/' + statusIconName;
		const QString& statusIconPath = Proxy_->
				GetResourceLoader (IProxyObject::PRLSystemIcons)->GetIconPath (fullName);
		const QImage& img = QImage (statusIconPath);
		return Util::GetAsBase64Src (img);
	}

	void StandardStyleSource::handleMessageDelivered ()
	{
		QWebFrame *frame = Msg2Frame_.take (sender ());
		if (!frame)
			return;

		const QString& msgId = GetMessageID (sender ());
		QWebElement elem = frame->findFirstElement ("img[id=\"" + msgId + "\"]");
		elem.setAttribute ("src", GetStatusImage ("notification_chat_delivery_ok"));

		disconnect (sender (),
				SIGNAL (messageDelivered ()),
				this,
				SLOT (handleMessageDelivered ()));
	}

	void StandardStyleSource::handleMessageDestroyed ()
	{
		Msg2Frame_.remove (sender ());
	}

	void StandardStyleSource::handleFrameDestroyed ()
	{
		IsLastMsgRead_.remove (static_cast<QWebFrame*> (sender ()));
		const QObject *snd = sender ();
		for (QHash<QObject*, QWebFrame*>::iterator i = Msg2Frame_.begin ();
				i != Msg2Frame_.end (); )
			if (i.value () == snd)
				i = Msg2Frame_.erase (i);
			else
				++i;
	}
}
}
}
