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

#include "lmp.h"
#include <QIcon>
#include <QFileInfo>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QtDeclarative>
#include <QGraphicsEffect>
#include <gst/gst.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include <interfaces/entitytesthandleresult.h>
#include <util/util.h>
#include "playertab.h"
#include "player.h"
#include "xmlsettingsmanager.h"
#include "core.h"
#include "rootpathsettingsmanager.h"
#include "collectionstatsdialog.h"
#include "artistbrowsertab.h"
#include "progressmanager.h"
#include "volumenotifycontroller.h"
#include "radiomanager.h"

typedef QList<QPair<QString, QUrl>> CustomStationsList_t;
Q_DECLARE_METATYPE (CustomStationsList_t);

namespace LeechCraft
{
namespace LMP
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("lmp");

#ifdef Q_OS_MAC
		if (qgetenv ("GST_PLUGIN_SYSTEM_PATH").isEmpty ())
			qputenv ("GST_PLUGIN_SYSTEM_PATH",
					QCoreApplication::applicationDirPath ().toUtf8 () + "/../PlugIns/gstreamer");

		qputenv ("GST_REGISTRY_FORK", "no");
#endif

		gint argc = 1;
		gchar *argvarr [] = { "leechcraft", nullptr };
		gchar **argv = argvarr;
		gst_init (&argc, &argv);

		qRegisterMetaType<QList<QPair<QString, QUrl>>> ("QList<QPair<QString, QUrl>>");
		qRegisterMetaTypeStreamOperators<QList<QPair<QString, QUrl>>> ();

		XSD_.reset (new Util::XmlSettingsDialog);
		XSD_->RegisterObject (&XmlSettingsManager::Instance (), "lmpsettings.xml");

		qmlRegisterType<QGraphicsBlurEffect> ("Effects", 1, 0, "Blur");
		qmlRegisterType<QGraphicsColorizeEffect> ("Effects", 1, 0, "Colorize");
		qmlRegisterType<QGraphicsDropShadowEffect> ("Effects", 1, 0, "DropShadow");
		qmlRegisterType<QGraphicsOpacityEffect> ("Effects", 1, 0, "OpacityEffect");

		PlayerTC_ =
		{
			GetUniqueID () + "_player",
			"LMP",
			GetInfo (),
			GetIcon (),
			40,
			TFSingle | TFByDefault | TFOpenableByRequest
		};

		ArtistBrowserTC_ =
		{
			GetUniqueID () + "_artistBrowser",
			tr ("Artist browser"),
			tr ("Allows one to browse information about different artists."),
			QIcon ("lcicons:/lmp/resources/images/lmp_artist_browser.svg"),
			35,
			TFSuggestOpening | TFOpenableByRequest
		};

		Core::Instance ().SetProxy (proxy);
		Core::Instance ().PostInit ();

		auto mgr = new RootPathSettingsManager (this);
		XSD_->SetDataSource ("RootPathsView", mgr->GetModel ());

		PlayerTab_ = new PlayerTab (PlayerTC_, this);

		connect (PlayerTab_,
				SIGNAL (removeTab (QWidget*)),
				this,
				SIGNAL (removeTab (QWidget*)));
		connect (PlayerTab_,
				SIGNAL (changeTabName (QWidget*, QString)),
				this,
				SIGNAL (changeTabName (QWidget*, QString)));
		connect (PlayerTab_,
				SIGNAL (raiseTab (QWidget*)),
				this,
				SIGNAL (raiseTab (QWidget*)));
		connect (PlayerTab_,
				SIGNAL (gotEntity (LeechCraft::Entity)),
				this,
				SIGNAL (gotEntity (LeechCraft::Entity)));
		connect (&Core::Instance (),
				SIGNAL (gotEntity (LeechCraft::Entity)),
				this,
				SIGNAL (gotEntity (LeechCraft::Entity)));
		connect (&Core::Instance (),
				SIGNAL (artistBrowseRequested (QString)),
				this,
				SLOT (handleArtistBrowseRequested (QString)));

		connect (PlayerTab_,
				SIGNAL (fullRaiseRequested ()),
				this,
				SLOT (handleFullRaiseRequested ()));

		ActionRescan_ = new QAction (tr ("Rescan collection"), this);
		ActionRescan_->setProperty ("ActionIcon", "view-refresh");
		connect (ActionRescan_,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (rescan ()));

		ActionCollectionStats_ = new QAction (tr ("Collection statistics"), this);
		ActionCollectionStats_->setProperty ("ActionIcon", "view-statistics");
		connect (ActionCollectionStats_,
				SIGNAL (triggered ()),
				this,
				SLOT (showCollectionStats ()));

		InitShortcuts ();
	}

	void Plugin::SecondInit ()
	{
		for (const auto& e : GlobAction2Entity_)
			emit gotEntity (e);

		Core::Instance ().InitWithOtherPlugins ();
		PlayerTab_->InitWithOtherPlugins ();
	}

	void Plugin::SetShortcut (const QString& id, const QKeySequences_t& sequences)
	{
		if (!GlobAction2Entity_.contains (id))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown id"
					<< id;
			return;
		}

		auto& e = GlobAction2Entity_ [id];
		e.Additional_ ["Shortcut"] = QVariant::fromValue (sequences.value (0));
		emit gotEntity (e);
	}

	QMap<QString, ActionInfo> Plugin::GetActionInfo () const
	{
		return GlobAction2Info_;
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.LMP";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "LMP";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("LeechCraft Music Player.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon ("lcicons:/lmp/resources/images/lmp.svg");
		return icon;
	}

	TabClasses_t Plugin::GetTabClasses () const
	{
		TabClasses_t tcs;
		tcs << PlayerTC_;
		tcs << ArtistBrowserTC_;
		return tcs;
	}

	void Plugin::TabOpenRequested (const QByteArray& tc)
	{
		if (tc == PlayerTC_.TabClass_)
		{
			emit addNewTab ("LMP", PlayerTab_);
			emit raiseTab (PlayerTab_);
		}
		else if (tc == ArtistBrowserTC_.TabClass_)
			handleArtistBrowseRequested (QString ());
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< tc;
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return XSD_;
	}

	EntityTestHandleResult Plugin::CouldHandle (const Entity& e) const
	{
		if (e.Mime_ == "x-leechcraft/power-state-changed")
			return EntityTestHandleResult (EntityTestHandleResult::PHigh);

		QString path = e.Entity_.toString ();
		const QUrl& url = e.Entity_.toUrl ();
		if (path.isEmpty () &&
					url.isValid () &&
					url.scheme () == "file")
			path = url.toLocalFile ();

		const auto& goodExt = XmlSettingsManager::Instance ()
				.property ("TestExtensions").toString ()
				.split (' ', QString::SkipEmptyParts);
		const QFileInfo fi (path);
		if ((fi.exists () && goodExt.contains (fi.suffix ())) ||
				e.Additional_ ["Action"] == "AudioEnqueuePlay" ||
				e.Additional_ ["Action"] == "AudioEnqueue")
			return EntityTestHandleResult (EntityTestHandleResult::PHigh);
		else
			return EntityTestHandleResult ();
	}

	void Plugin::Handle (Entity e)
	{
		auto player = PlayerTab_->GetPlayer ();

		if (e.Mime_ == "x-leechcraft/power-state-changed")
		{
			if (e.Entity_ == "Sleeping")
			{
				player->SavePlayState (true);
				player->setPause ();
			}
			else if (e.Entity_ == "WokeUp")
			{
				player->RestorePlayState ();
				Core::Instance ().GetRadioManager ()->HandleWokeUp ();
			}

			return;
		}

		QString path = e.Entity_.toString ();
		const QUrl& url = e.Entity_.toUrl ();
		if (path.isEmpty () &&
					url.isValid () &&
					url.scheme () == "file")
			path = url.toLocalFile ();

		if (e.Parameters_ & Internal)
		{
			/* TODO
			auto obj = Phonon::createPlayer (Phonon::NotificationCategory, path);
			obj->play ();
			connect (obj,
					SIGNAL (finished ()),
					obj,
					SLOT (deleteLater ()));
					*/
			return;
		}

		if (!(e.Parameters_ & FromUserInitiated))
			return;

		player->Enqueue ({ AudioSource (url) }, false);

		if (e.Additional_ ["Action"] == "AudioEnqueuePlay")
			player->AddToOneShotQueue (url);
	}

	QList<QAction*> Plugin::GetActions (ActionsEmbedPlace) const
	{
		return QList<QAction*> ();
	}

	QMap<QString, QList<QAction*>> Plugin::GetMenuActions () const
	{
		const auto& name = GetName ();

		QMap<QString, QList<QAction*>> result;
		result [name] << ActionRescan_;
		result [name] << ActionCollectionStats_;
		return result;
	}

	void Plugin::RecoverTabs (const QList<LeechCraft::TabRecoverInfo>& infos)
	{
		for (const auto& recInfo : infos)
		{
			QDataStream stream (recInfo.Data_);
			QByteArray key;
			stream >> key;

			if (recInfo.Data_ == "playertab")
			{
				for (const auto& pair : recInfo.DynProperties_)
					PlayerTab_->setProperty (pair.first, pair.second);

				TabOpenRequested (PlayerTC_.TabClass_);
			}
			else if (key == "artistbrowser")
			{
				QString artist;
				stream >> artist;
				handleArtistBrowseRequested (artist, recInfo.DynProperties_);
			}
			else
				qWarning () << Q_FUNC_INFO
						<< "unknown context"
						<< recInfo.Data_;
		}
	}

	QSet<QByteArray> Plugin::GetExpectedPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.LMP.General";
		result << "org.LeechCraft.LMP.CollectionSync";
		result << "org.LeechCraft.LMP.CloudStorage";
		result << "org.LeechCraft.LMP.PlaylistProvider";
		return result;
	}

	void Plugin::AddPlugin (QObject *plugin)
	{
		Core::Instance ().AddPlugin (plugin);
	}

	QAbstractItemModel* Plugin::GetRepresentation () const
	{
		return Core::Instance ().GetProgressManager ()->GetModel ();
	}

	void Plugin::InitShortcuts ()
	{
		Entity e = Util::MakeEntity (QVariant (), QString (), 0,
				"x-leechcraft/global-action-register");
		e.Additional_ ["Receiver"] = QVariant::fromValue<QObject*> (PlayerTab_->GetPlayer ());
		auto initShortcut = [&e, this] (const QByteArray& method, const QKeySequence& seq)
		{
			Entity thisE = e;
			thisE.Additional_ ["ActionID"] = "LMP_Global_" + method;
			thisE.Additional_ ["Method"] = method;
			thisE.Additional_ ["Shortcut"] = QVariant::fromValue (seq);
			GlobAction2Entity_ ["LMP_Global_" + method] = thisE;
		};
		initShortcut (SLOT (togglePause ()), QString ("Meta+C"));
		initShortcut (SLOT (previousTrack ()), QString ("Meta+V"));
		initShortcut (SLOT (nextTrack ()), QString ("Meta+B"));
		initShortcut (SLOT (stop ()), QString ("Meta+X"));

		auto output = PlayerTab_->GetPlayer ()->GetAudioOutput ();
		auto controller = new VolumeNotifyController (output, PlayerTab_->GetPlayer ());
		e.Additional_ ["Receiver"] = QVariant::fromValue<QObject*> (controller);
		initShortcut (SLOT (volumeUp ()), {});
		initShortcut (SLOT (volumeDown ()), {});

		e.Additional_ ["Receiver"] = QVariant::fromValue<QObject*> (PlayerTab_);
		initShortcut (SLOT (handleLoveTrack ()), QString ("Meta+L"));
		initShortcut (SLOT (notifyCurrentTrack ()), {});

		auto proxy = Core::Instance ().GetProxy ();
		auto setInfo = [this, proxy] (const QByteArray& method,
				const QString& userText, const QString& icon)
		{
			const auto& id = "LMP_Global_" + method;
			const auto& seq = GlobAction2Entity_ [id].Additional_ ["Shortcut"].value<QKeySequence> ();
			GlobAction2Info_ [id] = { userText, seq, proxy->GetIcon (icon) };
		};
		setInfo (SLOT (togglePause ()), tr ("Play/pause"), "media-playback-start");
		setInfo (SLOT (previousTrack ()), tr ("Previous track"), "media-skip-backward");
		setInfo (SLOT (nextTrack ()), tr ("Next track"), "media-skip-forward");
		setInfo (SLOT (stop ()), tr ("Stop playback"), "media-playback-stop");
		setInfo (SLOT (handleLoveTrack ()), tr ("Love track"), "emblem-favorite");
		setInfo (SLOT (notifyCurrentTrack ()),
				tr ("Notify about current track"),
				"dialog-information");
		setInfo (SLOT (volumeUp ()), tr ("Increase volume"), "audio-volume-high");
		setInfo (SLOT (volumeDown ()), tr ("Decrease volume"), "audio-volume-low");
	}

	void Plugin::handleFullRaiseRequested ()
	{
		TabOpenRequested (PlayerTC_.TabClass_);
	}

	void Plugin::showCollectionStats ()
	{
		auto dia = new CollectionStatsDialog ();
		dia->setAttribute (Qt::WA_DeleteOnClose);
		dia->show ();
	}

	void Plugin::handleArtistBrowseRequested (const QString& artist, const DynPropertiesList_t& props)
	{
		auto tab = new ArtistBrowserTab (ArtistBrowserTC_, this);

		for (const auto& pair : props)
			tab->setProperty (pair.first, pair.second);

		emit addNewTab (tr ("Artist browser"), tab);
		emit raiseTab (tab);

		connect (tab,
				SIGNAL (removeTab (QWidget*)),
				this,
				SIGNAL (removeTab (QWidget*)));

		if (!artist.isEmpty ())
			tab->Browse (artist);
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_lmp, LeechCraft::LMP::Plugin);
