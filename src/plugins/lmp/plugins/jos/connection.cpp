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

#include "connection.h"
#include <QTemporaryFile>
#include <QDesktopServices>
#include <QStringList>
#include <QtDebug>
#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <libimobiledevice/lockdown.h>
#include "afcfile.h"

namespace LeechCraft
{
namespace LMP
{
namespace jOS
{
	namespace
	{
		QString GetTemporaryDir ()
		{
			const auto& tempLocation = QDesktopServices::storageLocation (QDesktopServices::TempLocation);

			QTemporaryFile file { tempLocation + '/' + "lmp_jos_XXXXXX" };
			if (file.open ())
				return file.fileName ();
			else
				return tempLocation + "/lmp_jos";
		}
	}

	Connection::Connection (const QByteArray& udid)
	: Device_ { MakeRaii<idevice_t> ([udid] (idevice_t *dev)
				{ return idevice_new (dev, udid.constData ()); },
			idevice_free) }
	, Lockdown_ { MakeRaii<lockdownd_client_t> ([this] (lockdownd_client_t *ld)
				{ return lockdownd_client_new_with_handshake (Device_, ld, "LMP jOS"); },
			lockdownd_client_free) }
	, Service_ { MakeRaii<lockdownd_service_descriptor_t> ([this] (lockdownd_service_descriptor_t *service)
				{ return lockdownd_start_service (Lockdown_, "com.apple.afc", service); },
			lockdownd_service_descriptor_free) }
	, AFC_ {  MakeRaii<afc_client_t> ([this] (afc_client_t *afc)
				{ return afc_client_new (Device_, Service_, afc); },
			afc_client_free) }
	, TempDirPath_ { GetTemporaryDir () }
	{
		qDebug () << Q_FUNC_INFO
				<< "created connection to"
				<< udid
				<< "with temp dir"
				<< TempDirPath_;

		auto watcher = new QFutureWatcher<bool> ();
		connect (watcher,
				SIGNAL (finished ()),
				this,
				SLOT (itdbCopyFinished ()));
		const auto& future = QtConcurrent::run ([this] () -> bool
			{
				bool result = true;
				for (const auto& dir : QStringList { "Artwork", "Device", "iTunes" })
					if (!DownloadDir ("/iTunes_Control/" + dir, CopyCreate))
						result = false;
				return result;
			});
		watcher->setFuture (future);
	}

	afc_client_t Connection::GetAFC () const
	{
		return AFC_;
	}

	namespace
	{
		void ListCleanup (char **info)
		{
			if (!info)
				return;

			for (auto p = info; *p; ++p)
				free (*p);
			free (info);
		}
	}

	QString Connection::GetFileInfo (const QString& path, const QString& key) const
	{
		char **info = nullptr;
		if (const auto err = afc_get_file_info (AFC_, path.toUtf8 (), &info))
		{
			if (err != AFC_E_OBJECT_NOT_FOUND)
				qWarning () << Q_FUNC_INFO
						<< "error getting info for"
						<< path
						<< err;
			return {};
		}

		if (!info)
		{
			qWarning () << Q_FUNC_INFO
					<< "couldn't find any properties"
					<< key
					<< "for file"
					<< path
					<< "at all";
			return {};
		}

		auto guard = std::shared_ptr<void> (nullptr,
				[info] (void*) { ListCleanup (info); });

		QString lastKey;
		for (auto p = info; *p; ++p)
			if (lastKey.isEmpty ())
				lastKey = QString::fromUtf8 (*p);
			else
			{
				if (lastKey == key)
					return QString::fromUtf8 (*p);

				lastKey.clear ();
			}

		qWarning () << Q_FUNC_INFO
				<< "couldn't find property"
				<< key
				<< "for file"
				<< path;
		return {};
	}

	bool Connection::Exists (const QString& path)
	{
		return !GetFileInfo (path, "st_ifmt").isNull ();
	}

	QString Connection::GetNextFilename (const QString& origPath)
	{
		auto mdir = [] (int num)
		{
			return QString { "/iTunes_Control/Music/F%1" }
					.arg (num, 2, 10, QChar { '0' });
		};

		int lastMD = 0;
		for ( ; lastMD < 100; ++lastMD)
			if (!Exists (mdir (lastMD)))
				break;

		if (!lastMD)
		{
			qWarning () << Q_FUNC_INFO
					<< "no music dirs were found";
			return {};
		}

		const auto dirNum = qrand () % lastMD;
		const auto& dir = mdir (dirNum);
		if (!Exists (dir))
		{
			qWarning () << Q_FUNC_INFO
					<< "chosen directory for"
					<< dirNum
					<< "of"
					<< lastMD
					<< "doesn't exist";
			return {};
		}

		auto ext = origPath.section ('.', -1, -1).toLower ();
		if (ext.isEmpty ())
			ext = "mp3";

		QString filename;
		while (true)
		{
			filename = QString { "jos%1" }.arg (qrand () % 999999, 6, 10, QChar { '0' });
			filename += '.' + ext;
			filename.prepend (dir + '/');

			if (!Exists (filename))
				break;
		}

		return filename;
	}

	namespace
	{
		template<typename AFC>
		afc_error_t TryReadDir (const AFC& afc, const QString& path)
		{
			char **list = nullptr;
			const auto ret = afc_read_directory (afc, path.toUtf8 ().constData (), &list);
			ListCleanup (list);
			return ret;
		}
	}

	QStringList Connection::ReadDir (const QString& path, QDir::Filters filters)
	{
		char **list = nullptr;
		if (const auto err = afc_read_directory (AFC_, path.toUtf8 ().constData (), &list))
		{
			qWarning () << Q_FUNC_INFO
					<< "error reading"
					<< path
					<< err;
			return {};
		}

		if (!list)
			return {};

		QStringList result;

		for (auto p = list; *p; ++p)
		{
			const auto& filename = QString::fromUtf8 (*p);
			free (*p);

			if (filters == QDir::NoFilter)
			{
				result << filename;
				continue;
			}

			if (filters & QDir::NoDotAndDotDot &&
					(filename == "." || filename == ".."))
				continue;

			if (!(filters & QDir::Hidden) && filename.startsWith ("."))
				continue;

			const auto& filetype = GetFileInfo (path + '/' + filename, "st_ifmt");

			if ((filetype == "S_IFREG" && filters & QDir::Files) ||
				(filetype == "S_IFDIR" && filters & QDir::Dirs) ||
				(filetype == "S_IFLNK" && !(filters & QDir::NoSymLinks)))
				result << filename;
		}

		free (list);

		return result;
	}

	bool Connection::DownloadDir (const QString& dir, CopyOptions options)
	{
		qDebug () << "copying dir" << dir;

		if (TryReadDir (AFC_, dir) == AFC_E_OBJECT_NOT_FOUND)
		{
			qDebug () << "dir not found";
			if (options & CopyCreate)
				QDir {}.mkpath (TempDirPath_ + dir);

			return true;
		}

		bool result = true;

		for (const auto& file : ReadDir (dir, QDir::Files | QDir::NoDotAndDotDot))
			if (!DownloadFile (dir + '/' + file))
				result = false;

		for (const auto& subdir : ReadDir (dir, QDir::Dirs | QDir::NoDotAndDotDot))
			if (!DownloadDir (dir + '/' + subdir))
				result = false;

		return result;
	}

	namespace
	{
		template<typename Src, typename Dest>
		bool CopyIODevice (Src&& srcFile, const QString& srcId, Dest&& destFile, const QString& destId)
		{
			if (!srcFile.isOpen () && !srcFile.open (QIODevice::ReadOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot open src file at"
						<< srcId
						<< srcFile.errorString ();
				return false;
			}

			if (!destFile.isOpen () && !destFile.open (QIODevice::WriteOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot open dest file at"
						<< destId
						<< srcFile.errorString ();
				return false;
			}

			for (qint64 copied = 0, size = srcFile.size (); copied < size; )
			{
				const auto& data = srcFile.read (1024 * 1024);
				if (data.isEmpty ())
				{
					qWarning () << Q_FUNC_INFO
							<< "got empty data chunk";
					return false;
				}

				if (destFile.write (data) == -1)
				{
					qWarning () << Q_FUNC_INFO
							<< "cannot write to destination file"
							<< destId
							<< destFile.errorString ();
					return false;
				}

				copied += data.size ();
			}

			return true;
		}
	}

	bool Connection::DownloadFile (const QString& file)
	{
		qDebug () << "copying file" << file;

		const auto& localFilePath = TempDirPath_ + file;
		const auto& localDirPath = localFilePath.section ('/', 0, -2);

		QDir dir;
		dir.mkpath (localDirPath);

		return CopyIODevice (AfcFile { file, this }, file, QFile { localFilePath }, localFilePath);





	}

	void Connection::itdbCopyFinished ()
	{
		auto watcher = dynamic_cast<QFutureWatcher<bool>*> (sender ());
		const auto result = watcher->result ();
		watcher->deleteLater ();

		qDebug () << Q_FUNC_INFO
				<< "copy finished, is good?"
				<< result;

		CopiedDb_ = result;

		if (!result)
			emit error (tr ("Error loading iTunes database from the device."));

		DB_ = new GpodDb (TempDirPath_, this);
		auto loadWatcher = new QFutureWatcher<QString> (this);
		connect (loadWatcher,
				SIGNAL (finished ()),
				this,
				SLOT (handleDbLoaded ()));

		loadWatcher->setFuture (QtConcurrent::run ([this] ()
				{
					qDebug () << Q_FUNC_INFO << "loading db...";
					auto res = DB_->Load ();
					if (res.Result_ != GpodDb::Result::NotFound)
						return res.Message_;

					qDebug () << "cannot find one, reinitializing the device";

					res = DB_->Reinitialize ();

					if (res.Result_ != GpodDb::Result::Success)
					{
						qWarning () << Q_FUNC_INFO
								<< "failed to reinitialize";
						return res.Message_;
					}

					if (!UploadDir ("/iTunes_Control/Music"))
						return tr ("Cannot upload Music directory after reinitializing");

					return DB_->Load ().Message_;
				}));
	}

	void Connection::handleDbLoaded ()
	{
		auto watcher = dynamic_cast<QFutureWatcher<QString>*> (sender ());
		const auto& msg = watcher->result ();
		watcher->deleteLater ();

		if (!msg.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot load database:"
					<< msg;
			delete DB_;
			DB_ = nullptr;
			return;
		}

	}
}
}
}
