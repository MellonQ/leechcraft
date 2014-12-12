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

#include "filetransfermanager.h"
#include <QtDebug>
#include <tox/tox.h>
#include "toxaccount.h"
#include "toxthread.h"
#include "filetransferin.h"
#include "filetransferout.h"
#include "toxcontact.h"
#include "util.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	FileTransferManager::FileTransferManager (ToxAccount *acc)
	: QObject { acc }
	, Acc_ { acc }
	{
		connect (this,
				SIGNAL (requested (int32_t, QByteArray, uint8_t, uint64_t, QString)),
				this,
				SLOT (handleRequest (int32_t, QByteArray, uint8_t, uint64_t, QString)));
	}

	bool FileTransferManager::IsAvailable () const
	{
		return !ToxThread_.expired ();
	}

	QObject* FileTransferManager::SendFile (const QString& id,
			const QString&, const QString& name, const QString&)
	{
		const auto toxThread = ToxThread_.lock ();
		if (!toxThread)
		{
			qWarning () << Q_FUNC_INFO
					<< "Tox thread is not available";
			return nullptr;
		}

		const auto contact = Acc_->GetByAzothId (id);
		if (!contact)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to find contact by the ID"
					<< id;
			return nullptr;
		}

		const auto transfer = new FileTransferOut { id, contact->GetPubKey (), name, toxThread };
		connect (this,
				SIGNAL (gotFileControl (qint32, qint8, qint8, QByteArray)),
				transfer,
				SLOT (handleFileControl (qint32, qint8, qint8, QByteArray)));
		return transfer;
	}

	void FileTransferManager::handleToxThreadChanged (const std::shared_ptr<ToxThread>& thread)
	{
		ToxThread_ = thread;
	}

	void FileTransferManager::handleToxCreated (Tox *tox)
	{
		tox_callback_file_control (tox,
				[] (Tox*, int32_t friendNum,
						uint8_t, uint8_t filenum, uint8_t type,
						const uint8_t *rawData, uint16_t len, void *udata)
				{
					const QByteArray data { reinterpret_cast<const char*> (rawData), len };
					static_cast<FileTransferManager*> (udata)->
							gotFileControl (friendNum, filenum, type, data);
				},
				this);
		tox_callback_file_send_request (tox,
				[] (Tox *tox, int32_t friendNum,
						uint8_t filenum, uint64_t filesize,
						const uint8_t *rawFilename, uint16_t filenameLength,
						void *udata)
				{
					const auto filenameStr = reinterpret_cast<const char*> (rawFilename);
					const auto& name = QString::fromUtf8 (filenameStr, filenameLength);
					static_cast<FileTransferManager*> (udata)->requested (friendNum,
							GetFriendId (tox, friendNum), filenum, filesize, name);
				},
				this);
		tox_callback_file_data (tox,
				[] (Tox*,
						int32_t friendNum, uint8_t fileNum,
						const uint8_t *rawData, const uint16_t rawSize,
						void *udata)
				{
					const QByteArray data { reinterpret_cast<const char*> (rawData), rawSize };
					static_cast<FileTransferManager*> (udata)->gotData (friendNum, fileNum, data);
				},
				this);
	}

	void FileTransferManager::handleRequest (int32_t friendNum,
			const QByteArray& pkey, uint8_t filenum, uint64_t size, const QString& name)
	{
		const auto toxThread = ToxThread_.lock ();
		if (!toxThread)
		{
			qWarning () << Q_FUNC_INFO
					<< "Tox thread is not available";
			return;
		}

		const auto entry = Acc_->GetByPubkey (pkey);
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to find entry for pubkey"
					<< pkey;
			return;
		}

		const auto transfer = new FileTransferIn
		{
			entry->GetEntryID (),
			pkey,
			friendNum,
			filenum,
			static_cast<qint64> (size),
			name,
			toxThread
		};

		connect (this,
				SIGNAL (gotFileControl (qint32, qint8, qint8, QByteArray)),
				transfer,
				SLOT (handleFileControl (qint32, qint8, qint8, QByteArray)));
		connect (this,
				SIGNAL (gotData (qint32, qint8, QByteArray)),
				transfer,
				SLOT (handleData (qint32, qint8, QByteArray)));

		emit fileOffered (transfer);
	}
}
}
}