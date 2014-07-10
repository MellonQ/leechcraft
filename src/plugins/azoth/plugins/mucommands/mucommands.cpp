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

#include "mucommands.h"
#include <QIcon>
#include <interfaces/azoth/iclentry.h>
#include <interfaces/azoth/iproxyobject.h>
#include "commands.h"

namespace LeechCraft
{
namespace Azoth
{
namespace MuCommands
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		CoreProxy_ = proxy;
	}

	void Plugin::SecondInit ()
	{
		Names_ = StaticCommand
		{
			{ "/names" },
			[this] (ICLEntry *e, const QString& t) { return HandleNames (AzothProxy_, e, t); }
		};

		ListUrls_ = StaticCommand
		{
			{ "/urls" },
			[this] (ICLEntry *e, const QString& t) { return ListUrls (AzothProxy_, e, t); }
		};

		OpenUrl_ = StaticCommand
		{
			{ "/openurl" },
			[this] (ICLEntry *e, const QString& t)
				{ return OpenUrl (CoreProxy_, AzothProxy_, e, t, OnlyHandle); }
		};

		FetchUrl_ = StaticCommand
		{
			{ "/fetchurl" },
			[this] (ICLEntry *e, const QString& t)
				{ return OpenUrl (CoreProxy_, AzothProxy_, e, t, OnlyDownload); }
		};

		VCard_ = StaticCommand
		{
			{ "/vcard" },
			[this] (ICLEntry *e, const QString& t) { return ShowVCard (AzothProxy_, e, t); }
		};

		Version_ = StaticCommand
		{
			{ "/version" },
			[this] (ICLEntry *e, const QString& t) { return ShowVersion (AzothProxy_, e, t); }
		};

		Time_ = StaticCommand
		{
			{ "/time" },
			[this] (ICLEntry *e, const QString& t) { return ShowTime (AzothProxy_, e, t); }
		};

		ChangeNick_ = StaticCommand
		{
			{ "/nick" },
			[this] (ICLEntry *e, const QString& t) { return ChangeNick (AzothProxy_, e, t); }
		};

		ChangeSubject_ = StaticCommand
		{
			{ "/subject" },
			[this] (ICLEntry *e, const QString& t) { return ChangeSubject (AzothProxy_, e, t); }
		};

		LeaveMuc_ = StaticCommand
		{
			{ "/leave", "/part" },
			[this] (ICLEntry *e, const QString& t) { return LeaveMuc (AzothProxy_, e, t); }
		};

		RejoinMuc_ = StaticCommand
		{
			{ "/rejoin" },
			[this] (ICLEntry *e, const QString& t) { return RejoinMuc (AzothProxy_, e, t); }
		};
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Azoth.MuCommands";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Azoth MuCommands";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Provides some common commands, both for conferences and for private chats, for Azoth.");
	}

	QIcon Plugin::GetIcon () const
	{
		return {};
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Plugins.Azoth.Plugins.IGeneralPlugin";
		return result;
	}

	StaticCommands_t Plugin::GetStaticCommands (ICLEntry *entry)
	{
		if (entry->GetEntryType () != ICLEntry::ETMUC)
			return {};

		return
		{
			Names_, ListUrls_, OpenUrl_, FetchUrl_, VCard_, Version_, Time_,
			ChangeNick_, ChangeSubject_, LeaveMuc_, RejoinMuc_
		};
	}

	void Plugin::initPlugin (QObject *proxy)
	{
		AzothProxy_ = qobject_cast<IProxyObject*> (proxy);
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_azoth_mucommands, LeechCraft::Azoth::MuCommands::Plugin);
