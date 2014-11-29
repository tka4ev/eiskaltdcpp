/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>
#include <QString>
#include <QPixmap>
#include <QList>
#include <QHash>
#include <QStringList>
#include <QRegExp>
#include <QQueue>

#include "dcpp/stdinc.h"
#include "dcpp/SearchResult.h"
#include "dcpp/SearchManager.h"
#include "dcpp/QueueManager.h"
#include "dcpp/FavoriteManager.h"
#include "dcpp/UploadManager.h"

#include "WulforUtil.h"
#include "HubManager.h"
#include "HubFrame.h"
#include "SearchBlacklist.h"
#include "ItemModelSortAlgorithm.h"

#include <unordered_map>

using namespace dcpp;

class SearchViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    SearchViewDelegate(QObject* = NULL);
    virtual ~SearchViewDelegate();

public slots:
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index);
};

class SearchProxyModel: public QSortFilterProxyModel {
    Q_OBJECT
public:
    SearchProxyModel(QObject *parent = NULL) :
        QSortFilterProxyModel(parent) {}

    virtual ~SearchProxyModel() {}

    virtual void sort(int column, Qt::SortOrder order);
};

class SearchItem
{
    Q_DISABLE_COPY(SearchItem)
public:

    const CID &cid() const
    { return m_result->getUser()->getCID(); }

    const TTHValue &tth() const
    { return m_result->getTTH(); }

    const SearchResultPtr &result() const
    { return m_result; }

    QString getCidString() const;
    QString getTthString() const;
    QString getFullPath() const;
    QString getFileName() const;
    QString getFilePath() const;
    QString getFileSizeString() const;
    QString getFileExt() const;
    QString getUserNick() const;
    QString getUserIPString() const;
    QString getHubName() const;
    QString getHubUrl() const;
    QString getReal() const;
    QString getMagnet() const;

    const UserPtr &getUser() const;
    HintedUser getHintedUser() const;

    qlonglong getFileSize() const;
    quint32 getBinAddr() const;

    int getFreeSlots() const;
    int getSlots() const;

    bool isDir() const;
    bool isFile() const;
    bool isShared() const;
    bool isFavUser() const;


    void download(const string&) const;
    void downloadWhole(const string&) const;
    void getFileList(bool) const;
    void addAsFavorite() const;
    void grantSlot() const;
    void removeFromQueue() const;
    void openPm() const;

    struct CheckTTH {
        CheckTTH() :
            firstHubs(true),
            op(true),
            hasTTH(false),
            firstTTH(true)
        {}

        void operator()(SearchItem*);

        StringList hubs;
        TTHValue tth;

        bool firstHubs;
        bool op;
        bool hasTTH;
        bool firstTTH;
    };

private:
    friend class SearchModel;

    SearchItem();
    explicit SearchItem(const SearchResultPtr &sr);
    ~SearchItem();

    int row() const;
    int childCount() const;

    void setChilds(QList<SearchItem*>&);
    const QList<SearchItem*> &childs() const;

    SearchItem *child(int) const;
    SearchItem *parent() const;
    SearchItem *takeChildren(int);

    bool moveChildren(int, int);
    void appendChildren(SearchItem*);
    bool insertChildren(SearchItem*, int);

    bool exists(const SearchResultPtr&, bool = false) const;
    bool equal(const SearchResultPtr&, bool) const;

    void swapData(SearchItem*);

    void clearChilds();

    inline bool isRoot() const
    { return !parent() && !m_result; }

    inline bool hasChildren() const
    { return !childItems.isEmpty(); }

    inline bool validRow(int row) const
    { return row >= 0 && row < childCount(); }

    inline bool isExpanded() const
    { return expanded; }

    inline int hitCount() const
    { return childItems.count() + 1; }

    inline void setExpanded(bool state)
    { expanded = state; }

//data
    QList<SearchItem*> childItems;

    mutable QString cached_nick;

    SearchResultPtr m_result;

    SearchItem *parentItem;

    bool expanded;
};

class SearchModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    SearchModel(QObject *parent = 0);
    virtual ~SearchModel();

    enum Columns : int
    {
        ColumnFirst,
        ColumnHits = ColumnFirst,
        ColumnFileName,
        ColumnFileExtension,
        ColumnFileSize,
        ColumnFileESize,
        ColumnTth,
        ColumnFilePath,
        ColumnNick,
        ColumnFreeSlots,
        ColumnSlots,
        ColumnIp,
        ColumnHubName,
        ColumnHubAddress,
        ColumnLast
    };

    virtual QVariant data(const QModelIndex &, int) const;
    virtual Qt::ItemFlags flags(const QModelIndex &) const;
    virtual QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;
    virtual QModelIndex index(int, int, const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    virtual bool hasChildren(const QModelIndex &parent) const;
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());

    QModelIndex indexForParent(const SearchItem*) const;

    int getSortColumn() const;
    void setSortColumn(int);

    Qt::SortOrder getSortOrder() const;
    void setSortOrder(Qt::SortOrder);

    void setSearchType(SearchManager::TypeModes);

    /** Clear model and redraw view*/
    void clearModel();

    SearchItem *getItem(const QModelIndex &index, SearchItem *defItem = nullptr) const;

    struct Counters
    {
        Counters(qlonglong _result, qlonglong _dropped, qlonglong _unique) :
            result(_result), dropped(_dropped),
            unique(_unique) {}

        qlonglong result;
        qlonglong dropped;
        qlonglong unique;
    };

    Counters getCounters() const
    { return Counters(results, dropped, rowCount()); }

    inline bool isEmpty() const
    { return !rootItem->hasChildren(); }

    void resetCounters()
    { results = dropped = 0; }

public slots:
    void expandItem(const QModelIndex&);
    void collapseItem(const QModelIndex&);

    void processTasks();
    void appendTask(SearchResultPtr);

    void incDropped()
    { dropped++; }

    void setFilterRole(int);

signals:
    void expandIndex(QModelIndex);
    void updateStatus();
    void injectTasks();

private:

    typedef std::unordered_map
    <
        const TTHValue*,
        SearchItem*,
        hash<TTHValue*>,
        equal_to<TTHValue*>
    > ParentMap;

    ParentMap parents;

    inline SearchItem* findParent(const TTHValue &groupCond) {
        ParentMap::iterator i = parents.find(&groupCond);
        return i != parents.end() ? i->second : rootItem;
    }

    void addResult(const SearchResultPtr &sr);

    void resort(SearchItem*, int);

    void moveItem(SearchItem*);

    void insertItem(SearchItem*, SearchItem*);

    int getSortPos(const SearchItem*, const SearchItem*, int) const;
    int compareItems(const SearchItem*, const SearchItem*, int) const;


    int filterRole;
    int sortColumn;

    qulonglong dropped;
    qulonglong results;

    Qt::SortOrder sortOrder;

    SearchManager::TypeModes searchType;

    SearchItem *rootItem;

    QVariantList columns;

    QQueue<SearchResultPtr> resultTasks;
};
