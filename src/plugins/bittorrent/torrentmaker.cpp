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

#include "torrentmaker.h"
#include <deque>
#include <boost/filesystem.hpp>
#include <QFile>
#include <QFileInfo>
#include <QProgressDialog>
#include <QMessageBox>
#include <QDir>
#include <QtDebug>
#include <QMainWindow>
#include <libtorrent/create_torrent.hpp>
#include <util/xpc/util.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <interfaces/core/ientitymanager.h>
#include "core.h"

namespace LeechCraft
{
namespace BitTorrent
{
	namespace
	{
		bool FileFilter (const boost::filesystem::path& filename)
		{
#if BOOST_FILESYSTEM_VERSION == 2
			if (filename.leaf () [0] == '.')
#else
			if (filename.leaf ().string () [0] == '.')
#endif
				return false;
			QFileInfo fi (QString::fromUtf8 (filename.string ().c_str ()));
			if ((fi.isDir () ||
						fi.isFile () ||
						fi.isSymLink ()) &&
					fi.isReadable ())
				return true;
			return false;
		}

		void UpdateProgress (int i, QProgressDialog *pd)
		{
			pd->setValue (i);
		}
	}

	TorrentMaker::TorrentMaker (const ICoreProxy_ptr& proxy, QObject *parent)
	: QObject { parent }
	, Proxy_ { proxy }
	{
	}

	void TorrentMaker::Start (NewTorrentParams params)
	{
		QString filename = params.Output_;
		if (!filename.endsWith (".torrent"))
			filename.append (".torrent");
		QFile file (filename);
		if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
		{
			ReportError (tr ("Could not open file %1 for write!").arg (filename));
			return;
		}

#if BOOST_FILESYSTEM_VERSION == 2
		boost::filesystem::path::default_name_check (boost::filesystem::no_check);
#endif

		libtorrent::file_storage fs;
		const auto& fullPath = std::string (params.Path_.toUtf8 ().constData ());
		libtorrent::add_files (fs, fullPath, FileFilter);
		libtorrent::create_torrent ct (fs, params.PieceSize_);

		ct.set_creator (qPrintable (QString ("LeechCraft BitTorrent %1")
					.arg (Core::Instance ()->GetProxy ()->GetVersion ())));
		if (!params.Comment_.isEmpty ())
			ct.set_comment (params.Comment_.toUtf8 ());
		for (int i = 0; i < params.URLSeeds_.size (); ++i)
			ct.add_url_seed (params.URLSeeds_.at (0).toStdString ());
		ct.set_priv (!params.DHTEnabled_);

		if (params.DHTEnabled_)
			for (int i = 0; i < params.DHTNodes_.size (); ++i)
			{
				QStringList splitted = params.DHTNodes_.at (i).split (":");
				ct.add_node (std::pair<std::string, int> (splitted [0].trimmed ().toStdString (),
							splitted [1].trimmed ().toInt ()));
			}

		ct.add_tracker (params.AnnounceURL_.toStdString ());

		std::unique_ptr<QProgressDialog> pd (new QProgressDialog ());
		pd->setWindowTitle (tr ("Hashing torrent..."));
		pd->setMaximum (ct.num_pieces ());

		boost::system::error_code hashesError;
		libtorrent::set_piece_hashes (ct,
				fullPath,
				[this, &pd] (int i) { UpdateProgress (i, pd.get ()); },
				hashesError);
		if (hashesError)
		{
			QString message = QString::fromUtf8 (hashesError.message ().c_str ());
			libtorrent::file_entry entry = fs.at (hashesError.value ());
			qWarning () << Q_FUNC_INFO
				<< "while in libtorrent::set_piece_hashes():"
				<< message
				<< hashesError.category ().name ();
			ReportError (tr ("Torrent creation failed: %1")
					.arg (message));
			return;
		}

		libtorrent::entry e = ct.generate ();
		std::deque<char> outbuf;
		libtorrent::bencode (std::back_inserter (outbuf), e);

		for (size_t i = 0; i < outbuf.size (); ++i)
			file.write (&outbuf.at (i), 1);
		file.close ();

		auto rootWM = Core::Instance ()->GetProxy ()->GetRootWindowsManager ();
		if (QMessageBox::question (rootWM->GetPreferredWindow (),
					"LeechCraft",
					tr ("Torrent file generated: %1.<br />Do you want to start seeding now?")
						.arg (QDir::toNativeSeparators (filename)),
					QMessageBox::Yes | QMessageBox::No) ==
				QMessageBox::Yes)
			Core::Instance ()->AddFile (filename,
					QString::fromUtf8 (fullPath.c_str ()),
					QStringList (),
					false);
	}

	void TorrentMaker::ReportError (const QString& error)
	{
		const auto& entity = Util::MakeNotification ("BitTorrent", error, PCritical_);
		Proxy_->GetEntityManager ()->HandleEntity (entity);
	}
}
}
