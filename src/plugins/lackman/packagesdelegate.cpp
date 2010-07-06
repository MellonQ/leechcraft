/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2010  Georg Rudoy
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

#include "packagesdelegate.h"
#include <QPainter>
#include <QApplication>
#include <QTreeView>
#include <QAction>
#include <QToolButton>
#include <QScrollBar>
#include <QtDebug>
#include "packagesmodel.h"
#include "core.h"

namespace LeechCraft
{
	namespace Plugins
	{
		namespace LackMan
		{
			const int PackagesDelegate::CPadding = 7;
			const int PackagesDelegate::CIconSize = 32;
			const int PackagesDelegate::CActionsSize = 32;
			const int PackagesDelegate::CTitleSizeDelta = 2;
			const int PackagesDelegate::CNumLines = 3;

			PackagesDelegate::PackagesDelegate (QTreeView *parent)
			: QStyledItemDelegate (parent)
			, Viewport_ (parent->viewport ())
			{
				connect (parent->verticalScrollBar (),
						SIGNAL (valueChanged (int)),
						this,
						SLOT (invalidateWidgetPositions ()),
						Qt::QueuedConnection);
			}

			void PackagesDelegate::paint (QPainter *painter,
					const QStyleOptionViewItem& option, const QModelIndex& index) const
			{
				QStyleOptionViewItemV4 opt (option);
				QStyle *style = opt.widget ?
						opt.widget->style () :
						QApplication::style ();
				QFontMetrics fontMetrics = opt.widget ?
						opt.widget->fontMetrics () :
						QApplication::fontMetrics ();

				const QRect& r = option.rect;
				bool ltr = (painter->layoutDirection () == Qt::LeftToRight);
				bool selected = option.state & QStyle::State_Selected;
				if (selected)
					painter->fillRect (option.rect, option.palette.highlight ());

				QString title = index.data (Qt::DisplayRole).toString ();
				QString shortDescr = index.data (PackagesModel::PMRShortDescription).toString ();
				QString version = index.data (PackagesModel::PMRVersion).toString ();
				QStringList tags = index.data (PackagesModel::PMRTags).toStringList ();
				QColor fgColor = selected ?
						option.palette.color (QPalette::HighlightedText) :
						option.palette.color (QPalette::Text);

				QIcon icon = index.data (Qt::DecorationRole).value<QIcon> ();

				QStyleOptionViewItem titleOption (option);
				titleOption.font.setBold (true);
				titleOption.font.setPointSize (titleOption.font.pointSize () + CTitleSizeDelta);

				QPixmap pixmap (option.rect.size ());
				pixmap.fill (Qt::transparent);
				QPainter p (&pixmap);
				p.translate (-option.rect.topLeft ());

				int textShift = 2 * CPadding + CIconSize;
				int leftPos = r.left () + (ltr ? textShift : 0);
				int textWidth = r.width () - textShift - CPadding;
				int shiftFromTop = r.top () + CPadding;
				int textHeight = TextHeight (option);

				p.setPen (fgColor);
				p.setFont (titleOption.font);
				p.drawText (leftPos, shiftFromTop,
						textWidth, TitleHeight (option),
						Qt::AlignBottom | Qt::AlignLeft, title);

				shiftFromTop += TitleHeight (option);

				p.setFont (option.font);
				shortDescr = fontMetrics.elidedText (shortDescr,
						option.textElideMode, textWidth);
				p.drawText (leftPos, shiftFromTop,
						textWidth, textHeight,
						Qt::AlignTop | Qt::AlignLeft, shortDescr);

				shiftFromTop += textHeight;

				QFont tagsFont = option.font;
				tagsFont.setItalic (true);
				p.setFont (tagsFont);
				if (selected)
					p.drawText (leftPos, shiftFromTop,
							textWidth, textHeight,
							Qt::AlignBottom | Qt::AlignLeft,
							tr ("Version %1 available").arg (version));
				p.drawText (leftPos, shiftFromTop,
						textWidth, textHeight,
						Qt::AlignBottom | Qt::AlignRight, tags.join ("; "));
				style->drawPrimitive (QStyle::PE_FrameGroupBox, &option, &p);

				shiftFromTop += textHeight;

				p.end ();

				QToolButton *instRem = GetInstallRemove (index);
				instRem->move (leftPos, shiftFromTop);
				instRem->show ();

				QToolButton *update = GetUpdate (index);
				update->move (leftPos + instRem->width () + CPadding, shiftFromTop);
				update->show ();

				shiftFromTop += CActionsSize;

				if (selected)
				{
					PrepareSelectableBrowser ();
					SelectableBrowser_->SetHtml (index.data (PackagesModel::PMRLongDescription).toString ());
					SelectableBrowser_->move (leftPos, shiftFromTop);
					SelectableBrowser_->resize (textWidth, CurrentInfoHeight (option));
					SelectableBrowser_->show ();
				}

				painter->drawPixmap (option.rect, pixmap);

				icon.paint (painter,
						ltr ?
								r.left () + CPadding :
								r.left () + r.width () - CPadding - CIconSize,
						r.top () + CPadding,
						CIconSize, CIconSize,
						Qt::AlignCenter, QIcon::Normal);
			}

			QSize PackagesDelegate::sizeHint (const QStyleOptionViewItem& option,
					const QModelIndex& index) const
			{
				QSize result = index.data (Qt::SizeHintRole).toSize ();
				result.rheight () = TitleHeight (option) + TextHeight (option) * 2 + CActionsSize + CPadding * 2;
				if (index == CurrentSelection_)
					result.rheight () += CurrentInfoHeight (option);
				return result;
			}

			int PackagesDelegate::TitleHeight (const QStyleOptionViewItem& option) const
			{
				QFont boldFont = option.font;

				boldFont.setBold (true);
				boldFont.setPointSize (boldFont.pointSize () + CTitleSizeDelta);

				return QFontInfo (boldFont).pixelSize () + CPadding;
			}

			int PackagesDelegate::TextHeight (const QStyleOptionViewItem& option) const
			{
				return QFontInfo (option.font).pixelSize () + CPadding;
			}

			int PackagesDelegate::CurrentInfoHeight (const QStyleOptionViewItem& option) const
			{
				return 500;
			}

			void PackagesDelegate::PrepareSelectableBrowser () const
			{
				if (SelectableBrowser_)
					return;

				SelectableBrowser_ = new Util::SelectableBrowser ();
				QList<IWebBrowser*> browsers = Core::Instance ().GetProxy ()->
						GetPluginsManager ()->GetAllCastableTo<IWebBrowser*> ();
				if (browsers.size ())
					SelectableBrowser_->Construct (browsers.at (0));
				SelectableBrowser_->setParent (Viewport_);
				SelectableBrowser_->SetNavBarVisible (false);
			}

			QToolButton* PackagesDelegate::GetInstallRemove (const QModelIndex& index) const
			{
				int row = index.row ();
				if (!Row2InstallRemove_.contains (row))
				{
					QAction *action = new QAction (Viewport_);
					connect (action,
							SIGNAL (triggered ()),
							&Core::Instance (),
							SLOT (installRemoveRequested ()));

					QToolButton *toolButton = new QToolButton (Viewport_);
					toolButton->resize (CActionsSize, CActionsSize);
					toolButton->setDefaultAction (action);
					Row2InstallRemove_ [row] = toolButton;
				}

				QToolButton *button = Row2InstallRemove_ [row];

				bool installed = index.data (PackagesModel::PMRInstalled).toBool ();
				QString label;
				QString iconName;
				if (installed)
				{
					label = tr ("Remove");
					iconName = "remove";
				}
				else
				{
					label = tr ("Install");
					iconName = "addjob";
				}

				QAction *action = button->defaultAction ();
				action->setText (label);
				action->setIcon (Core::Instance ().GetProxy ()->GetIcon (iconName));
				action->setData (index.data (PackagesModel::PMRPackageID));

				return button;
			}

			QToolButton* PackagesDelegate::GetUpdate (const QModelIndex& index) const
			{
				int row = index.row ();
				if (!Row2Update_.contains (row))
				{
					QAction *action = new QAction (Core::Instance ()
								.GetProxy ()->GetIcon ("update"),
							tr ("Update"),
							Viewport_);
					connect (action,
							SIGNAL (triggered ()),
							&Core::Instance (),
							SLOT (updateRequested ()));

					QToolButton *toolButton = new QToolButton (Viewport_);
					toolButton->resize (CActionsSize, CActionsSize);
					toolButton->setDefaultAction (action);
					Row2Update_ [row] = toolButton;
				}

				QToolButton *button = Row2Update_ [row];
				QAction *action = button->defaultAction ();

				bool upgradable = index.data (PackagesModel::PMRUpgradable).toBool ();
				action->setEnabled (upgradable);
				action->setData (index.data (PackagesModel::PMRPackageID));

				return button;
			}

			void PackagesDelegate::handleRowChanged (const QModelIndex& current, const QModelIndex& previous)
			{
				CurrentSelection_ = current;
				if (SelectableBrowser_)
					SelectableBrowser_->hide ();
				emit sizeHintChanged (previous);
				emit sizeHintChanged (current);
			}

			void PackagesDelegate::invalidateWidgetPositions ()
			{
				SelectableBrowser_->hide ();
				QTreeView *view = qobject_cast<QTreeView*> (parent ());
				QAbstractItemModel *model = view->model ();
				for (int i = 0, rows = model->rowCount ();
						i < rows; ++i)
					emit sizeHintChanged (model->index (i, 0));
			}
		}
	}
}
