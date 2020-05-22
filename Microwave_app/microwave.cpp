#include "microwave.h"
#include "ui_microwave.h"

#include <QDebug>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QStateMachine>
#include <QState>
#include <QFinalState>
#include <QTimer>
#include <QDataStream>

namespace {
enum class Command : uint32_t {
    NONE            = 0x00000000,
    TIME_COOK       = 0x00000001,
    POWER_LEVEL     = 0x00000002,
    KITCHEN_TIMER   = 0x00000004,
    CLOCK           = 0x00000008,
    DIGIT_0         = 0x00000010,
    DIGIT_1         = 0x00000020,
    DIGIT_2         = 0x00000040,
    DIGIT_3         = 0x00000080,
    DIGIT_4         = 0x00000100,
    DIGIT_5         = 0x00000200,
    DIGIT_6         = 0x00000400,
    DIGIT_7         = 0x00000800,
    DIGIT_8         = 0x00001000,
    DIGIT_9         = 0x00002000,
    STOP            = 0x00004000,
    START           = 0x00008000,
    MOD_LEFT_TENS   = 0x00010000,
    MOD_LEFT_ONES   = 0x00020000,
    MOD_RIGHT_TENS  = 0x00040000,
    MOD_RIGHT_ONES  = 0x00080000,
    CURRENT_CLOCK   = 0x80000000
};

const quint16 APP_RECV_PORT {64000};
const quint16 SIM_RECV_PORT {64001};
const QHostAddress local {QHostAddress::LocalHost};
QByteArray buf{};
}


Microwave::Microwave(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Microwave)
    , inSocket{new QUdpSocket(this)}
    , outSocket{new QUdpSocket(this)}
    , blinkTimer{new QTimer(this)}
    , colonBlinkTimer{new QTimer(this)}
    , clockTime()
    , proposedClockTime()
    , countdownTime()
    , blinkage{false}
    , colon_blink{false}
    , setting_clock{false}
    , sm{new QStateMachine(this)}
    , display_clock{Q_NULLPTR}
    , set_cook_time{Q_NULLPTR}
    , set_power_level{Q_NULLPTR}
    , set_kitchen_timer{Q_NULLPTR}
    , display_timer{Q_NULLPTR}
{
    ui->setupUi(this);
    connect(ui->pb_timeCook, SIGNAL(clicked()),
            this, SLOT(sendTimeCook()));
    connect(ui->pb_powerLevel, SIGNAL(clicked()),
            this, SLOT(sendPowerLevel()));
    connect(ui->pb_kitchenTimer, SIGNAL(clicked()),
            this, SLOT(sendKitchenTimer()));
    connect(ui->pb_clock, SIGNAL(clicked()),
            this, SLOT(sendClock()));
    connect(ui->pb_0, SIGNAL(clicked()),
            this, SLOT(send0()));
    connect(ui->pb_1, SIGNAL(clicked()),
            this, SLOT(send1()));
    connect(ui->pb_2, SIGNAL(clicked()),
            this, SLOT(send2()));
    connect(ui->pb_3, SIGNAL(clicked()),
            this, SLOT(send3()));
    connect(ui->pb_4, SIGNAL(clicked()),
            this, SLOT(send4()));
    connect(ui->pb_5, SIGNAL(clicked()),
            this, SLOT(send5()));
    connect(ui->pb_6, SIGNAL(clicked()),
            this, SLOT(send6()));
    connect(ui->pb_7, SIGNAL(clicked()),
            this, SLOT(send7()));
    connect(ui->pb_8, SIGNAL(clicked()),
            this, SLOT(send8()));
    connect(ui->pb_9, SIGNAL(clicked()),
            this, SLOT(send9()));
    connect(ui->pb_stop, SIGNAL(clicked()),
            this, SLOT(sendStop()));
    connect(ui->pb_start, SIGNAL(clicked()),
            this, SLOT(sendStart()));

    inSocket->bind(local, APP_RECV_PORT);

    connect(inSocket, SIGNAL(readyRead()),
            this, SLOT(readDatagram()));

    connect(colonBlinkTimer, SIGNAL(timeout()),
            this, SLOT(blink_colon()));

    blinkTimer->setInterval(500);
    blinkTimer->setSingleShot(false);

    colonBlinkTimer->setInterval(500);
    colonBlinkTimer->setSingleShot(false);

    sm = new QStateMachine(this);
    display_clock = create_display_clock_state(sm);

    sm->setInitialState(display_clock);
    sm->start();
}

Microwave::~Microwave()
{
    delete ui;
}

QState* Microwave::create_display_clock_state(QState *parent)
{
    QState* display_clock {new QState(parent)};
    QState* display_clock_init {new QState(display_clock)};

    QState* set_clock {new QState(display_clock)};
    QState* set_clock_init {new QState(set_clock)};
    QState* select_hour_tens {new QState(set_clock)};
    QState* select_hour_ones {new QState(set_clock)};
    QState* select_minute_tens {new QState(set_clock)};
    QState* select_minute_ones {new QState(set_clock)};

    display_clock->setObjectName("display_clock");
    display_clock_init->setObjectName("init");
    set_clock->setObjectName("set_clock");
    set_clock_init->setObjectName("set_clock_init");
    select_hour_tens->setObjectName("select_hour_tens");
    select_hour_ones->setObjectName("select_hour_ones");
    select_minute_tens->setObjectName("select_minute_tens");
    select_minute_ones->setObjectName("select_minute_ones");

    //display_clock
    connect(display_clock, SIGNAL(entered()), this, SLOT(display_left()));
    connect(display_clock, SIGNAL(entered()), this, SLOT(display_right()));
    connect(display_clock, SIGNAL(entered()), this, SLOT(display_colon()));
    connect(display_clock, SIGNAL(entered()), this, SLOT(sendCurrentClockReq()));
    display_clock_init->addTransition(this, SIGNAL(clock_sig()), set_clock);

    //set_clock
    connect(set_clock, SIGNAL(entered()), this, SLOT(set_clock_entry()));
    connect(set_clock, SIGNAL(exited()), this, SLOT(set_clock_exit()));
    set_clock->setInitialState(set_clock_init);
    set_clock_init->addTransition(this, SIGNAL(select_left_tens_sig()), select_hour_tens);

    //select_hour_tens
    connect(select_hour_tens, SIGNAL(entered()), this, SLOT(select_hour_tens_entry()));
    connect(select_hour_tens, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_hour_tens, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_hour_tens, SIGNAL(exited()), this, SLOT(display_left()));
    connect(select_hour_tens, SIGNAL(exited()), this, SLOT(select_hour_tens_exit()));
    select_hour_tens->addTransition(this, SIGNAL(select_left_ones_sig()), select_hour_ones);

    //select_hour_ones
    connect(select_hour_ones, SIGNAL(entered()), this, SLOT(select_hour_ones_entry()));
    connect(select_hour_ones, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_hour_ones, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_hour_ones, SIGNAL(exited()), this, SLOT(display_left()));
    connect(select_hour_ones, SIGNAL(exited()), this, SLOT(select_hour_ones_exit()));
    select_hour_ones->addTransition(this, SIGNAL(select_right_tens_sig()), select_minute_tens);

    //select_minute_tens
    connect(select_minute_tens, SIGNAL(entered()), this, SLOT(select_minute_tens_entry()));
    connect(select_minute_tens, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_minute_tens, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_minute_tens, SIGNAL(exited()), this, SLOT(display_right()));
    connect(select_minute_tens, SIGNAL(exited()), this, SLOT(select_minute_tens_exit()));
    select_minute_tens->addTransition(this, SIGNAL(select_right_ones_sig()), select_minute_ones);

    //select_minute_ones
    connect(select_minute_ones, SIGNAL(entered()), this, SLOT(select_minute_ones_entry()));
    connect(select_minute_ones, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_minute_ones, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_minute_ones, SIGNAL(exited()), this, SLOT(display_right()));
    connect(select_minute_ones, SIGNAL(exited()), this, SLOT(select_minute_ones_exit()));
    select_minute_ones->addTransition(this, SIGNAL(select_left_tens_sig()), select_hour_tens);

    //transitions to finish exit set_clock
    set_clock->addTransition(this, SIGNAL(clock_done_sig()), display_clock);

    display_clock->setInitialState(display_clock_init);
    return display_clock;
}

QState* Microwave::create_set_cook_time_state(QState *parent)
{
    Q_UNUSED(parent);
    return new QState();
}

QState* Microwave::create_set_power_level_state(QState *parent)
{
    Q_UNUSED(parent);
    return new QState();
}

QState* Microwave::create_set_kitchen_timer_state(QState *parent)
{
    Q_UNUSED(parent);
    return new QState();
}

QState* Microwave::create_display_timer_state(QState *parent)
{
    Q_UNUSED(parent);
    return new QState();
}

void Microwave::set_clock_entry()
{
    qDebug() << "entered set_clock";
    if(!setting_clock) {
        connect(this, SIGNAL(clock_sig()), this, SLOT(accept_clock()));
        connect(this, SIGNAL(stop_sig()), this, SLOT(decline_clock()));
        proposedClockTime = clockTime;
        setting_clock = true;
    }
    if(colonBlinkTimer->isActive()) {
        colonBlinkTimer->stop();
    }
}

void Microwave::set_clock_exit()
{
    qDebug() << "left set_clock";
    if(!setting_clock) {
        disconnect(this, SIGNAL(clock_sig()), this, SLOT(accept_clock()));
        disconnect(this, SIGNAL(stop_sig()), this, SLOT(decline_clock()));
    }
}

void Microwave::select_hour_tens_entry()
{
    connect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_left_tens(quint32)));
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_tens()));
}

void Microwave::select_hour_tens_exit()
{
    disconnect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_left_tens(quint32)));
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_tens()));
}

void Microwave::select_hour_ones_entry()
{
    connect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_left_ones(quint32)));
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_ones()));
}

void Microwave::select_hour_ones_exit()
{
    disconnect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_left_ones(quint32)));
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_ones()));
}

void Microwave::select_minute_tens_entry()
{
    connect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_right_tens(quint32)));
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_tens()));
}

void Microwave::select_minute_tens_exit()
{
    disconnect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_right_tens(quint32)));
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_tens()));
}

void Microwave::select_minute_ones_entry()
{
    connect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_right_ones(quint32)));
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_ones()));
}

void Microwave::select_minute_ones_exit()
{
    disconnect(this, SIGNAL(digit_entered(quint32)), this, SLOT(update_right_ones(quint32)));
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_ones()));
}

void Microwave::readDatagram()
{
    while(inSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram {inSocket->receiveDatagram()};
        QByteArray rbuf {datagram.data()};
        Command data {Command::NONE};
        memcpy(&data, rbuf.data(), sizeof(data));

        qDebug() << "Data = 0x" << QString::number(static_cast<quint32>(data), 16);
        //now parse data
        switch(data) {
        case Command::TIME_COOK:
            emit cook_time_sig();
            break;
        case Command::POWER_LEVEL:
            emit power_level_sig();
            break;
        case Command::KITCHEN_TIMER:
            //emit set_kitchen_timer();
            break;
        case Command::CLOCK:
            emit clock_sig();
            break;
        case Command::DIGIT_1:
            emit digit_entered(1);
            break;
        case Command::DIGIT_2:
            emit digit_entered(2);
            break;
        case Command::DIGIT_3:
            emit digit_entered(3);
            break;
        case Command::DIGIT_4:
            emit digit_entered(4);
            break;
        case Command::DIGIT_5:
            emit digit_entered(5);
            break;
        case Command::DIGIT_6:
            emit digit_entered(6);
            break;
        case Command::DIGIT_7:
            emit digit_entered(7);
            break;
        case Command::DIGIT_8:
            emit digit_entered(8);
            break;
        case Command::DIGIT_9:
            emit digit_entered(9);
            break;
        case Command::DIGIT_0:
            emit digit_entered(0);
            break;
        case Command::STOP:
            emit stop_sig();
            break;
        case Command::START:
            emit start_sig();
            break;
        case Command::MOD_LEFT_TENS:
            emit select_left_tens_sig();
            break;
        case Command::MOD_LEFT_ONES:
            emit select_left_ones_sig();
            break;
        case Command::MOD_RIGHT_TENS:
            emit select_right_tens_sig();
            break;
        case Command::MOD_RIGHT_ONES:
            emit select_right_ones_sig();
            break;
        case Command::CURRENT_CLOCK:
            memcpy(&clockTime, rbuf.data() + sizeof(quint32), sizeof(Time));
            break;
        case Command::NONE:
        default:
            //do nothing
            break;
        }
    }
}

void Microwave::sendTimeCook()
{
    qDebug() << "time cook";
    Command cmd {Command::TIME_COOK};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendPowerLevel()
{
    qDebug() << "power level";
    Command cmd {Command::POWER_LEVEL};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendKitchenTimer()
{
    qDebug() << "kitchen timer";
    Command cmd {Command::KITCHEN_TIMER};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendClock()
{
    qDebug() << "clock";
    Command cmd {Command::CLOCK};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(cmd), local, SIM_RECV_PORT);
}

void Microwave::send0()
{
    qDebug() << "0";
    Command cmd {Command::DIGIT_0};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send1()
{
    qDebug() << "1";
    Command cmd {Command::DIGIT_1};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send2()
{
    qDebug() << "2";
    Command cmd {Command::DIGIT_2};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send3()
{
    qDebug() << "3";
    Command cmd {Command::DIGIT_3};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send4()
{
    qDebug() << "4";
    Command cmd {Command::DIGIT_4};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send5()
{
    qDebug() << "5";
    Command cmd {Command::DIGIT_5};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send6()
{
    qDebug() << "6";
    Command cmd {Command::DIGIT_6};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send7()
{
    qDebug() << "7";
    Command cmd {Command::DIGIT_7};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send8()
{
    qDebug() << "8";
    Command cmd {Command::DIGIT_8};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send9()
{
    qDebug() << "9";
    Command cmd {Command::DIGIT_9};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendStop()
{
    qDebug() << "stop";
    Command cmd {Command::STOP};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendStart()
{
    qDebug() << "start";
    Command cmd {Command::START};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendCurrentClockReq()
{
    Command cmd {Command::CURRENT_CLOCK};
    memcpy(buf.data(), &cmd, sizeof(cmd));

    outSocket->writeDatagram(buf.data(), sizeof(cmd), local, SIM_RECV_PORT);
}

void Microwave::display_left()
{
    if(setting_clock) {
        ui->left_tens->setText(QString::number(proposedClockTime.left_tens));
        ui->left_ones->setText(QString::number(proposedClockTime.left_ones));
    }
    else {
        ui->left_tens->setText(QString::number(clockTime.left_tens));
        ui->left_ones->setText(QString::number(clockTime.left_ones));
    }
    blinkage = true;
}

void Microwave::display_right()
{
    if(setting_clock) {
        ui->right_tens->setText(QString::number(proposedClockTime.right_tens));
        ui->right_ones->setText(QString::number(proposedClockTime.right_ones));
    }
    else {
        ui->right_tens->setText(QString::number(clockTime.right_tens));
        ui->right_ones->setText(QString::number(clockTime.right_ones));
    }
    blinkage = true;
}

void Microwave::display_colon()
{
    ui->colon->setText(":");
    colon_blink = true;
}

void Microwave::blink_colon()
{
    ui->colon->setText(colon_blink ? "" : ":");
    colon_blink = !colon_blink;
}

void Microwave::blink_left_tens()
{
    ui->left_tens->setText(blinkage ? "" : QString::number(proposedClockTime.left_tens));
    blinkage = !blinkage;
}

void Microwave::blink_left_ones()
{
    ui->left_ones->setText(blinkage ? "" : QString::number(proposedClockTime.left_ones));
    blinkage = !blinkage;
}

void Microwave::blink_right_tens()
{
    ui->right_tens->setText(blinkage ? "" : QString::number(proposedClockTime.right_tens));
    blinkage = !blinkage;
}

void Microwave::blink_right_ones()
{
    ui->right_ones->setText(blinkage ? "" : QString::number(proposedClockTime.right_ones));
    blinkage = !blinkage;
}

void Microwave::accept_clock()
{
    setting_clock = false;
    clockTime = proposedClockTime;
    if(!colonBlinkTimer->isActive()) {
        colonBlinkTimer->start();
    }
    emit clock_done_sig();
}

void Microwave::decline_clock()
{
    setting_clock = false;
    emit clock_done_sig();
}

void Microwave::update_left_tens(quint32 digit)
{
    proposedClockTime.left_tens = digit;
    blinkage = true;
}

void Microwave::update_left_ones(quint32 digit)
{
    proposedClockTime.left_ones = digit;
    blinkage = true;
}

void Microwave::update_right_tens(quint32 digit)
{
    proposedClockTime.right_tens = digit;
    blinkage = true;
}

void Microwave::update_right_ones(quint32 digit)
{
    proposedClockTime.right_ones = digit;
    blinkage = true;
}

