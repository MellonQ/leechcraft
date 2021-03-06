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

#include "datafilteruploader.h"
#include <memory>
#include <QtDebug>
#include <QTemporaryFile>
#include <QUrl>
#include <QInputDialog>
#include <util/sll/slotclosure.h>
#include <interfaces/structures.h>
#include <interfaces/idatafilter.h>
#include "accountsmanager.h"
#include "interfaces/blasq/iaccount.h"
#include "interfaces/blasq/isupportuploads.h"
#include "uploadphotosdialog.h"

namespace LeechCraft
{
namespace Blasq
{
	DataFilterUploader::DataFilterUploader (const Entity& e, AccountsManager *accMgr, QObject *parent)
	: QObject { parent }
	, AccMgr_ { accMgr }
	, Entity_ { e }
	{
		const auto& accId = e.Additional_ ["DataFilter"].toByteArray ();

		if (!accId.isEmpty ())
			UploadToAcc (accId);
		else
			SelectAcc ();
	}

	void DataFilterUploader::SelectAcc ()
	{
		const auto& accs = AccMgr_->GetAccounts ();
		QStringList accNames;
		for (auto acc : accs)
			accNames << acc->GetName ();

		bool ok = false;
		const auto& chosenAcc = QInputDialog::getItem (nullptr,
				tr ("Select account"),
				tr ("Please select the account to use while uploading the photo:"),
				accNames,
				0,
				false,
				&ok);

		if (!ok)
		{
			deleteLater ();
			return;
		}

		const auto acc = accs.value (accNames.indexOf (chosenAcc));
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< "no account for account name"
					<< chosenAcc;
			deleteLater ();
			return;
		}

		UploadToAcc (acc->GetID ());
	}

	void DataFilterUploader::UploadToAcc (const QByteArray& accId)
	{
		bool shouldCleanup = true;
		auto deleteGuard = std::shared_ptr<void> (nullptr,
				[this, shouldCleanup] (void*)
				{
					if (shouldCleanup)
						deleteLater ();
				});

		const auto acc = AccMgr_->GetAccount (accId);
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< "no account for ID"
					<< accId;
			return;
		}

		const auto& image = Entity_.Entity_.value<QImage> ();
		const auto& localFile = Entity_.Entity_.toUrl ().toLocalFile ();
		if (!image.isNull ())
		{
			auto tempFile = new QTemporaryFile { this };
			Entity_.Entity_.value<QImage> ().save (tempFile, "PNG", 0);
			UploadFileName_ = tempFile->fileName ();
		}
		else if (QFile::exists (localFile))
			UploadFileName_ = localFile;

		shouldCleanup = false;

		const auto dia = new UploadPhotosDialog { acc->GetQObject () };
		dia->LockFiles ();
		dia->SetFiles ({ { UploadFileName_, {} } });
		dia->open ();
		dia->setAttribute (Qt::WA_DeleteOnClose);

		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, dia, acc]
			{
				connect (acc->GetQObject (),
						SIGNAL (itemUploaded (UploadItem, QUrl)),
						this,
						SLOT (checkItemUploaded (UploadItem, QUrl)));

				const auto isu = qobject_cast<ISupportUploads*> (acc->GetQObject ());
				isu->UploadImages (dia->GetSelectedCollection (), dia->GetSelectedFiles ());
			},
			dia,
			SIGNAL (accepted ()),
			dia
		};

		connect (dia,
				SIGNAL (finished (int)),
				this,
				SLOT (deleteLater ()));
	}

	void DataFilterUploader::checkItemUploaded (const UploadItem& item, const QUrl& url)
	{
		if (item.FilePath_ != UploadFileName_)
			return;

		const auto cb = Entity_.Additional_ ["DataFilterCallback"].value<DataFilterCallback_f> ();
		if (cb)
			cb (url);

		deleteLater ();
	}
}
}
