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

#include "inbandaccountregsecondpage.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <util/network/socketerrorstrings.h>
#include <util/util.h>
#include <interfaces/core/ientitymanager.h>
#include "xmppbobmanager.h"
#include "inbandaccountregfirstpage.h"
#include "util.h"
#include "regformhandlerwidget.h"
#include "core.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	namespace
	{
		QXmppClient* MakeClient (InBandAccountRegSecondPage *page)
		{
			auto client = new QXmppClient (page);
			Q_FOREACH (auto ext, client->extensions ())
				client->removeExtension (ext);

			client->addExtension (new XMPPBobManager);

			return client;
		}
	}

	InBandAccountRegSecondPage::InBandAccountRegSecondPage (InBandAccountRegFirstPage *first, QWidget *parent)
	: QWizardPage (parent)
	, Client_ (MakeClient (this))
	, RegForm_ (new RegFormHandlerWidget (Client_))
	, FirstPage_ (first)
	{
		setLayout (new QVBoxLayout);
		layout ()->addWidget (RegForm_);

		connect (Client_,
				SIGNAL (connected ()),
				this,
				SLOT (handleConnected ()));
		connect (Client_,
				SIGNAL (error (QXmppClient::Error)),
				this,
				SLOT (handleClientError (QXmppClient::Error)));

		connect (RegForm_,
				SIGNAL (completeChanged ()),
				this,
				SIGNAL (completeChanged ()));
		connect (RegForm_,
				SIGNAL (successfulReg ()),
				this,
				SIGNAL (successfulReg ()));
		connect (RegForm_,
				SIGNAL (regError (QString)),
				this,
				SIGNAL (regError (QString)));
	}

	void InBandAccountRegSecondPage::Register ()
	{
		RegForm_->Register ();
	}

	QString InBandAccountRegSecondPage::GetJID () const
	{
		return RegForm_->GetUser () + '@' + FirstPage_->GetServerName ();
	}

	QString InBandAccountRegSecondPage::GetPassword () const
	{
		return RegForm_->GetPassword ();
	}

	bool InBandAccountRegSecondPage::isComplete () const
	{
		return RegForm_->IsComplete ();
	}

	void InBandAccountRegSecondPage::initializePage ()
	{
		QWizardPage::initializePage ();

		const QString& server = FirstPage_->GetServerName ();

		if (Client_->isConnected ())
			Client_->disconnectFromServer ();

		QXmppConfiguration conf;
		conf.setDomain (server);
		conf.setUseNonSASLAuthentication (false);
		conf.setUseSASLAuthentication (false);
		Client_->connectToServer (conf);
	}

	void InBandAccountRegSecondPage::handleConnected ()
	{
		RegForm_->SendRequest ();
	}

	void InBandAccountRegSecondPage::handleClientError (QXmppClient::Error error)
	{
		qWarning () << Q_FUNC_INFO
				<< error
				<< Client_->socketError ()
				<< Client_->xmppStreamError ();

		if (error == QXmppClient::SocketError &&
				wizard ()->currentPage () == this)
			initializePage ();
	}
}
}
}
