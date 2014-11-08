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

#include "tabsessmanager.h"
#include <algorithm>
#include <QIcon>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>
#include <QMenu>
#include <QInputDialog>
#include <QMainWindow>
#include <QtDebug>
#include <util/util.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/icoretabwidget.h>
#include <interfaces/ihaverecoverabletabs.h>
#include <interfaces/ihavetabs.h>
#include "restoresessiondialog.h"
#include "recinfo.h"
#include "sessionmenumanager.h"

namespace LeechCraft
{
namespace TabSessManager
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("tabsessmanager");

		IsScheduled_ = false;
		UncloseMenu_ = new QMenu (tr ("Unclose tabs"));

		SessionMenuMgr_ = new SessionMenuManager;
		connect (SessionMenuMgr_,
				SIGNAL (sessionRequested (QString)),
				this,
				SLOT (loadCustomSession (QString)));
		connect (SessionMenuMgr_,
				SIGNAL (saveCustomSessionRequested ()),
				this,
				SLOT (saveCustomSession ()));

		Proxy_ = proxy;
		IsRecovering_ = true;

		const auto& roots = Proxy_->GetPluginsManager ()->
				GetAllCastableRoots<IHaveRecoverableTabs*> ();
		for (QObject *root : roots)
			connect (root,
					SIGNAL (addNewTab (const QString&, QWidget*)),
					this,
					SLOT (handleNewTab (const QString&, QWidget*)),
					Qt::QueuedConnection);

		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_TabSessManager");
		for (const auto& group : settings.childGroups ())
			SessionMenuMgr_->AddCustomSession (group);

		auto rootWM = Proxy_->GetRootWindowsManager ();
		for (int i = 0; i < rootWM->GetWindowsCount (); ++i)
			handleWindow (i);

		connect (rootWM->GetQObject (),
				SIGNAL (windowAdded (int)),
				this,
				SLOT (handleWindow (int)));
	}

	void Plugin::SecondInit ()
	{
		QTimer::singleShot (5000,
				this,
				SLOT (recover ()));
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.TabSessManager";
	}

	void Plugin::Release ()
	{
		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_TabSessManager");
		settings.setValue ("CleanShutdown", true);
	}

	QString Plugin::GetName () const
	{
		return "TabSessManager";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Manages sessions of tabs in LeechCraft.");
	}

	QIcon Plugin::GetIcon () const
	{
		return QIcon ();
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Core.Plugins/1.0";
		return result;
	}

	QList<QAction*> Plugin::GetActions (ActionsEmbedPlace place) const
	{
		QList<QAction*> result;
		if (place == ActionsEmbedPlace::ToolsMenu)
		{
			result << SessionMenuMgr_->GetSessionsMenu ()->menuAction ();
			result << UncloseMenu_->menuAction ();
		}
		else if (place == ActionsEmbedPlace::CommonContextMenu)
			result << UncloseMenu_->menuAction ();
		return result;
	}

	bool Plugin::eventFilter (QObject*, QEvent *e)
	{
		if (e->type () != QEvent::DynamicPropertyChange)
			return false;

		auto propEvent = static_cast<QDynamicPropertyChangeEvent*> (e);
		if (propEvent->propertyName ().startsWith ("SessionData/"))
			handleTabRecoverDataChanged ();

		return false;
	}

	namespace
	{
		QList<QPair<QByteArray, QVariant>> GetSessionProps (QObject *tab)
		{
			QList<QPair<QByteArray, QVariant>> props;
			Q_FOREACH (const QByteArray& propName, tab->dynamicPropertyNames ())
			{
				if (!propName.startsWith ("SessionData/"))
					continue;

				props << qMakePair (propName, tab->property (propName));
			}
			return props;
		}
	}

	QByteArray Plugin::GetCurrentSession () const
	{
		QByteArray result;
		QDataStream str (&result, QIODevice::WriteOnly);

		int windowIndex = 0;
		for (const auto& list : Tabs_)
		{
			for (auto tab : list)
			{
				auto rec = qobject_cast<IRecoverableTab*> (tab);
				if (!rec)
					continue;

				auto tw = qobject_cast<ITabWidget*> (tab);
				if (!tw)
					continue;

				auto plugin = qobject_cast<IInfo*> (tw->ParentMultiTabs ());
				if (!plugin)
					continue;

				const auto& data = rec->GetTabRecoverData ();
				if (data.isEmpty ())
					continue;

				QIcon forRecover = QIcon (rec->GetTabRecoverIcon ().pixmap (32, 32));

				str << plugin->GetUniqueID ()
						<< data
						<< rec->GetTabRecoverName ()
						<< forRecover
						<< GetSessionProps (tab)
						<< windowIndex;
			}

			++windowIndex;
		}

		return result;
	}

	bool Plugin::HasTab (QWidget *widget) const
	{
		return std::any_of (Tabs_.begin (), Tabs_.end (),
				[widget] (const QList<QObject*>& list)
				{
					return std::any_of (list.begin (), list.end (),
							[widget] (QObject *obj) { return obj == widget; });
				});
	}

	void Plugin::hookTabIsRemoving (IHookProxy_ptr, int index, int windowId)
	{
		const auto rootWM = Proxy_->GetRootWindowsManager ();
		const auto tabWidget = rootWM->GetTabWidget (windowId);
		const auto widget = tabWidget->Widget (index);

		handleRemoveTab (widget);
	}

	void Plugin::handleNewTab (const QString&, QWidget *widget)
	{
		if (HasTab (widget))
			return;

		auto rootWM = Proxy_->GetRootWindowsManager ();
		auto windowIndex = rootWM->GetWindowForTab (qobject_cast<ITabWidget*> (widget));

		if (windowIndex < 0 || windowIndex >= Tabs_.size ())
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown window index for"
					<< widget;
			return;
		}

		Tabs_ [windowIndex] << widget;

		auto tab = qobject_cast<IRecoverableTab*> (widget);
		if (!tab)
			return;

		connect (widget,
				SIGNAL (tabRecoverDataChanged ()),
				this,
				SLOT (handleTabRecoverDataChanged ()));

		widget->installEventFilter (this);

		if (!tab->GetTabRecoverData ().isEmpty ())
			handleTabRecoverDataChanged ();

		const auto& posProp = widget->property ("TabSessManager/Position");
		if (posProp.isValid ())
		{
			const auto prevPos = posProp.toInt ();

			const auto tabWidget = rootWM->GetTabWidget (windowIndex);
			const auto currentIdx = tabWidget->IndexOf (widget);
			if (prevPos < tabWidget->WidgetCount () && currentIdx != prevPos)
				tabWidget->MoveTab (currentIdx, prevPos);
		}
	}

	void Plugin::handleRemoveTab (QWidget *widget)
	{
		auto tab = qobject_cast<ITabWidget*> (widget);
		if (!tab)
			return;

		auto removeGuard = [this, widget] (void*)
		{
			for (auto& list : Tabs_)
				list.removeAll (widget);
			handleTabRecoverDataChanged ();
		};
		std::shared_ptr<void> guard (static_cast<void*> (0), removeGuard);

		auto recTab = qobject_cast<IRecoverableTab*> (widget);
		if (!recTab)
			return;

		const auto& recoverData = recTab->GetTabRecoverData ();
		if (recoverData.isEmpty ())
			return;

		TabUncloseInfo info
		{
			{
				recoverData,
				GetSessionProps (widget)
			},
			qobject_cast<IHaveRecoverableTabs*> (tab->ParentMultiTabs ())
		};

		const auto rootWM = Proxy_->GetRootWindowsManager ();
		const auto winIdx = rootWM->GetWindowForTab (tab);
		const auto tabIdx = rootWM->GetTabWidget (winIdx)->IndexOf (widget);
		info.RecInfo_.DynProperties_.append ({ "TabSessManager/Position", tabIdx });

		const auto pos = std::find_if (UncloseAct2Data_.begin (), UncloseAct2Data_.end (),
				[&info] (const TabUncloseInfo& that) { return that.RecInfo_.Data_ == info.RecInfo_.Data_; });
		if (pos != UncloseAct2Data_.end ())
		{
			auto act = pos.key ();
			UncloseMenu_->removeAction (act);
			UncloseAct2Data_.erase (pos);
			delete act;
		}

		const auto& fm = qApp->fontMetrics ();
		const QString& elided = fm.elidedText (recTab->GetTabRecoverName (), Qt::ElideMiddle, 300);
		QAction *action = new QAction (recTab->GetTabRecoverIcon (), elided, this);
		UncloseAct2Data_ [action] = info;

		connect (action,
				SIGNAL (triggered ()),
				this,
				SLOT (handleUnclose ()));

		if (UncloseMenu_->defaultAction ())
			UncloseMenu_->defaultAction ()->setShortcut (QKeySequence ());
		UncloseMenu_->insertAction (UncloseMenu_->actions ().value (0), action);
		UncloseMenu_->setDefaultAction (action);
		action->setShortcut (QString ("Ctrl+Shift+T"));
	}

	void Plugin::handleTabMoved (int from, int to)
	{
		const auto rootWM = Proxy_->GetRootWindowsManager ();
		const auto tabWidget = qobject_cast<ICoreTabWidget*> (sender ());
		const auto pos = rootWM->GetTabWidgetIndex (tabWidget);

		auto& tabs = Tabs_ [pos];

		if (std::max (from, to) >= tabs.size () ||
			std::min (from, to) < 0)
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid"
					<< from
					<< "->"
					<< to
					<< "; total tabs:"
					<< Tabs_.size ();
			return;
		}

		auto tab = tabs.takeAt (from);
		tabs.insert (to, tab);

		handleTabRecoverDataChanged ();
	}

	namespace
	{
		QHash<QObject*, QList<RecInfo>> GetTabsFromStream (QDataStream& str, ICoreProxy_ptr proxy)
		{
			QHash<QByteArray, QObject*> pluginCache;
			QHash<QObject*, QList<RecInfo>> tabs;

			int order = 0;
			while (!str.atEnd ())
			{
				QByteArray pluginId;
				QByteArray recData;
				QString name;
				QIcon icon;
				QList<QPair<QByteArray, QVariant>> props;
				int winId = 0;

				str >> pluginId >> recData >> name >> icon >> props >> winId;
				if (!pluginCache.contains (pluginId))
				{
					QObject *obj = proxy->GetPluginsManager ()->
							GetPluginByID (pluginId);
					pluginCache [pluginId] = obj;
				}

				QObject *plugin = pluginCache [pluginId];
				if (!plugin)
				{
					qWarning () << "null plugin for" << pluginId;
					continue;
				}

				tabs [plugin] << RecInfo { order++, recData, props, name, icon, winId };

				qDebug () << Q_FUNC_INFO << "got restore data for"
						<< pluginId << name << plugin << "; window" << winId;
			}

			Q_FOREACH (QObject *obj, tabs.keys (QList<RecInfo> ()))
				tabs.remove (obj);

			return tabs;
		}

		void AskTabs (QHash<QObject*, QList<RecInfo>>& tabs)
		{
			if (tabs.isEmpty ())
				return;

			RestoreSessionDialog dia;
			dia.SetPages (tabs);

			if (dia.exec () != QDialog::Accepted)
			{
				tabs.clear ();
				return;
			}

			tabs = dia.GetPages ();
		}

		void OpenTabs (const QHash<QObject*, QList<RecInfo>>& tabs)
		{
			QList<QPair<IHaveRecoverableTabs*, RecInfo>> ordered;
			for (auto i = tabs.begin (); i != tabs.end (); ++i)
			{
				const auto plugin = i.key ();

				auto ihrt = qobject_cast<IHaveRecoverableTabs*> (plugin);
				if (!ihrt)
					continue;

				for (const auto& info : i.value ())
					ordered << qMakePair (ihrt, info);
			}

			std::sort (ordered.begin (), ordered.end (),
					[] (decltype (ordered.at (0)) left, decltype (ordered.at (0)) right)
						{ return left.second.Order_ < right.second.Order_; });

			for (const auto& pair : ordered)
			{
				auto props = pair.second.Props_;
				props.append ({ "SessionData/RootWindowIndex", pair.second.WindowID_ });
				pair.first->RecoverTabs ({ TabRecoverInfo { pair.second.Data_, props } });
			}
		}
	}

	void Plugin::handleUnclose ()
	{
		auto action = qobject_cast<QAction*> (sender ());
		if (!action)
		{
			qWarning () << Q_FUNC_INFO
					<< "sender is not an action:"
					<< sender ();
			return;
		}

		if (!UncloseAct2Data_.contains (action))
			return;

		action->deleteLater ();

		auto data = UncloseAct2Data_.take (action);
		if (UncloseMenu_->defaultAction () == action)
		{
			auto nextAct = UncloseMenu_->actions ().value (1);
			if (nextAct)
			{
				UncloseMenu_->setDefaultAction (nextAct);
				nextAct->setShortcut (QString ("Ctrl+Shift+T"));
			}
		}
		UncloseMenu_->removeAction (action);

		data.Plugin_->RecoverTabs (QList<TabRecoverInfo> () << data.RecInfo_);
	}

	void Plugin::recover ()
	{
		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_TabSessManager");

		QDataStream str (settings.value ("Data").toByteArray ());
		auto tabs = GetTabsFromStream (str, Proxy_);

		if (!settings.value ("CleanShutdown", false).toBool ())
			AskTabs (tabs);

		OpenTabs (tabs);

		IsRecovering_ = false;
		settings.setValue ("CleanShutdown", false);
	}

	void Plugin::handleTabRecoverDataChanged ()
	{
		if (IsRecovering_ || Proxy_->IsShuttingDown ())
			return;

		if (IsScheduled_)
			return;

		IsScheduled_ = true;
		QTimer::singleShot (2000,
				this,
				SLOT (saveDefaultSession ()));
	}

	void Plugin::saveDefaultSession ()
	{
		IsScheduled_ = false;

		const auto& result = GetCurrentSession ();

		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_TabSessManager");
		settings.setValue ("Data", result);
	}

	void Plugin::saveCustomSession ()
	{
		auto rootWM = Proxy_->GetRootWindowsManager ();
		const QString& name = QInputDialog::getText (rootWM->GetPreferredWindow (),
				tr ("Custom session"),
				tr ("Enter the name of the session:"));
		if (name.isEmpty ())
			return;

		const auto& result = GetCurrentSession ();
		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_TabSessManager");
		settings.beginGroup (name);
		settings.setValue ("Data", result);
		settings.endGroup ();

		SessionMenuMgr_->AddCustomSession (name);
	}

	void Plugin::loadCustomSession (const QString& name)
	{
		QSettings settings (QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_TabSessManager");
		settings.beginGroup (name);
		QDataStream str (settings.value ("Data").toByteArray ());
		settings.endGroup ();

		auto tabs = GetTabsFromStream (str, Proxy_);
		OpenTabs (tabs);
	}

	void Plugin::handleWindow (int index)
	{
		Tabs_ << QList<QObject*> ();
		connect (Proxy_->GetRootWindowsManager ()->GetTabWidget (index)->GetQObject (),
				SIGNAL (tabWasMoved (int, int)),
				this,
				SLOT (handleTabMoved (int, int)));
	}

	void Plugin::handleWindowRemoved (int index)
	{
		Tabs_.removeAt (index);
		handleTabRecoverDataChanged ();
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_tabsessmanager, LeechCraft::TabSessManager::Plugin);
