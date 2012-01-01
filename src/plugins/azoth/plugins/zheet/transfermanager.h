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

#ifndef PLUGINS_AZOTH_PLUGINS_ZHEET_TRANSFERMANAGER_H
#define PLUGINS_AZOTH_PLUGINS_ZHEET_TRANSFERMANAGER_H
#include <QObject>
#include <msn/util.h>
#include <interfaces/itransfermanager.h>

namespace LeechCraft
{
namespace Azoth
{
namespace Zheet
{
	class Callbacks;
	class MSNAccount;

	class TransferManager : public QObject
						  , public ITransferManager
	{
		Q_OBJECT
		Q_INTERFACES (LeechCraft::Azoth::ITransferManager);
		
		MSNAccount *A_;
		Callbacks *CB_;
		uint SessID_;
	public:
		TransferManager (Callbacks*, MSNAccount*);

		QObject* SendFile (const QString& id,
				const QString&, const QString& name);
	private slots:
		void handleSuggestion (MSN::fileTransferInvite);
	signals:
		void fileOffered (QObject *job);
	};
}
}
}

#endif
