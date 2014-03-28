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

#include <algorithm>
#include <stdexcept>
#include <QMimeData>
#include <QUrl>
#include <QtDebug>
#include "mergemodel.h"

namespace LeechCraft
{
namespace Util
{
	typedef std::weak_ptr<ModelItem> ModelItem_wtr;
	typedef QVector<ModelItem_ptr> ModelItemsList_t;

	typedef std::shared_ptr<const ModelItem> ModelItem_cptr;

	class ModelItem : public std::enable_shared_from_this<ModelItem>
	{
		ModelItem_wtr Parent_;
		ModelItemsList_t Children_;

		QAbstractItemModel * const Model_;
		QModelIndex SrcIdx_;
	public:
		typedef ModelItemsList_t::iterator iterator;
		typedef ModelItemsList_t::const_iterator const_iterator;

		ModelItem ()
		: Model_ { nullptr }
		{
		}

		ModelItem (QAbstractItemModel *model, const QModelIndex& idx, const ModelItem_wtr& parent)
		: Parent_ { parent }
		, Model_ { model }
		, SrcIdx_ { idx }
		{
		}

		iterator begin ()
		{
			return Children_.begin ();
		}

		iterator end ()
		{
			return Children_.end ();
		}

		const_iterator begin () const
		{
			return Children_.begin ();
		}

		const_iterator end () const
		{
			return Children_.end ();
		}

		ModelItem_ptr GetChild (int row) const
		{
			return Children_.value (row);
		}

		const ModelItemsList_t& GetChildren () const
		{
			return Children_;
		}

		int GetRowCount () const
		{
			return Children_.size ();
		}

		ModelItem* EnsureChild (int row)
		{
			if (Children_.value (row))
				return Children_.at (row).get ();

			if (Children_.size () <= row)
				Children_.resize (row + 1);

			const auto& childIdx = Model_->index (row, 0, SrcIdx_);
			Children_ [row].reset (new ModelItem { Model_, childIdx, shared_from_this () });
			return Children_.at (row).get ();
		}

		iterator EraseChild (iterator it)
		{
			return Children_.erase (it);
		}

		iterator EraseChildren (iterator begin, iterator end)
		{
			return Children_.erase (begin, end);
		}

		template<typename... Args>
		void AppendChild (Args&&... args)
		{
			Children_ << std::make_shared<ModelItem> (std::forward<Args> (args)...);
		}

		template<typename... Args>
		void InsertChild (int pos, Args&&... args)
		{
			Children_.insert (pos, std::make_shared<ModelItem> (std::forward<Args> (args)...));
		}

		const QModelIndex& GetIndex () const
		{
			return SrcIdx_;
		}

		void RefreshIndex (int modelStartingRow)
		{
			if (SrcIdx_.isValid ())
				SrcIdx_ = Model_->index (GetRow () - modelStartingRow, 0, Parent_.lock ()->GetIndex ());
		}

		QAbstractItemModel* GetModel () const
		{
			return Model_;
		}

		ModelItem_ptr GetParent () const
		{
			return Parent_.lock ();
		}

		int GetRow (const ModelItem_ptr& item) const
		{
			return Children_.indexOf (item);
		}

		int GetRow (const ModelItem_cptr& item) const
		{
			const auto pos = std::find (Children_.begin (), Children_.end (), item);
			return pos == Children_.end () ?
					-1 :
					std::distance (Children_.begin (), pos);
		}

		int GetRow () const
		{
			return Parent_.lock ()->GetRow (shared_from_this ());
		}

		ModelItem_ptr FindChild (QModelIndex index) const
		{
			index = index.sibling (index.row (), 0);

			const auto pos = std::find_if (Children_.begin (), Children_.end (),
					[&index] (const ModelItem_ptr& item) { return item->GetIndex () == index; });
			return pos == Children_.end () ? ModelItem_ptr {} : *pos;
		}
	};

	MergeModel::MergeModel (const QStringList& headers, QObject *parent)
	: QAbstractItemModel (parent)
	, DefaultAcceptsRowImpl_ (false)
	, Headers_ (headers)
	, Root_ (new ModelItem)
	{
	}

	int MergeModel::columnCount (const QModelIndex& index) const
	{
		if (index.isValid ())
		{
			const auto& mapped = mapToSource (index);
			return mapped.model ()->columnCount (mapped);
		}
		else
			return Headers_.size ();
	}

	QVariant MergeModel::headerData (int column, Qt::Orientation orient, int role) const
	{
		if (orient != Qt::Horizontal || role != Qt::DisplayRole)
			return QVariant ();

		return Headers_.at (column);
	}

	QVariant MergeModel::data (const QModelIndex& index, int role) const
	{
		if (!index.isValid ())
			return QVariant ();

		try
		{
			return mapToSource (index).data (role);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
			return {};
		}
	}

	Qt::ItemFlags MergeModel::flags (const QModelIndex& index) const
	{
		try
		{
			return mapToSource (index).flags ();
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
			return {};
		}
	}

	QModelIndex MergeModel::index (int row, int column, const QModelIndex& parent) const
	{
		if (!hasIndex (row, column, parent))
			return {};

		auto parentItem = parent.isValid () ?
				static_cast<ModelItem*> (parent.internalPointer ()) :
				Root_.get ();

		return createIndex (row, column, parentItem->EnsureChild (row));
	}

	QModelIndex MergeModel::parent (const QModelIndex& index) const
	{
		if (!index.isValid () || index.internalPointer () == Root_.get ())
			return {};

		auto item = static_cast<ModelItem*> (index.internalPointer ());
		auto parent = item->GetParent ();
		if (parent == Root_)
			return {};

		return createIndex (parent->GetRow (), 0, parent.get ());
	}

	int MergeModel::rowCount (const QModelIndex& parent) const
	{
		if (!parent.isValid ())
			return Root_->GetRowCount ();

		const auto item = static_cast<ModelItem*> (parent.internalPointer ());
		return item->GetModel ()->rowCount (item->GetIndex ());
	}

	QStringList MergeModel::mimeTypes () const
	{
		QStringList result;
		for (const auto model : GetAllModels ())
			for (const auto& type : model->mimeTypes ())
				if (!result.contains (type))
					result << type;
		return result;
	}

	namespace
	{
		void Merge (QMimeData *out, const QMimeData *sub)
		{
			for (const auto& format : sub->formats ())
				if (format != "text/uri-list" && !out->hasFormat (format))
					out->setData (format, sub->data (format));

			out->setUrls (out->urls () + sub->urls ());
		}
	}

	QMimeData* MergeModel::mimeData (const QModelIndexList& indexes) const
	{
		QMimeData *result = nullptr;

		for (const auto& index : indexes)
		{
			const auto& src = mapToSource (index);

			const auto subresult = src.model ()->mimeData ({ src });

			if (!subresult)
				continue;

			if (!result)
				result = subresult;
			else
			{
				Merge (result, subresult);
				delete subresult;
			}
		}

		return result;
	}

	QModelIndex MergeModel::mapFromSource (const QModelIndex& sourceIndex) const
	{
		if (!sourceIndex.isValid ())
			return {};

		QList<QModelIndex> hier;
		auto parent = sourceIndex;
		while (parent.isValid ())
		{
			hier.prepend (parent);
			parent = parent.parent ();
		}

		auto currentItem = Root_;
		for (const auto& idx : hier)
		{
			currentItem = currentItem->FindChild (idx);
			if (!currentItem)
			{
				qWarning () << Q_FUNC_INFO
						<< "no next item for"
						<< idx
						<< hier;
				return {};
			}
		}

		return createIndex (currentItem->GetRow (), sourceIndex.column (), currentItem.get ());
	}

	QModelIndex MergeModel::mapToSource (const QModelIndex& proxyIndex) const
	{
		const auto item = proxyIndex.isValid () ?
				static_cast<ModelItem*> (proxyIndex.internalPointer ()) :
				Root_.get ();

		const auto& srcIdx = item->GetIndex ();
		return srcIdx.sibling (srcIdx.row (), proxyIndex.column ());
	}

	void MergeModel::setSourceModel (QAbstractItemModel*)
	{
		throw std::runtime_error ("You should not set source model via setSourceModel()");
	}

	void MergeModel::SetHeaders (const QStringList& headers)
	{
		Headers_ = headers;
	}

	void MergeModel::AddModel (QAbstractItemModel *model)
	{
		if (!model)
			return;

		Models_.push_back (model);

		connect (model,
				SIGNAL (columnsAboutToBeInserted (const QModelIndex&, int, int)),
				this,
				SLOT (handleColumnsAboutToBeInserted (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (columnsAboutToBeRemoved (const QModelIndex&, int, int)),
				this,
				SLOT (handleColumnsAboutToBeRemoved (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (columnsInserted (const QModelIndex&, int, int)),
				this,
				SLOT (handleColumnsInserted (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (columnsRemoved (const QModelIndex&, int, int)),
				this,
				SLOT (handleColumnsRemoved (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (dataChanged (const QModelIndex&, const QModelIndex&)),
				this,
				SLOT (handleDataChanged (const QModelIndex&, const QModelIndex&)));
		connect (model,
				SIGNAL (layoutAboutToBeChanged ()),
				this,
				SLOT (handleModelAboutToBeReset ()));
		connect (model,
				SIGNAL (layoutChanged ()),
				this,
				SLOT (handleModelReset ()));
		connect (model,
				SIGNAL (modelAboutToBeReset ()),
				this,
				SLOT (handleModelAboutToBeReset ()));
		connect (model,
				SIGNAL (modelReset ()),
				this,
				SLOT (handleModelReset ()));
		connect (model,
				SIGNAL (rowsAboutToBeInserted (const QModelIndex&, int, int)),
				this,
				SLOT (handleRowsAboutToBeInserted (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (rowsAboutToBeRemoved (const QModelIndex&, int, int)),
				this,
				SLOT (handleRowsAboutToBeRemoved (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (rowsInserted (const QModelIndex&, int, int)),
				this,
				SLOT (handleRowsInserted (const QModelIndex&, int, int)));
		connect (model,
				SIGNAL (rowsRemoved (const QModelIndex&, int, int)),
				this,
				SLOT (handleRowsRemoved (const QModelIndex&, int, int)));

		if (const auto rc = model->rowCount ())
		{
			beginInsertRows ({}, rowCount ({}), rowCount ({}) + rc - 1);

			for (auto i = 0; i < rc; ++i)
				Root_->AppendChild (model, model->index (i, 0), Root_);

			endInsertRows ();
		}
	}

	MergeModel::const_iterator MergeModel::FindModel (const QAbstractItemModel *model) const
	{
		return std::find (Models_.begin (), Models_.end (), model);
	}

	MergeModel::iterator MergeModel::FindModel (const QAbstractItemModel *model)
	{
		return std::find (Models_.begin (), Models_.end (), model);
	}

	void MergeModel::RemoveModel (QAbstractItemModel *model)
	{
		auto i = FindModel (model);

		if (i == Models_.end ())
		{
			qWarning () << Q_FUNC_INFO << "not found model" << model;
			return;
		}

		for (auto r = Root_->begin (); r != Root_->end (); )
			if ((*r)->GetModel () == model)
			{
				const auto idx = std::distance (Root_->begin (), r);

				beginRemoveRows ({}, idx, idx);
				r = Root_->EraseChild (r);
				endRemoveRows ();
			}
			else
				++r;
	}

	size_t MergeModel::Size () const
	{
		return Models_.size ();
	}

	int MergeModel::GetStartingRow (MergeModel::const_iterator it) const
	{
		int result = 0;
		for (auto i = Models_.begin (); i != it; ++i)
			result += (*i)->rowCount ({});
		return result;
	}

	MergeModel::const_iterator MergeModel::GetModelForRow (int row, int *starting) const
	{
		const auto child = Root_->GetChild (row);
		const auto it = FindModel (child->GetModel ());

		if (starting)
			*starting = GetStartingRow (it);

		return it;
	}

	MergeModel::iterator MergeModel::GetModelForRow (int row, int *starting)
	{
		const auto child = Root_->GetChild (row);
		const auto it = FindModel (child->GetModel ());

		if (starting)
			*starting = GetStartingRow (it);

		return it;
	}

	QList<QAbstractItemModel*> MergeModel::GetAllModels () const
	{
		QList<QAbstractItemModel*> result;
		for (auto p : Models_)
			if (p)
				result << p.data ();
		return result;
	}

	void MergeModel::handleColumnsAboutToBeInserted (const QModelIndex&, int, int)
	{
		qWarning () << "model" << sender ()
			<< "called handleColumnsAboutToBeInserted, ignoring it";
		return;
	}

	void MergeModel::handleColumnsAboutToBeRemoved (const QModelIndex&, int, int)
	{
		qWarning () << "model" << sender ()
			<< "called handleColumnsAboutToBeRemoved, ignoring it";
		return;
	}

	void MergeModel::handleColumnsInserted (const QModelIndex&, int, int)
	{
		qWarning () << "model" << sender ()
			<< "called handleColumnsInserted, ignoring it";
		return;
	}

	void MergeModel::handleColumnsRemoved (const QModelIndex&, int, int)
	{
		qWarning () << "model" << sender ()
			<< "called handleColumnsRemoved, ignoring it";
		return;
	}

	void MergeModel::handleDataChanged (const QModelIndex& topLeft,
			const QModelIndex& bottomRight)
	{
		emit dataChanged (mapFromSource (topLeft), mapFromSource (bottomRight));
	}

	void MergeModel::handleRowsAboutToBeInserted (const QModelIndex& parent,
			int first, int last)
	{
		const auto model = static_cast<QAbstractItemModel*> (sender ());

		const auto startingRow = parent.isValid () ?
				0 :
				GetStartingRow (FindModel (model));
		beginInsertRows (mapFromSource (parent),
				first + startingRow, last + startingRow);
	}

	void MergeModel::handleRowsAboutToBeRemoved (const QModelIndex& parent,
			int first, int last)
	{
		auto model = static_cast<QAbstractItemModel*> (sender ());

		const auto startingRow = parent.isValid () ?
				0 :
				GetStartingRow (FindModel (model));
		beginRemoveRows (mapFromSource (parent),
				first + startingRow, last + startingRow);

		const auto rawItem = parent.isValid () ?
				static_cast<ModelItem*> (mapFromSource (parent).internalPointer ()) :
				Root_.get ();
		const auto& item = rawItem->shared_from_this ();

		auto it = item->EraseChildren (item->begin () + startingRow + first,
				item->begin () + startingRow + last + 1);

		for ( ; it < item->end (); ++it)
		{
			if ((*it)->GetModel () != model)
				break;

			(*it)->RefreshIndex (startingRow);
		}
	}

	void MergeModel::handleRowsInserted (const QModelIndex& parent, int first, int last)
	{
		const auto model = static_cast<QAbstractItemModel*> (sender ());

		const auto startingRow = parent.isValid () ?
				0 :
				GetStartingRow (FindModel (model));

		const auto rawItem = parent.isValid () ?
				static_cast<ModelItem*> (mapFromSource (parent).internalPointer ()) :
				Root_.get ();
		const auto& item = rawItem->shared_from_this ();

		for ( ; first <= last; ++first)
		{
			const auto& srcIdx = model->index (first, 0, parent);
			item->InsertChild (startingRow + first, model, srcIdx, item);
		}

		++last;
		last += startingRow;

		for (int rc = item->GetRowCount (); last < rc; ++last)
		{
			const auto child = item->GetChild (last);
			if (child->GetModel () != model)
				break;

			child->RefreshIndex (startingRow);
		}

		endInsertRows ();
	}

	void MergeModel::handleRowsRemoved (const QModelIndex&, int, int)
	{
		endRemoveRows ();
	}

	void MergeModel::handleModelAboutToBeReset ()
	{
		const auto model = static_cast<QAbstractItemModel*> (sender ());
		if (const auto rc = model->rowCount ())
		{
			const auto startingRow = GetStartingRow (FindModel (model));
			beginRemoveRows ({}, startingRow, rc + startingRow - 1);
			Root_->EraseChildren (Root_->begin () + startingRow, Root_->begin () + startingRow + rc);
			endRemoveRows ();
		}
	}

	void MergeModel::handleModelReset ()
	{
		const auto model = static_cast<QAbstractItemModel*> (sender ());
		if (const auto rc = model->rowCount ())
		{
			const auto startingRow = GetStartingRow (FindModel (model));

			beginInsertRows ({}, startingRow, rc + startingRow - 1);

			for (int i = 0; i < rc; ++i)
				Root_->InsertChild (startingRow + i, model, model->index (i, 0, {}), Root_);

			endInsertRows ();
		}
	}

	bool MergeModel::AcceptsRow (QAbstractItemModel*, int) const
	{
		DefaultAcceptsRowImpl_ = true;
		return true;
	}

	int MergeModel::RowCount (QAbstractItemModel *model) const
	{
		if (!model)
			return 0;

		int orig = model->rowCount ();
		if (DefaultAcceptsRowImpl_)
			return orig;

		int result = 0;
		for (int i = 0; i < orig; ++i)
			result += AcceptsRow (model, i) ? 1 : 0;
		return result;
	}
}
}
