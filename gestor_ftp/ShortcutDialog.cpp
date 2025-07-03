#include "ShortcutDialog.h"
#include <QApplication>
#include <QMessageBox>

ShortcutDialog::ShortcutDialog(ShortcutManager *manager, QWidget *parent)
    : QDialog(parent), m_shortcutManager(manager)
{
    setWindowTitle(tr("Editar Atajos de Teclado"));
    setupUi();
    loadShortcuts();
    resize(550, 400);
}

ShortcutDialog::~ShortcutDialog()
{
}

void ShortcutDialog::setupUi()
{
    // Crear tabla de atajos
    m_tableShortcuts = new QTableWidget(this);
    m_tableShortcuts->setColumnCount(3);
    m_tableShortcuts->setHorizontalHeaderLabels(QStringList() << tr("Acción") << tr("Atajo Actual") << tr("Nuevo Atajo"));
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableShortcuts->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableShortcuts->verticalHeader()->setVisible(false);
    m_tableShortcuts->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Crear botones
    m_btnOk = new QPushButton(tr("Aceptar"), this);
    m_btnCancel = new QPushButton(tr("Cancelar"), this);
    m_btnReset = new QPushButton(tr("Restablecer Valores Predeterminados"), this);

    // Conectar señales
    connect(m_btnOk, &QPushButton::clicked, this, &ShortcutDialog::acceptChanges);
    connect(m_btnCancel, &QPushButton::clicked, this, &ShortcutDialog::rejectChanges);
    connect(m_btnReset, &QPushButton::clicked, this, &ShortcutDialog::resetToDefaults);

    // Crear layout de botones
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_btnReset);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_btnCancel);
    buttonLayout->addWidget(m_btnOk);

    // Crear layout principal
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(new QLabel(tr("Configure los atajos de teclado para las acciones disponibles:"), this));
    mainLayout->addWidget(m_tableShortcuts);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void ShortcutDialog::loadShortcuts()
{
    // Limpiar tabla
    m_tableShortcuts->clearContents();
    m_rowToActionMap.clear();
    m_editedShortcuts.clear();

    // Obtener acciones registradas
    QMap<QString, QAction*> actions = m_shortcutManager->getRegisteredActions();
    
    // Establecer número de filas
    m_tableShortcuts->setRowCount(actions.size());

    // Llenar tabla
    int row = 0;
    for (auto it = actions.begin(); it != actions.end(); ++it, ++row) {
        QString actionName = it.key();
        QAction *action = it.value();

        // Guardar mapeo de fila a nombre de acción
        m_rowToActionMap[row] = actionName;

        // Agregar nombre de la acción
        QTableWidgetItem *nameItem = new QTableWidgetItem(action->text().remove('&'));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_tableShortcuts->setItem(row, 0, nameItem);

        // Agregar atajo actual
        QTableWidgetItem *shortcutItem = new QTableWidgetItem(action->shortcut().toString());
        shortcutItem->setFlags(shortcutItem->flags() & ~Qt::ItemIsEditable);
        m_tableShortcuts->setItem(row, 1, shortcutItem);

        // Agregar editor de atajo
        QKeySequenceEdit *keyEdit = new QKeySequenceEdit(action->shortcut(), this);
        m_tableShortcuts->setCellWidget(row, 2, keyEdit);
        
        // Conectar señal de cambio de atajo
        connect(keyEdit, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutDialog::shortcutChanged);
        
        // Almacenar atajo original para detectar cambios
        m_editedShortcuts[actionName] = action->shortcut();
    }
}

void ShortcutDialog::checkForConflicts(int row, const QKeySequence &sequence)
{
    if (sequence.isEmpty()) {
        return;
    }

    QString currentAction = m_rowToActionMap[row];
    
    // Comprobar conflictos con otros atajos
    for (int i = 0; i < m_tableShortcuts->rowCount(); ++i) {
        if (i == row) {
            continue;
        }
        
        QString actionName = m_rowToActionMap[i];
        QKeySequence otherSequence = m_editedShortcuts[actionName];
        
        if (!otherSequence.isEmpty() && sequence == otherSequence) {
            QMessageBox::warning(this, tr("Conflicto de Atajo"),
                tr("El atajo '%1' ya está asignado a la acción '%2'.")
                .arg(sequence.toString())
                .arg(m_tableShortcuts->item(i, 0)->text()));
            
            // Restablecer el editor
            QKeySequenceEdit *keyEdit = qobject_cast<QKeySequenceEdit*>(m_tableShortcuts->cellWidget(row, 2));
            if (keyEdit) {
                keyEdit->setKeySequence(m_editedShortcuts[currentAction]);
            }
            return;
        }
    }
    
    // Actualizar el atajo editado
    m_editedShortcuts[currentAction] = sequence;
}

void ShortcutDialog::shortcutChanged(const QKeySequence &sequence)
{
    // Encontrar la fila del editor que envió la señal
    QKeySequenceEdit *sender = qobject_cast<QKeySequenceEdit*>(QObject::sender());
    if (!sender) {
        return;
    }
    
    for (int row = 0; row < m_tableShortcuts->rowCount(); ++row) {
        if (m_tableShortcuts->cellWidget(row, 2) == sender) {
            checkForConflicts(row, sequence);
            break;
        }
    }
}

void ShortcutDialog::acceptChanges()
{
    // Aplicar todos los atajos editados
    for (int row = 0; row < m_tableShortcuts->rowCount(); ++row) {
        QString actionName = m_rowToActionMap[row];
        QKeySequenceEdit *keyEdit = qobject_cast<QKeySequenceEdit*>(m_tableShortcuts->cellWidget(row, 2));
        
        if (keyEdit) {
            m_shortcutManager->setShortcut(actionName, keyEdit->keySequence());
        }
    }
    
    // Guardar cambios
    m_shortcutManager->saveShortcuts();
    
    accept();
}

void ShortcutDialog::rejectChanges()
{
    reject();
}

void ShortcutDialog::resetToDefaults()
{
    if (QMessageBox::question(this, tr("Restablecer Atajos"),
                             tr("¿Está seguro de que desea restablecer todos los atajos a sus valores predeterminados?"),
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_shortcutManager->resetAllShortcuts();
        loadShortcuts();
    }
}
