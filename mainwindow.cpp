#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow){
    ui->setupUi(this);
    setWindowTitle(QCoreApplication::applicationName() + " v" + QCoreApplication::applicationVersion() + " alpha");
    CoresspondingTypes["WRITE"]   = write;
    CoresspondingTypes["READ"]    = read;
    CoresspondingTypes["NIREAD"]  = nonIncrementingRead;
    CoresspondingTypes["NIWRITE"] = nonIncrementingWrite;
    CoresspondingTypes["RMWSUM"]  = RMWsum;
    CoresspondingTypes["RMWBITS"] = RMWbits;
    socket = new QUdpSocket(this);
    //find all radioButtons to connect them with onr slot
    QList<QRadioButton*> radioButtons = ui->centralwidget->findChildren<QRadioButton*>(QRegularExpression("radioButton_*"));
    QRadioButton* but;
    foreach (but, radioButtons)
        connect(but, &QRadioButton::clicked, this, [but, this](){selectedTransactionChanged(CoresspondingTypes[but->objectName().remove("radioButton_")]);});
    connect(ui->pushButton_ADD, &QPushButton::clicked, ui->treeWidget_PACKET_WIEVER, [=](){
        //in case packet was sent recently we need to clear both trees
        if(!ui->treeWidget_PACKET_WIEVER->topLevelItem(0)){
            ui->pushButton_SEND->setEnabled(true);
            ui->treeWidget_PACKET_WIEVER->addIPbusPacketHeader();}
        const IPbusWord address = ui->lineEdit_ADDRESS->text().toUInt(nullptr, 16);
        const quint8     nWords = static_cast<quint8>(ui->lineEdit_NWORDS->text().toUInt(nullptr, 10));
        const IPbusWord andTerm = ui->lineEdit_ANDTERM->text().toUInt(nullptr, 16);
        const IPbusWord  orTerm = ui->lineEdit_ORTERM->text().toUInt(nullptr, 16);
        ui->treeWidget_PACKET_WIEVER->addIPbusTransaction(currentType, nWords, address, andTerm, orTerm);
        nWordsChanged();});
    //progress bar processing
    connect(ui->treeWidget_PACKET_WIEVER, &packetViewer::wordsAmountChanged, this, &MainWindow::packetSizeChanged);
    //reaction on nWords changing as packet size is restricted by MTU
    connect(ui->lineEdit_NWORDS, &QLineEdit::textEdited, this, &MainWindow::nWordsChanged);
    //processing of the response from server to see it in tree widget
    connect(socket, &QUdpSocket::readyRead, this, &MainWindow::getResponse);
    //building packet after send button pushed
    connect(ui->pushButton_SEND, &QPushButton::clicked, this, &MainWindow::sendPacket);
    //clear outputs
    connect(ui->pushButton_CLEAR, &QPushButton::clicked, this, &MainWindow::clear);

    ui->lineEdit_NWORDS->setValidator(new QRegExpValidator(QRegExp("[1-9]|[1-9][0-9]|1[0-9]{1,2}|2[0-4][0-9]|25[0-5]")));
    //validator for IP address
    ui->lineEdit_IPADDRESS->setValidator(new QRegExpValidator(IP));
    //installation of eventFilter for delete button
    ui->treeWidget_PACKET_WIEVER->installEventFilter(this);

    //resize width of coloumns in appropriate form in the beginning of the program
    ui->treeWidget_PACKET_WIEVER->header()->resizeSection(0, 380);
    ui->treeWidget_RESPONSE->header()->resizeSection(0, 380);
    ui->treeWidget_PACKET_WIEVER->header()->resizeSection(1, 40);
    ui->treeWidget_RESPONSE->header()->resizeSection(1, 40);
    ui->treeWidget_PACKET_WIEVER->header()->resizeSection(2, 40);
    ui->treeWidget_RESPONSE->header()->resizeSection(2, 40);

    //as th window is empty -> nothing to send
    ui->pushButton_SEND->setEnabled(false);

    //setting font for bars -- kostyl
    ui->progressBar_WORDS->setFont(QFont("FranklinGothic", 12));
    ui->progressBar_WORDS_EXPECTED->setFont(QFont("FranklinGothic", 12));

    //setting OR term nd AND term unabled, as in the start of program read transaction - default
    ui->lineEdit_ORTERM->setEnabled(false);
    ui->lineEdit_ANDTERM->setEnabled(false);
}

bool MainWindow::eventFilter(QObject * obj, QEvent* event){
    if (obj == ui->treeWidget_PACKET_WIEVER){
        if (event->type() == QEvent::KeyPress){
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Delete){
                if(ui->treeWidget_PACKET_WIEVER->itemAbove(ui->treeWidget_PACKET_WIEVER->currentItem()) && !ui->treeWidget_PACKET_WIEVER->currentItem()->parent()){
                    for(quint16 i = 0; i < ui->treeWidget_PACKET_WIEVER->currentItem()->childCount(); ++i)
                        delete ui->treeWidget_PACKET_WIEVER->currentItem()->child(i);
                    if(ui->treeWidget_PACKET_WIEVER->transactionsAmount() == 1){
                        delete ui->treeWidget_PACKET_WIEVER->topLevelItem(0);
                        ui->pushButton_SEND->setEnabled(false);
                    }
                    delete ui->treeWidget_PACKET_WIEVER->currentItem();
                    ui->treeWidget_PACKET_WIEVER->reinit();
                    nWordsChanged();
                    return true;
                }else{
                    return false;
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::selectedTransactionChanged(const TransactionType type){
    currentType = type;
    switch (type) {
    case write:
    case read:
    case nonIncrementingRead:
    case nonIncrementingWrite:{
        ui->lineEdit_ORTERM->setEnabled(false);
        ui->lineEdit_ANDTERM->setEnabled(false);
        ui->lineEdit_NWORDS->setEnabled(true);
        break;
    }
    case RMWsum:{
        ui->lineEdit_ANDTERM->setEnabled(true);
        ui->lineEdit_ORTERM->setEnabled(false);
        ui->lineEdit_NWORDS->setText("1");
        ui->lineEdit_NWORDS->setEnabled(false);
        ui->label_ANDTERM->setText("ADDEND");
        break;
    }
    case RMWbits:{
        ui->lineEdit_ANDTERM->setEnabled(true);
        ui->lineEdit_ORTERM->setEnabled(true);
        ui->lineEdit_NWORDS->setText("1");
        ui->lineEdit_NWORDS->setEnabled(false);
        ui->label_ANDTERM->setText("AND term");
        break;
    }
    default:
        break;
    }
    //checking if packet is possible to send
    nWordsChanged();
}

void MainWindow::packetSizeChanged(){
    const counter newAmount = ui->treeWidget_PACKET_WIEVER->packetSize(), expectedAmount = ui->treeWidget_PACKET_WIEVER->expextedResponseSize();
    QProgressBar* request = ui->progressBar_WORDS, *response = ui->progressBar_WORDS_EXPECTED;
    changeProgressBar(request, newAmount);
    changeProgressBar(response, expectedAmount);
}

void MainWindow::changeProgressBar(QProgressBar * const bar, const quint16 value){
    if(value < 111)
        bar->setStyleSheet(" QProgressBar::chunk {background-color: rgb( 67,255, 76);}");
    if(value > 110 && value < 367)
        bar->setStyleSheet(" QProgressBar::chunk {background-color: rgb(247,255,149);}");
    if(value > 366)
        bar->setStyleSheet(" QProgressBar::chunk {background-color: rgb(231, 88,129);}");
    bar->setValue(value);
}

void MainWindow::nWordsChanged(){
    const counter currentFreeSpaceRequest = maxWordsPerPacket - ui->treeWidget_PACKET_WIEVER->packetSize(),
                  currentFreeSpaceResponse = maxWordsPerPacket - ui->treeWidget_PACKET_WIEVER->expextedResponseSize();
    const quint8 currentNWords = static_cast<quint8>(ui->lineEdit_NWORDS->text().toUInt());
    bool addButtonEnabled = true;
    switch (currentType) {
    case read:
    case nonIncrementingRead:
        addButtonEnabled = currentFreeSpaceRequest >= 2 && currentFreeSpaceResponse >= 1 + currentNWords;
        break;
    case write:
    case nonIncrementingWrite:
        addButtonEnabled = currentFreeSpaceRequest >= 2 + currentNWords && currentFreeSpaceResponse >= 1;
        break;
    case RMWsum:
        addButtonEnabled = currentFreeSpaceRequest >= 3 && currentFreeSpaceResponse >= 2;
        break;
    case RMWbits:
        addButtonEnabled = currentFreeSpaceRequest >= 4 && currentFreeSpaceResponse >= 2;
        break;
    default: break;
    }
    ui->pushButton_ADD->setEnabled(addButtonEnabled);
}

void MainWindow::sendPacket(){
    ui->treeWidget_RESPONSE->clear();
    //Declaration of the variables numWord - counter of words in packet, transactionCounter - counter of transactions in the packet
    quint16 numWord = 0, transactionCounter = 0;
    //DInitialisation of requestViewer will let me write less code to apple QTreeWidget functions
    packetViewer* requestViewer = ui->treeWidget_PACKET_WIEVER;
    //First element in packet is Packet header/ It is getting from the first child of the first item in tree
    request[numWord++] = requestViewer->topLevelItem(transactionCounter++)->child(0)->text(0).toUInt(nullptr, 16);
    //while words amount in packet less than the building packet size
    while(numWord < requestViewer->packetSize()){
        //we will get top level items from tree, which correspond to every transaction
        QTreeWidgetItem* parentTransaction = requestViewer->topLevelItem(transactionCounter++);
        //the body (children) of every transaction will be placed in packet
        for(quint16 i = 0; i < parentTransaction->childCount(); ++i){
            request[numWord++] = parentTransaction->child(i)->text(0).toUInt(nullptr, 16);
            //and we need to set them uneditable after sending, as we want user to check what he wanted and what he got
            parentTransaction->child(i)->setFlags(i == 0 ? Qt::NoItemFlags : Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
        //and parent are not dragable anymore
        parentTransaction->setFlags(Qt::ItemIsEnabled);
    }
    //After our packet was filled we send it to server
    if(ui->lineEdit_IPADDRESS->text().contains(IP))
        socket->writeDatagram(Crequest, requestViewer->packetSize() * sizeof (IPbusWord), QHostAddress(ui->lineEdit_IPADDRESS->text()), 50001);
    else ui->statusbar->showMessage("No such address");
    ui->treeWidget_PACKET_WIEVER->reinit();
    nWordsChanged();
}

void MainWindow::getResponse(){
    //when we have pending dataram
    if(socket->hasPendingDatagrams()){
        //we get response size, to get coressponding amount of bytes from the packet, which we got
        quint16 responseSize = static_cast<quint16>(socket->pendingDatagramSize());
        //getting that bytes in char array Cresponse
        socket->readDatagram(Cresponse, responseSize);
        //new packet should be displayed, so it's neccesarry to clear response viewer
        ui->treeWidget_RESPONSE->clear();
        //call function, which will display response to user
        ui->treeWidget_RESPONSE->displayResponse(response, responseSize / sizeof (IPbusWord));
    }
}

void MainWindow::clear(){
    ui->treeWidget_PACKET_WIEVER->clear();
    ui->treeWidget_RESPONSE->clear();
    ui->treeWidget_PACKET_WIEVER->reinit();
    nWordsChanged();
    ui->pushButton_SEND->setEnabled(false);
}

