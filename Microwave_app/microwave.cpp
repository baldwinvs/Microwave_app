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
    BLINK           = 0x00100000,
    CURRENT_POWER   = 0x10000000,
    CURRENT_KITCHEN = 0x20000000,
    CURRENT_COOK    = 0x40000000,
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
    , time()
    , powerLevel{}
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

    connect(this, SIGNAL(blink_sig()),
            this, SLOT(blink_colon()));

    blinkTimer->setInterval(500);
    blinkTimer->setSingleShot(false);

    display_clock = create_display_clock_state(sm);
    set_cook_time = create_set_cook_time_state(sm);
    set_power_level = create_set_power_level_state(sm);
    display_timer = create_display_timer_state(sm);

    display_clock->addTransition(this, SIGNAL(cook_time_sig()), set_cook_time);
    set_cook_time->addTransition(this, SIGNAL(power_level_sig()), set_power_level);
    set_power_level->addTransition(this, SIGNAL(cook_time_sig()), set_cook_time);
    set_cook_time->addTransition(this, SIGNAL(start_sig()), display_timer);
    set_power_level->addTransition(this, SIGNAL(start_sig()), display_timer);
    display_timer->addTransition(this, SIGNAL(display_timer_done_sig()), display_clock);

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
    connect(display_clock, SIGNAL(entered()), this, SLOT(displayClock()));
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
    connect(select_hour_tens, SIGNAL(exited()), this, SLOT(displayClock()));
    connect(select_hour_tens, SIGNAL(exited()), this, SLOT(select_hour_tens_exit()));
    select_hour_tens->addTransition(this, SIGNAL(select_left_ones_sig()), select_hour_ones);

    //select_hour_ones
    connect(select_hour_ones, SIGNAL(entered()), this, SLOT(select_hour_ones_entry()));
    connect(select_hour_ones, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_hour_ones, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_hour_ones, SIGNAL(exited()), this, SLOT(displayClock()));
    connect(select_hour_ones, SIGNAL(exited()), this, SLOT(select_hour_ones_exit()));
    select_hour_ones->addTransition(this, SIGNAL(select_right_tens_sig()), select_minute_tens);

    //select_minute_tens
    connect(select_minute_tens, SIGNAL(entered()), this, SLOT(select_minute_tens_entry()));
    connect(select_minute_tens, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_minute_tens, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_minute_tens, SIGNAL(exited()), this, SLOT(displayClock()));
    connect(select_minute_tens, SIGNAL(exited()), this, SLOT(select_minute_tens_exit()));
    select_minute_tens->addTransition(this, SIGNAL(select_right_ones_sig()), select_minute_ones);

    //select_minute_ones
    connect(select_minute_ones, SIGNAL(entered()), this, SLOT(select_minute_ones_entry()));
    connect(select_minute_ones, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_minute_ones, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_minute_ones, SIGNAL(exited()), this, SLOT(displayClock()));
    connect(select_minute_ones, SIGNAL(exited()), this, SLOT(select_minute_ones_exit()));
    select_minute_ones->addTransition(this, SIGNAL(select_left_tens_sig()), select_hour_tens);

    //transitions to finish exit set_clock
    set_clock->addTransition(this, SIGNAL(clock_done_sig()), display_clock);

    display_clock->setInitialState(display_clock_init);
    return display_clock;
}

QState* Microwave::create_set_cook_time_state(QState *parent)
{
    QState* set_cook_time {new QState(parent)};
    QState* cook_timer_initial {new QState(set_cook_time)};

    set_cook_time->setObjectName("set_cook_time");
    cook_timer_initial->setObjectName("cook_timer_initial");

    connect(set_cook_time, SIGNAL(entered()), this, SLOT(set_cook_time_entry()));
    connect(set_cook_time, SIGNAL(exited()), this, SLOT(set_cook_time_exit()));

    set_cook_time->setInitialState(cook_timer_initial);
    return set_cook_time;
}

QState* Microwave::create_set_power_level_state(QState *parent)
{
    QState* set_power_level {new QState(parent)};
    QState* set_power_level_initial {new QState(set_power_level)};

    set_power_level->setObjectName("set_power_level");
    set_power_level_initial->setObjectName("set_power_level_initial");

    connect(set_power_level, SIGNAL(entered()), this, SLOT(set_power_level_entry()));
    connect(set_power_level, SIGNAL(exited()), this, SLOT(set_power_level_exit()));
    set_power_level->setInitialState(set_power_level_initial);
    return set_power_level;
}

QState* Microwave::create_set_kitchen_timer_state(QState *parent)
{
    Q_UNUSED(parent)
    return new QState();
}

QState* Microwave::create_display_timer_state(QState *parent)
{
    QState* display_timer {new QState(parent)};
    QState* display_timer_running {new QState(display_timer)};
    QState* display_timer_paused {new QState(display_timer)};

    connect(display_timer, SIGNAL(entered()), this, SLOT(display_timer_entry()));
    connect(display_timer, SIGNAL(exited()), this, SLOT(display_timer_exit()));

    display_timer_running->addTransition(this, SIGNAL(stop_sig()), display_timer_paused);
    display_timer_paused->addTransition(this, SIGNAL(start_sig()), display_timer_running);
    display_timer->setInitialState(display_timer_running);
    return display_timer;
}

void Microwave::set_clock_entry()
{
    qDebug() << "entered set_clock";
    if(!setting_clock) {
        connect(this, SIGNAL(clock_sig()), this, SLOT(accept_clock()));
        connect(this, SIGNAL(stop_sig()), this, SLOT(decline_clock()));
        setting_clock = true;
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
    qDebug() << "entered select_hour_tens";
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_tens()));
}

void Microwave::select_hour_tens_exit()
{
    qDebug() << "left select_hour_tens";
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_tens()));
}

void Microwave::select_hour_ones_entry()
{
    qDebug() << "entered select_hour_ones";
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_ones()));
}

void Microwave::select_hour_ones_exit()
{
    qDebug() << "left select_hour_ones";
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_left_ones()));
}

void Microwave::select_minute_tens_entry()
{
    qDebug() << "entered select_minute_tens";
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_tens()));
}

void Microwave::select_minute_tens_exit()
{
    qDebug() << "left select_minute_tens";
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_tens()));
}

void Microwave::select_minute_ones_entry()
{
    qDebug() << "entered select_minute_ones";
    connect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_ones()));
}

void Microwave::select_minute_ones_exit()
{
    qDebug() << "left select_minute_ones";
    disconnect(blinkTimer, SIGNAL(timeout()), this, SLOT(blink_right_ones()));
}

void Microwave::set_cook_time_entry()
{
    qDebug() << "entered set_cook_time";
}

void Microwave::set_cook_time_exit()
{
    qDebug() << "left set_cook_time";
}

void Microwave::set_power_level_entry()
{
    qDebug() << "entered set_power_level";
}

void Microwave::set_power_level_exit()
{
    qDebug() << "left set_power_level";
}

void Microwave::display_timer_entry()
{
    qDebug() << "entered display_timer";
}

void Microwave::display_timer_exit()
{
    qDebug() << "left display_timer";
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
        case Command::BLINK:
            emit blink_sig();
            break;
        case Command::CURRENT_POWER:
            memcpy(&powerLevel, rbuf.data() + sizeof(quint32), sizeof(quint32));
            displayPowerLevel();
            break;
        case Command::CURRENT_KITCHEN:
        case Command::CURRENT_COOK:
        case Command::CURRENT_CLOCK:
            memcpy(&time, rbuf.data() + sizeof(quint32), sizeof(Time));
            displayClock();
            break;
        default:
        case Command::NONE:
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
    Command cmd {Command::DIGIT_0};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send1()
{
    Command cmd {Command::DIGIT_1};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send2()
{
    Command cmd {Command::DIGIT_2};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send3()
{
    Command cmd {Command::DIGIT_3};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send4()
{
    Command cmd {Command::DIGIT_4};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send5()
{
    Command cmd {Command::DIGIT_5};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send6()
{
    Command cmd {Command::DIGIT_6};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send7()
{
    Command cmd {Command::DIGIT_7};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send8()
{
    Command cmd {Command::DIGIT_8};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::send9()
{
    Command cmd {Command::DIGIT_9};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendStop()
{
    Command cmd {Command::STOP};
    memcpy(buf.data(), &cmd, sizeof(Command));

    outSocket->writeDatagram(buf.data(), sizeof(Command), local, SIM_RECV_PORT);
}

void Microwave::sendStart()
{
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

void Microwave::displayClock()
{
    ui->left_tens->setText(QString::number(time.left_tens));
    ui->left_ones->setText(QString::number(time.left_ones));
    ui->right_tens->setText(QString::number(time.right_tens));
    ui->right_ones->setText(QString::number(time.right_ones));
    blinkage = true;

    ui->colon->setText(":");
    colon_blink = true;
}

void Microwave::displayPowerLevel()
{
    ui->left_tens->setText("P");
    ui->left_ones->setText("L");
    ui->right_tens->setText(QString::number(powerLevel / 10));
    ui->right_ones->setText(QString::number(powerLevel % 10));
    ui->colon->setText("");
}

void Microwave::blink_colon()
{
    ui->colon->setText(colon_blink ? "" : ":");
    colon_blink = !colon_blink;
}

void Microwave::blink_left_tens()
{
    ui->left_tens->setText(blinkage ? "" : QString::number(time.left_tens));
    blinkage = !blinkage;
}

void Microwave::blink_left_ones()
{
    ui->left_ones->setText(blinkage ? "" : QString::number(time.left_ones));
    blinkage = !blinkage;
}

void Microwave::blink_right_tens()
{
    ui->right_tens->setText(blinkage ? "" : QString::number(time.right_tens));
    blinkage = !blinkage;
}

void Microwave::blink_right_ones()
{
    ui->right_ones->setText(blinkage ? "" : QString::number(time.right_ones));
    blinkage = !blinkage;
}

void Microwave::accept_clock()
{
    setting_clock = false;
    emit clock_done_sig();
}

void Microwave::decline_clock()
{
    setting_clock = false;
    emit clock_done_sig();
}

