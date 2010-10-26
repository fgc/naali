/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EntityActionDialog.cpp
 *  @brief  Dialog for invoking entity actions.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "EntityActionDialog.h"

#include "Entity.h"

#include "MemoryLeakCheck.h"


EntityActionDialog::EntityActionDialog(const QList<Scene::EntityWeakPtr> &entities, QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f)
{
    this->entities = entities;

    setAttribute(Qt::WA_DeleteOnClose);
    if (graphicsProxyWidget())
        graphicsProxyWidget()->setWindowTitle(tr("Trigger Entity Action"));

    setWindowTitle(tr("Trigger Entity Action"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5,5,5,5);
    mainLayout->setSpacing(6);

    QSet<QString> actions;
    QString targets;
    assert(entities.size());
    if (entities.size() == 1)
        targets.append(tr("Target: "));
    else
        targets.append(tr("Targets: "));

    for(int i = 0; i < entities.size(); ++i)
    {
        Scene::EntityPtr e = entities[i].lock();
        if (e)
        {
            targets.append(QString::number(e->GetId()));
            if (i < entities.size() - 1)
                targets.append(", ");

            foreach(EntityAction *act, e->Actions())
                actions.insert(act->Name());
        }
    }

    QLabel *targetsLabel = new QLabel(targets);

    actionComboBox = new QComboBox;
    actionComboBox->setEditable(true);
    actionComboBox->addItems(actions.toList());

    QLabel *actionLabel = new QLabel(tr("Action: "));
    QLabel *executionTypeLabel = new QLabel(tr("Execution type:"));
    QLabel *parametersLabel = new QLabel(tr("Parameters (use comma as separator):"), this);
    parametersLineEdit = new QLineEdit;

    localCheckBox = new QCheckBox(tr("Local"));
    serverComboBox = new QCheckBox(tr("Server"));
    peersComboBox = new QCheckBox(tr("Peers"));
    localCheckBox->setChecked(true);

    connect(localCheckBox, SIGNAL(toggled(bool)), SLOT(CheckExecuteAccepted()));
    connect(serverComboBox, SIGNAL(toggled(bool)), SLOT(CheckExecuteAccepted()));
    connect(peersComboBox, SIGNAL(toggled(bool)), SLOT(CheckExecuteAccepted()));

    mainLayout->addWidget(targetsLabel);
    mainLayout->addWidget(actionLabel);
    mainLayout->addWidget(actionComboBox);
    mainLayout->addWidget(parametersLabel);
    mainLayout->addWidget(parametersLineEdit);

    QHBoxLayout *checkBoxLayout = new QHBoxLayout;
    checkBoxLayout->addWidget(executionTypeLabel);
    checkBoxLayout->addWidget(localCheckBox);
    checkBoxLayout->addWidget(serverComboBox);
    checkBoxLayout->addWidget(peersComboBox);
    mainLayout->insertLayout(-1, checkBoxLayout);

    QSpacerItem *spacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
    mainLayout->insertSpacerItem(-1, spacer);

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    QSpacerItem *buttonSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonsLayout->addSpacerItem(buttonSpacer);
    mainLayout->insertLayout(-1, buttonsLayout);

    execButton = new QPushButton(tr("Execute"));
    execAndCloseButton = new QPushButton(tr("Execute and Close"));
    QPushButton *closeButton = new QPushButton(tr("Close"));

    connect(execButton, SIGNAL(clicked()), SLOT(Execute()));
    connect(execAndCloseButton, SIGNAL(clicked()), SLOT(accept()));
    connect(closeButton, SIGNAL(clicked()), SLOT(reject()));

    buttonsLayout->addWidget(execButton);
    buttonsLayout->addWidget(execAndCloseButton);
    buttonsLayout->addWidget(closeButton);
}

EntityActionDialog::~EntityActionDialog()
{
}

QList<Scene::EntityWeakPtr> EntityActionDialog::Entities() const
{
    return entities;
}

    QString EntityActionDialog::Action() const
{
    return actionComboBox->currentText();
}

EntityAction::ExecutionTypeField EntityActionDialog::ExecutionType() const
{
    EntityAction::ExecutionTypeField type = EntityAction::Invalid;
    if (localCheckBox->isChecked())
        type |=  EntityAction::Local;
    if (serverComboBox->isChecked())
        type |=  EntityAction::Server;
    if (peersComboBox->isChecked())
        type |=  EntityAction::Peers;

    return type;
}

QStringList EntityActionDialog::Parameters() const
{
    QString params = parametersLineEdit->text();
    params = params.trimmed();
    return params.split(',');
}

void EntityActionDialog::hideEvent(QHideEvent *)
{
    close();
}

void EntityActionDialog::CheckExecuteAccepted()
{
    // If any of the execution type check boxes are not checked, disable exec buttons.
    if (!localCheckBox->isChecked() && !serverComboBox->isChecked() && !peersComboBox->isChecked())
    {
        execButton->setEnabled(false);
        execAndCloseButton->setEnabled(false);
    }
    else
    {
        execButton->setEnabled(true);
        execAndCloseButton->setEnabled(true);
    }
}

void EntityActionDialog::Execute()
{
    emit finished(2);
}