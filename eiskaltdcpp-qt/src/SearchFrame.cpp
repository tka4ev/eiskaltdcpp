/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 3 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <QComboBox>
#include <QTreeView>
#include <QAction>
#include <QTextCodec>
#include <QDir>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QClipboard>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QStringListModel>
#include <QKeyEvent>
#include <QTimer>
#include <QShortcut>
#include <QStringListModel>
#include <QCompleter>
#include <QListWidget>
#include <QListWidgetItem>

#include "SearchFrame.h"
#include "MainWindow.h"
#include "HubFrame.h"
#include "HubManager.h"
#include "SearchModel.h"
#include "SearchBlacklist.h"
#include "SearchBlacklistDialog.h"
#include "WulforUtil.h"
#include "Magnet.h"
#include "ArenaWidgetManager.h"
#include "ArenaWidgetFactory.h"
#include "DownloadToHistory.h"
#include "GlobalTimer.h"

#include "dcpp/CID.h"
#include "dcpp/ClientManager.h"
#include "dcpp/FavoriteManager.h"
#include "dcpp/StringTokenizer.h"
#include "dcpp/SettingsManager.h"
#include "dcpp/Encoder.h"
#include "dcpp/UserCommand.h"

#include <QtDebug>

using namespace dcpp;

class SearchFramePrivate {
public:
    SearchFramePrivate() :
        isHash(false),
        stop(false),
        saveFileType(true),
        withFreeSlots(false),
        waitingResults(false),
        searchStartTime(0),
        searchEndTime(1),
        filterShared(SearchFrame::None),
        completer(NULL),
        focusShortcut(NULL),
        model(NULL),
        proxy(NULL),
        str_model(NULL)
    {}

    bool isHash;
    bool stop;
    bool saveFileType;
    bool withFreeSlots;
    bool waitingResults;

    int left_pane_old_size;

    uint64_t searchStartTime;
    uint64_t searchEndTime;

    SearchFrame::AlreadySharedAction filterShared;

    QCompleter *completer;
    QShortcut *focusShortcut;
    SearchModel *model;
    SearchProxyModel *proxy;
    SearchStringListModel *str_model;

    QString target;
    QString arena_title;

    string token;

    QStringList hubs;
    QStringList searchHistory;

    QList<Client*> client_list;

    StringList currentSearch;
    StringMap ucLineParams;
};

QVariant SearchStringListModel::data(const QModelIndex &index, int role) const {
    if (role != Qt::CheckStateRole || !index.isValid())
        return QStringListModel::data(index, role);

    return (checked.contains(index.data().toString()) ? Qt::Checked : Qt::Unchecked);
}

bool SearchStringListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role != Qt::CheckStateRole || !index.isValid())
        return QStringListModel::setData(index, value, role);

    if (value.toInt() == Qt::Checked)
        checked.push_back(index.data().toString());
    else if (checked.contains(index.data().toString()))
        checked.removeAt(checked.indexOf(index.data().toString()));

    return true;
}

Qt::ItemFlags SearchStringListModel::flags(const QModelIndex &) const {
    return (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
}

SearchFrame::Menu::Menu(){
    WulforUtil *WU = WulforUtil::getInstance();

    menu = new QMenu();

    magnet_menu = new QMenu(tr("Magnet"));

    QAction *down       = new QAction(tr("Download"), NULL);
    down->setIcon(WU->getPixmap(WulforUtil::eiDOWNLOAD));

    down_to             = new QMenu(tr("Download to..."));
    down_to->setIcon(WU->getPixmap(WulforUtil::eiDOWNLOAD_AS));

    QAction *down_wh    = new QAction(tr("Download Whole Directory"), NULL);
    down_wh->setIcon(WU->getPixmap(WulforUtil::eiDOWNLOAD));

    down_wh_to          = new QMenu(tr("Download Whole Directory to..."));
    down_wh_to->setIcon(WU->getPixmap(WulforUtil::eiDOWNLOAD_AS));

    QAction *sep        = new QAction(menu);
    sep->setSeparator(true);

    QAction *find_tth   = new QAction(tr("Search TTH"), NULL);
    find_tth->setIcon(WU->getPixmap(WulforUtil::eiFILEFIND));

    QAction *magnet     = new QAction(tr("Copy magnet"), NULL);
    magnet->setIcon(WU->getPixmap(WulforUtil::eiEDITCOPY));

    QAction *magnet_web     = new QAction(tr("Copy web-magnet"), NULL);
    magnet_web->setIcon(WU->getPixmap(WulforUtil::eiEDITCOPY));

    QAction *magnet_info    = new QAction(tr("Properties of magnet"), NULL);
    magnet_info->setIcon(WU->getPixmap(WulforUtil::eiDOWNLOAD));

    QAction *browse     = new QAction(tr("Browse files"), NULL);
    browse->setIcon(WU->getPixmap(WulforUtil::eiFOLDER_BLUE));

    QAction *match      = new QAction(tr("Match Queue"), NULL);
    match->setIcon(WU->getPixmap(WulforUtil::eiDOWN));

    QAction *send_pm    = new QAction(tr("Send Private Message"), NULL);
    send_pm->setIcon(WU->getPixmap(WulforUtil::eiMESSAGE));

    QAction *add_to_fav = new QAction(tr("Add to favorites"), NULL);
    add_to_fav->setIcon(WU->getPixmap(WulforUtil::eiBOOKMARK_ADD));

    QAction *grant      = new QAction(tr("Grant extra slot"), NULL);
    grant->setIcon(WU->getPixmap(WulforUtil::eiEDITADD));

    QAction *sep1       = new QAction(menu);
    sep1->setSeparator(true);

    QAction *sep2       = new QAction(menu);
    sep2->setSeparator(true);

    QAction *sep3       = new QAction(menu);
    sep3->setSeparator(true);

    QAction *rem_queue  = new QAction(tr("Remove from Queue"), NULL);
    rem_queue->setIcon(WU->getPixmap(WulforUtil::eiEDITDELETE));

    QAction *rem        = new QAction(tr("Remove"), NULL);
    rem->setIcon(WU->getPixmap(WulforUtil::eiEDITDELETE));

    black_list_menu     = new QMenu(tr("Blacklist..."));
    black_list_menu->setIcon(WU->getPixmap(WulforUtil::eiFILTER));

    QAction *blacklist = new QAction(tr("Blacklist"), NULL);
    blacklist->setIcon(WU->getPixmap(WulforUtil::eiFILTER));

    QAction *add_to_blacklist = new QAction(tr("Add to Blacklist"), NULL);
    add_to_blacklist->setIcon(WU->getPixmap(WulforUtil::eiEDITADD));

    black_list_menu->addActions(QList<QAction*>()
                    << add_to_blacklist << blacklist);

    magnet_menu->addActions(QList<QAction*>()
                    << magnet << magnet_web << sep3 << magnet_info);

    actions.insert(down, Download);
    actions.insert(down_wh, DownloadWholeDir);
    actions.insert(find_tth, SearchTTH);
    actions.insert(magnet, Magnet);
    actions.insert(magnet_web, MagnetWeb);
    actions.insert(magnet_info, MagnetInfo);
    actions.insert(browse, Browse);
    actions.insert(match, MatchQueue);
    actions.insert(send_pm, SendPM);
    actions.insert(add_to_fav, AddToFav);
    actions.insert(grant, GrantExtraSlot);
    actions.insert(rem_queue, RemoveFromQueue);
    actions.insert(rem, Remove);
    actions.insert(blacklist, Blacklist);
    actions.insert(add_to_blacklist, AddToBlacklist);

    action_list   << down
                  << down_wh
                  << sep
                  << find_tth
                  << browse
                  << match
                  << send_pm
                  << add_to_fav
                  << grant
                  << sep1
                  << rem_queue
                  << rem
                  << sep2;
}

SearchFrame::Menu::~Menu(){
    qDeleteAll(action_list);

    magnet_menu->deleteLater();
    menu->deleteLater();
    down_to->deleteLater();
    down_wh_to->deleteLater();
    black_list_menu->deleteLater();
}

SearchFrame::Menu::Action SearchFrame::Menu::exec(StringList list) {
    for (const auto &a : action_list)
        a->setParent(NULL);

    qDeleteAll(down_to->actions());
    qDeleteAll(down_wh_to->actions());
    down_to->clear();
    down_wh_to->clear();

    QString aliases, paths;

    aliases = QByteArray::fromBase64(WSGET(WS_DOWNLOADTO_ALIASES).toUtf8());
    paths   = QByteArray::fromBase64(WSGET(WS_DOWNLOADTO_PATHS).toUtf8());

    QStringList a = aliases.split("\n", QString::SkipEmptyParts);
    QStringList p = paths.split("\n", QString::SkipEmptyParts);

    QStringList temp_pathes = DownloadToDirHistory::get();

    if (!temp_pathes.isEmpty()){
        for (const auto &t : temp_pathes){
            QAction *act = new QAction(WICON(WulforUtil::eiFOLDER_BLUE), QDir(t).dirName(), down_to);
            act->setToolTip(t);
            act->setData(t);

            down_to->addAction(act);

            QAction *act1 = new QAction(WICON(WulforUtil::eiFOLDER_BLUE), QDir(t).dirName(), down_to);
            act1->setToolTip(t);
            act1->setData(t);

            down_wh_to->addAction(act1);
        }

        down_to->addSeparator();
        down_wh_to->addSeparator();
    }

    if (a.size() == p.size() && !a.isEmpty()){
        for (int i = 0; i < a.size(); i++){
            QAction *act = new QAction(WICON(WulforUtil::eiFOLDER_BLUE), a.at(i), down_to);
            act->setData(p.at(i));

            down_to->addAction(act);

            QAction *act1 = new QAction(WICON(WulforUtil::eiFOLDER_BLUE), a.at(i), down_to);
            act1->setData(p.at(i));

            down_wh_to->addAction(act1);
        }

        down_to->addSeparator();
        down_wh_to->addSeparator();
    }

    QAction *browse = new QAction(WICON(WulforUtil::eiFOLDER_BLUE), tr("Browse"), down_to);
    browse->setData("");

    QAction *browse1 = new QAction(WICON(WulforUtil::eiFOLDER_BLUE), tr("Browse"), down_to);
    browse->setData("");

    down_to->addAction(browse);
    down_wh_to->addAction(browse1);

    menu->clear();
    menu->addActions(action_list);
    menu->insertMenu(action_list.at(1), down_to);
    menu->insertMenu(action_list.at(2), down_wh_to);
    menu->insertMenu(action_list.at(5), magnet_menu);
    menu->insertMenu(action_list.at(12),black_list_menu);

    QScopedPointer<QMenu> userm( WulforUtil::getInstance()->buildUserCmdMenu(list, UserCommand::CONTEXT_SEARCH) );

    if (!userm.isNull() && !userm->actions().isEmpty())
        menu->addMenu(userm.data());

    QAction *ret = menu->exec(QCursor::pos());

    if (actions.contains(ret)) {
        return actions.value(ret);
    } else if (down_to->actions().contains(ret)) {
        downToPath = ret->data().toString();
        return DownloadTo;
    } else if (down_wh_to->actions().contains(ret)) {
        downToPath = ret->data().toString();
        return DownloadWholeDirTo;
    } else if (ret) {
        ucParams["LAST"] = ret->toolTip();
        ucParams["NAME"] = ret->statusTip();
        ucParams["HOST"] =  ret->data().toString();
        return UserCommands;
    } else {
        return None;
    }
}

void SearchFrame::Menu::addTempPath(const QString &path){
    QStringList temp_pathes = DownloadToDirHistory::get();
    temp_pathes.push_front(path);

    DownloadToDirHistory::put(temp_pathes);
}

SearchFrame::SearchFrame(QWidget *parent) :
    QWidget(parent),
    d_ptr(new SearchFramePrivate())
{
    if (!SearchBlacklist::getInstance())
        SearchBlacklist::newInstance();

    Q_D(SearchFrame);

    d->arena_title = tr("Search");

    setupUi(this);

    init();

    ClientManager* clientMgr = ClientManager::getInstance();

#ifdef DO_NOT_USE_MUTEX
    clientMgr->lock();
#else // DO_NOT_USE_MUTEX
    auto lock = clientMgr->lock();
#endif // DO_NOT_USE_MUTEX
    clientMgr->addListener(this);
    auto& clients = clientMgr->getClients();

    for (const auto &client : clients) {
        if(!client->isConnected())
            continue;

        d->hubs.push_back(_q(client->getHubUrl()));
        d->client_list.push_back(client);
    }

#ifdef DO_NOT_USE_MUTEX
    clientMgr->unlock();
#endif // DO_NOT_USE_MUTEX

    d->str_model->setStringList(d->hubs);

    for (int i = 0; i < d->str_model->rowCount(); i++)
       d->str_model->setData(d->str_model->index(i, 0), Qt::Checked, Qt::CheckStateRole);

    SearchManager::getInstance()->addListener(this);
}

SearchFrame::~SearchFrame(){
    Menu::deleteInstance();

    Q_D(SearchFrame);

    treeView_RESULTS->setModel(NULL);

    delete d_ptr;
}

void SearchFrame::closeEvent(QCloseEvent *e){
    SearchManager::getInstance()->removeListener(this);
    ClientManager::getInstance()->removeListener(this);

    Q_D(SearchFrame);

    save();

    setAttribute(Qt::WA_DeleteOnClose);

    QWidget::disconnect(this, NULL, this, NULL);

    e->accept();
}

bool SearchFrame::eventFilter(QObject *obj, QEvent *e){
    if (e->type() == QEvent::KeyRelease){
        QKeyEvent *k_e = reinterpret_cast<QKeyEvent*>(e);

        if (static_cast<LineEdit*>(obj) == lineEdit_FILTER && k_e->key() == Qt::Key_Escape){
            lineEdit_FILTER->clear();

            requestFilter();

            return true;
        }
    }

    return QWidget::eventFilter(obj, e);
}

void SearchFrame::init(){
    Q_D(SearchFrame);

    d->model = new SearchModel(this);
    d->proxy = new SearchProxyModel(this);
    d->str_model = new SearchStringListModel(this);

    for (int i = 0; i < d->model->columnCount(); i++)
        comboBox_FILTERCOLUMNS->addItem(d->model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());

    comboBox_FILTERCOLUMNS->setCurrentIndex(SearchModel::ColumnFileName);

    frame_FILTER->setVisible(false);

    pushButton_STOP->hide();

    toolButton_CLOSEFILTER->setIcon(WICON(WulforUtil::eiEDITDELETE));

    treeView_RESULTS->setModel(d->model);
    treeView_RESULTS->setItemDelegate(new SearchViewDelegate(this));
    treeView_RESULTS->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_RESULTS->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_RESULTS->setUniformRowHeights(true);
    treeView_RESULTS->setAllColumnsShowFocus(true);

    treeView_HUBS->setModel(d->str_model);

    // Predefined
    QStringList filetypes;
    for (int i = SearchManager::TYPE_ANY; i < SearchManager::TYPE_LAST; i++) {
            filetypes << _q(SearchManager::getTypeStr(i));
    }

    // Customs
    const auto &searchTypes = SettingsManager::getInstance()->getSearchTypes();
    for (const auto &i : searchTypes) {
        string type = i.first;
        if (!(type.size() == 1 && type[0] >= '1' && type[0] <= '7')) {
                filetypes << _q(type);
        }
    }

    comboBox_FILETYPES->addItems(filetypes);
    comboBox_FILETYPES->setCurrentIndex(0);

    QList<WulforUtil::Icons> icons;
    icons   << WulforUtil::eiFILETYPE_UNKNOWN  << WulforUtil::eiFILETYPE_MP3         << WulforUtil::eiFILETYPE_ARCHIVE
            << WulforUtil::eiFILETYPE_DOCUMENT << WulforUtil::eiFILETYPE_APPLICATION << WulforUtil::eiFILETYPE_PICTURE
            << WulforUtil::eiFILETYPE_VIDEO    << WulforUtil::eiFOLDER_BLUE          << WulforUtil::eiFIND
            << WulforUtil::eiFILETYPE_ARCHIVE;

    for (int i = 0; i < icons.size(); i++)
        comboBox_FILETYPES->setItemIcon(i, WICON(icons.at(i)));

    QString raw  = QByteArray::fromBase64(WSGET(WS_SEARCH_HISTORY).toUtf8());
    d->searchHistory = raw.replace("\r","").split('\n', QString::SkipEmptyParts);

    d->focusShortcut = new QShortcut(QKeySequence(Qt::Key_F6), this);
    d->focusShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    QMenu *historyMenu = new QMenu();
    for (const auto &s : d->searchHistory)
        historyMenu->addAction(s);

    lineEdit_SIZE->setValidator(new QIntValidator(this));

    lineEdit_SEARCHSTR->setMenu(historyMenu);
    lineEdit_SEARCHSTR->setPixmap(WICON(WulforUtil::eiEDITADD).scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    lineEdit_FILTER->installEventFilter(this);

    connect(this, SIGNAL(coreClientConnected(QString)),    this, SLOT(onHubAdded(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(coreClientDisconnected(QString)), this, SLOT(onHubRemoved(QString)),Qt::QueuedConnection);
    connect(this, SIGNAL(coreClientUpdated(QString)),      this, SLOT(onHubChanged(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(coreAppendTask(SearchResultPtr)), d->model, SLOT(appendTask(SearchResultPtr)), Qt::QueuedConnection);
    connect(this, SIGNAL(coreProcessTasks()), d->model, SLOT(processTasks()), Qt::QueuedConnection);
    connect(this, SIGNAL(dropResult()), d->model, SLOT(incDropped()), Qt::QueuedConnection);

    connect(d->model, SIGNAL(updateStatus()), this, SLOT(slotUpdateStatus()), Qt::QueuedConnection);
    connect(d->model, SIGNAL(expandIndex(QModelIndex)), treeView_RESULTS, SLOT(expand(QModelIndex)), Qt::QueuedConnection);

    connect(treeView_RESULTS, SIGNAL(expanded(QModelIndex)), d->model, SLOT(expandItem(QModelIndex)));
    connect(treeView_RESULTS, SIGNAL(collapsed(QModelIndex)), d->model, SLOT(collapseItem(QModelIndex)));


    connect(d->focusShortcut, SIGNAL(activated()), lineEdit_SEARCHSTR, SLOT(setFocus()));
    connect(d->focusShortcut, SIGNAL(activated()), lineEdit_SEARCHSTR, SLOT(selectAll()));
    connect(pushButton_SEARCH, SIGNAL(clicked()), this, SLOT(slotStartSearch()));
    connect(pushButton_STOP, SIGNAL(clicked()), this, SLOT(slotStopSearch()));
    connect(pushButton_CLEAR, SIGNAL(clicked()), this, SLOT(slotClear()));
    connect(treeView_RESULTS, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotResultDoubleClicked(QModelIndex)));
    connect(treeView_RESULTS, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotContextMenu(QPoint)));
    connect(treeView_RESULTS->header(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotHeaderMenu(QPoint)));
    connect(GlobalTimer::getInstance(), SIGNAL(second()), this, SLOT(slotTimer()));
    connect(pushButton_SIDEPANEL, SIGNAL(clicked()), this, SLOT(slotToggleSidePanel()));
    connect(lineEdit_SEARCHSTR, SIGNAL(returnPressed()), this, SLOT(slotStartSearch()));
    connect(lineEdit_SIZE,      SIGNAL(returnPressed()), this, SLOT(slotStartSearch()));
    connect(comboBox_FILETYPES, SIGNAL(currentIndexChanged(int)), lineEdit_SEARCHSTR, SLOT(setFocus()));
    connect(comboBox_FILETYPES, SIGNAL(currentIndexChanged(int)), lineEdit_SEARCHSTR, SLOT(selectAll()));
    connect(toolButton_CLOSEFILTER, SIGNAL(clicked()), this, SLOT(slotFilter()));
    connect(comboBox_FILTERCOLUMNS, SIGNAL(currentIndexChanged(int)), lineEdit_FILTER, SLOT(selectAll()));
    connect(comboBox_FILTERCOLUMNS, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChangeProxyColumn(int)));
    connect(comboBox_SHARED, SIGNAL(currentIndexChanged(int)), d->model, SLOT(setFilterRole(int)));

    connect(WulforSettings::getInstance(), SIGNAL(strValueChanged(QString,QString)), this, SLOT(slotSettingsChanged(QString,QString)));

    load();

    setAttribute(Qt::WA_DeleteOnClose);

    QList<int> panes = splitter->sizes();
    d->left_pane_old_size = panes[0];

    lineEdit_SEARCHSTR->setFocus();
}

void SearchFrame::load(){
    Q_D(SearchFrame);

    treeView_RESULTS->header()->restoreState(QByteArray::fromBase64(WSGET(WS_SEARCH_STATE).toUtf8()));
    treeView_RESULTS->setSortingEnabled(true);

    d->filterShared = static_cast<SearchFrame::AlreadySharedAction>(WIGET(WI_SEARCH_SHARED_ACTION));

    comboBox_SHARED->setCurrentIndex(static_cast<int>(d->filterShared));

    checkBox_FILTERSLOTS->setChecked(WBGET(WB_SEARCHFILTER_NOFREE));
    checkBox_HIDEPANEL->setChecked(WBGET(WB_SEARCH_DONTHIDEPANEL));

    comboBox_FILETYPES->setCurrentIndex(WIGET(WI_SEARCH_LAST_TYPE));

    treeView_RESULTS->sortByColumn(WIGET(WI_SEARCH_SORT_COLUMN), WulforUtil::getInstance()->intToSortOrder(WIGET(WI_SEARCH_SORT_ORDER)));

    QString raw = QByteArray::fromBase64(WSGET(WS_SEARCH_HISTORY).toUtf8());
    QStringList list = raw.replace("\r","").split('\n', QString::SkipEmptyParts);

    d->completer = new QCompleter(list, lineEdit_SEARCHSTR);
    d->completer->setCaseSensitivity(Qt::CaseInsensitive);
    d->completer->setWrapAround(false);

    lineEdit_SEARCHSTR->setCompleter(d->completer);
}

void SearchFrame::save(){
    Q_D(SearchFrame);

    WSSET(WS_SEARCH_STATE, treeView_RESULTS->header()->saveState().toBase64());
    WISET(WI_SEARCH_SORT_COLUMN, d->model->getSortColumn());
    WISET(WI_SEARCH_SORT_ORDER, WulforUtil::getInstance()->sortOrderToInt(d->model->getSortOrder()));
    WISET(WI_SEARCH_SHARED_ACTION, static_cast<int>(d->filterShared));

    if (d->saveFileType)
        WISET(WI_SEARCH_LAST_TYPE, comboBox_FILETYPES->currentIndex());

    WBSET(WB_SEARCHFILTER_NOFREE, checkBox_FILTERSLOTS->isChecked());
    WBSET(WB_SEARCH_DONTHIDEPANEL, checkBox_HIDEPANEL->isChecked());
}

QWidget *SearchFrame::getWidget(){
    return this;
}

QString SearchFrame::getArenaTitle(){
    Q_D(SearchFrame);

    return d->arena_title;
}

QString SearchFrame::getArenaShortTitle(){
    return getArenaTitle();
}

QMenu *SearchFrame::getMenu(){
    return NULL;
}

const QPixmap &SearchFrame::getPixmap(){
    return WICON(WulforUtil::eiFILEFIND);
}

void SearchFrame::onHubAdded(const QString &info){
    Q_D(SearchFrame);

    if (info.isEmpty() || d->hubs.contains(info))
        return;

    d->hubs.push_back(info);
    d->client_list.push_back(ClientManager::getInstance()->getClient(_tq(info)));

    d->str_model->setStringList(d->hubs);
}

void SearchFrame::onHubChanged(const QString &info){
    Q_D(SearchFrame);

    if (info.isEmpty() || !d->hubs.contains(info))
        return;

    Client *cl = ClientManager::getInstance()->getClient(_tq(info));
    if (!cl || !d->client_list.contains(cl))
        return;

    d->hubs.removeAt(d->client_list.indexOf(cl));
    d->client_list.removeAt(d->client_list.indexOf(cl));

    d->hubs.push_back(info);
    d->client_list.push_back(cl);

    d->str_model->setStringList(d->hubs);
}

void SearchFrame::onHubRemoved(const QString &info){
    Q_D(SearchFrame);

    if (info.isEmpty() || !d->hubs.contains(info))
        return;

    d->client_list.removeAt(d->hubs.indexOf(info));
    d->hubs.removeAt(d->hubs.indexOf(info));

    d->str_model->setStringList(d->hubs);
}

void SearchFrame::searchAlternates(const QString &tth){
    if (tth.isEmpty())
        return;

    Q_D(SearchFrame);

    lineEdit_SEARCHSTR->setText(tth);
    comboBox_FILETYPES->setCurrentIndex(SearchManager::TYPE_TTH);
    lineEdit_SIZE->clear();

    slotStartSearch();

    d->saveFileType = false;
}

void SearchFrame::searchFile(const QString &file){
    if (file.isEmpty())
        return;

    Q_D(SearchFrame);

    lineEdit_SEARCHSTR->setText(file);
    comboBox_FILETYPES->setCurrentIndex(SearchManager::TYPE_ANY);
    lineEdit_SIZE->clear();

    d->saveFileType = false;

    slotStartSearch();
}

void SearchFrame::fastSearch(const QString &text, bool isTTH){
    if (text.isEmpty())
        return;

    if (!isTTH)
        comboBox_FILETYPES->setCurrentIndex(0); // set type "Any"
    else
        comboBox_FILETYPES->setCurrentIndex(8); // set type "TTH"

    lineEdit_SEARCHSTR->setText(text);

    slotStartSearch();
}

void SearchFrame::slotStartSearch(){
    if (qobject_cast<QPushButton*>(sender()) != pushButton_SEARCH){
        pushButton_SEARCH->click(); //Generating clicked() signal that shows pushButton_STOP button.
                                    //Anybody can suggest something better?
        return;
    }

    Q_D(SearchFrame);

    QString searchStr = lineEdit_SEARCHSTR->text().trimmed();

    if (searchStr.isEmpty())
        return;

    StringList hubUrls;
    for (int i = 0; i < d->str_model->rowCount(); i++){
        QModelIndex index = d->str_model->index(i, 0);

        if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
            hubUrls.push_back(_tq(index.data().toString()));
    }

#ifndef WITH_DHT
    if (clients.empty())
        return;
#endif

    double lsize = lineEdit_SIZE->text().toDouble();

    switch (comboBox_SIZE->currentIndex())
    {
        case 1: lsize *= 1024.0; break;
        case 2: lsize *= 1024.0*1024.0; break;
        case 3: lsize *= 1024.0*1024.0*1024.0; break;
    }

    auto llsize = static_cast<int64_t>(lsize);

    if (!d->searchHistory.contains(searchStr)) {
        bool isTTH = WulforUtil::isTTH(searchStr);

        if ((WBGET("memorize-tth-search-phrases", false) && isTTH) || !isTTH)
            d->searchHistory.prepend(searchStr);

        QMenu *historyMenu = new QMenu();

        for (const auto &s : d->searchHistory)
            historyMenu->addAction(s);

        lineEdit_SEARCHSTR->setMenu(historyMenu);

        uint maxItemsNumber = WIGET("search-history-items-number", 10);
        while (d->searchHistory.count() > maxItemsNumber)
                d->searchHistory.removeLast();

        QString hist = d->searchHistory.join("\n");
        WSSET(WS_SEARCH_HISTORY, hist.toUtf8().toBase64());
    }

    {
        d->currentSearch = StringTokenizer<string>(searchStr.toStdString(), ' ').getTokens();
        searchStr.clear();

        //strip out terms beginning with -
        for (auto si = d->currentSearch.begin(); si != d->currentSearch.end(); ) {
            if(si->empty()) {
                si = d->currentSearch.erase(si);
                continue;
            }

            if ((*si)[0] != '-')
                searchStr += QString::fromStdString(*si) + ' ';

            ++si;
        }

        d->token = Util::toString(Util::rand());
    }

    auto searchMode(static_cast<SearchManager::SizeModes>(comboBox_SIZETYPE->currentIndex()));

    if (!llsize || lineEdit_SIZE->text().isEmpty())
        searchMode = SearchManager::SIZE_DONTCARE;

    auto ftype(static_cast<SearchManager::TypeModes>(comboBox_FILETYPES->currentIndex()));

    d->isHash = (ftype == SearchManager::TYPE_TTH);
    d->filterShared = static_cast<AlreadySharedAction>(comboBox_SHARED->currentIndex());
    d->withFreeSlots = checkBox_FILTERSLOTS->isChecked();

    d->model->setFilterRole(static_cast<int>(d->filterShared));
    d->model->clearModel();

    string ftypeStr;
    if (ftype > SearchManager::TYPE_ANY && ftype < SearchManager::TYPE_LAST) {
        ftypeStr = SearchManager::getInstance()->getTypeStr(ftype);
    } else {
        ftypeStr = _tq(lineEdit_SEARCHSTR->text());
        ftype = SearchManager::TYPE_ANY;
    }

    StringList exts;
    try {
        if (ftype == SearchManager::TYPE_ANY) {
            // Custom searchtype
            exts = SettingsManager::getInstance()->getExtensions(ftypeStr);
        } else if ((ftype > SearchManager::TYPE_ANY && ftype < SearchManager::TYPE_DIRECTORY) || ftype == SearchManager::TYPE_CD_IMAGE) {
            // Predefined searchtype
            exts = SettingsManager::getInstance()->getExtensions(string(1, '0' + ftype));
        }
    } catch (const SearchTypeException&) {
        ftype = SearchManager::TYPE_ANY;
    }

    d->model->setSearchType(ftype);

    d->target = searchStr;
    d->searchStartTime = GlobalTimer::getInstance()->getTicks()*1000;

    d->stop = false;

    uint64_t maxDelayBeforeSearch = SearchManager::getInstance()->search(hubUrls, searchStr.toStdString(), llsize, ftype, searchMode, d->token, exts, (void*)this);
    uint64_t waitingResultsTime = 20000; // just assumption that user receives most of results in 20 seconds

    d->searchEndTime = d->searchStartTime + maxDelayBeforeSearch + waitingResultsTime;
    d->waitingResults = true;

    if (!checkBox_HIDEPANEL->isChecked()) {
        QList<int> panes = splitter->sizes();

        panes[1] = panes[0] + panes[1];

        d->left_pane_old_size = panes[0] > 15 ? panes[0] : d->left_pane_old_size;

        panes[0] = 0;

        splitter->setSizes(panes);
    }

    d->arena_title = tr("Search - %1").arg(searchStr);

    lineEdit_SEARCHSTR->setEnabled(false);

    MainWindow::getInstance()->redrawToolPanel();
}

void SearchFrame::slotUpdateStatus() {
    Q_D(SearchFrame);

    QString statusText;
    if (d->model->isEmpty()) {
        if (!d->currentSearch.empty())
            statusText = tr("<b>No results</b>");
    } else {
        auto counters = d->model->getCounters();

        statusText = tr("Found: <b>%1</b>  Uniques: <b>%2</b>  Dropped: <b>%3</b>")
                .arg(counters.result)
                .arg(counters.unique)
                .arg(counters.dropped);
    }
    status->setText(statusText);
}

void SearchFrame::slotClear() {
    Q_D(SearchFrame);

    d->model->clearModel();

    treeView_RESULTS->clearSelection();
    lineEdit_SEARCHSTR->clear();
    lineEdit_SIZE->clear();
}

void SearchFrame::removeSelected()
{
    int rows_removed = 1;

    auto model = treeView_RESULTS->model();
    auto sel_model = treeView_RESULTS->selectionModel();

    QItemSelection selection(sel_model->selection());

    int last_row = selection.last().top();
    QModelIndex last_parent = selection.last().parent();

    bool ok = false;
    for (const QItemSelectionRange& range : selection) {
        if (!range.isValid() || range.isEmpty())
            continue;

        if (range.parent() == last_parent && range.top() < last_row)
            rows_removed += range.height();

        ok = ok || model->removeRows(range.top(), range.height(), range.parent());
    }

    QModelIndex current = sel_model->currentIndex();

    if (!current.isValid()) {
        int new_row;
        if ((new_row = (last_row - rows_removed)) < 0)
            new_row = last_row;

        if (!model->rowCount(last_parent)) {
            current = model->index(last_parent.row(), 0, last_parent.parent());
        } else {
            current = model->index(new_row, 0, last_parent);
        }

        if (!current.isValid())
            current = model->index(0, 0);
    }

    sel_model->select(current, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    if (ok)
        slotUpdateStatus();
}

void SearchFrame::execUserCommand()
{
    Q_D(SearchFrame);

    QString cmd_name = Menu::getInstance()->ucParams["NAME"];
    QString hub      = Menu::getInstance()->ucParams["HOST"];

    int id = FavoriteManager::getInstance()->findUserCommand(cmd_name.toStdString(), hub.toStdString());

    UserCommand uc;
    if (id == -1 || !FavoriteManager::getInstance()->getUserCommand(id, uc))
        return;

    if (!WulforUtil::getInstance()->getUserCommandParams(uc, d->ucLineParams))
        return;

    StringMap params = d->ucLineParams;

    set<CID> users;

    auto indexes =
            treeView_RESULTS->selectionModel()->selectedRows();

    for (const QModelIndex &index: indexes) {
        SearchItem *item = getValidItem(index);

        if (!item->getUser()->isOnline())
            continue;

        if (uc.once()) {
            if(users.find(item->cid()) != users.end())
                continue;
            users.insert(item->cid());
        }

        params["fileFN"] = item->result()->getFile();
        params["fileSI"] = Util::toString(item->result()->getSize());
        params["fileSIshort"] = Util::formatBytes(item->result()->getSize());

        if(item->isFile())
            params["fileTR"] = item->tth().toBase32();

        params["fileMN"] = item->getMagnet().toStdString();

        // compatibility with 0.674 and earlier
        params["file"] = params["fileFN"];
        params["filesize"] = params["fileSI"];
        params["filesizeshort"] = params["fileSIshort"];
        params["tth"] = params["fileTR"];

        StringMap tmp = params;
        ClientManager::getInstance()->userCommand(item->getHintedUser(), uc, tmp, true);
    }
}

void SearchFrame::ignoreSelected()
{
    static SearchBlacklist *SB = SearchBlacklist::getInstance();

    QModelIndexList indexes =
            treeView_RESULTS->selectionModel()->selectedRows();

    QStringList new_inst;
    for (QModelIndex &index: indexes) {
        SearchItem *item = getValidItem(index);
        new_inst << item->getFileName();
    }

    if (!new_inst.isEmpty()) {
        QStringList list = SB->getList(SearchBlacklist::NAME);
        list.append(new_inst);
        SB->setList(SearchBlacklist::NAME, list);
    }
}

void SearchFrame::clipMagnets(bool web)
{
    static const QString webTemplate = "[magnet=\"%1\"]%2[/magnet]";

    QModelIndexList indexes =
            treeView_RESULTS->selectionModel()->selectedRows();

    QStringList magnets;
    for (const QModelIndex &index: indexes) {
        SearchItem *item = getValidItem(index);
        if (item && item->isFile()) {
            QString magnet = item->getMagnet();

            if (magnet.isEmpty())
                continue;

            magnets << ((web) ?
                    webTemplate.arg(magnet).arg(item->getFileName()):
                    magnet);
        }
    }

    if (!magnets.isEmpty())
        qApp->clipboard()->setText('\n' + magnets.join("\n"), QClipboard::Clipboard);
}

QString SearchFrame::getTargetDir()
{
    static QString old_target = QDir::homePath();

    QString target = Menu::getInstance()->getDownloadToPath();

    if (target.isEmpty() || !QDir(target).exists()) {
        target = QFileDialog::getExistingDirectory(this,
                        tr("Select directory"), old_target);

        target = QDir::toNativeSeparators(target);

        Menu::getInstance()->addTempPath(target);
    }

    if (target.isEmpty())
        return QString();

    if (!target.endsWith(QDir::separator()))
        target += QDir::separator();

    old_target = target;

    return target;
}

QModelIndex SearchFrame::getValidIndex(const QModelIndex &index) {
    if (!index.isValid())
        return index;

    Q_D(SearchFrame);

    return (index.model() == d->proxy) ?
                d->proxy->mapToSource(index):
                index;
}

SearchItem * SearchFrame::getValidItem(const QModelIndex &index) {
    Q_D(SearchFrame);

    return d->model->getItem(getValidIndex(index));
}

void SearchFrame::slotResultDoubleClicked(const QModelIndex &index) {
    if (!index.isValid())
        return;

    forEachSelected(&SearchItem::download, SETTING(DOWNLOAD_DIRECTORY));
}

void SearchFrame::slotContextMenu(const QPoint &) {
    auto sel_model = treeView_RESULTS->selectionModel();

    if (!sel_model->hasSelection())
        return;

    if (!Menu::getInstance())
        Menu::newInstance();

    SearchItem::CheckTTH checkTTH = forEachSelected(SearchItem::CheckTTH());

    Menu::Action act = Menu::getInstance()->exec(checkTTH.hubs);

    switch (act) {
        case Menu::None:
        {
            break;
        }
        case Menu::Download:
        {
            forEachSelected(&SearchItem::download, SETTING(DOWNLOAD_DIRECTORY));

            break;
        }
        case Menu::DownloadTo:
        {
            QString target = getTargetDir();

            if (!target.isEmpty())
                forEachSelected(&SearchItem::download, target.toStdString());

            break;
        }
        case Menu::DownloadWholeDir:
        {
            forEachSelected(&SearchItem::downloadWhole, SETTING(DOWNLOAD_DIRECTORY));

            break;
        }
        case Menu::DownloadWholeDirTo:
        {
            QString target = getTargetDir();

            if (!target.isEmpty())
                forEachSelected(&SearchItem::downloadWhole, target.toStdString());

            break;
        }
        case Menu::SearchTTH:
        {
            const SearchItem *item = getValidItem(sel_model->currentIndex());

            if (item && item->isFile()) {
                SearchFrame *searchFrame = ArenaWidgetFactory().create<SearchFrame>();
                searchFrame->searchAlternates(item->getTthString());
            }

            break;
        }
        case Menu::Magnet:
        {
            clipMagnets(false);

            break;
        }
        case Menu::MagnetWeb:
        {
            clipMagnets(true);

            break;
        }
        case Menu::MagnetInfo:
        {
            const SearchItem *item = getValidItem(sel_model->currentIndex());

            if (item && item->isFile()) {
                QString magnet = item->getMagnet();

                if (!magnet.isEmpty()) {
                    Magnet m(this);
                    m.setLink(magnet);
                    m.exec();
                }
            }

            break;
        }
        case Menu::Browse:
        {
            forEachSelected(&SearchItem::getFileList, false);

            break;
        }
        case Menu::MatchQueue:
        {
            forEachSelected(&SearchItem::getFileList, true);

            break;
        }
        case Menu::SendPM:
        {
            forEachSelected(&SearchItem::openPm);

            break;
        }
        case Menu::AddToFav:
        {
            forEachSelected(&SearchItem::addAsFavorite);

            break;
        }
        case Menu::GrantExtraSlot:
        {
            forEachSelected(&SearchItem::grantSlot);

            break;
        }
        case Menu::RemoveFromQueue:
        {
            forEachSelected(&SearchItem::removeFromQueue);

             break;
        }
        case Menu::Remove:
        {
            removeSelected();

            break;
        }
        case Menu::UserCommands:
        {
            execUserCommand();

            break;
        }
        case Menu::Blacklist:
        {
            SearchBlackListDialog dlg(this);

            dlg.exec();

            break;
        }
        case Menu::AddToBlacklist:
        {
            ignoreSelected();

            break;
        }
        default:
        {
            break;
        }
    }
}

void SearchFrame::slotHeaderMenu(const QPoint&){
    WulforUtil::headerMenu(treeView_RESULTS);
}

void SearchFrame::slotTimer(){
    Q_D(SearchFrame);

    if (d->waitingResults) {
        uint64_t now = GlobalTimer::getInstance()->getTicks()*1000;
        float fraction  = 100.0f*(now - d->searchStartTime)/(d->searchEndTime - d->searchStartTime);
        if (fraction >= 100.0) {
            fraction = 100.0;
            d->waitingResults = false;
        }
        QString msg = tr("Searching for %1 ...").arg(d->target);
        progressBar->setFormat(msg);
        progressBar->setValue(static_cast<unsigned>(fraction));
    } else {
        QString msg;
        progressBar->setFormat(msg);
        progressBar->setValue(0);
        lineEdit_SEARCHSTR->setEnabled(true);

        emit coreProcessTasks();
    }

    if (d->model->isEmpty()) {
        if (d->currentSearch.empty())
            frame_PROGRESS->hide();
        else
            frame_PROGRESS->show();
    } else {
        if (!frame_PROGRESS->isVisible())
            frame_PROGRESS->show();
    }
}

void SearchFrame::slotToggleSidePanel(){
    QList<int> panes = splitter->sizes();
    Q_D(SearchFrame);

    if (panes[0] < 15){//left pane can't have width less than 15px
        panes[0] = d->left_pane_old_size;
        panes[1] = panes[1] - d->left_pane_old_size;
    }
    else {
        panes[1] = panes[0] + panes[1];
        d->left_pane_old_size = panes[0];
        panes[0] = 0;
    }

    splitter->setSizes(panes);
}

void SearchFrame::slotFilter(){
    Q_D(SearchFrame);

    if (frame_FILTER->isVisible()){
        treeView_RESULTS->setModel(d->model);

        disconnect(lineEdit_FILTER, SIGNAL(textChanged(QString)), d->proxy, SLOT(setFilterFixedString(QString)));
    } else {
        d->proxy->setDynamicSortFilter(false);
        d->proxy->setFilterFixedString(lineEdit_FILTER->text());
        d->proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        d->proxy->setFilterKeyColumn(comboBox_FILTERCOLUMNS->currentIndex());
        d->proxy->setSourceModel(d->model);

        treeView_RESULTS->setModel(d->proxy);

        connect(lineEdit_FILTER, SIGNAL(textChanged(QString)), d->proxy, SLOT(setFilterFixedString(QString)));

        if (!lineEdit_SEARCHSTR->selectedText().isEmpty()){
            lineEdit_FILTER->setText(lineEdit_SEARCHSTR->selectedText());
            lineEdit_FILTER->selectAll();
        }

        lineEdit_FILTER->setFocus();

        if (!lineEdit_FILTER->text().isEmpty())
            lineEdit_FILTER->selectAll();
    }

    frame_FILTER->setVisible(!frame_FILTER->isVisible());
}

void SearchFrame::slotChangeProxyColumn(int col){
    Q_D(SearchFrame);

    if (d->proxy)
        d->proxy->setFilterKeyColumn(col);
}

void SearchFrame::slotSettingsChanged(const QString &key, const QString &value){
    if (key == WS_TRANSLATION_FILE)
        retranslateUi(this);
}

void SearchFrame::on(SearchManagerListener::SR, const dcpp::SearchResultPtr& aResult) noexcept {
    Q_D(SearchFrame);

    if (d->currentSearch.empty() || !aResult || d->stop)
        return;

    if (!aResult->getToken().empty() && d->token != aResult->getToken()) {
        emit dropResult();
        return;
    }

    if(d->isHash) {
        if(aResult->getType() != SearchResult::TYPE_FILE || TTHValue(Text::fromT(d->currentSearch[0])) != aResult->getTTH()) {
            emit dropResult();
            return;
        }
    } else {
        for (const auto &j : d->currentSearch) {
            if((*j.begin() != ('-') && Util::findSubString(aResult->getFile(), j) == string::npos) ||
               (*j.begin() == ('-') && j.size() != 1 && Util::findSubString(aResult->getFile(), j.substr(1)) != string::npos)
              )
           {
                    emit dropResult();
                    return;
           }
        }
    }

    if (d->filterShared == Filter && aResult->getType() == SearchResult::TYPE_FILE){
        const TTHValue& t = aResult->getTTH();

        if (ShareManager::getInstance()->isTTHShared(t)) {
            emit dropResult();
            return;
        }
    }

    if (d->withFreeSlots && !aResult->getFreeSlots()){
        emit dropResult();
        return;
    }

    emit coreAppendTask(aResult);
}

void SearchFrame::slotClose() {
    ArenaWidgetManager::getInstance()->rem(this);
}

void SearchFrame::on(ClientConnected, Client* c) noexcept{
    emit coreClientConnected(_q(c->getHubUrl()));
}

void SearchFrame::on(ClientUpdated, Client* c) noexcept{
    emit coreClientUpdated((_q(c->getHubUrl())));
}

void SearchFrame::on(ClientDisconnected, Client* c) noexcept{
    emit coreClientDisconnected((_q(c->getHubUrl())));
}

void SearchFrame::slotStopSearch(){
    Q_D(SearchFrame);

    d->stop = true;
    d->waitingResults = false;
}
