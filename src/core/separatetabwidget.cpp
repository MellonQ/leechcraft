/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2011  Oleg Linkin
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

#include "separatetabwidget.h"
#include <algorithm>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QToolBar>
#include <QMenu>
#include <QMouseEvent>
#include <QLayoutItem>
#include <QLayout>
#include <QShortcut>
#include <QStyle>
#include <QtDebug>
#include <interfaces/ihavetabs.h>
#include <interfaces/ihaverecoverabletabs.h>
#include "coreproxy.h"
#include "separatetabbar.h"
#include "xmlsettingsmanager.h"
#include "core.h"
#include "util/xpc/defaulthookproxy.h"
#include "coreinstanceobject.h"
#include "coreplugin2manager.h"
#include "tabmanager.h"
#include "rootwindowsmanager.h"
#include "mainwindow.h"
#include "iconthemeengine.h"

namespace LeechCraft
{
	SeparateTabWidget::SeparateTabWidget (QWidget *parent)
	: QWidget (parent)
	, Window_ (0)
	, LastContextMenuTab_ (-1)
	, MainStackedWidget_ (new QStackedWidget)
	, MainTabBar_ (new SeparateTabBar)
	, AddTabButton_ (new QToolButton)
	, LeftToolBar_ (new QToolBar)
	, RightToolBar_ (new QToolBar)
	, DefaultTabAction_ (new QAction (QString (), this))
	, CurrentWidget_ (0)
	, CurrentIndex_ (-1)
	, PreviousWidget_ (0)
	, CurrentToolBar_ (0)
	{
		XmlSettingsManager::Instance ()->RegisterObject ("SelectionBehavior",
			this, "handleSelectionBehavior");
		Core::Instance ().GetCoreInstanceObject ()->
				GetCorePluginManager ()->RegisterHookable (this);
		handleSelectionBehavior ();

		MainTabBar_->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Minimum);
		MainTabBar_->SetTabWidget (this);

		LeftToolBar_->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Minimum);
		RightToolBar_->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Minimum);

		LeftToolBar_->setMaximumHeight (25);
		RightToolBar_->setMaximumHeight (25);

		auto leftToolBarLayout = new QVBoxLayout;
		auto rightToolBarLayout = new QVBoxLayout;
		leftToolBarLayout->addWidget (LeftToolBar_);
		leftToolBarLayout->setContentsMargins (0, 0, 0, 0);
		rightToolBarLayout->addWidget (RightToolBar_);
		rightToolBarLayout->setContentsMargins (0, 0, 0, 0);

		MainTabBarLayout_ = new QHBoxLayout;
		MainTabBarLayout_->setContentsMargins (0, 0, 0, 0);
		MainTabBarLayout_->setSpacing (0);
		MainTabBarLayout_->addLayout (leftToolBarLayout);
		MainTabBarLayout_->addWidget (MainTabBar_);
		MainTabBarLayout_->addLayout (rightToolBarLayout);

		MainToolBarLayout_ = new QHBoxLayout;
		MainToolBarLayout_->setSpacing (0);
		MainToolBarLayout_->setContentsMargins (0, 0, 0, 0);

		MainToolBarLayout_->addSpacerItem (new QSpacerItem (1, 1,
				QSizePolicy::Minimum, QSizePolicy::Minimum));

		MainLayout_ = new QVBoxLayout (this);
		MainLayout_->setContentsMargins (0, 0, 0, 0);
		MainLayout_->setSpacing (0);
		MainLayout_->addLayout (MainToolBarLayout_);
		MainLayout_->addWidget (MainStackedWidget_);

		XmlSettingsManager::Instance ()->RegisterObject ("TabBarPosition",
				this, "handleTabBarPosition");
		handleTabBarPosition ();

		Init ();
		AddTabButtonInit ();
	}

	void SeparateTabWidget::SetWindow (MainWindow *window)
	{
		Window_ = window;

		MainTabBar_->SetWindow (window);
	}

	QObject* SeparateTabWidget::GetQObject ()
	{
		return this;
	}

	int SeparateTabWidget::WidgetCount () const
	{
		return MainStackedWidget_->count ();
	}

	QWidget* SeparateTabWidget::Widget (int index) const
	{
		return MainStackedWidget_->widget (index);
	}

	QList<QAction*> SeparateTabWidget::GetPermanentActions () const
	{
		QList<QAction*> result;
		std::transform (TabBarActions_.begin (), TabBarActions_.end (),
				std::back_inserter (result),
				[] (decltype (TabBarActions_.front ()) action)
				{
					return action.data ();
				});
		return result;
	}

	QString SeparateTabWidget::TabText (int index) const
	{
		return TabNames_.value (index);
	}

	void SeparateTabWidget::SetTabText (int index, const QString& text)
	{
		if (index < 0 || index >= WidgetCount ())
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid index"
					<< index;
			return;
		}

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookTabSetText (proxy, index,
				Core::Instance ().GetRootWindowsManager ()->GetWindowIndex (Window_));
		if (proxy->IsCancelled ())
			return;

		MainTabBar_->setTabText (index, text);
		MainTabBar_->setTabToolTip (index, text);
		if (!text.isEmpty ())
			TabNames_ [index] = text;
	}

	QIcon SeparateTabWidget::TabIcon (int index) const
	{
		return MainTabBar_->tabIcon (index);
	}

	void SeparateTabWidget::SetTabIcon (int index, const QIcon& icon)
	{
		if (index < 0 ||
				index >= WidgetCount ())
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid index"
					<< index;
			return;
		}

		MainTabBar_->setTabIcon (index, icon);
	}

	QString SeparateTabWidget::TabToolTip (int index) const
	{
		return MainTabBar_->tabToolTip (index);
	}

	void SeparateTabWidget::SetTabToolTip (int index, const QString& tip)
	{
		if (index < 0 ||
				index >= WidgetCount ())
		{
			qWarning () << Q_FUNC_INFO
				<< "invalid index"
				<< index;
			return;
		}

		MainTabBar_->setTabToolTip (index, tip);
	}

	QWidget* SeparateTabWidget::TabButton (int index, QTabBar::ButtonPosition positioin) const
	{
		return MainTabBar_->tabButton (index, positioin);
	}

	QTabBar::ButtonPosition SeparateTabWidget::GetCloseButtonPosition () const
	{
		return MainTabBar_->GetCloseButtonPosition ();
	}

	void SeparateTabWidget::SetTabClosable (int index, bool closable, QWidget *closeButton)
	{
		MainTabBar_->SetTabClosable (index, closable, closeButton);
	}

	void SeparateTabWidget::SetTabsClosable (bool closable)
	{
		MainTabBar_->setTabsClosable (closable);
		MainTabBar_->SetTabClosable (WidgetCount (), false);
	}

	void SeparateTabWidget::AddWidget2TabBarLayout (QTabBar::ButtonPosition pos,
			QWidget *w)
	{
		if (pos == QTabBar::LeftSide)
			LeftToolBar_->addWidget (w);
		else
			RightToolBar_->addWidget (w);
	}

	void SeparateTabWidget::AddAction2TabBarLayout (QTabBar::ButtonPosition pos,
			QAction *action)
	{
		if (pos == QTabBar::LeftSide)
			LeftToolBar_->addAction (action);
		else
			RightToolBar_->addAction (action);
	}

	void SeparateTabWidget::InsertAction2TabBarLayout (QTabBar::ButtonPosition pos,
			QAction *action, int index)
	{
		const auto tb = pos == QTabBar::LeftSide ? LeftToolBar_ : RightToolBar_;
		tb->insertAction (tb->actions ().value (index), action);
	}

	void SeparateTabWidget::RemoveActionFromTabBarLayout (QTabBar::ButtonPosition pos,
			QAction *action)
	{
		if (pos == QTabBar::LeftSide)
			LeftToolBar_->removeAction (action);
		else
			RightToolBar_->removeAction (action);
	}

	void SeparateTabWidget::AddAction2TabBar (QAction *act)
	{
		TabBarActions_ << act;
		connect (act,
				SIGNAL (destroyed (QObject*)),
				this,
				SLOT (handleActionDestroyed ()));
	}

	void SeparateTabWidget::InsertAction2TabBar (int index, QAction *act)
	{
		TabBarActions_.insert (index, act);
		connect (act,
				SIGNAL (destroyed (QObject*)),
				this,
				SLOT (handleActionDestroyed ()));
	}

	void SeparateTabWidget::InsertAction2TabBar (QAction *before, QAction *action)
	{
		int idx = TabBarActions_.indexOf (before);
		if (idx < 0)
			idx = TabBarActions_.size ();
		InsertAction2TabBar (idx, action);
	}

	int SeparateTabWidget::CurrentIndex () const
	{
		return MainTabBar_->currentIndex ();
	}

	QWidget* SeparateTabWidget::CurrentWidget () const
	{
		return MainStackedWidget_->currentWidget ();
	}

	QMenu* SeparateTabWidget::GetTabMenu (int index)
	{
		QMenu *menu = new QMenu ();

		const auto widget = Widget (index);
		const auto imtw = qobject_cast<ITabWidget*> (widget);

		if (XmlSettingsManager::Instance ()->
				property ("ShowPluginMenuInTabs").toBool ())
		{
			bool asSub = XmlSettingsManager::Instance ()->
				property ("ShowPluginMenuInTabsAsSubmenu").toBool ();
			if (imtw)
			{
				const auto& tabActions = imtw->GetTabBarContextMenuActions ();

				QMenu *subMenu = asSub ?
						new QMenu (TabText (index), menu) :
						nullptr;
				for (auto act : tabActions)
					(asSub ? subMenu : menu)->addAction (act);

				if (asSub)
					menu->addMenu (subMenu);

				if (tabActions.size ())
					menu->addSeparator ();
			}
		}

		auto rootWM = Core::Instance ().GetRootWindowsManager ();
		const int windowIndex = rootWM->GetWindowIndex (Window_);

		auto moveMenu = menu->addMenu (tr ("Move tab to"));
		auto toNew = moveMenu->addAction (tr ("New window"),
				rootWM, SLOT (moveTabToNewWindow ()));
		toNew->setProperty ("TabIndex", index);
		toNew->setProperty ("FromWindowIndex", windowIndex);
		if (rootWM->GetWindowsCount () > 1)
		{
			moveMenu->addSeparator ();

			for (int i = 0; i < rootWM->GetWindowsCount (); ++i)
			{
				auto thatWin = rootWM->GetMainWindow (i);
				if (thatWin == Window_)
					continue;

				const auto& actTitle = tr ("To window %1 (%2)")
							.arg (i + 1)
							.arg (thatWin->windowTitle ());
				auto toExisting = moveMenu->addAction (actTitle,
						rootWM, SLOT (moveTabToExistingWindow ()));
				toExisting->setProperty ("TabIndex", index);
				toExisting->setProperty ("FromWindowIndex", windowIndex);
				toExisting->setProperty ("ToWindowIndex", i);
			}
		}

		const auto irt = qobject_cast<IRecoverableTab*> (widget);
		if (imtw &&
				irt &&
				(imtw->GetTabClassInfo ().Features_ & TabFeature::TFOpenableByRequest) &&
				!(imtw->GetTabClassInfo ().Features_ & TabFeature::TFSingle))
		{
			const auto cloneAct = menu->addAction (tr ("Clone tab"),
					this, SLOT (handleCloneTab ()));
			cloneAct->setProperty ("TabIndex", index);
			cloneAct->setProperty ("ActionIcon", "tab-duplicate");
		}

		for (auto act : TabBarActions_)
		{
			if (!act)
			{
				qWarning () << Q_FUNC_INFO
						<< "detected null pointer";
				continue;
			}
			menu->addAction (act);
		}

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookTabContextMenuFill (proxy, menu, index,
				Core::Instance ().GetRootWindowsManager ()->GetWindowIndex (Window_));

		return menu;
	}

	int SeparateTabWidget::IndexOf (QWidget *page) const
	{
		return MainStackedWidget_->indexOf (page);
	}

	int SeparateTabWidget::GetLastContextMenuTab () const
	{
		return LastContextMenuTab_;
	}

	void SeparateTabWidget::SetAddTabButtonContextMenu (QMenu *menu)
	{
		AddTabButtonContextMenu_ = menu;
		AddTabButton_->setMenu (AddTabButtonContextMenu_);
	}

	SeparateTabBar* SeparateTabWidget::TabBar () const
	{
		return MainTabBar_;
	}

	int SeparateTabWidget::AddTab (QWidget *page, const QString& text)
	{
		return AddTab (page, QIcon (), text);
	}

	int SeparateTabWidget::AddTab (QWidget *page,
			const QIcon& icon, const QString& text)
	{
		if (!page)
		{
			qWarning () << Q_FUNC_INFO
					<< "There is no widget to add to SeaprateTabWidget"
					<< page;
			return -1;
		}

		if (MainStackedWidget_->indexOf (page) != -1)
			return -1;

		MainStackedWidget_->addWidget (page);
		TabNames_ << text;
		const auto newIndex = MainTabBar_->
				insertTab (MainTabBar_->count () - 1, icon, text);

		MainTabBar_->setTabToolTip (newIndex, text);

		if (MainTabBar_->currentIndex () >= WidgetCount ())
			setCurrentTab (WidgetCount () - 1);

		return newIndex;
	}

	int SeparateTabWidget::InsertTab (int index, QWidget *page,
			const QString& text)
	{
		return InsertTab (index, page, QIcon (), text);
	}

	int SeparateTabWidget::InsertTab (int index, QWidget *page,
			const QIcon& icon, const QString& text)
	{
		const int newIndex = std::min (index, WidgetCount ());

		MainStackedWidget_->insertWidget (index, page);
		int idx = MainTabBar_->insertTab (newIndex, icon, text);
		MainTabBar_->setTabToolTip (idx, text);

		TabNames_.insert (index, text);

		if (MainTabBar_->currentIndex () >= WidgetCount ())
			setCurrentTab (WidgetCount () - 1);

		return idx;
	}

	void SeparateTabWidget::RemoveTab (int index)
	{
		if (index >= WidgetCount ())
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid index"
					<< index;
			return;
		}

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		const auto winId = Core::Instance ().GetRootWindowsManager ()->GetWindowIndex (Window_);
		emit hookTabIsRemoving (proxy, index, winId);
		if (proxy->IsCancelled ())
			return;

		const auto widget = Widget (index);

		if (widget == PreviousWidget_)
			PreviousWidget_ = 0;
		else if (widget == CurrentWidget_)
			CurrentWidget_ = 0;

		if (auto itw = qobject_cast<ITabWidget*> (widget))
			if (auto bar = itw->GetToolBar ())
				RemoveWidgetFromSeparateTabWidget (bar);

		if (!CurrentWidget_)
		{
			int nextIdx = -1;
			switch (MainTabBar_->selectionBehaviorOnRemove ())
			{
			case QTabBar::SelectLeftTab:
				nextIdx = index - 1;
				if (nextIdx == -1 && WidgetCount () > 1)
					nextIdx = 1;
				break;
			case QTabBar::SelectRightTab:
				nextIdx = index == WidgetCount () - 1 ?
						WidgetCount () - 2 :
						index + 1;
				break;
			default:
				nextIdx = IndexOf (PreviousWidget_);
				break;
			}

			if (nextIdx >= 0)
				setCurrentTab (nextIdx);
		}

		MainStackedWidget_->removeWidget (widget);
		MainTabBar_->removeTab (index);

		TabNames_.removeAt (index);

		widget->setParent (0);
	}

	void SeparateTabWidget::AddWidget2SeparateTabWidget (QWidget *widget)
	{
		widget->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Minimum);
		SavedWidgetParents_ [widget] = widget->parentWidget ();
		MainToolBarLayout_->addWidget (widget);
	}

	void SeparateTabWidget::RemoveWidgetFromSeparateTabWidget (QWidget *widget)
	{
		MainToolBarLayout_->removeWidget (widget);
		widget->setParent (SavedWidgetParents_.take (widget));
		widget->hide ();
	}

	int SeparateTabWidget::TabAt (const QPoint& point)
	{
		return MainTabBar_->tabAt (point);
	}

	void SeparateTabWidget::MoveTab (int from, int to)
	{
		MainTabBar_->moveTab (from, to);
	}

	QWidget* SeparateTabWidget::GetPreviousWidget () const
	{
		return PreviousWidget_;
	}

	void SeparateTabWidget::resizeEvent (QResizeEvent *event)
	{
		QWidget::resizeEvent (event);
	}

	void SeparateTabWidget::mousePressEvent (QMouseEvent *event)
	{
		const bool mBack = event->button () == Qt::XButton1;
		const bool mForward = event->button () == Qt::XButton2;
		if (mBack || mForward)
		{
			auto rootWM = Core::Instance ().GetRootWindowsManager ();
			auto tm = rootWM->GetTabManager (Window_);
			mBack ? tm->rotateLeft () : tm->rotateRight ();
			event->accept ();
			return;
		}

		QWidget::mousePressEvent (event);
	}

	void SeparateTabWidget::Init ()
	{
		connect (MainTabBar_,
				SIGNAL (addDefaultTab ()),
				this,
				SLOT (handleAddDefaultTab ()));

		connect (DefaultTabAction_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleAddDefaultTab ()));

		connect (MainTabBar_,
				SIGNAL (tabWasInserted (int)),
				this,
				SIGNAL (tabInserted (int)));
		connect (MainTabBar_,
				SIGNAL (tabWasRemoved (int)),
				this,
				SIGNAL (tabWasRemoved (int)));
		connect (MainTabBar_,
				SIGNAL (tabCloseRequested (int)),
				this,
				SIGNAL (tabCloseRequested (int)));
		connect (MainTabBar_,
				SIGNAL (tabMoved (int, int)),
				this,
				SLOT (handleTabMoved (int, int)));
		connect (MainTabBar_,
				SIGNAL (currentChanged (int)),
				this,
				SLOT (setCurrentIndex (int)));
		connect (MainTabBar_,
				SIGNAL (customContextMenuRequested (const QPoint&)),
				this,
				SLOT (handleContextMenuRequested (const QPoint&)));
		connect (MainTabBar_,
				SIGNAL (releasedMouseAfterMove (int)),
				this,
				SLOT (releaseMouseAfterMove (int)));
	}

	void SeparateTabWidget::AddTabButtonInit ()
	{
		DefaultTabAction_->setProperty ("ActionIcon", "list-add");

		AddTabButton_->setPopupMode (QToolButton::MenuButtonPopup);
		AddTabButton_->setToolButtonStyle (Qt::ToolButtonIconOnly);
		AddTabButton_->setDefaultAction (DefaultTabAction_);

		const auto iconSize = QApplication::style ()->pixelMetric (QStyle::PM_TabBarIconSize);
		AddTabButton_->setIconSize ({ iconSize, iconSize });
		AddTabButton_->setMaximumHeight (iconSize);

		AddTabButton_->setAutoRaise (true);

		auto cont = new QWidget;
		auto lay = new QHBoxLayout;
		lay->setContentsMargins (0, 0, 0, 0);
		lay->addWidget (AddTabButton_);
		cont->setLayout (lay);
		MainTabBar_->SetAddTabButton (cont);
	}

	void SeparateTabWidget::setCurrentIndex (int index)
	{
		if (index >= WidgetCount ())
			index = WidgetCount () - 1;

		auto rootWM = Core::Instance ().GetRootWindowsManager ();
		auto tabManager = rootWM->GetTabManager (Window_);

		MainStackedWidget_->setCurrentIndex (-1);
		if (CurrentToolBar_)
		{
			RemoveWidgetFromSeparateTabWidget (CurrentToolBar_);
			CurrentToolBar_ = nullptr;
		}

		MainTabBar_->setCurrentIndex (index);

		if (auto bar = tabManager->GetToolBar (index))
		{
			AddWidget2SeparateTabWidget (bar);
			bar->show ();
			CurrentToolBar_ = bar;
		}
		MainStackedWidget_->setCurrentIndex (index);

		CurrentIndex_ = index;
		if (CurrentWidget_ != Widget (index))
		{
			PreviousWidget_ = CurrentWidget_;
			CurrentWidget_ = Widget (index);
			emit currentChanged (index);
		}
	}

	void SeparateTabWidget::setCurrentTab (int tabIndex)
	{
		MainTabBar_->setCurrentIndex (tabIndex);
	}

	void SeparateTabWidget::setCurrentWidget (QWidget *widget)
	{
		if (!widget)
			return;

		int index = MainStackedWidget_->indexOf (widget);
		setCurrentTab (index);
	}

	void SeparateTabWidget::handleNewTabShortcutActivated ()
	{
		handleAddDefaultTab ();
	}

	void SeparateTabWidget::setPreviousTab ()
	{
		setCurrentWidget (PreviousWidget_);
	}

	void SeparateTabWidget::handleTabMoved (int from, int to)
	{
		if (from == MainTabBar_->count () - 1)
		{
			MainTabBar_->moveTab (to, from);
			return;
		}

		if (to == MainTabBar_->count () - 1)
			return;

		const auto& str = TabNames_.takeAt (from);
		TabNames_.insert (to, str);

		MainStackedWidget_->insertWidget (to, MainStackedWidget_->widget (from));

		MainTabBar_->SetInMove (true);
		setCurrentIndex (to);
		emit tabWasMoved (from, to);
	}

	void SeparateTabWidget::handleContextMenuRequested (const QPoint& point)
	{
		if (point.isNull ())
			return;

		QMenu *menu = new QMenu ("", MainTabBar_);
		int index = MainTabBar_->tabAt (point);

		if (index == -1)
			for (auto act : TabBarActions_)
			{
				if (!act)
				{
					qWarning () << Q_FUNC_INFO
							<< "detected null pointer";
					continue;
				}
				menu->addAction (act);
			}
		else if (index == MainTabBar_->count () - 1)
			menu->addActions (AddTabButtonContextMenu_->actions ());
		else
		{
			LastContextMenuTab_ = index;
			delete menu;
			menu = GetTabMenu (index);
		}
		menu->exec (MainTabBar_->mapToGlobal (point));
		delete menu;
	}

	void SeparateTabWidget::handleActionDestroyed ()
	{
		for (const auto& act : TabBarActions_)
			if (!act || act == sender ())
				TabBarActions_.removeAll (act);
	}

	void SeparateTabWidget::releaseMouseAfterMove (int index)
	{
		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookTabFinishedMoving (proxy, index,
				Core::Instance ().GetRootWindowsManager ()->GetWindowIndex (Window_));
	}

	void SeparateTabWidget::handleTabBarPosition ()
	{
		MainLayout_->removeItem (MainTabBarLayout_);

		const auto& settingVal = XmlSettingsManager::Instance ()->property ("TabBarPosition").toString ();
		const bool isBottom = settingVal == "Bottom";
		if (isBottom)
			MainLayout_->addLayout (MainTabBarLayout_);
		else
			MainLayout_->insertLayout (0, MainTabBarLayout_);

		MainTabBar_->setShape (isBottom ? QTabBar::RoundedSouth : QTabBar::RoundedNorth);
	}

	void SeparateTabWidget::handleSelectionBehavior ()
	{
		const QString& selection = XmlSettingsManager::Instance ()->
				property ("SelectionBehavior").toString ();
		if (selection == "PreviousActive")
			MainTabBar_->setSelectionBehaviorOnRemove (QTabBar::SelectPreviousTab);
		else if (selection == "NextIndex")
			MainTabBar_->setSelectionBehaviorOnRemove (QTabBar::SelectRightTab);
		else if (selection == "PreviousIndex")
			MainTabBar_->setSelectionBehaviorOnRemove (QTabBar::SelectLeftTab);
	}

	void SeparateTabWidget::handleAddDefaultTab ()
	{
		const auto& combined = XmlSettingsManager::Instance ()->
				property ("DefaultNewTab").toString ().toLatin1 ();
		if (combined != "contextdependent")
		{
			const auto& parts = combined.split ('|');
			if (parts.size () != 2)
				qWarning () << Q_FUNC_INFO
						<< "incorrect split"
						<< parts
						<< combined;
			else
			{
				const QByteArray& newTabId = parts.at (0);
				const QByteArray& tabClass = parts.at (1);
				QObject *plugin = Core::Instance ()
						.GetPluginManager ()->GetPluginByID (newTabId);
				IHaveTabs *iht = qobject_cast<IHaveTabs*> (plugin);
				if (!iht)
					qWarning () << Q_FUNC_INFO
							<< "plugin with id"
							<< newTabId
							<< "is not a IMultiTabs";
				else
				{
					iht->TabOpenRequested (tabClass);
					return;
				}
			}
		}

		IHaveTabs *highestIHT = 0;
		QByteArray highestTabClass;
		int highestPriority = 0;
		for (auto iht : Core::Instance ().GetPluginManager ()->GetAllCastableTo<IHaveTabs*> ())
			for (const auto& info : iht->GetTabClasses ())
			{
				if (!(info.Features_ & TFOpenableByRequest))
					continue;

				if (info.Priority_ <= highestPriority)
					continue;

				highestIHT = iht;
				highestTabClass = info.TabClass_;
				highestPriority = info.Priority_;
			}

		const auto imtw = qobject_cast<ITabWidget*> (CurrentWidget ());
		const int delta = 15;
		if (imtw && imtw->GetTabClassInfo ().Priority_ + delta > highestPriority)
		{
			highestIHT = qobject_cast<IHaveTabs*> (imtw->ParentMultiTabs ());
			highestTabClass = imtw->GetTabClassInfo ().TabClass_;
		}

		if (!highestIHT)
		{
			qWarning () << Q_FUNC_INFO
					<< "no IHT detected";
			return;
		}

		highestIHT->TabOpenRequested (highestTabClass);
	}

	void SeparateTabWidget::handleCloneTab ()
	{
		const auto index = sender ()->property ("TabIndex").toInt ();
		const auto widget = Widget (index);
		const auto irt = qobject_cast<IRecoverableTab*> (widget);

		const auto plugin = qobject_cast<ITabWidget*> (widget)->ParentMultiTabs ();
		const auto ihrt = qobject_cast<IHaveRecoverableTabs*> (plugin);

		if (!widget || !irt || !ihrt)
		{
			qWarning () << Q_FUNC_INFO
					<< "something required is null:"
					<< widget
					<< irt
					<< ihrt;
			return;
		}

		const auto& data = irt->GetTabRecoverData ();

		QList<QPair<QByteArray, QVariant>> props;
		for (const auto& name : widget->dynamicPropertyNames ())
			if (name.startsWith ("SessionData/"))
				props.append ({ name, widget->property (name) });

		ihrt->RecoverTabs ({ { data, props } });
	}
}
