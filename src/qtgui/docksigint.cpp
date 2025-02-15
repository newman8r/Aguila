#include <QDebug>
#include "docksigint.h"
#include "ui_docksigint.h"

DockSigint::DockSigint(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockSigint)
{
    ui->setupUi(this);

    // Connect signals/slots
    connect(ui->sendButton, &QPushButton::clicked, this, &DockSigint::onSendClicked);
    connect(ui->chatInput, &QLineEdit::returnPressed, this, &DockSigint::onReturnPressed);

    // Set initial welcome message
    ui->chatDisplay->append("<span style='color: #569cd6;'>Welcome to the SIGINT Chat Interface!</span>");
}

DockSigint::~DockSigint()
{
    delete ui;
}

void DockSigint::saveSettings(QSettings *settings)
{
    if (!settings)
        return;

    // Save any settings specific to the SIGINT panel
    settings->beginGroup("SIGINT");
    // Add settings as needed
    settings->endGroup();
}

void DockSigint::readSettings(QSettings *settings)
{
    if (!settings)
        return;

    // Read any settings specific to the SIGINT panel
    settings->beginGroup("SIGINT");
    // Add settings as needed
    settings->endGroup();
}

void DockSigint::onSendClicked()
{
    QString message = ui->chatInput->text().trimmed();
    if (!message.isEmpty()) {
        appendMessage(message);
        emit messageSubmitted(message);
        ui->chatInput->clear();
    }
}

void DockSigint::onReturnPressed()
{
    onSendClicked();
}

void DockSigint::appendMessage(const QString &message, bool isUser)
{
    QString formattedMessage;
    if (isUser) {
        formattedMessage = QString("<div style='margin: 5px;'><span style='color: #4ec9b0;'>User:</span> %1</div>")
                          .arg(message.toHtmlEscaped());
    } else {
        formattedMessage = QString("<div style='margin: 5px;'><span style='color: #569cd6;'>Assistant:</span> %1</div>")
                          .arg(message.toHtmlEscaped());
    }
    ui->chatDisplay->append(formattedMessage);
} 