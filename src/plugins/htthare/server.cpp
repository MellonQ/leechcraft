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

#include "server.h"
#include <QString>
#include <QtDebug>
#include "connection.h"
#include "iconresolver.h"
#include "trmanager.h"

namespace LeechCraft
{
namespace HttHare
{
	namespace ip = boost::asio::ip;

	Server::Server (const QString& address, const QString& port)
	: Acceptor_ { IoService_ }
	, Socket_ { IoService_ }
	, IconResolver_ { new IconResolver  }
	, TrManager_ { new TrManager }
	{
		ip::tcp::resolver resolver { IoService_ };
		const ip::tcp::endpoint endpoint = *resolver.resolve ({ address.toStdString (), port.toStdString () });

		Acceptor_.open (endpoint.protocol ());
		Acceptor_.set_option (ip::tcp::acceptor::reuse_address (true));
		Acceptor_.bind (endpoint);
		Acceptor_.listen ();

		StartAccept ();
	}

	Server::~Server ()
	{
		if (!IoService_.stopped ())
			Stop ();
	}

	void Server::Start ()
	{
		for (auto i = 0; i < 2; ++i)
			Threads_.emplace_back ([this] { IoService_.run (); });
	}

	void Server::Stop ()
	{
		IoService_.stop ();
		for (auto& thread : Threads_)
			thread.join ();
		Threads_.clear ();
	}

	void Server::StartAccept ()
	{
		Connection_ptr connection { new Connection { IoService_, StorageMgr_, IconResolver_, TrManager_ } };
		Acceptor_.async_accept (connection->GetSocket (),
				[this, connection] (const boost::system::error_code& ec)
				{
					if (!ec)
						connection->Start ();
					else
						qWarning () << Q_FUNC_INFO
								<< "cannot accept:"
								<< ec.message ().c_str ();

					StartAccept ();
				});
	}
}
}
