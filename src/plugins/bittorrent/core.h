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

#include <map>
#include <list>
#include <deque>
#include <memory>
#include <QAbstractItemModel>
#include <QPair>
#include <QList>
#include <QVector>
#include <QIcon>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/session_status.hpp>
#include <libtorrent/session.hpp>
#include <interfaces/iinfo.h>
#include <interfaces/structures.h>
#include <util/tags/tagscompletionmodel.h>
#include "torrentinfo.h"
#include "fileinfo.h"
#include "peerinfo.h"

class QTimer;
class QDomElement;
class QToolBar;
class QStandardItemModel;
class QDataStream;

namespace libtorrent
{
	struct cache_status;
	class session;
};

struct EntityTestHandleResult;

namespace LeechCraft
{
namespace Util
{
class ShortcutManager;
}

namespace Plugins
{
namespace BitTorrent
{
	class NotifyManager;
	class PiecesModel;
	class PeersModel;
	class TorrentFilesModel;
	class RepresentationModel;
	class LiveStreamManager;
	struct NewTorrentParams;

	class Core : public QAbstractItemModel
	{
		Q_OBJECT

		enum TorrentState
		{
			TSIdle
			, TSPreparing
			, TSDownloading
			, TSSeeding
		};

		struct TorrentStruct
		{
			std::vector<int> FilePriorities_;
			libtorrent::torrent_handle Handle_;
			QByteArray TorrentFileContents_;
			QString TorrentFileName_;
			TorrentState State_;
			double Ratio_;
			/** Holds the IDs of tags of the torrent.
				*/
			QStringList Tags_;
			bool AutoManaged_;

			int ID_;
			LeechCraft::TaskParameters Parameters_;
		};


		mutable QMap<libtorrent::torrent_handle, libtorrent::torrent_status> Handle2Status_;
	public:
		struct PerTrackerStats
		{
			qint64 DownloadRate_;
			qint64 UploadRate_;

			PerTrackerStats ();
		};
		typedef QMap<QString, PerTrackerStats> pertrackerstats_t;
	private:
		struct PerTrackerAccumulator
		{
			pertrackerstats_t& Stats_;

			PerTrackerAccumulator (pertrackerstats_t&);
			int operator() (int, const Core::TorrentStruct& str);
		};

		NotifyManager *NotifyManager_;

		libtorrent::session *Session_;

		typedef QList<TorrentStruct> HandleDict_t;
		HandleDict_t Handles_;
		QList<QString> Headers_;
		mutable int CurrentTorrent_;
		std::shared_ptr<QTimer> SettingsSaveTimer_, FinishedTimer_, WarningWatchdog_, ScrapeTimer_;
		std::shared_ptr<LiveStreamManager> LiveStreamManager_;
		QString ExternalAddress_;
		bool SaveScheduled_;
		QToolBar *Toolbar_;
		QWidget *TabWidget_;
		ICoreProxy_ptr Proxy_;
		QMenu *Menu_;
		Util::ShortcutManager *ShortcutMgr_;

		const QIcon TorrentIcon_;

		Core ();
	public:
		enum Columns
		{
			ColumnID,
			ColumnName,
			ColumnState,
			ColumnProgress,  // percentage, Downloaded of Size
			ColumnDownSpeed,
			ColumnUpSpeed,
			ColumnLeechers,
			ColumnSeeders,
			ColumnUploaded
		};
		enum AddType
		{
			Started,
			Paused
		};
		enum SettingsPreset
		{
			SPDefault,
			SPMinMemoryUsage,
			SPHighPerfSeed
		};
		enum Roles
		{
			FullLengthText = Qt::UserRole + 1,
			SortRole
		};

		static Core* Instance ();

		void SetWidgets (QToolBar*, QWidget*);
		void SetMenu (QMenu*);
		void DoDelayedInit ();
		void Release ();

		void SetProxy (ICoreProxy_ptr);
		ICoreProxy_ptr GetProxy () const;

		Util::ShortcutManager* GetShortcutManager () const;

		EntityTestHandleResult CouldDownload (const LeechCraft::Entity&) const;
		EntityTestHandleResult CouldHandle (const LeechCraft::Entity&) const;
		void Handle (LeechCraft::Entity);
		PiecesModel* GetPiecesModel (int);
		PeersModel* GetPeersModel (int);
		QAbstractItemModel* GetWebSeedsModel (int);
		TorrentFilesModel* GetTorrentFilesModel (int);

		virtual int columnCount (const QModelIndex& = QModelIndex ()) const;
		virtual QVariant data (const QModelIndex&, int = Qt::DisplayRole) const;
		virtual Qt::ItemFlags flags (const QModelIndex&) const;
		virtual bool hasChildren (const QModelIndex&) const;
		virtual QVariant headerData (int, Qt::Orientation, int = Qt::DisplayRole) const;
		virtual QModelIndex index (int, int, const QModelIndex& = QModelIndex ()) const;
		virtual QModelIndex parent (const QModelIndex&) const;
		virtual int rowCount (const QModelIndex& = QModelIndex ()) const;

		QIcon GetTorrentIcon (int) const;

		libtorrent::torrent_handle GetTorrentHandle (int) const;

		libtorrent::torrent_info GetTorrentInfo (const QString&);
		libtorrent::torrent_info GetTorrentInfo (const QByteArray&);
		bool IsValidTorrent (const QByteArray&) const;
		std::unique_ptr<TorrentInfo> GetTorrentStats (int) const;
		libtorrent::session_status GetOverallStats () const;
		void GetPerTracker (pertrackerstats_t&) const;
		int GetListenPort () const;
		libtorrent::cache_status GetCacheStats () const;
		QList<PeerInfo> GetPeers (int = -1) const;
		QStringList GetTagsForIndex (int = -1) const;
		void UpdateTags (const QStringList&, int = -1);
		/** @brief Adds the  given magnet link to the queue.
			*
			* Fetches the torrent and starts downloading the magnet link to
			* the given path, sets the given tags and takes into the account
			* the given parameters.
			*
			* @param[in] magnet The magnet link.
			* @param[in] path The save path.
			* @param[in] tags The IDs of the tags of the torrent.
			* @param[in] params Task parameters.
			* @return The ID of the task.
			*/
		int AddMagnet (const QString& magnet,
				const QString& path,
				const QStringList& tags,
				LeechCraft::TaskParameters params = LeechCraft::NoParameters);
		/** @brief Adds the given torrent file from the filename to the
			* queue.
			*
			* Starts downloading the torrent to the given path, sets the
			* passed IDs of the tags of the torrent, marks only selected files
			* for the download and takes into account the given params.
			*
			* @param[in] filename The file name of the torrent.
			* @param[in] path The save path.
			* @param[in] tags The IDs of the tags of the torrent.
			* @param[in] tryLive Try to play this torrent live.
			* @param[in] files The list of initial file selections.
			* @param[in] params Task parameters.
			* @return The ID of the task.
			*/
		int AddFile (const QString& filename,
				const QString& path,
				const QStringList& tags,
				bool tryLive,
				const QVector<bool>& files = QVector<bool> (),
				LeechCraft::TaskParameters params = LeechCraft::NoParameters);
		void KillTask (int);
		void RemoveTorrent (int, int opt = 0);
		void PauseTorrent (int);
		void ResumeTorrent (int);
		void ForceReannounce (int);
		void ForceRecheck (int);
		void SetOverallDownloadRate (int);
		void SetOverallUploadRate (int);
		void SetMaxDownloadingTorrents (int);
		void SetMaxUploadingTorrents (int);
		int GetOverallDownloadRate () const;
		int GetOverallUploadRate () const;
		int GetMaxDownloadingTorrents () const;
		int GetMaxUploadingTorrents () const;
		void SetTorrentDownloadRate (int, int);
		void SetTorrentUploadRate (int, int);
		int GetTorrentDownloadRate (int) const;
		int GetTorrentUploadRate (int) const;
		void AddPeer (const QString&, int, int);
		void AddWebSeed (const QString&, bool, int);
		void RemoveWebSeed (const QString&, bool, int);
		void SetFilePriority (int, int, int);
		void SetFilename (int, const QString&, int);

		std::vector<libtorrent::announce_entry> GetTrackers (const boost::optional<int>& = {}) const;
		void SetTrackers (const std::vector<libtorrent::announce_entry>&, const boost::optional<int>& = {});

		QString GetMagnetLink (int) const;

		QString GetTorrentDirectory (int) const;
		bool MoveTorrentFiles (const QString&, int);

		void SetCurrentTorrent (int);
		int GetCurrentTorrent () const;
		bool IsTorrentManaged (int) const;
		void SetTorrentManaged (bool, int);
		bool IsTorrentSequentialDownload (int) const;
		void SetTorrentSequentialDownload (bool, int);
		bool IsTorrentSuperSeeding (int) const;
		void SetTorrentSuperSeeding (bool, int);
		void MakeTorrent (const NewTorrentParams&) const;
		void SetExternalAddress (const QString&);
		QString GetExternalAddress () const;
		typedef QPair<QString, QString> BanRange_t;
		void BanPeers (const BanRange_t&, bool = true);
		void ClearFilter ();
		QMap<BanRange_t, bool> GetFilter () const;
		bool CheckValidity (int) const;

		void SaveResumeData (const libtorrent::save_resume_data_alert&) const;
		void HandleMetadata (const libtorrent::metadata_received_alert&);
		void PieceRead (const libtorrent::read_piece_alert&);
		void UpdateStatus (const std::vector<libtorrent::torrent_status>&);

		void MoveUp (const std::vector<int>&);
		void MoveDown (const std::vector<int>&);
		void MoveToTop (const std::vector<int>&);
		void MoveToBottom (const std::vector<int>&);

		void SetPreset (SettingsPreset);

		QList<FileInfo> GetTorrentFiles (int = -1) const;
	private:
		HandleDict_t::iterator FindHandle (const libtorrent::torrent_handle&);
		HandleDict_t::const_iterator FindHandle (const libtorrent::torrent_handle&) const;

		void MoveToTop (int);
		void MoveToBottom (int);
		QString GetStringForState (libtorrent::torrent_status::state_t) const;
		void RestoreTorrents ();
		bool DecodeEntry (const QByteArray&, libtorrent::lazy_entry&);
		libtorrent::torrent_handle RestoreSingleTorrent (const QByteArray&,
				const QByteArray&,
				const boost::filesystem::path&,
				bool,
				bool);
		void HandleSingleFinished (int);
		void ManipulateSettings ();
		/** Returns human-readable list of tags for the given torrent.
		 *
		 * @param[in] torrent The ID of the torrent.
		 * @return The human-readable list of tags.
		 */
		QStringList GetTagsForIndexImpl (int torrent) const;
		/** Sets the tags for the given torrent.
		 *
		 * @param[in] tags The human-readable list of tags.
		 * @param[in] torrent The ID of the torrent.
		 */
		void UpdateTagsImpl (const QStringList& tags, int torrent);
		void ScheduleSave ();
		void HandleLibtorrentException (const libtorrent::libtorrent_exception&);
	private slots:
		void writeSettings ();
		void checkFinished ();
		void scrape ();
	public slots:
		void queryLibtorrentForWarnings ();
		void tcpPortRangeChanged ();
		void sslPortChanged ();
		void autosaveIntervalChanged ();
		void maxUploadsChanged ();
		void maxConnectionsChanged ();
		void setProxySettings ();
		void setGeneralSettings ();
		void setDHTSettings ();
		void setLoggingSettings ();
		void setScrapeInterval ();
		void updateRows ();
	signals:
		void error (QString) const;
		void gotEntity (const LeechCraft::Entity&);
		void addToHistory (const QString&, const QString&, quint64,
				const QDateTime&, const QStringList&);
		void taskFinished (int);
		void taskRemoved (int);
	};
}
}
}

namespace libtorrent
{
	QDataStream& operator<< (QDataStream&, const libtorrent::entry&);
	QDataStream& operator>> (QDataStream&, libtorrent::entry&);
}
