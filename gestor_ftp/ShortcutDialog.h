#ifndef SHORTCUTDIALOG_H
#define SHORTCUTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMap>
#include <QString>
#include <QKeySequence>
#include "ShortcutManager.h"

class ShortcutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutDialog(ShortcutManager *manager, QWidget *parent = nullptr);
    ~ShortcutDialog();

private slots:
    void acceptChanges();
    void rejectChanges();
    void resetToDefaults();
    void shortcutChanged(const QKeySequence &sequence);

private:
    void setupUi();
    void loadShortcuts();
    void checkForConflicts(int row, const QKeySequence &sequence);

    ShortcutManager *m_shortcutManager;
    QTableWidget *m_tableShortcuts;
    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;
    QPushButton *m_btnReset;
    QMap<int, QString> m_rowToActionMap;
    QMap<QString, QKeySequence> m_editedShortcuts;
};

#endif // SHORTCUTDIALOG_H
