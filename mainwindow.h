#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include "packetviewer.h"
#include "writedata.h"
#include <QtNetwork/QUdpSocket>

const QRegExp IP("(0?0?[1-9]|0?[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.(([01]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])\\.){2}(0?0?[1-9]|0?[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* , QEvent*);

private:
    QUdpSocket* socket;
    IPbusWord request[maxWordsPerPacket], response[maxWordsPerPacket];
    char* const Crequest = reinterpret_cast<char*>(request), * const Cresponse = reinterpret_cast<char*>(response);
    Ui::MainWindow *ui;
    QVector<quint32> writeData;
    TransactionType currentType = read;
    QHash<QString, TransactionType> CoresspondingTypes;
    //become true after sending packet
    bool sendFlag = false;

private slots:

    void selectedTransactionChanged(const TransactionType type);
    void packetSizeChanged();
    void changeProgressBar(QProgressBar* const bar, const quint16 value);
    void nWordsChanged();
    void sendPacket();
    void getResponse();
    void clear();
};
#endif // MAINWINDOW_H
