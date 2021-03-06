// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "AddComponentDialog.h"
#include "Framework.h"
#include "Scene.h"
#include "SceneAPI.h"
#include "Entity.h"

#include <QLayout>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "MemoryLeakCheck.h"

AddComponentDialog::AddComponentDialog(Framework *fw, const QList<entity_id_t> &ids, QWidget *parent, Qt::WindowFlags f):
    QDialog(parent, f),
    framework_(fw),
    entities_(ids),
    name_line_edit_(0),
    type_combo_box_(0),
    ok_button_(0),
    cancel_button_(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);
    setStyleSheet("font-size: 9pt;");
    setWindowTitle(tr("Add New Component"));
    if (graphicsProxyWidget())
        graphicsProxyWidget()->setWindowTitle(windowTitle());

    // Create widgets
    QLabel *component_type_label = new QLabel(tr("Component"), this);
    QLabel *component_name_label = new QLabel(tr("Name"), this);
    QLabel *component_sync_label = new QLabel(tr("Local"), this);
    QLabel *component_temp_label = new QLabel(tr("Temporary"), this);
    component_temp_label->setMinimumWidth(70);

    errorLabel = new QLabel(this);
    errorLabel->setStyleSheet("QLabel { background-color: rgba(255,0,0,150); padding: 4px; border: 1px solid grey; }");
    errorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->hide();

    name_line_edit_ = new QLineEdit(this);
    name_line_edit_->setFocus(Qt::ActiveWindowFocusReason);
    name_line_edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    type_combo_box_ = new QComboBox(this);
    type_combo_box_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    sync_check_box_ = new QCheckBox(this);
    sync_check_box_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    temp_check_box_ = new QCheckBox(this);
    temp_check_box_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ok_button_ = new QPushButton(tr("Add"), this);
    ok_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ok_button_->setDefault(true);
    
    cancel_button_ = new QPushButton(tr("Cancel"), this);
    cancel_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    cancel_button_->setAutoDefault(false);

    // Layouts
    QGridLayout *grid = new QGridLayout();
    grid->setVerticalSpacing(8);
    grid->addWidget(component_name_label, 0, 0);
    grid->addWidget(name_line_edit_, 0, 1, Qt::AlignLeft, 1);
    grid->addWidget(component_type_label, 1, 0);
    grid->addWidget(type_combo_box_, 1, 1, Qt::AlignLeft, 1);
    grid->addWidget(component_sync_label, 2, 0);
    grid->addWidget(sync_check_box_, 2, 1);
    grid->addWidget(component_temp_label, 3, 0);
    grid->addWidget(temp_check_box_, 3, 1);

    QHBoxLayout *buttons_layout = new QHBoxLayout();
    buttons_layout->addWidget(ok_button_);
    buttons_layout->addWidget(cancel_button_);

    QVBoxLayout *vertLayout = new QVBoxLayout();

    if (entities_.size() > 1)
    {
        QLabel *labelCompCount = new QLabel(tr("Adding component to ") + QString::number(entities_.size()) + tr(" selected entities"), this);
        labelCompCount->setStyleSheet("QLabel { background-color: rgba(230,230,230,255); padding: 4px; border: 1px solid grey; }");
        labelCompCount->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        labelCompCount->setAlignment(Qt::AlignCenter);
        vertLayout->addWidget(labelCompCount);
    }

    vertLayout->addLayout(grid);
    vertLayout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Fixed, QSizePolicy::Expanding));
    vertLayout->addWidget(errorLabel);
    vertLayout->addLayout(buttons_layout);

    setLayout(vertLayout);
    resize(300, width() + 60);

    // Connect signals
    connect(name_line_edit_, SIGNAL(textChanged(const QString&)), this, SLOT(CheckComponentName()));
    connect(type_combo_box_, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(CheckComponentName()));
    connect(ok_button_, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancel_button_, SIGNAL(clicked()), this, SLOT(reject()));
    connect(sync_check_box_, SIGNAL(toggled(bool)), this, SLOT(CheckTempAndSync()));
    connect(temp_check_box_, SIGNAL(toggled(bool)), this, SLOT(CheckTempAndSync()));

    CheckTempAndSync();
}

AddComponentDialog::~AddComponentDialog()
{
}

void AddComponentDialog::SetComponentList(const QStringList &component_types)
{
    foreach(const QString &type, component_types)
        if (type.startsWith("EC_"))
            type_combo_box_->addItem(type.mid(3));
        else
            type_combo_box_->addItem(type);
}

void AddComponentDialog::SetComponentName(const QString &name)
{
    name_line_edit_->setText(name);
}

QString AddComponentDialog::GetTypeName() const
{
    if (!type_combo_box_->currentText().startsWith("EC_"))
        return "EC_" + type_combo_box_->currentText();
    else
        return type_combo_box_->currentText();
}

QString AddComponentDialog::GetName() const
{
    return name_line_edit_->text();
}

bool AddComponentDialog::GetSynchronization() const
{
    return !sync_check_box_->isChecked();
}

bool AddComponentDialog::GetTemporary() const
{
    return temp_check_box_->isChecked();
}

QList<entity_id_t> AddComponentDialog::GetEntityIds() const
{
    return entities_;
}

void AddComponentDialog::CheckComponentName()
{
    Scene *scene = framework_->Scene()->MainCameraScene();
    if (!scene)
        return;

    QString typeName = type_combo_box_->currentText();
    if (!typeName.startsWith("EC_"))
        typeName.prepend("EC_");
    QString compName = name_line_edit_->text().trimmed();

    bool nameDuplicates = false;
    for(uint i = 0; i < (uint)entities_.size(); i++)
    {
        EntityPtr entity = scene->GetEntity(entities_[i]);
        if (!entity)
            continue;
        if (entity->GetComponent(typeName, compName))
        {
            nameDuplicates = true;
            break;
        }
    }

    QString errorText = "";
    if (nameDuplicates)
    {
        errorText += type_combo_box_->currentText() + tr(" component with name ");
        errorText += compName.isEmpty() ? "<no name>" : "\"" + compName + "\"";
        errorText += tr(" already exists. Pick a unique name.");
    }

    ok_button_->setDisabled(nameDuplicates);
    errorLabel->setVisible(!errorText.isEmpty());
    errorLabel->setText(errorText);
    layout()->update();
}

void AddComponentDialog::CheckTempAndSync()
{
    sync_check_box_->setText(sync_check_box_->isChecked() ? "Creating as Local" : "Creating as Replicated");
    temp_check_box_->setText(temp_check_box_->isChecked() ? "Creating as Temporary" : " ");

    sync_check_box_->setStyleSheet(sync_check_box_->isChecked() ? "color: blue;" : "color: black;");
    temp_check_box_->setStyleSheet(temp_check_box_->isChecked() ? "color: red;" : "color: black;");
}

void AddComponentDialog::hideEvent(QHideEvent *event)
{
    deleteLater();
}
