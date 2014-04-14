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

#include <QString>
#include <QMetaType>
#include "idocument.h"

namespace LeechCraft
{
namespace Monocle
{
	class IRedirectProxy;
	typedef std::shared_ptr<IRedirectProxy> IRedirectProxy_ptr;

	/** @brief Basic interface for format backends plugins for Monocle.
	 *
	 * This interface should be implemented by plugins that provide
	 * format backends for Monocle document reader — that is, for those
	 * plugins that can load documents.
	 *
	 * Some backends only convert a document from their format to another
	 * format, probably supported by another Monocle plugin. This is
	 * called a redirection, and the backend should return
	 * LoadCheckResult::Redirect from the CanLoadDocument() method for
	 * such documents. The backend should also return a valid redirect
	 * proxy from the GetRedirection() method in this case.
	 *
	 * @sa IDocument
	 */
	class IBackendPlugin
	{
	public:
		/** @brief Virtual destructor.
		 */
		virtual ~IBackendPlugin () {}

		/** @brief Describes the result of checking whether a file can be
		 * loaded.
		 *
		 * @sa CanLoadDocument()
		 */
		enum class LoadCheckResult
		{
			/** @brief The file cannot be loaded by this backend.
			 */
			Cannot,

			/** @brief The file can be loaded by this backend.
			 */
			Can,

			/** @brief The file cannot be loaded by this backend, but can
			 * be converted to another format.
			 */
			Redirect
		};

		/** @brief Checks whether the given document can be loaded.
		 *
		 * This method should return ::LoadCheckResult::Can if the document
		 * can possibly be loaded, ::LoadCheckResult::Cannot if it can't be
		 * loaded at all, and ::LoadCheckResult::Redirect if the document
		 * can be preprocessed and converted to some other format probably
		 * loadable by another Monocle plugin.
		 *
		 * The cheaper this function is, the better. It is discouraged to
		 * check by extension, though.
		 *
		 * It is OK to return nullptr or invalid document from
		 * LoadDocument() even if this method returns
		 * ::LoadCheckResult::Can for a given \em filename.
		 *
		 * If this function returns ::LoadCheckResult::Redirect, then
		 * the GetRedirection() method should return a non-null redirect
		 * proxy.
		 *
		 * @param[in] filename Path to the document to check.
		 * @return Whether the document at \em filename can be loaded.
		 *
		 * @sa LoadDocument()
		 * @sa GetRedirection()
		 */
		virtual LoadCheckResult CanLoadDocument (const QString& filename) = 0;

		/** @brief Loads the given document.
		 *
		 * This method should load the document at \em filename and
		 * return a pointer to it, or a null pointer if the document is
		 * invalid.
		 *
		 * The ownership is passed to the caller: that is, this backend
		 * plugin should keep no strong shared references to the returned
		 * document.
		 *
		 * It is OK for this method to return a null or invalid document
		 * even if CanLoadDocument() returned <code>true</code> for this
		 * \em filename,
		 *
		 * @param[in] filename The document to load.
		 * @return The document object for \em filename, or null pointer,
		 * or invalid document if an error has occurred.
		 *
		 * @sa CanLoadDocument()
		 * @sa GetRedirection()
		 * @sa IDocument
		 */
		virtual IDocument_ptr LoadDocument (const QString& filename) = 0;

		/** @brief Returns the redirection proxy for the given document.
		 *
		 * This function should return a redirect proxy for the document
		 * at \em filename, or a null pointer if the document cannot be
		 * redirected (for example, if it is invalid).
		 *
		 * The default implementation simply does nothing and returns a
		 * null pointer.
		 *
		 * @param[in] filename The document to redirect.
		 * @return The redirect proxy for \em filename, or null pointer.
		 *
		 * @sa LoadDocument()
		 * @sa IRedirectProxy
		 */
		virtual IRedirectProxy_ptr GetRedirection (const QString& filename)
		{
			Q_UNUSED (filename)
			return {};
		}

		/** @brief Returns the list MIME types supported by the backend.
		 *
		 * The returned MIME type is only considered when dealing with
		 * redirections. CanLoadDocument() and LoadDocument() methods can
		 * still be called on a file whose MIME isn't contained in the
		 * returned list.
		 *
		 * CanLoadDocument() and LoadDocument() can reject loading a
		 * document even if its MIME is contained in the list returned by
		 * this method.
		 *
		 * @return The list of MIMEs the backend supports.
		 */
		virtual QStringList GetSupportedMimes () const = 0;

		/** @brief Returns true whether the backend is threaded.
		 *
		 * This function returns true if the implementation supports
		 * threaded pages rendering.
		 *
		 * The default implementation simply returns false.
		 *
		 * @return Whether threaded rendering is supported by this
		 * backend.
		 */
		virtual bool IsThreaded () const
		{
			return false;
		}
	};
}
}

Q_DECLARE_INTERFACE (LeechCraft::Monocle::IBackendPlugin,
		"org.LeechCraft.Monocle.IBackendPlugin/1.0");
