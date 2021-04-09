#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QModelIndex>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QDialog>
#include <QtWidgets/QProgressBar> 
#include <QtWidgets/QToolButton> 
#include "binaryninjaapi.h"
#include "dockhandler.h"
#include "viewframe.h"
#include "filter.h"
#include "expandablegroup.h"

#define FIND_RESULT_LIST_UPDATE_INTERVAL 250
#define COLUMN_MIN_WIDTH_IN_CHAR 10

class CachedTokens
{
public:
    QVariant tokens;
    QVariant flattenedTokens;
    bool valid;

    CachedTokens(): valid(false) {}
    CachedTokens(const CachedTokens& other):
        tokens(other.tokens), flattenedTokens(other.flattenedTokens), valid(other.valid)
    {}
};


class SearchResultItem
{
private:
    uint64_t m_addr;
    BinaryNinja::DataBuffer m_buffer;
    FunctionRef m_func;
    CachedTokens m_tokensCache[4];

public:
    SearchResultItem();
    SearchResultItem(uint64_t addr, const BinaryNinja::DataBuffer& buffer, FunctionRef func);
    SearchResultItem(const SearchResultItem& other);
    uint64_t addr() const { return m_addr; }
    BinaryNinja::DataBuffer buffer() const { return m_buffer; }
    FunctionRef func() const { return m_func; }
    bool operator==(const SearchResultItem& other) const { return m_addr == other.addr(); }
    bool operator!=(const SearchResultItem& other) const { return m_addr != other.addr(); }
    bool operator<(const SearchResultItem& other) const { return m_addr < other.addr(); }

    // TODO: check if i is beyond the range
    CachedTokens getCachedTokens(size_t i) const { return m_tokensCache[i]; }
    CachedTokens& getCachedTokens(size_t i) { return m_tokensCache[i]; }
    void setCachedTokens(size_t i, QVariant tokens, QVariant flattenedTokens)
    {
        m_tokensCache[i].tokens = tokens;
        m_tokensCache[i].flattenedTokens = flattenedTokens;
        m_tokensCache[i].valid = true;
    }
};

Q_DECLARE_METATYPE(SearchResultItem);


class BINARYNINJAUIAPI SearchResultModel: public QAbstractTableModel
{
    Q_OBJECT

protected:
    QWidget* m_owner;
    BinaryViewRef m_data;
    ViewFrame* m_view;
    BinaryNinja::FindParameters m_params;
    std::vector<SearchResultItem> m_refs;
    mutable size_t m_columnWidths[4];

    std::mutex m_updateMutex;
    std::set<SearchResultItem> m_pendingSearchResults;

public:
    enum ColumnHeaders
    {
        AddressColumn = 0,
        DataColumn = 1,
        FunctionColumn = 2,
        PreviewColumn = 3
    };

    SearchResultModel(QWidget* parent, BinaryViewRef data, ViewFrame* view);
    virtual ~SearchResultModel();

    virtual QModelIndex index(int row, int col, const QModelIndex& parent = QModelIndex()) const override;

    void reset();
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { (void) parent; return (int)m_refs.size(); }
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override { (void) parent; return 4; }
    SearchResultItem getRow(int row) const;
    virtual QVariant data(const QModelIndex& i, int role) const override;
    virtual QVariant headerData(int column, Qt::Orientation orientation, int role) const override;
    void addItem(const SearchResultItem& addr);
    void clear();
    void updateFindParameters(const BinaryNinja::FindParameters params);
    void updateSearchResults();

    size_t getColumnWidth(size_t column) const { return m_columnWidths[column]; }
    void resetColumnWidth();
};


class BINARYNINJAUIAPI SearchResultFilterProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SearchResultFilterProxyModel(QObject* parent);
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual QVariant data(const QModelIndex& idx, int role) const override;
};


class BINARYNINJAUIAPI SearchResultItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT

    QFont m_font;
    int m_baseline, m_charWidth, m_charHeight, m_charOffset;

public:
    SearchResultItemDelegate(QWidget* parent);
    void updateFonts();
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& idx) const;
};

class SearchResultWidget;
class BINARYNINJAUIAPI SearchResultTable: public QTableView
{
    Q_OBJECT

    ViewFrame* m_view;
    SearchResultModel* m_table;
    SearchResultFilterProxyModel* m_model;
    SearchResultItemDelegate* m_itemDelegate;
    BinaryViewRef m_data;
    BinaryNinja::FindParameters m_params;
    UIActionHandler m_actionHandler;
    QTimer* m_updateTimer;

    int m_charWidth, m_charHeight;

public:
    SearchResultTable(SearchResultWidget* parent, ViewFrame* view, BinaryViewRef data);
    virtual ~SearchResultTable();

    void addSearchResult(const SearchResultItem& addr);
    void updateFindParameters(const BinaryNinja::FindParameters& params);
    void clearSearchResult();

    void updateFonts();
    void updateHeaderFontAndSize();

    virtual void keyPressEvent(QKeyEvent* e) override;

    virtual bool hasSelection() const { return selectionModel()->selectedRows().size() != 0; }
	virtual QModelIndexList selectedRows() const { return selectionModel()->selectedRows(); }

    void goToResult(const QModelIndex& idx);

    int rowCount() const;
    int filteredCount() const;

    void updateColumnWidth();
    void resetColumnWidth();

public Q_SLOTS:
    void resultActivated(const QModelIndex& idx);
    void updateFilter(const QString& filterText);
    void updateTimerEvent();

Q_SIGNALS:
    void newSelection();
};

class SearchProgressBar;
class BINARYNINJAUIAPI SearchResultWidget: public QWidget, public DockContextHandler
{
    Q_OBJECT
    Q_INTERFACES(DockContextHandler)

    ViewFrame* m_view;
    BinaryViewRef m_data;
    SearchResultTable* m_table;
    QLabel* m_label;
    QLineEdit* m_lineEdit;
    ExpandableGroup* m_group;
    SearchProgressBar* m_progress;
    BinaryNinja::FindParameters m_params;

    virtual void contextMenuEvent(QContextMenuEvent* event) override;

public:
    SearchResultWidget(ViewFrame* frame, BinaryViewRef data);
    ~SearchResultWidget();

    virtual void notifyFontChanged() override;

    void startNewFind(const BinaryNinja::FindParameters& params);
    virtual QString getHeaderText();

    void addSearchResult(uint64_t addr, const BinaryNinja::DataBuffer& match);
    void clearSearchResult();
    bool updateProgress(uint64_t cur, uint64_t total);
    void notifySearchCompleted();

public Q_SLOTS:
    void updateTotal();
};


class BINARYNINJAUIAPI SearchProgressBar: public QWidget
{
    Q_OBJECT

private:
    QProgressBar* m_progress;
    QToolButton* m_cancel;
    bool m_maxSet;
    bool m_running;
    // The minimal duration (in milliseconds) the progress bar must last, before it is displayed
    int m_minimalDuration;
    std::chrono::steady_clock::time_point m_lastUpdate;

public:
    explicit SearchProgressBar(QWidget* parent = nullptr);
    bool updateProgress(uint64_t cur, uint64_t total);
    void init();
    void reset();
    void show();
};
