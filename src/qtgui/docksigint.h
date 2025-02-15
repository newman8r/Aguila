#ifndef DOCKSIGINT_H
#define DOCKSIGINT_H

#include <QDockWidget>
#include <QSettings>

namespace Ui {
    class DockSigint;
}

/*! \brief Dock widget with SIGINT chat interface. */
class DockSigint : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockSigint(QWidget *parent = nullptr);
    ~DockSigint();

    void saveSettings(QSettings *settings);
    void readSettings(QSettings *settings);

signals:
    void messageSubmitted(const QString &message);

private slots:
    void onSendClicked();
    void onReturnPressed();

private:
    Ui::DockSigint *ui;
    void appendMessage(const QString &message, bool isUser = true);
};

#endif // DOCKSIGINT_H 