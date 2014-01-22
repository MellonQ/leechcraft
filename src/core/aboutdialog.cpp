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

#include "aboutdialog.h"
#include <QDomDocument>
#include "util/sys/sysinfo.h"
#include "interfaces/ihavediaginfo.h"
#include "core.h"
#include "coreproxy.h"

namespace LeechCraft
{
	namespace
	{
		struct ContributorInfo
		{
			const QString Name_;
			const QString Nick_;
			const QString JID_;
			const QString Email_;
			const QStringList Roles_;
			const QList<int> Years_;

			ContributorInfo (const QString& name, const QString& nick,
					const QString& jid, const QString& email,
					const QStringList& roles, const QList<int>& years)
			: Name_ (name)
			, Nick_ (nick)
			, JID_ (jid)
			, Email_ (email)
			, Roles_ (roles)
			, Years_ (years)
			{
			}

			QString Fmt () const
			{
				QString result = "<strong>";
				if (!Name_.isEmpty ())
					result += Name_;
				if (!Name_.isEmpty () && !Nick_.isEmpty ())
					result += " aka ";
				if (!Nick_.isEmpty ())
					result += Nick_;
				result += "</strong><br/>";

				if (!JID_.isEmpty ())
					result += QString ("JID: <a href=\"xmpp:%1\">%1</a>")
							.arg (JID_);
				if (!JID_.isEmpty () && !Email_.isEmpty ())
					result += "<br />";
				if (!Email_.isEmpty ())
					result += QString ("Email: <a href=\"mailto:%1\">%1</a>")
							.arg (Email_);

				result += "<ul>";
				Q_FOREACH (const QString& r, Roles_)
					result += QString ("<li>%1</li>")
							.arg (r);
				result += "</ul>";

				QString yearsStr;
				if (Years_.size () > 1 && Years_.back () - Years_.front () == Years_.size () - 1)
					yearsStr = QString::fromUtf8 ("%1–%2")
							.arg (Years_.front ())
							.arg (Years_.back ());
				else
				{
					QStringList yearsStrs;
					for (auto year : Years_)
						yearsStrs << QString::number (year);
					yearsStr = yearsStrs.join (", ");
				}

				result += AboutDialog::tr ("Years: %1")
						.arg (yearsStr);

				return result;
			}
		};

		QStringList ParseRoles (const QDomElement& contribElem)
		{
			QStringList result;

			auto roleElem = contribElem
					.firstChildElement ("roles")
					.firstChildElement ("role");
			while (!roleElem.isNull ())
			{
				result << roleElem.text ();
				roleElem = roleElem.nextSiblingElement ("role");
			}

			return result;
		}

		QList<int> ParseYears (const QDomElement& contribElem)
		{
			QList<int> result;

			auto yearsElem = contribElem
					.firstChildElement ("years");
			if (yearsElem.hasAttribute ("start") &&
					yearsElem.hasAttribute ("end"))
			{
				for (auto start = yearsElem.attribute ("start").toInt (),
						end = yearsElem.attribute ("end").toInt ();
						start <= end; ++start)
					result << start;

				return result;
			}

			auto yearElem = yearsElem.firstChildElement ("year");
			while (!yearElem.isNull ())
			{
				result << yearElem.text ().toInt ();
				yearElem = yearElem.nextSiblingElement ("year");
			}

			return result;
		}

		QList<ContributorInfo> ParseContributors (const QDomDocument& doc, const QString& type)
		{
			QList<ContributorInfo> result;

			auto contribElem = doc.documentElement ().firstChildElement ("contrib");
			while (!contribElem.isNull ())
			{
				auto nextGuard = std::shared_ptr<void> (nullptr,
						[&contribElem] (void*)
						{
							contribElem = contribElem.nextSiblingElement ("contrib");
						});

				if (!type.isEmpty () && contribElem.attribute ("type") != type)
					continue;

				result.append ({
						contribElem.attribute ("name"),
						contribElem.attribute ("nick"),
						contribElem.attribute ("jid"),
						contribElem.attribute ("email"),
						ParseRoles (contribElem),
						ParseYears (contribElem)
					});
			}

			return result;
		}
	}

	AboutDialog::AboutDialog (QWidget *parent)
	: QDialog (parent)
	{
		Ui_.setupUi (this);

		Ui_.ProgramName_->setText (QString ("LeechCraft %1")
				.arg (CoreProxy ().GetVersion ()));

		QFile authorsFile (":/resources/data/authors.xml");
		authorsFile.open (QIODevice::ReadOnly);

		QDomDocument authorsDoc;
		authorsDoc.setContent (&authorsFile);

		QStringList formatted;
		for (const auto& i : ParseContributors (authorsDoc, "author"))
			formatted << i.Fmt ();
		Ui_.Authors_->setHtml (formatted.join ("<hr />"));

		formatted.clear ();
		for (const auto& i : ParseContributors (authorsDoc, "contrib"))
			formatted << i.Fmt ();
		Ui_.Contributors_->setHtml (formatted.join ("<hr />"));

		BuildDiagInfo ();
	}

	void AboutDialog::BuildDiagInfo ()
	{
		QString text = QString ("LeechCraft ") + CoreProxy ().GetVersion () + "\n";
		text += QString ("Built with Qt %1, running with Qt %2\n")
				.arg (QT_VERSION_STR)
				.arg (qVersion ());

		text += QString ("Running on: %1\n").arg (Util::SysInfo::GetOSName ());
		text += "--------------------------------\n\n";

		QStringList loadedModules;
		QStringList unPathedModules;
		PluginManager *pm = Core::Instance ().GetPluginManager ();
		Q_FOREACH (QObject *plugin, pm->GetAllPlugins ())
		{
			const QString& path = pm->GetPluginLibraryPath (plugin);

			IInfo *ii = qobject_cast<IInfo*> (plugin);
			if (path.isEmpty ())
				unPathedModules << ("* " + ii->GetName ());
			else
				loadedModules << ("* " + ii->GetName () + " (" + path + ")");

			IHaveDiagInfo *diagInfo = qobject_cast<IHaveDiagInfo*> (plugin);
			if (diagInfo)
			{
				text += "Diag info for " + ii->GetName () + ":\n";
				text += diagInfo->GetDiagInfoString ();
				text += "\n--------------------------------\n\n";
			}
		}

		text += QString ("Normal plugins:") + "\n" + loadedModules.join ("\n") + "\n\n";
		if (!unPathedModules.isEmpty ())
			text += QString ("Adapted plugins:") + "\n" + unPathedModules.join ("\n") + "\n\n";

		Ui_.DiagInfo_->setPlainText (text);
	}
}
