/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 3 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#pragma once

#include <QWidget>
#include <QModelIndex>
#include <QMap>
#include <QList>
#include <QMenu>
#include <QCloseEvent>
#include <QMetaType>
#include <QStringListModel>

#include <memory>

#include "ui_UISearchFrame.h"
#include "ArenaWidget.h"
#include "WulforUtil.h"
#include "DownloadToHistory.h"

#include "dcpp/stdinc.h"
#include "dcpp/SearchResult.h"
#include "dcpp/SearchManager.h"
#include "dcpp/SettingsManager.h"
#include "dcpp/ClientManagerListener.h"
#include "dcpp/Singleton.h"

using namespace dcpp;

class SearchItem;
class SearchFramePrivate;

class SearchStringListModel : public QStringListModel
{
public:
    SearchStringListModel(QObject *parent = 0) :
        QStringListModel(parent)
    {}

    virtual ~SearchStringListModel()
    {}

    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    Qt::ItemFlags flags(const QModelIndex &) const;

private:
   QList<QString> checked;
};

class SearchFrame : public QWidget,
                    public ArenaWidget,
                    private Ui::SearchFrame,
                    private SearchManagerListener,
                    private ClientManagerListener
{
    Q_OBJECT
    Q_INTERFACES(ArenaWidget)

    class Menu : public dcpp::Singleton<Menu>
    {
        friend class dcpp::Singleton<Menu>;
    public:
        enum Action {
            Download = 0,
            DownloadTo,
            DownloadWholeDir,
            DownloadWholeDirTo,
            SearchTTH,
            Magnet,
            MagnetWeb,
            MagnetInfo,
            Browse,
            MatchQueue,
            SendPM,
            AddToFav,
            GrantExtraSlot,
            RemoveFromQueue,
            Remove,
            UserCommands,
            Blacklist,
            AddToBlacklist,
            None
        };

        Action exec(StringList);
        QMap<QString, QString> ucParams;
        QString getDownloadToPath() {return downToPath; }
        void addTempPath(const QString &path);

    private:
        Menu();
        virtual ~Menu();

        QMap<QAction*, Action> actions;
        QList<QAction*> action_list;

        QString downToPath;

        int uc_cmd_id;

        QMenu *menu;
        QMenu *magnet_menu;
        QMenu *down_to;
        QMenu *down_wh_to;
        QMenu *black_list_menu;
    };

public:
    enum AlreadySharedAction {
        None = 0,
        Filter,
        Highlight
    };

    SearchFrame(QWidget* = 0);
    virtual ~SearchFrame();

    QWidget *getWidget();
    QString  getArenaTitle();
    QString  getArenaShortTitle();
    QMenu   *getMenu();
    const QPixmap &getPixmap();
    ArenaWidget::Role role() const { return ArenaWidget::Search; }

    void requestFilter() { slotFilter(); }
    void requestFocus() { lineEdit_SEARCHSTR->setFocus(); }

public Q_SLOTS:
    void searchAlternates(const QString &);
    void searchFile(const QString &);
    void fastSearch(const QString &, bool);
    void slotUpdateStatus();

protected:
    virtual void closeEvent(QCloseEvent*);
    virtual bool eventFilter(QObject *, QEvent *);

Q_SIGNALS:
    /** SearchManager signals */
    void coreAppendTask(SearchResultPtr);
    void coreProcessTasks();

    /** ClienManager signals */
    void coreClientConnected(const QString &info);
    void coreClientUpdated(const QString &info);
    void coreClientDisconnected(const QString &info);

    void dropResult();

private Q_SLOTS:
    void slotFilter();
    void slotClear();
    void slotTimer();
    void slotResultDoubleClicked(const QModelIndex&);
    void slotContextMenu(const QPoint&);
    void slotHeaderMenu(const QPoint&);
    void slotToggleSidePanel();
    void slotStartSearch();
    void slotStopSearch();
    void slotChangeProxyColumn(int);
    void slotClose();

    void slotSettingsChanged(const QString &key, const QString &value);

    void onHubAdded(const QString &info);
    void onHubChanged(const QString &info);
    void onHubRemoved(const QString &info);

private:

    template<typename ...Params, typename ...Args>
    void forEachSelected(
            void (SearchItem::*func)(Params ...) const,
            const Args& ...args)
    {
        auto indexes =
                treeView_RESULTS->selectionModel()->selectedRows();

        for (const auto &index: indexes) {
            auto item = getValidItem(index);
            if (item) (item->*func)(args...);
        }
    }

    template<typename _Function>
    _Function forEachSelected(_Function pred) {
        auto indexes =
                treeView_RESULTS->selectionModel()->selectedRows();

        for (const auto &index: indexes) {
            auto item = getValidItem(index);
            if (item) pred(item);
        }
        return pred;
    }

    void init();
    void load();
    void save();

    void clipMagnets(bool web);
    void execUserCommand();
    void removeSelected();
    void ignoreSelected();

    QString getTargetDir();

    QModelIndex getValidIndex(const QModelIndex&);
    SearchItem *getValidItem(const QModelIndex&);

    // SearchManagerListener
    virtual void on(SearchManagerListener::SR, const SearchResultPtr& aResult) noexcept;

    // ClientManagerListener
    virtual void on(ClientConnected, Client* c) noexcept;
    virtual void on(ClientUpdated, Client* c) noexcept;
    virtual void on(ClientDisconnected, Client* c) noexcept;

    Q_DECLARE_PRIVATE (SearchFrame)
    SearchFramePrivate* d_ptr;
};

Q_DECLARE_METATYPE(SearchFrame*)
