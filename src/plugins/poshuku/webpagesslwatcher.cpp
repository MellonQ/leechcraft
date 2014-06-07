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

#include "webpagesslwatcher.h"
#include <cstring>
#include <QSslConfiguration>
#include <QNetworkReply>
#include <qwebpage.h>
#include <qwebframe.h>

namespace LeechCraft
{
namespace Poshuku
{
	WebPageSslWatcher::WebPageSslWatcher (QWebPage *page)
	: QObject { page }
	, Page_ { page }
	{
		connect (page->networkAccessManager (),
				SIGNAL (requestCreated (QNetworkAccessManager::Operation,
						QNetworkRequest, QNetworkReply*)),
				this,
				SLOT (handleReplyCreated (QNetworkAccessManager::Operation,
						QNetworkRequest, QNetworkReply*)));
		connect (page,
				SIGNAL (hookAcceptNavigationRequest (LeechCraft::IHookProxy_ptr,
						QWebPage*,
						QWebFrame*,
						const QNetworkRequest&,
						QWebPage::NavigationType)),
				this,
				SLOT (handleNavigationRequest (LeechCraft::IHookProxy_ptr,
						QWebPage*,
						QWebFrame*,
						const QNetworkRequest&,
						QWebPage::NavigationType)));
	}

	WebPageSslWatcher::State WebPageSslWatcher::GetPageState () const
	{
		if (!ErrSslResources_.isEmpty ())
			return State::SslErrors;
		else if (SslResources_.isEmpty ())
			return State::NoSsl;
		else if (!NonSslResources_.isEmpty ())
			return State::UnencryptedElems;
		else
			return State::FullSsl;
	}

	const QSslConfiguration& WebPageSslWatcher::GetPageConfiguration () const
	{
		return PageConfig_;
	}

	QList<QUrl> WebPageSslWatcher::GetNonSslUrls () const
	{
		return NonSslResources_;
	}

	void WebPageSslWatcher::handleReplyFinished ()
	{
		const auto reply = qobject_cast<QNetworkReply*> (sender ());
		const auto& url = reply->url ();

		if (url.scheme () == "data")
			return;

		const bool isCached = reply->attribute (QNetworkRequest::SourceIsFromCacheAttribute).toBool ();
		const bool connEncrypted = reply->attribute (QNetworkRequest::ConnectionEncryptedAttribute).toBool ();

		if (isCached && !connEncrypted)
			return;

		if (PendingErrors_.remove (reply))
			ErrSslResources_ << url;

		const auto& sslConfig = reply->sslConfiguration ();
		if (sslConfig.peerCertificate ().isNull ())
			NonSslResources_ << url;
		else
		{
			SslResources_ << url;

			const auto& frameUrl = Page_->mainFrame ()->url ();
			if (url.host () == frameUrl.host ())
			{
				qDebug () << Q_FUNC_INFO
						<< "detected main frame cert for URL"
						<< url;
				PageConfig_ = sslConfig;
			}
		}

		emit sslStateChanged (this);
	}

	void WebPageSslWatcher::handleSslErrors (const QList<QSslError>&)
	{
		PendingErrors_ << qobject_cast<QNetworkReply*> (sender ());
	}

	namespace
	{
		bool CheckReplyFrame (QObject *original, QWebFrame *mainFrame)
		{
			if (!original ||
					std::strcmp (original->metaObject ()->className (), "QWebFrame"))
				return false;

			auto webFrame = qobject_cast<QWebFrame*> (original);
			while (const auto parent = webFrame->parentFrame ())
				webFrame = parent;

			return webFrame == mainFrame;
		}
	}

	void WebPageSslWatcher::handleReplyCreated (QNetworkAccessManager::Operation,
			const QNetworkRequest& req, QNetworkReply *reply)
	{
		if (!CheckReplyFrame (req.originatingObject (), Page_->mainFrame ()))
			return;

		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleReplyFinished ()));
		connect (reply,
				SIGNAL (sslErrors (QList<QSslError>)),
				this,
				SLOT (handleSslErrors (QList<QSslError>)));
	}

	void WebPageSslWatcher::resetStats ()
	{
		qDebug () << Q_FUNC_INFO;
		SslResources_.clear ();
		NonSslResources_.clear ();
		ErrSslResources_.clear ();

		PageConfig_ = {};

		emit sslStateChanged (this);
	}

	void WebPageSslWatcher::handleNavigationRequest (IHookProxy_ptr,
			QWebPage*, QWebFrame *frame, const QNetworkRequest&, QWebPage::NavigationType type)
	{
		if (frame != Page_->mainFrame () ||
				type == QWebPage::NavigationTypeOther)
			return;

		resetStats ();
	}
}
}