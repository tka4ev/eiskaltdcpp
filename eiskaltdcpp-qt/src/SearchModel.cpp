/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#else
#include <QtGui>
#endif

#include <QFileInfo>
#include <QList>
#include <QStringList>
#include <QPalette>
#include <QColor>
#include <QDir>

#include "SearchModel.h"
#include "SearchFrame.h"
#include "WulforUtil.h"

#include "dcpp/stdinc.h"
#include "dcpp/Util.h"
#include "dcpp/User.h"
#include "dcpp/CID.h"
#include "dcpp/ShareManager.h"

//#define _DEBUG_MODEL_

using namespace dcpp;

#ifdef _DEBUG_MODEL_
#include <QtDebug>
#include <QElapsedTimer>
#endif

void SearchProxyModel::sort(int column, Qt::SortOrder order) {
    if (sourceModel())
        sourceModel()->sort(column, order);
}

SearchModel::SearchModel(QObject *parent):
    QAbstractItemModel(parent),
    filterRole(SearchFrame::None),
    sortColumn(ColumnFileSize),
    sortOrder(Qt::DescendingOrder),
    searchType(SearchManager::TYPE_ANY),
    dropped(0), results(0),
    rootItem(new SearchItem())
{
    qRegisterMetaType<SearchResultPtr>("SearchResultPtr");

    columns = {
        tr("Count"), tr("File"), tr("Ext"), tr("Size"),
        tr("Exact size"), tr("TTH"), tr("Path"), tr("Nick"),
        tr("Free slots"), tr("Total slots"), tr("IP"), tr("Hub name"),
        tr("Hub address")};

    connect(this, SIGNAL(injectTasks()), this, SLOT(processTasks()), Qt::QueuedConnection);
}

SearchModel::~SearchModel() {
    delete rootItem;
    resultTasks.clear();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    SearchItem *item = getItem(index);

    if (!item)
        return QVariant();

    switch(role) {
        case Qt::DisplayRole:
        {
            switch (index.column()) {
                case ColumnHits:
                    if (!item->hasChildren() || !item->parent())
                        return QString();
                    return item->hitCount();

                case ColumnFileName:      return item->getFileName();
                case ColumnFileExtension: return item->getFileExt();
                case ColumnFileSize:      return item->getFileSizeString();
                case ColumnFileESize:     return item->getFileSize();
                case ColumnTth:           return item->getTthString();
                case ColumnFilePath:      return item->getFilePath();
                case ColumnNick:          return item->getUserNick();
                case ColumnFreeSlots:     return item->getFreeSlots();
                case ColumnSlots:         return item->getSlots();
                case ColumnIp:            return item->getUserIPString();
                case ColumnHubName:       return item->getHubName();
                case ColumnHubAddress:    return item->getHubUrl();
            }

            break;
        }
        case Qt::DecorationRole:
        {
            if (index.column() == ColumnFileName) {
                if (item->isFile())
                    return WulforUtil::getInstance()->getPixmapForFile(item->getFileName()).scaled(16, 16);
                else
                    return WICON(WulforUtil::eiFOLDER_BLUE).scaled(16, 16);
            }

            break;
        }
        case Qt::ToolTipRole:
        {
            if (index.column() == ColumnFileName) {
                if (item->isShared())
                    return tr("File already exists: %1").arg(item->getReal());
            } else if (index.column() == ColumnNick) {
                const UserPtr &user = item->getUser();
                if (!user || !user->isOnline())
                    return tr("User offline.");
            }

            break;
        }
        case Qt::TextAlignmentRole:
        {
            const int i_column = index.column();

            bool align_center = (i_column == ColumnSlots) ||
                    (i_column == ColumnFileExtension) ||
                    (i_column == ColumnFreeSlots);

            bool align_right  = (i_column == ColumnFileESize) ||
                    (i_column == ColumnFileSize);

            if (align_center)
                return Qt::AlignCenter;
            else if (align_right)
                return Qt::AlignRight;
            else if (i_column == ColumnHits)
                return Qt::AlignLeft;

            break;
        }
        case Qt::ForegroundRole:
        {
            if (index.column() == ColumnFileName) {
                if (item->isFile()) {
                    StringList targets;
                    QueueManager::getInstance()->getTargets(item->tth(), targets);
                    if (!targets.empty()) {
                        return qApp->palette().link().color();
                    } else if (filterRole == static_cast<int>(SearchFrame::Highlight) && item->isShared()) {
                        static QColor c;
                        c.setNamedColor(WSGET(WS_APP_SHARED_FILES_COLOR));
                        c.setAlpha(WIGET(WI_APP_SHARED_FILES_ALPHA));

                        return c;
                    }
                }
            } else if (index.column() == ColumnNick) {
                const UserPtr &user = item->getUser();
                if (user && user->isOnline())
                    return qApp->palette().foreground().color();

                return qApp->palette().dark().color();
            }

            break;
        }
        default: break;
    }

    return QVariant();
}

Qt::ItemFlags SearchModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant SearchModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        return columns.at(section);

    return QVariant();
}

SearchItem *SearchModel::getItem(const QModelIndex &index, SearchItem *defItem) const {
    SearchItem *item = defItem;

    if (index.isValid())
        item = static_cast<SearchItem*>(index.internalPointer());

    return item;
}

bool SearchModel::hasChildren(const QModelIndex &parent) const {
    if (parent.column() > ColumnFirst)
        return false;

    if (parent.isValid()) {
         const SearchItem *parentItem = getItem(parent);

         Q_ASSERT(parentItem);

         return parentItem->hasChildren();
     }

     return QAbstractItemModel::hasChildren(parent);
}

QModelIndex SearchModel::index(int row, int column, const QModelIndex &parent) const {
    if (parent.column() > ColumnFirst)
        return QModelIndex();

    SearchItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = getItem(parent);

    Q_ASSERT(parentItem);

    if (row >= 0 && row < parentItem->childCount())
        return createIndex(row, column, parentItem->child(row));

    return QModelIndex();
}

QModelIndex SearchModel::parent(const QModelIndex &child) const {
    if (!child.isValid())
        return QModelIndex();

    SearchItem *childItem = getItem(child);

    Q_ASSERT(childItem);

    SearchItem *parentItem = childItem->parent();

    Q_ASSERT(parentItem);

    if (parentItem->isRoot())
        return QModelIndex();

    return createIndex(parentItem->row(), ColumnHits, parentItem);
}

int SearchModel::columnCount(const QModelIndex &parent) const {
    return (parent.column() > ColumnFirst) ?
                ColumnFirst:
                ColumnLast;
}

int SearchModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > ColumnFirst)
        return 0;

    if (!parent.isValid())
        return rootItem->childCount();

    const SearchItem* parentItem = getItem(parent);

    Q_ASSERT(parentItem);

    return parentItem->childCount();
}

QModelIndex SearchModel::indexForParent(const SearchItem *parentItem) const {
    Q_ASSERT(parentItem);

    if (parentItem->isRoot())
        return QModelIndex();

    return index(parentItem->row(), ColumnHits);
}

void SearchModel::sort(int column, Qt::SortOrder order) {
    if (column < ColumnFirst || column >= ColumnLast)
        return;

    sortColumn = column;
    sortOrder = order;

    resort(rootItem, sortColumn);
}

void SearchModel::resort(SearchItem *parentItem, int column) {
    if (!parentItem->hasChildren())
        return;

    static const int numberOfThreads = QThread::idealThreadCount();

    QApplication::setOverrideCursor(Qt::BusyCursor);

    QList<SearchItem*> childList(parentItem->childs());

#ifdef _DEBUG_MODEL_
    QElapsedTimer timer;
    int items_count = parentItem->childCount();

    qDebug() << "===========================================================";
    qDebug() << "Resorting" << items_count << "items";

    timer.start();
#endif

    bool reverse = (sortOrder == Qt::AscendingOrder);

    // multi-threaded sorting.
    parallelMergeSort(childList.begin(), childList.end(),
        [=](const SearchItem *left, const SearchItem *right)
    {
        return (compareItems(left, right, column) > 0) ^ reverse;
    }, numberOfThreads);

/*
    // single-threaded sorting.
    qStableSort(childList.begin(), childList.end(),
        [=](const SearchItem *left, const SearchItem *right)
    {
        return (compareItems(left, right, sortColumn) > 0) ^ reverse;
    });
*/

#ifdef _DEBUG_MODEL_
    qDebug() << "[TIME] Resorting of" << items_count << "items:" << timer.elapsed();
#endif

    QApplication::restoreOverrideCursor();

    // set sorted list to node and rebuild (associated) persistent indexes.
    emit layoutAboutToBeChanged();
    {
        const QModelIndexList &persistents = persistentIndexList();

        int row = -1;
        SearchItem *changedItem = nullptr;
        for (const QModelIndex &from: persistents) {
            SearchItem *item = getItem(from);

            if (!item || item->parent() != parentItem)
                continue;

            if (item != changedItem) {
                row = childList.indexOf(item);
                changedItem = item;
            }
            QModelIndex to = createIndex(row, from.column(), item);

            changePersistentIndex(from, to);
        }
        parentItem->setChilds(childList);
    }
    emit layoutChanged();
}

void SearchModel::addResult(const SearchResultPtr &result) {
    SearchItem *parentItem = rootItem;

    if (result->getType() == SearchResult::TYPE_FILE)
        parentItem = findParent(result->getTTH());

    if (!parentItem->isRoot() && parentItem->exists(result)) {
        dropped++;
        return;
    }

    SearchItem *item = new SearchItem(result);

    if (parentItem->isRoot() && item->isFile())
        parents.emplace(&result->getTTH(), item);

    insertItem(parentItem, item);

    // auto expand node if the search by TTH
    if (!parentItem->isRoot() && parentItem->childCount() == 1 &&
            searchType == SearchManager::TYPE_TTH)
    {
        emit expandIndex(indexForParent(parentItem));
    }

    if (!parentItem->isRoot() && sortColumn == ColumnHits)
        moveItem(parentItem);

    results++;
}

void SearchModel::insertItem(SearchItem *parentItem, SearchItem *item) {
    if (!parentItem->isRoot() &&
            item->getFreeSlots() > parentItem->getFreeSlots())
    {
        parentItem->swapData(item);
    }

    if (parentItem->isExpanded()) {
        int column = sortColumn;

        if (!parentItem->isRoot() ||
                searchType == SearchManager::TYPE_TTH ||
                searchType == SearchManager::TYPE_DIRECTORY)
        {
            column = (sortColumn == ColumnFileSize ||
                    sortColumn == ColumnFileESize ||
                    sortColumn == ColumnTth ||
                    sortColumn == ColumnHits) ?
            ColumnFreeSlots:
            sortColumn;
        }

        const int row = getSortPos(parentItem, item, column);

        beginInsertRows(indexForParent(parentItem), row, row);
        {
            bool ok = parentItem->insertChildren(item, row);

            Q_ASSERT(ok);
        }
        endInsertRows();
    } else {
        parentItem->appendChildren(item);

        if (sortColumn != ColumnHits) {
            QModelIndex idx = createIndex(parentItem->row(), ColumnHits, parentItem);
            emit dataChanged(idx, idx);
        }
    }
}

void SearchModel::moveItem(SearchItem* item) {
    const int from = item->row();
    const int to = getSortPos(rootItem, item, sortColumn);

    if (from == to) {
        QModelIndex idx = createIndex(from, ColumnHits, item);
        emit dataChanged(idx, idx);
        return;
    }

    const int diff = int(to > from);

    QModelIndex idx;
    if (beginMoveRows(idx, from, from, idx, to)) {
        bool ok = rootItem->moveChildren(from, to - diff);

        Q_ASSERT(ok);

        endMoveRows();
    }
}

bool SearchModel::removeRows(int row, int count, const QModelIndex& parent) {
    if (count < 1 || row < 0 || (row + count) > rowCount(parent))
        return false;

    int nums = 0;

    SearchItem *parentItem = getItem(parent, rootItem);

    beginRemoveRows(parent, row, row + count - 1);
    {
        for (int i = 0; i < count; ++i) {
            SearchItem *item = parentItem->takeChildren(row);

            if (parentItem->isRoot() && item->isFile())
                parents.erase(&item->tth());

            if (item->hasChildren())
                nums += item->childCount();

            delete item;
        }
    }
    endRemoveRows();

    results -= (nums + count);

    if (results == 0)
        dropped = results;

    return true;
}

void SearchModel::appendTask(SearchResultPtr result) {
    static SearchBlacklist *SB = SearchBlacklist::getInstance();

    if (SB->ok(_q(result->getFile()), SearchBlacklist::NAME) &&
            SB->ok(_q(result->getTTH().toBase32()), SearchBlacklist::TTH))
    {
        resultTasks.enqueue(result);
    }

    int threshold = 1;
    const int count = rowCount();

    if (count > 300)
        threshold = 15;
    else if (count > 100)
        threshold = 7;
    else if (count > 50)
        threshold = 3;

    if (resultTasks.count() > threshold)
        emit injectTasks();
}

void SearchModel::processTasks() {
    if (resultTasks.isEmpty())
        return;

    int ticker = 16;
    while(!resultTasks.isEmpty() && ticker > 0) {
        addResult(resultTasks.dequeue());
        ticker--;
    }

    if (!resultTasks.isEmpty())
        emit injectTasks();
    else
        emit updateStatus();
}

void SearchModel::expandItem(const QModelIndex &parent) {
    if (!parent.isValid())
        return;

    QModelIndex index = parent;
    if (parent.model() != this) {
        const auto proxy = qobject_cast<const SearchProxyModel*>(parent.model());
        if (proxy)
            index = proxy->mapToSource(parent);
    }

    SearchItem *parentItem = getItem(index);

    if (!parentItem)
        return;

    int column = (sortColumn == ColumnFileSize ||
                  sortColumn == ColumnFileESize ||
                  sortColumn == ColumnTth ||
                  sortColumn == ColumnHits) ?
        ColumnFreeSlots:
        sortColumn;

    resort(parentItem, column);

    parentItem->setExpanded(true);
}

void SearchModel::collapseItem(const QModelIndex &parent) {
    if (!parent.isValid())
        return;

    QModelIndex index = parent;
    if (parent.model() != this) {
        const auto proxy = qobject_cast<const SearchProxyModel*>(parent.model());
        if (proxy)
            index = proxy->mapToSource(parent);
    }

    SearchItem *item = getItem(index);

    if (!item)
        return;

    item->setExpanded(false);
}

int SearchModel::getSortPos(const SearchItem* parentItem, const SearchItem* item, int column) const {
    bool reverse = (sortOrder == Qt::AscendingOrder);

    const QList<SearchItem*> &childList = parentItem->childs();

    auto position = qLowerBound(childList.constBegin(), childList.constEnd(), item,
                [=](const SearchItem *left, const SearchItem *right)
    {
        return (compareItems(left, right, column) > 0) ^ reverse;
    });

    return int(position - childList.constBegin());
}

void SearchModel::clearModel() {
    beginResetModel();

    resetCounters();
    rootItem->clearChilds();
    resultTasks.clear();
    parents.clear();

    endResetModel();

    emit updateStatus();
}

int SearchModel::compareItems(const SearchItem *left, const SearchItem *right, int column) const {
    if (left == right)
        return (sortOrder == Qt::AscendingOrder) ? 1 : -1;

    switch(column) {
        case ColumnHits:
        {
            int pp = dcpp::compare(left->hitCount(), right->hitCount());
            if (pp != 0)
                return pp;
            pp = dcpp::compare(left->getFileSize(), right->getFileSize());
            if (pp != 0)
                return pp;
            return dcpp::compare(left->getFreeSlots(), right->getFreeSlots());
        }
        case ColumnFileName:
            return QString::localeAwareCompare(left->getFileName(), right->getFileName());
        case ColumnFileExtension:
            return QString::localeAwareCompare(left->getFileExt(), right->getFileExt());
        case ColumnFileSize:
        case ColumnFileESize:
            return dcpp::compare(left->getFileSize(), right->getFileSize());
        case ColumnTth:
            return dcpp::compare(left->tth(), right->tth());
        case ColumnFilePath:
            return QString::localeAwareCompare(left->getFilePath(), right->getFilePath());
        case ColumnNick:
            return QString::localeAwareCompare(left->getUserNick(), right->getUserNick());
        case ColumnFreeSlots:
        case ColumnSlots:
            return (left->getFreeSlots() == right->getFreeSlots())?
                        dcpp::compare(left->getSlots(), right->getSlots()):
                        dcpp::compare(left->getFreeSlots(), right->getFreeSlots());
        case ColumnIp:
            return dcpp::compare(left->getBinAddr(), right->getBinAddr());
        case ColumnHubName:
            return QString::localeAwareCompare(left->getHubName(), right->getHubName());
        case ColumnHubAddress:
            return QString::localeAwareCompare(left->getHubUrl(), right->getHubUrl());
    }
}

int SearchModel::getSortColumn() const {
    return sortColumn;
}

void SearchModel::setSortColumn(int column) {
    sortColumn = column;
}

Qt::SortOrder SearchModel::getSortOrder() const {
    return sortOrder;
}

void SearchModel::setSortOrder(Qt::SortOrder order) {
    sortOrder = order;
}

void SearchModel::setFilterRole(int role) {
    filterRole = role;

    // force repaint viewport
    emit dataChanged(QModelIndex(), QModelIndex());
}

void SearchModel::setSearchType(SearchManager::TypeModes type) {
    searchType = type;
}
//------------------------------------------------------------------------
SearchItem::SearchItem() :
    parentItem(nullptr),
    m_result(nullptr),
    expanded(true)
{}

SearchItem::SearchItem(const SearchResultPtr &aSr) :
    parentItem(nullptr),
    m_result(aSr),
    expanded(false)
{}

SearchItem::~SearchItem() {
    clearChilds();
}

const QList<SearchItem*> &SearchItem::childs() const {
    return childItems;
}

void SearchItem::setChilds(QList<SearchItem*> &itemList) {
    childItems.swap(itemList);
}

SearchItem *SearchItem::child(int row) const {
    return childItems.value(row);
}

int SearchItem::childCount() const {
    return childItems.count();
}

SearchItem *SearchItem::parent() const {
    return parentItem;
}

int SearchItem::row() const {
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<SearchItem*>(this));

    return 0;
}

bool SearchItem::exists(const SearchResultPtr &result, bool filterIp) const {
    if (parent() && equal(result, filterIp))
        return true;

    for (SearchItem *child: childItems) {
        if (child->equal(result, filterIp))
            return true;
    }
    return false;
}

bool SearchItem::equal(const SearchResultPtr &result, bool filterIp) const {
    if (m_result->getFile() == result->getFile()) {
        if (filterIp)
            return (m_result->getIP() == result->getIP());

        return (cid() == result->getUser()->getCID());
    }
    return false;
}

SearchItem *SearchItem::takeChildren(int position) {
    if (!validRow(position))
        return nullptr;

    SearchItem *child = childItems.takeAt(position);

    Q_ASSERT(child);

    child->parentItem = nullptr;

    return child;
}

bool SearchItem::moveChildren(int from, int to) {
    if (!validRow(from) || !validRow(to))
        return false;

    childItems.move(from, to);

    return true;
}

void SearchItem::appendChildren(SearchItem *item) {
    childItems.append(item);
    item->parentItem = this;
}

bool SearchItem::insertChildren(SearchItem *child, int position) {
    if (position < 0 || position > childCount() || !child)
        return false;

    childItems.insert(position, child);
    child->parentItem = this;

    return true;
}

void SearchItem::swapData(SearchItem *other) {
    m_result.swap(other->m_result);
}

void SearchItem::clearChilds() {
    for(auto begin: childItems)
        delete begin;

    childItems.clear();
}

bool SearchItem::isDir() const {
    return m_result->getType() == SearchResult::TYPE_DIRECTORY;
}

bool SearchItem::isFile() const {
    return m_result->getType() == SearchResult::TYPE_FILE;
}

bool SearchItem::isShared() const {
    return ShareManager::getInstance()->isTTHShared(tth());
}

bool SearchItem::isFavUser() const {
    return FavoriteManager::getInstance()->isFavoriteUser(getUser());
}

QString SearchItem::getReal() const {
    static ShareManager *SM = ShareManager::getInstance();

    QString path;

    try {
        path = _q(SM->toReal(SM->toVirtual(tth())));
    } catch( ... ) {}

    return path;
}

QString SearchItem::getFullPath() const {
    return _q(m_result->getFile());
}

QString SearchItem::getFileName() const {
    if (isFile())
        return _q(m_result->getBaseName());

    return getFullPath();
}

QString SearchItem::getFilePath() const {
    QString fname = getFullPath();
    if (isFile()) {
        int i = fname.lastIndexOf('\\');

        if (i > 0 && i + 1 < fname.size())
            return fname.left(i + 1);
    }
    return fname;
}

QString SearchItem::getFileExt() const {
    if (isFile()) {
        QString fname = getFullPath();

        int i = fname.lastIndexOf('.');

        if (i > 0 && i + 1 < fname.size())
            return fname.mid(i + 1).toUpper();
    }
    return QString();
}

qlonglong SearchItem::getFileSize() const {
    return qlonglong(m_result->getSize());
}

QString SearchItem::getFileSizeString() const {
    return WulforUtil::formatBytes(getFileSize());
}

QString SearchItem::getCidString() const {
    return _q(getUser()->getCID().toBase32());
}

QString SearchItem::getTthString() const {
    return isFile() ? _q(m_result->getTTH().toBase32()) : QString();
}

const UserPtr &SearchItem::getUser() const {
    return m_result->getUser();
}

HintedUser SearchItem::getHintedUser() const {
    return HintedUser(getUser(), m_result->getHubURL());
}

QString SearchItem::getUserNick() const {
    if (cached_nick.isNull())
        cached_nick = _q(Util::toString(ClientManager::getInstance()->getNicks(getHintedUser())));

    return cached_nick;
}

QString SearchItem::getUserIPString() const {
    return _q(m_result->getIP());
}

quint32 SearchItem::getBinAddr() const {
    struct in_addr addr;

    if (!inet_pton(AF_INET, m_result->getIP().c_str(), &addr) ||
            addr.s_addr == INADDR_NONE)
    {
        return 0;
    }

    return quint32(ntohl(addr.s_addr));
}

QString SearchItem::getHubName() const {
    return _q(m_result->getHubName());
}

QString SearchItem::getHubUrl() const {
    return _q(m_result->getHubURL());
}

QString SearchItem::getMagnet() const {
    if (isFile()) {
        qlonglong size = getFileSize();
        QString tth = getTthString();
        QString name = getFileName();

        return WulforUtil::getInstance()->makeMagnet(name, size, tth);
    }

    return QString();
}

int SearchItem::getFreeSlots() const {
    return m_result->getFreeSlots();
}

int SearchItem::getSlots() const {
    return m_result->getSlots();
}


void SearchItem::download(const string &aTarget) const
{
    try {
        if (isFile()) {
            string target_path = aTarget + getFileName().toStdString();
            QueueManager::getInstance()->add(target_path, result()->getSize(),
                                                tth(), getHintedUser());

            if (!SETTING(DONT_DL_ALREADY_QUEUED)) {
                const SearchItem *parent_item = this;

                // added sibling items
                if (!hasChildren() && !parent()->isRoot())
                    parent_item = parent();

                const auto &items = parent_item->childs();
                for (const SearchItem *item: items) {
                    if (item == this)
                        continue;

                    try {
                        QueueManager::getInstance()->add(target_path, item->result()->getSize(),
                                                         item->tth(), item->getHintedUser());
                    } catch(const Exception&) {
                        continue;
                    }
                }
            }
        } else {
            QueueManager::getInstance()->addDirectory(result()->getFile(),
                                                getHintedUser(), aTarget);
        }
    } catch(const Exception&) {}
}

void SearchItem::downloadWhole(const string &aTarget) const
{
    try {
        if (isFile()) {
            QueueManager::getInstance()->addDirectory(getFilePath().toStdString(),
                                                      getHintedUser(), aTarget);
        } else {
            QueueManager::getInstance()->addDirectory(result()->getFile(),
                                                      getHintedUser(), aTarget);
        }
    } catch(const Exception&) {}
}

void SearchItem::getFileList(bool match) const
{
    string dir;

    QueueItem::FileFlags flag = match ?
                QueueItem::FLAG_MATCH_QUEUE:
                QueueItem::FLAG_CLIENT_VIEW;

    try {
        if (!match)
            dir = getFilePath().toStdString();

        QueueManager::getInstance()->addList(getHintedUser(), flag, dir);
    } catch (const Exception&) {}
}

void SearchItem::addAsFavorite() const
{
    try {
        FavoriteManager::getInstance()->addFavoriteUser(getUser());
    } catch (const Exception&) {}
}

void SearchItem::grantSlot() const
{
    try {
        UploadManager::getInstance()->reserveSlot(getHintedUser());
    } catch (const Exception&) {}
}

void SearchItem::removeFromQueue() const
{
    try {
        StringList targets;
        QueueManager::getInstance()->getTargets(tth(), targets);
        if (!targets.empty()) {
            for (const string &target: targets)
                QueueManager::getInstance()->remove(target);
        }
    } catch (const Exception&) {}
}

void SearchItem::openPm() const
{
    HubFrame *hub = qobject_cast<HubFrame*>
        (HubManager::getInstance()->getHub(getHubUrl()));

    if (hub)
        hub->createPMWindow(cid());
}

void SearchItem::CheckTTH::operator()(SearchItem *item) {
    if (firstTTH) {
        hasTTH = true;
        firstTTH = false;
        tth = item->tth();
    } else if(hasTTH) {
        if(tth != item->tth()) {
            hasTTH = false;
        }
    }

    if (firstHubs && hubs.empty()) {
        hubs = ClientManager::getInstance()->getHubs(item->getHintedUser());
        firstHubs = false;
    } else if(!hubs.empty()) {
        // we will merge hubs of all users to ensure we can use OP commands in all hubs
        StringList sl = ClientManager::getInstance()->getHubs(item->getHintedUser());
        hubs.insert(hubs.end(), sl.begin(), sl.end());
    }
}
