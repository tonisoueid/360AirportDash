#pragma once

#include "SiteData.h"

#include <QDialog>
#include <QVector>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// ----------------------------------------------------------------------------
// In-app roster editor, opened from the header "SITE ADMIN" button or Ctrl+E.
// Works on a copy of the watched-site list: roster table on the left, entry
// form on the right (add / update / remove), move up/down for grid order, and
// reset-to-defaults. Accepting returns the edited list; the caller persists
// it through SiteRegistry. Entries are normalized (trimmed, identity fields
// upper-cased) and validated row-by-row; the table therefore only ever holds
// valid rows. This is the template's only stylesheet user — instrument
// surfaces stay custom-painted.
// ----------------------------------------------------------------------------
class AdminDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AdminDialog(const QVector<Site> &sites, QWidget *parent = nullptr);

    QVector<Site> sites() const { return m_sites; }

private slots:
    void onAdd();
    void onUpdate();
    void onRemove();
    void onMoveUp();
    void onMoveDown();
    void onResetDefaults();
    void onFormReturn();
    void onSave();

private:
    void rebuildTable(int selectRow = -1);
    void loadForm(int row);
    bool formToSite(Site *out, QString *err) const;
    bool codeTaken(const QString &code, int exceptRow) const;
    void flashError(const QString &msg);
    void updateButtons();
    void applyStyleSheet();

    QVector<Site> m_sites;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_code = nullptr;
    QLineEdit *m_region = nullptr;
    QLineEdit *m_endpoint = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_count = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_updateBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
    QPushButton *m_upBtn = nullptr;
    QPushButton *m_downBtn = nullptr;
    QPushButton *m_resetBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};
