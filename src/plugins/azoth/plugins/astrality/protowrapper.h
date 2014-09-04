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

#pragma once

#include <QObject>
#include <Types>
#include <ConnectionManager>
#include <AccountManager>
#include <interfaces/structures.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/azoth/iprotocol.h>
#include "accountwrapper.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Astrality
{
	class AccountWrapper;

	class ProtoWrapper : public QObject
					   , public IProtocol
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Azoth::IProtocol);

		Tp::ConnectionManagerPtr CM_;
		const QString ProtoName_;
		const ICoreProxy_ptr Proxy_;
		const Tp::ProtocolInfo ProtoInfo_;

		Tp::AccountManagerPtr AM_;

		QList<AccountWrapper*> Accounts_;
		QMap<Tp::PendingAccount*, AccountWrapper::Settings> PendingSettings_;
	public:
		ProtoWrapper (Tp::ConnectionManagerPtr, const QString&, const ICoreProxy_ptr&, QObject*);

		void Release ();

		QVariantMap GetParamsFromWidgets (const QList<QWidget*>&) const;
		AccountWrapper::Settings GetSettingsFromWidgets (const QList<QWidget*>&) const;

		QObject* GetQObject ();
		ProtocolFeatures GetFeatures () const;
		QList<QObject*> GetRegisteredAccounts ();
		QObject* GetParentProtocolPlugin () const;
		QString GetProtocolName () const;
		QIcon GetProtocolIcon () const;
		QByteArray GetProtocolID () const;
		QList<QWidget*> GetAccountRegistrationWidgets (AccountAddOptions);
		void RegisterAccount (const QString&, const QList<QWidget*>&);
		void RemoveAccount (QObject*);
	private slots:
		void handleAMReady (Tp::PendingOperation*);
		void handleAccountCreated (Tp::PendingOperation*);
		AccountWrapper* handleNewAccount (Tp::AccountPtr);
		void handleAccountRemoved (AccountWrapper*);
	signals:
		void accountAdded (QObject*);
		void accountRemoved (QObject*);

		void gotEntity (const LeechCraft::Entity&);
		void delegateEntity (LeechCraft::Entity, int*, QObject**);
	};
}
}
}
