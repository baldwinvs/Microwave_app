#include "microwave.h"
#include "MicrowaveMessageFormat.h"
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
    , msg{new MicrowaveMsgFormat::Message()}
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

    blinkTimer->setInterval(500);
    blinkTimer->setSingleShot(false);

    msg->dst = MicrowaveMsgFormat::Destination::DEV;

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
    connect(display_clock, SIGNAL(entered()), this, SLOT(display_clock_entry()));
    connect(display_clock, SIGNAL(entered()), this, SLOT(sendCurrentClockRequest()));
    connect(display_clock, SIGNAL(exited()), this, SLOT(display_clock_exit()));
    connect(this, SIGNAL(blink_sig()),
            this, SLOT(blink_colon()));
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
    connect(select_hour_tens, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(select_hour_tens, SIGNAL(exited()), this, SLOT(select_hour_tens_exit()));
    select_hour_tens->addTransition(this, SIGNAL(select_left_ones_sig()), select_hour_ones);

    //select_hour_ones
    connect(select_hour_ones, SIGNAL(entered()), this, SLOT(select_hour_ones_entry()));
    connect(select_hour_ones, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_hour_ones, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_hour_ones, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(select_hour_ones, SIGNAL(exited()), this, SLOT(select_hour_ones_exit()));
    select_hour_ones->addTransition(this, SIGNAL(select_right_tens_sig()), select_minute_tens);

    //select_minute_tens
    connect(select_minute_tens, SIGNAL(entered()), this, SLOT(select_minute_tens_entry()));
    connect(select_minute_tens, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_minute_tens, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_minute_tens, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(select_minute_tens, SIGNAL(exited()), this, SLOT(select_minute_tens_exit()));
    select_minute_tens->addTransition(this, SIGNAL(select_right_ones_sig()), select_minute_ones);

    //select_minute_ones
    connect(select_minute_ones, SIGNAL(entered()), this, SLOT(select_minute_ones_entry()));
    connect(select_minute_ones, SIGNAL(entered()), blinkTimer, SLOT(start()));
    connect(select_minute_ones, SIGNAL(exited()), blinkTimer, SLOT(stop()));
    connect(select_minute_ones, SIGNAL(exited()), this, SLOT(displayTime()));
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

void Microwave::display_clock_entry()
{
    qDebug() << "entered display_clock";
    connect(this, SIGNAL(blink_sig()), this, SLOT(blink_colon()));
}

void Microwave::display_clock_exit()
{
    qDebug() << "left display_clock";
    connect(this, SIGNAL(blink_sig()), this, SLOT(blink_colon()));
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
    using namespace MicrowaveMsgFormat;
    while(inSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram {inSocket->receiveDatagram()};
        QByteArray rbuf {datagram.data()};

        Message rxMsg(rbuf.data());

        if(Destination::APP != rxMsg.dst) {
            return;
        }

        Type type = static_cast<Type>(static_cast<uint32_t>(rxMsg.state) >> 24);

        switch(type) {
        case Type::STATE:
            handleState(rxMsg);
            break;
        case Type::SIGNAL:
            handleSignal(rxMsg);
            break;
        case Type::UPDATE:
            handleUpdate(rxMsg);
            break;
        }
    }
}

void Microwave::handleState(const MicrowaveMsgFormat::Message &msg)
{
    using namespace MicrowaveMsgFormat;
    switch(msg.state) {
    case State::DISPLAY_CLOCK:
        break;
    case State::SET_CLOCK:
        break;
    case State::CLOCK_SELECT_HOUR_TENS:
        break;
    case State::CLOCK_SELECT_HOUR_ONES:
        break;
    case State::CLOCK_SELECT_MINUTE_TENS:
        break;
    case State::CLOCK_SELECT_MINUTE_ONES:
        break;
    case State::SET_COOK_TIME:
        break;
    case State::SET_POWER_LEVEL:
        break;
    case State::SET_KITCHEN_TIMER:
        break;
    case State::KITCHEN_SELECT_HOUR_TENS:
        break;
    case State::KITCHEN_SELECT_HOUR_ONES:
        break;
    case State::KITCHEN_SELECT_MINUTE_TENS:
        break;
    case State::KITCHEN_SELECT_MINUTE_ONES:
        break;
    case State::DISPLAY_TIMER:
        break;
    case State::DISPLAY_TIMER_RUNNING:
        break;
    case State::DISPLAY_TIMER_PAUSED:
        break;
    case State::NONE:
        break;
    }
}

void Microwave::handleSignal(const MicrowaveMsgFormat::Message &msg)
{
    using namespace MicrowaveMsgFormat;
    switch(msg.signal) {
    case Signal::CLOCK:
        emit clock_sig();
        break;
    case Signal::COOK_TIME:
        emit cook_time_sig();
        break;
    case Signal::POWER_LEVEL:
        emit power_level_sig();
        break;
    case Signal::KITCHEN_TIMER:
        emit kitchen_timer_sig();
        break;
    case Signal::STOP:
        emit stop_sig();
        break;
    case Signal::START:
        emit start_sig();
        break;
    case Signal::BLINK:
        emit blink_sig();
        break;
    case Signal::MOD_LEFT_TENS:
        emit select_left_tens_sig();
        break;
    case Signal::MOD_LEFT_ONES:
        emit select_left_ones_sig();
        break;
    case Signal::MOD_RIGHT_TENS:
        emit select_right_tens_sig();
        break;
    case Signal::MOD_RIGHT_ONES:
        emit select_right_ones_sig();
        break;
    case Signal::NONE:
    default:
        break;
    }
}

void Microwave::handleUpdate(const MicrowaveMsgFormat::Message &msg)
{
    using namespace MicrowaveMsgFormat;
    switch(msg.update) {
    case Update::CLOCK:
    case Update::DISPLAY_TIMER:
        memcpy(&time, msg.data, sizeof(Time));
        displayTime();
        break;
    case Update::POWER_LEVEL:
        memcpy(&powerLevel, msg.data, sizeof(quint32));
        displayPowerLevel();
        break;
    case Update::NONE:
        break;
    }
}

void Microwave::writeData(const MicrowaveMsgFormat::Message* const msg)
{
    memcpy(buf.data(), msg, sizeof(MicrowaveMsgFormat::Message));
    outSocket->writeDatagram(buf.data(), sizeof(MicrowaveMsgFormat::Message), local, SIM_RECV_PORT);
}
void Microwave::sendTimeCook()
{
    qDebug() << "time cook";
    msg->signal = MicrowaveMsgFormat::Signal::COOK_TIME;
    writeData(msg);
}

void Microwave::sendPowerLevel()
{
    qDebug() << "power level";
    msg->signal = MicrowaveMsgFormat::Signal::POWER_LEVEL;
    writeData(msg);
}

void Microwave::sendKitchenTimer()
{
    qDebug() << "kitchen timer";
    msg->signal = MicrowaveMsgFormat::Signal::KITCHEN_TIMER;
    writeData(msg);
}

void Microwave::sendClock()
{
    qDebug() << "clock";
    msg->signal = MicrowaveMsgFormat::Signal::CLOCK;
    writeData(msg);
}

void Microwave::send0()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_0;
    writeData(msg);
}

void Microwave::send1()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_1;
    writeData(msg);
}

void Microwave::send2()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_2;
    writeData(msg);
}

void Microwave::send3()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_3;
    writeData(msg);
}

void Microwave::send4()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_4;
    writeData(msg);
}

void Microwave::send5()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_5;
    writeData(msg);
}

void Microwave::send6()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_6;
    writeData(msg);
}

void Microwave::send7()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_7;
    writeData(msg);
}

void Microwave::send8()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_8;
    writeData(msg);
}

void Microwave::send9()
{
    msg->signal = MicrowaveMsgFormat::Signal::DIGIT_9;
    writeData(msg);
}

void Microwave::sendStop()
{
    msg->signal = MicrowaveMsgFormat::Signal::STOP;
    writeData(msg);
}

void Microwave::sendStart()
{
    msg->signal = MicrowaveMsgFormat::Signal::START;
    writeData(msg);
}

void Microwave::sendCurrentClockRequest()
{
    Command cmd {Command::CURRENT_CLOCK};
    memcpy(buf.data(), &cmd, sizeof(cmd));

    outSocket->writeDatagram(buf.data(), sizeof(cmd), local, SIM_RECV_PORT);
}

void Microwave::displayTime()
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
    const quint32 left_digit {powerLevel / 10};
    ui->right_tens->setText(1 == left_digit ? QString::number(left_digit) : "");
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

