#include "AdminDialog.h"
#include "SiteRegistry.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

constexpr int kNameMaxLen = 18;
constexpr int kCodeMaxLen = 6;
constexpr int kRegionMaxLen = 16;
constexpr int kEndpointMaxLen = 64;

// Neutral chip button, tinted per [accent="..."] property set on the widget.
QString baseButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  color:%1; background:rgba(169,181,194,0.08); border:1px solid rgba(169,181,194,0.45);"
        "  border-radius:8px; padding:6px 16px; font-weight:600; }"
        "QPushButton:hover { background:rgba(169,181,194,0.18); }"
        "QPushButton:pressed { background:rgba(169,181,194,0.26); }"
        "QPushButton:disabled { color:%2; border-color:rgba(138,150,163,0.25);"
        "  background:transparent; }"
        "QPushButton[accent=\"blue\"] { color:%3; border-color:rgba(88,166,255,0.55);"
        "  background:rgba(88,166,255,0.10); }"
        "QPushButton[accent=\"blue\"]:hover { background:rgba(88,166,255,0.22); }"
        "QPushButton[accent=\"blue\"]:disabled { color:%2; border-color:rgba(88,166,255,0.20);"
        "  background:transparent; }"
        "QPushButton[accent=\"red\"] { color:%4; border-color:rgba(248,81,73,0.55);"
        "  background:rgba(248,81,73,0.10); }"
        "QPushButton[accent=\"red\"]:hover { background:rgba(248,81,73,0.22); }"
        "QPushButton[accent=\"red\"]:disabled { color:%2; border-color:rgba(248,81,73,0.20);"
        "  background:transparent; }")
        .arg(Theme::TextSoft.name(), Theme::TextFaint.name(),
             Theme::BlueBright.name(), Theme::RedBright.name());
}

QString countChipStyle()
{
    return QStringLiteral(
        "color:%1; background:rgba(88,166,255,0.10); border:1px solid rgba(88,166,255,0.55);"
        "border-radius:9px; padding:3px 12px; font-weight:600;")
        .arg(Theme::BlueBright.name());
}

} // namespace

AdminDialog::AdminDialog(const QVector<Site> &sites, QWidget *parent)
    : QDialog(parent), m_sites(sites)
{
    setWindowTitle(QStringLiteral("360AirportDash — Site Administration"));
    setModal(true);
    setMinimumSize(880, 640);
    resize(920, 680);

    applyStyleSheet();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(10);

    // ---- header ----------------------------------------------------------
    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("SITE ADMINISTRATION"));
    title->setFont(Theme::sans(16, true, true));
    QPalette pal = title->palette();
    pal.setColor(QPalette::WindowText, Theme::Text);
    title->setPalette(pal);
    head->addWidget(title);
    head->addStretch(1);

    m_count = new QLabel;
    m_count->setObjectName(QStringLiteral("countLabel"));
    m_count->setFont(Theme::sans(11, true));
    m_count->setStyleSheet(countChipStyle());
    head->addWidget(m_count);
    root->addLayout(head);

    auto *subtitle = new QLabel(QStringLiteral("Watched-site roster · changes persist to %1")
                                    .arg(SiteRegistry::configPath()));
    subtitle->setFont(Theme::sans(10));
    pal = subtitle->palette();
    pal.setColor(QPalette::WindowText, Theme::TextFaint);
    subtitle->setPalette(pal);
    root->addWidget(subtitle);

    // ---- body: roster table | entry form ---------------------------------
    auto *body = new QHBoxLayout;
    body->setSpacing(12);
    root->addLayout(body, 1);

    // roster table (left)
    m_table = new QTableWidget;
    m_table->setObjectName(QStringLiteral("adminTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("NAME"), QStringLiteral("CODE"),
                                        QStringLiteral("REGION"), QStringLiteral("ENDPOINT")});
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setColumnWidth(1, 74);
    m_table->setColumnWidth(2, 120);
    m_table->setFont(Theme::sans(11));
    connect(m_table, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int) {
                if (row >= 0)
                    loadForm(row);
                updateButtons();
            });
    body->addWidget(m_table, 1);

    // entry form (right)
    auto *form = new QWidget;
    form->setObjectName(QStringLiteral("entryForm"));
    form->setFixedWidth(268);
    form->setStyleSheet(QStringLiteral(
        "#entryForm { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "#entryForm QLabel { border: none; background: transparent; }")
        .arg(Theme::PanelBg.name(), Theme::Border.name()));
    auto *flay = new QVBoxLayout(form);
    flay->setContentsMargins(14, 12, 14, 12);
    flay->setSpacing(7);

    auto mkCaption = [](const QString &t) {
        QLabel *l = new QLabel(t);
        l->setFont(Theme::sans(10, true));
        QPalette p = l->palette();
        p.setColor(QPalette::WindowText, Theme::TextMuted);
        l->setPalette(p);
        return l;
    };
    auto mkEdit = [this](const QString &name, const QString &ph, int maxLen) {
        QLineEdit *e = new QLineEdit;
        e->setObjectName(name);
        e->setFont(Theme::sans(11));
        e->setPlaceholderText(ph);
        e->setMaxLength(maxLen);
        e->setFrame(false);
        connect(e, &QLineEdit::returnPressed, this, &AdminDialog::onFormReturn);
        return e;
    };

    flay->addWidget(mkCaption(QStringLiteral("ENTRY FORM")));
    flay->addWidget(mkCaption(QStringLiteral("NAME")));
    m_name = mkEdit(QStringLiteral("nameEdit"), QStringLiteral("EDGE-FRANKFURT"), kNameMaxLen);
    flay->addWidget(m_name);
    auto *crRow = new QHBoxLayout;
    auto *crCol = new QVBoxLayout;
    crCol->setSpacing(7);
    crCol->addWidget(mkCaption(QStringLiteral("CODE")));
    m_code = mkEdit(QStringLiteral("codeEdit"), QStringLiteral("FRA1"), kCodeMaxLen);
    crCol->addWidget(m_code);
    crRow->addLayout(crCol, 1);
    auto *rgCol = new QVBoxLayout;
    rgCol->setSpacing(7);
    rgCol->addWidget(mkCaption(QStringLiteral("REGION")));
    m_region = mkEdit(QStringLiteral("regionEdit"), QStringLiteral("EU"), kRegionMaxLen);
    rgCol->addWidget(m_region);
    crRow->addLayout(rgCol, 1);
    flay->addLayout(crRow);
    flay->addWidget(mkCaption(QStringLiteral("ENDPOINT (FEED LOCATOR — URL, HOST:PORT, DEVICE ID…)")));
    m_endpoint = mkEdit(QStringLiteral("endpointEdit"),
                        QStringLiteral("https://example.com/api/v1/nodes/…"), kEndpointMaxLen);
    flay->addWidget(m_endpoint);

    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("errorLabel"));
    m_error->setFont(Theme::sans(10, true));
    m_error->setWordWrap(true);
    m_error->setFixedHeight(34);
    pal = m_error->palette();
    pal.setColor(QPalette::WindowText, Theme::RedBright);
    m_error->setPalette(pal);
    flay->addWidget(m_error);

    auto mkBtn = [this](const QString &name, const QString &text, const char *accent) {
        QPushButton *b = new QPushButton(text);
        b->setObjectName(name);
        b->setFont(Theme::sans(11, true));
        b->setCursor(Qt::PointingHandCursor);
        if (accent)
            b->setProperty("accent", QLatin1String(accent));
        return b;
    };

    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(8);
    m_addBtn = mkBtn(QStringLiteral("addButton"), QStringLiteral("ADD SITE"), "blue");
    m_updateBtn = mkBtn(QStringLiteral("updateButton"), QStringLiteral("UPDATE"), nullptr);
    addRow->addWidget(m_addBtn, 1);
    addRow->addWidget(m_updateBtn, 1);
    flay->addLayout(addRow);
    m_removeBtn = mkBtn(QStringLiteral("removeButton"), QStringLiteral("REMOVE SITE"), "red");
    flay->addWidget(m_removeBtn);
    flay->addStretch(1);
    flay->addWidget(mkCaption(QStringLiteral("ADD APPENDS · UPDATE EDITS THE SELECTED ROW · ENTER COMMITS")));
    body->addWidget(form);

    // ---- roster row: reorder + reset -------------------------------------
    auto *rosterRow = new QHBoxLayout;
    rosterRow->setSpacing(8);
    m_upBtn = mkBtn(QStringLiteral("upButton"), QStringLiteral("MOVE UP"), nullptr);
    m_downBtn = mkBtn(QStringLiteral("downButton"), QStringLiteral("MOVE DOWN"), nullptr);
    m_resetBtn = mkBtn(QStringLiteral("resetButton"), QStringLiteral("RESET DEFAULTS"), "red");
    rosterRow->addWidget(m_upBtn);
    rosterRow->addWidget(m_downBtn);
    rosterRow->addStretch(1);
    rosterRow->addWidget(m_resetBtn);
    root->addLayout(rosterRow);

    // ---- bottom: cancel / save -------------------------------------------
    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(8);
    bottom->addStretch(1);
    m_cancelBtn = mkBtn(QStringLiteral("cancelButton"), QStringLiteral("CANCEL"), nullptr);
    m_saveBtn = mkBtn(QStringLiteral("saveButton"), QStringLiteral("SAVE ROSTER"), "blue");
    m_saveBtn->setDefault(true);
    bottom->addWidget(m_cancelBtn);
    bottom->addWidget(m_saveBtn);
    root->addLayout(bottom);

    connect(m_addBtn, &QPushButton::clicked, this, &AdminDialog::onAdd);
    connect(m_updateBtn, &QPushButton::clicked, this, &AdminDialog::onUpdate);
    connect(m_removeBtn, &QPushButton::clicked, this, &AdminDialog::onRemove);
    connect(m_upBtn, &QPushButton::clicked, this, &AdminDialog::onMoveUp);
    connect(m_downBtn, &QPushButton::clicked, this, &AdminDialog::onMoveDown);
    connect(m_resetBtn, &QPushButton::clicked, this, &AdminDialog::onResetDefaults);
    connect(m_saveBtn, &QPushButton::clicked, this, &AdminDialog::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    rebuildTable(m_sites.isEmpty() ? -1 : 0);
    if (!m_sites.isEmpty())
        loadForm(0);
    m_name->setFocus();
    updateButtons();
}

void AdminDialog::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(
        "AdminDialog { background: %1; }"
        "QLabel { border: none; background: transparent; }"
        "QTableWidget { background: %2; border: 1px solid %3; border-radius: 8px;"
        "  gridline-color: %4; color: %5; selection-background-color: rgba(88,166,255,0.16);"
        "  selection-color: %6; alternate-background-color: %7; }"
        "QTableWidget::item { padding: 4px 8px; border: none; }"
        "QTableWidget::item:selected { color: %6; }"
        "QHeaderView::section { background: %8; color: %9; border: none;"
        "  border-bottom: 1px solid %3; padding: 7px 8px; font-weight: 600; }"
        "QLineEdit { background: %2; border: 1px solid %3; border-radius: 6px;"
        "  padding: 6px 8px; color: %6; selection-background-color: %10; }"
        "QLineEdit:focus { border: 1px solid %10; }"
        "%11"
        "QMessageBox { background: %1; }"
        "QMessageBox QLabel { color: %5; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(Theme::Bg.name(), Theme::BgDeep.name(), Theme::Border.name(),
             Theme::GridSoft.name(), Theme::TextSoft.name(), Theme::Text.name(),
             Theme::PanelBg.name(), Theme::PanelBgHi.name(), Theme::TextMuted.name(),
             Theme::Blue.name(), baseButtonStyle()));
}

void AdminDialog::rebuildTable(int selectRow)
{
    m_table->setRowCount(m_sites.size());
    for (int r = 0; r < m_sites.size(); ++r) {
        const Site &s = m_sites[r];
        const auto mk = [](const QString &t) {
            QTableWidgetItem *it = new QTableWidgetItem(t);
            it->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            it->setFont(Theme::sans(11));
            return it;
        };
        m_table->setItem(r, 0, mk(s.name));
        m_table->setItem(r, 1, mk(s.code));
        m_table->setItem(r, 2, mk(s.region));
        m_table->setItem(r, 3, mk(s.endpoint));
    }
    m_count->setText(QStringLiteral("%1 SITES").arg(m_sites.size()));
    m_table->setCurrentCell(qBound(-1, selectRow, m_sites.size() - 1), 0);
    updateButtons();
}

void AdminDialog::loadForm(int row)
{
    if (row < 0 || row >= m_sites.size())
        return;
    const Site &s = m_sites[row];
    m_name->setText(s.name);
    m_code->setText(s.code);
    m_region->setText(s.region);
    m_endpoint->setText(s.endpoint);
    m_error->clear();
}

bool AdminDialog::codeTaken(const QString &code, int exceptRow) const
{
    for (int i = 0; i < m_sites.size(); ++i)
        if (i != exceptRow && m_sites[i].code == code)
            return true;
    return false;
}

bool AdminDialog::formToSite(Site *out, QString *err) const
{
    const QString name = m_name->text().trimmed().toUpper();
    const QString code = m_code->text().trimmed().toUpper();
    const QString region = m_region->text().trimmed().toUpper();
    const QString endpoint = m_endpoint->text().trimmed();

    if (name.isEmpty()) { *err = QStringLiteral("NAME REQUIRED"); return false; }
    if (name.size() > kNameMaxLen) { *err = QStringLiteral("NAME TOO LONG"); return false; }
    if (code.isEmpty()) { *err = QStringLiteral("CODE REQUIRED"); return false; }
    if (code.size() > kCodeMaxLen) { *err = QStringLiteral("CODE TOO LONG"); return false; }
    if (region.size() > kRegionMaxLen) { *err = QStringLiteral("REGION TOO LONG"); return false; }

    out->name = name;
    out->code = code;
    out->region = region;
    out->endpoint = endpoint;
    return true;
}

void AdminDialog::onAdd()
{
    Site s;
    QString err;
    if (!formToSite(&s, &err)) {
        flashError(err);
        return;
    }
    if (codeTaken(s.code, -1)) {
        flashError(QStringLiteral("CODE %1 ALREADY IN ROSTER").arg(s.code));
        return;
    }
    m_sites.push_back(s);
    rebuildTable(m_sites.size() - 1);
    flashError(QString());
    m_name->setFocus();
    m_name->selectAll();
}

void AdminDialog::onUpdate()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_sites.size())
        return;
    Site s;
    QString err;
    if (!formToSite(&s, &err)) {
        flashError(err);
        return;
    }
    if (codeTaken(s.code, row)) {
        flashError(QStringLiteral("CODE %1 ALREADY IN ROSTER").arg(s.code));
        return;
    }
    m_sites[row] = s;
    rebuildTable(row);
    flashError(QString());
}

void AdminDialog::onRemove()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_sites.size())
        return;
    const int next = qMin(row, m_sites.size() - 2);
    m_sites.removeAt(row);
    rebuildTable(next);
    flashError(QString());
}

void AdminDialog::onMoveUp()
{
    const int row = m_table->currentRow();
    if (row <= 0 || row >= m_sites.size())
        return;
    m_sites.swapItemsAt(row, row - 1);
    rebuildTable(row - 1);
}

void AdminDialog::onMoveDown()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_sites.size() - 1)
        return;
    m_sites.swapItemsAt(row, row + 1);
    rebuildTable(row + 1);
}

void AdminDialog::onResetDefaults()
{
    if (QMessageBox::question(this, QStringLiteral("RESET ROSTER"),
                              QStringLiteral("Restore the built-in example roster?\n"
                                             "Current roster and unsaved edits will be lost."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;
    m_sites = siteList();
    rebuildTable(0);
    loadForm(0);
    flashError(QString());
}

void AdminDialog::onFormReturn()
{
    // Enter commits the form: update the selected row when its code matches,
    // otherwise append a new site.
    const int row = m_table->currentRow();
    if (row >= 0 && row < m_sites.size()
        && m_sites[row].code == m_code->text().trimmed().toUpper())
        onUpdate();
    else
        onAdd();
}

void AdminDialog::onSave()
{
    if (m_sites.isEmpty()) {
        flashError(QStringLiteral("ROSTER NEEDS AT LEAST ONE SITE"));
        return;
    }
    accept();
}

void AdminDialog::flashError(const QString &msg)
{
    m_error->setText(msg);
}

void AdminDialog::updateButtons()
{
    const int row = m_table->currentRow();
    const bool has = row >= 0 && row < m_sites.size();
    m_updateBtn->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_upBtn->setEnabled(has && row > 0);
    m_downBtn->setEnabled(has && row < m_sites.size() - 1);
    m_saveBtn->setEnabled(!m_sites.isEmpty());
}
