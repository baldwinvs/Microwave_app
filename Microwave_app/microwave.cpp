#include "microwave.h"
#include "MicrowaveMessageFormat.h"
#include "ui_microwave.h"

#include <QDebug>
#include <QTcpSocket>
#include <QNetworkDatagram>
#include <QStateMachine>
#include <QState>
#include <QSignalTransition>
#include <QTimer>

namespace {

const quint16 DEV_RECV_PORT {60002};
const QHostAddress server {QHostAddress("192.168.0.10")};

}

Microwave::Microwave(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Microwave)
    , socket{new QTcpSocket(this)}
    , txBuf{}
    , rxBuf{}
    , timer{new QTimer(this)}
    , txMessage{new MicrowaveMsgFormat::Message()}
    , rxMessage{new MicrowaveMsgFormat::Message()}
    , time{new MicrowaveMsgFormat::Time()}
    , powerLevel{}
    , disableClockDisplay{false}
    , disablePowerLevel{false}
    , sm{new QStateMachine(this)}
    , InitialState{new QState(sm)}
    , DisplayClock{Q_NULLPTR}
    , DisplayClockInit{Q_NULLPTR}
    , SetClock{Q_NULLPTR}
    , ClockSelectHourTens{Q_NULLPTR}
    , ClockSelectHourOnes{Q_NULLPTR}
    , ClockSelectMinuteTens{Q_NULLPTR}
    , ClockSelectMinuteOnes{Q_NULLPTR}
    , SetCookTimer{Q_NULLPTR}
    , SetCookTimerInit{Q_NULLPTR}
    , SetPowerLevel{Q_NULLPTR}
    , SetPowerLevelInit{Q_NULLPTR}
    , SetKitchenTimer{Q_NULLPTR}
    , SetKitchenTimerInit{Q_NULLPTR}
    , KitchenSelectMinuteTens{Q_NULLPTR}
    , KitchenSelectMinuteOnes{Q_NULLPTR}
    , KitchenSelectSecondTens{Q_NULLPTR}
    , KitchenSelectSecondOnes{Q_NULLPTR}
    , DisplayTimer{Q_NULLPTR}
    , DisplayTimerInit{Q_NULLPTR}
    , SetCookTimerTransition{Q_NULLPTR}
    , SetKitchenTimerTransition{Q_NULLPTR}
    , DisplayTimerTransition{Q_NULLPTR}
{
    ui->setupUi(this);
    connect(ui->pb_timeCook, SIGNAL(clicked()), this, SLOT(sendTimeCook()));
    connect(ui->pb_powerLevel, SIGNAL(clicked()), this, SLOT(sendPowerLevel()));
    connect(ui->pb_kitchenTimer, SIGNAL(clicked()), this, SLOT(sendKitchenTimer()));
    connect(ui->pb_clock, SIGNAL(clicked()), this, SLOT(sendClock()));
    connect(ui->pb_0, SIGNAL(clicked()), this, SLOT(send0()));
    connect(ui->pb_1, SIGNAL(clicked()), this, SLOT(send1()));
    connect(ui->pb_2, SIGNAL(clicked()), this, SLOT(send2()));
    connect(ui->pb_3, SIGNAL(clicked()), this, SLOT(send3()));
    connect(ui->pb_4, SIGNAL(clicked()), this, SLOT(send4()));
    connect(ui->pb_5, SIGNAL(clicked()), this, SLOT(send5()));
    connect(ui->pb_6, SIGNAL(clicked()), this, SLOT(send6()));
    connect(ui->pb_7, SIGNAL(clicked()), this, SLOT(send7()));
    connect(ui->pb_8, SIGNAL(clicked()), this, SLOT(send8()));
    connect(ui->pb_9, SIGNAL(clicked()), this, SLOT(send9()));
    connect(ui->pb_stop, SIGNAL(clicked()), this, SLOT(sendStop()));
    connect(ui->pb_start, SIGNAL(clicked()), this, SLOT(sendStart()));

    connect(socket, SIGNAL(connected()), this, SLOT(onTcpConnect()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onTcpDisconnect()));

    txMessage->dst = MicrowaveMsgFormat::Destination::DEV;

    connect(InitialState, SIGNAL(entered()), this, SLOT(InitialStateEntry()));
    connect(InitialState, SIGNAL(exited()), this, SLOT(InitialStateExit()));

    SetupDisplayClockState(sm);
    SetupSetCookTimerState(sm);
    SetupSetPowerLevelState(sm);
    SetupSetKitchenTimerState(sm);
    SetupDisplayTimerState(sm);

    //normal transitions between higher level states

    //transitions from DisplayClock to the following need to be saved:
    // - SetCookTimer
    // - SetKitchenTimer
    // - DisplayTimer
    SetCookTimerTransition    = DisplayClock->addTransition(this, SIGNAL(cook_time_sig()), SetCookTimer);
    SetKitchenTimerTransition = DisplayClock->addTransition(this, SIGNAL(kitchen_timer_sig()), SetKitchenTimer);
    DisplayTimerTransition     = DisplayClock->addTransition(this, SIGNAL(start_sig()), DisplayTimer);

    SetCookTimer->addTransition(this, SIGNAL(power_level_sig()), SetPowerLevel);
    SetCookTimer->addTransition(this, SIGNAL(stop_sig()), DisplayClock);
    SetCookTimer->addTransition(this, SIGNAL(start_sig()), DisplayTimer);

    SetPowerLevel->addTransition(this, SIGNAL(cook_time_sig()), SetCookTimer);
    SetPowerLevel->addTransition(this, SIGNAL(stop_sig()), DisplayClock);
    SetPowerLevel->addTransition(this, SIGNAL(start_sig()), DisplayTimer);

    SetKitchenTimer->addTransition(this, SIGNAL(stop_sig()), DisplayClock);
    SetKitchenTimer->addTransition(this, SIGNAL(start_sig()), DisplayTimer);

    DisplayTimer->addTransition(this, SIGNAL(display_timer_done_sig()), DisplayClock);
    DisplayTimer->addTransition(this, SIGNAL(stop_sig()), DisplayClock);

    //state request resultant transitions
    InitialState->addTransition(this, SIGNAL(state_req_display_clock()), DisplayClock);

    InitialState->addTransition(this, SIGNAL(state_req_set_cook_timer()), SetCookTimer);
    InitialState->addTransition(this, SIGNAL(state_req_set_power_level()), SetPowerLevel);
    InitialState->addTransition(this, SIGNAL(state_req_display_timer()), DisplayTimer);

    InitialState->addTransition(this, SIGNAL(state_req_clock_select_left_tens()), ClockSelectHourTens);
    InitialState->addTransition(this, SIGNAL(state_req_clock_select_left_ones()), ClockSelectHourOnes);
    InitialState->addTransition(this, SIGNAL(state_req_clock_select_right_tens()), ClockSelectMinuteTens);
    InitialState->addTransition(this, SIGNAL(state_req_clock_select_right_ones()), ClockSelectMinuteOnes);

    InitialState->addTransition(this, SIGNAL(state_req_kitchen_select_left_tens()), KitchenSelectMinuteTens);
    InitialState->addTransition(this, SIGNAL(state_req_kitchen_select_left_ones()), KitchenSelectMinuteOnes);
    InitialState->addTransition(this, SIGNAL(state_req_kitchen_select_right_tens()), KitchenSelectSecondTens);
    InitialState->addTransition(this, SIGNAL(state_req_kitchen_select_right_ones()), KitchenSelectSecondOnes);

    sm->setInitialState(InitialState);
    sm->start();

    socket->connectToHost(server, DEV_RECV_PORT, QIODevice::ReadWrite);
}

Microwave::~Microwave()
{
    delete time;
    delete txMessage;
    delete ui;
}

void Microwave::SetupDisplayClockState(QState *parent)
{
    DisplayClock = new QState(parent);
    DisplayClockInit = new QState(DisplayClock);

    SetClock = new QState(DisplayClock);
    SetClockInit = new QState(SetClock);
    ClockSelectHourTens = new QState(SetClock);
    ClockSelectHourOnes = new QState(SetClock);
    ClockSelectMinuteTens = new QState(SetClock);
    ClockSelectMinuteOnes = new QState(SetClock);

    DisplayClock->setObjectName("DisplayClock");
    DisplayClockInit->setObjectName("DisplayClockInit");
    SetClock->setObjectName("SetClock");
    SetClockInit->setObjectName("SetClockInit");
    ClockSelectHourTens->setObjectName("ClockSelectHourTens");
    ClockSelectHourOnes->setObjectName("ClockSelectHourOnes");
    ClockSelectMinuteTens->setObjectName("ClockSelectMinuteTens");
    ClockSelectMinuteOnes->setObjectName("ClockSelectMinuteOnes");

    //display_clock
    connect(DisplayClock, SIGNAL(entered()), this, SLOT(DisplayClockInitEntry()));
    connect(DisplayClock, SIGNAL(exited()), this, SLOT(DisplayClockInitExit()));
    DisplayClockInit->addTransition(this, SIGNAL(clock_sig()), SetClock);

    //set_clock
    connect(SetClock, SIGNAL(entered()), this, SLOT(SetClockEntry()));
    connect(SetClock, SIGNAL(exited()), this, SLOT(SetClockExit()));
    SetClock->setInitialState(SetClockInit);
    SetClockInit->addTransition(this, SIGNAL(select_left_tens_sig()), ClockSelectHourTens);

    //select_hour_tens
    connect(ClockSelectHourTens, SIGNAL(entered()), this, SLOT(SelectLeftTensEntry()));
    connect(ClockSelectHourTens, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(ClockSelectHourTens, SIGNAL(exited()), this, SLOT(SelectLeftTensExit()));
    ClockSelectHourTens->addTransition(this, SIGNAL(select_left_ones_sig()), ClockSelectHourOnes);

    //select_hour_ones
    connect(ClockSelectHourOnes, SIGNAL(entered()), this, SLOT(SelectLeftOnesEntry()));
    connect(ClockSelectHourOnes, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(ClockSelectHourOnes, SIGNAL(exited()), this, SLOT(SelectLeftOnesExit()));
    ClockSelectHourOnes->addTransition(this, SIGNAL(select_right_tens_sig()), ClockSelectMinuteTens);

    //select_minute_tens
    connect(ClockSelectMinuteTens, SIGNAL(entered()), this, SLOT(SelectRightTensEntry()));
    connect(ClockSelectMinuteTens, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(ClockSelectMinuteTens, SIGNAL(exited()), this, SLOT(SelectRightTensExit()));
    ClockSelectMinuteTens->addTransition(this, SIGNAL(select_right_ones_sig()), ClockSelectMinuteOnes);

    //select_minute_ones
    connect(ClockSelectMinuteOnes, SIGNAL(entered()), this, SLOT(SelectRightOnesEntry()));
    connect(ClockSelectMinuteOnes, SIGNAL(exited()), this, SLOT(displayTime()));
    connect(ClockSelectMinuteOnes, SIGNAL(exited()), this, SLOT(SelectRightOnesExit()));
    ClockSelectMinuteOnes->addTransition(this, SIGNAL(select_left_tens_sig()), ClockSelectHourTens);

    //transitions to finish exit set_clock
    SetClock->addTransition(this, SIGNAL(clock_done_sig()), DisplayClock);

    DisplayClock->setInitialState(DisplayClockInit);
}

void Microwave::SetupSetCookTimerState(QState *parent)
{
    SetCookTimer = new QState(parent);
    SetCookTimerInit = new QState(SetCookTimer);

    SetCookTimer->setObjectName("SetCookTimer");
    SetCookTimerInit->setObjectName("SetCookTimerInit");

    connect(SetCookTimer, SIGNAL(entered()), this, SLOT(SetCookTimerEntry()));
    connect(SetCookTimer, SIGNAL(exited()), this, SLOT(SetCookTimerExit()));

    SetCookTimer->setInitialState(SetCookTimerInit);
}

void Microwave::SetupSetPowerLevelState(QState *parent)
{
    SetPowerLevel = new QState(parent);
    SetPowerLevelInit = new QState(SetPowerLevel);

    SetPowerLevel->setObjectName("SetPowerLevel");
    SetPowerLevelInit->setObjectName("SetPowerLevelInit");

    connect(SetPowerLevel, SIGNAL(entered()), this, SLOT(SetPowerLevelEntry()));
    connect(SetPowerLevel, SIGNAL(exited()), this, SLOT(SetPowerLevelExit()));
    SetPowerLevel->setInitialState(SetPowerLevelInit);
}

void Microwave::SetupSetKitchenTimerState(QState *parent)
{
    SetKitchenTimer = new QState(parent);
    SetKitchenTimerInit = new QState(SetKitchenTimer);
    KitchenSelectMinuteTens = new QState(SetKitchenTimer);
    KitchenSelectMinuteOnes = new QState(SetKitchenTimer);
    KitchenSelectSecondTens = new QState(SetKitchenTimer);
    KitchenSelectSecondOnes = new QState(SetKitchenTimer);

    SetKitchenTimer->setObjectName("SetKitchenTimer");
    SetKitchenTimerInit->setObjectName("SetKitchenTimerInit");
    KitchenSelectMinuteTens->setObjectName("KitchenSelectMinuteTens");
    KitchenSelectMinuteOnes->setObjectName("KitchenSelectMinuteOnes");
    KitchenSelectSecondTens->setObjectName("KitchenSelectSecondTens");
    KitchenSelectSecondOnes->setObjectName("KitchenSelectSecondOnes");

    SetKitchenTimerInit->addTransition(this, SIGNAL(select_left_tens_sig()), KitchenSelectMinuteTens);

    connect(KitchenSelectMinuteTens, SIGNAL(entered()), this, SLOT(SelectLeftTensEntry()));
    connect(KitchenSelectMinuteTens, SIGNAL(exited()), this, SLOT(SelectLeftTensExit()));
    KitchenSelectMinuteTens->addTransition(this, SIGNAL(select_left_ones_sig()), KitchenSelectMinuteOnes);

    connect(KitchenSelectMinuteOnes, SIGNAL(entered()), this, SLOT(SelectLeftOnesEntry()));
    connect(KitchenSelectMinuteOnes, SIGNAL(exited()), this, SLOT(SelectLeftOnesExit()));
    KitchenSelectMinuteOnes->addTransition(this, SIGNAL(select_right_tens_sig()), KitchenSelectSecondTens);

    connect(KitchenSelectSecondTens, SIGNAL(entered()), this, SLOT(SelectRightTensEntry()));
    connect(KitchenSelectSecondTens, SIGNAL(exited()), this, SLOT(SelectRightTensExit()));
    KitchenSelectSecondTens->addTransition(this, SIGNAL(select_right_ones_sig()), KitchenSelectSecondOnes);

    connect(KitchenSelectSecondOnes, SIGNAL(entered()), this, SLOT(SelectRightOnesEntry()));
    connect(KitchenSelectSecondOnes, SIGNAL(exited()), this, SLOT(SelectRightOnesExit()));
    KitchenSelectSecondOnes->addTransition(this, SIGNAL(select_left_tens_sig()), KitchenSelectMinuteTens);

    SetKitchenTimer->setInitialState(SetKitchenTimerInit);
}

void Microwave::SetupDisplayTimerState(QState *parent)
{
    DisplayTimer = new QState(parent);
    DisplayTimerInit = new QState(DisplayTimer);

    DisplayTimer->setObjectName("DisplayTimer");
    DisplayTimerInit->setObjectName("DisplayTimerInit");

    connect(DisplayTimer, SIGNAL(entered()), this, SLOT(DisplayTimerInitEntry()));
    connect(DisplayTimer, SIGNAL(exited()), this, SLOT(DisplayTimerInitExit()));

    DisplayTimer->setInitialState(DisplayTimerInit);
}

void Microwave::InitialStateEntry()
{
    connect(timer, SIGNAL(timeout()), this, SLOT(onStateRequestTimeout()));
    timer->setInterval(500); // half second
    timer->setSingleShot(false);
    timer->start();
}

void Microwave::InitialStateExit()
{
    disconnect(timer, SIGNAL(timeout()), this, SLOT(onStateRequestTimeout()));
    timer->stop();
}

void Microwave::DisplayClockInitEntry()
{
    qDebug() << "entered display_clock";
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_colon(bool)));
}

void Microwave::DisplayClockInitExit()
{
    qDebug() << "left display_clock";
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_colon(bool)));
}

void Microwave::SetClockEntry()
{
    qDebug() << "entered set_clock";

    connect(this, SIGNAL(clock_sig()), this, SLOT(clock_done()));
    connect(this, SIGNAL(stop_sig()), this, SLOT(clock_done()));
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_colon(bool)));

    DisplayClock->removeTransition(dynamic_cast<QAbstractTransition*>(SetCookTimerTransition));
    DisplayClock->removeTransition(dynamic_cast<QAbstractTransition*>(SetKitchenTimerTransition));
    DisplayClock->removeTransition(dynamic_cast<QAbstractTransition*>(DisplayTimerTransition));
}

void Microwave::SetClockExit()
{
    qDebug() << "left set_clock";

    disconnect(this, SIGNAL(clock_sig()), this, SLOT(clock_done()));
    disconnect(this, SIGNAL(stop_sig()), this, SLOT(clock_done()));
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_colon(bool)));

    DisplayClock->addTransition(dynamic_cast<QAbstractTransition*>(SetCookTimerTransition));
    DisplayClock->addTransition(dynamic_cast<QAbstractTransition*>(SetKitchenTimerTransition));
    DisplayClock->addTransition(dynamic_cast<QAbstractTransition*>(DisplayTimerTransition));
}

void Microwave::SelectLeftTensEntry()
{
    qDebug() << "entered select_hour_tens";
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_left_tens(bool)));
}

void Microwave::SelectLeftTensExit()
{
    qDebug() << "left select_hour_tens";
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_left_tens(bool)));
}

void Microwave::SelectLeftOnesEntry()
{
    qDebug() << "entered select_hour_ones";
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_left_ones(bool)));
}

void Microwave::SelectLeftOnesExit()
{
    qDebug() << "left select_hour_ones";
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_left_ones(bool)));
}

void Microwave::SelectRightTensEntry()
{
    qDebug() << "entered select_minute_tens";
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_right_tens(bool)));
}

void Microwave::SelectRightTensExit()
{
    qDebug() << "left select_minute_tens";
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_right_tens(bool)));
}

void Microwave::SelectRightOnesEntry()
{
    qDebug() << "entered select_minute_ones";
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_right_ones(bool)));
}

void Microwave::SelectRightOnesExit()
{
    qDebug() << "left select_minute_ones";
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_right_ones(bool)));
}

void Microwave::SetCookTimerEntry()
{
    qDebug() << "entered set_cook_time";
    disableClockDisplay = true;
    disablePowerLevel = true;
    displayTime();
}

void Microwave::SetCookTimerExit()
{
    qDebug() << "left set_cook_time";
    disableClockDisplay = false;
    disablePowerLevel = false;
}

void Microwave::SetPowerLevelEntry()
{
    qDebug() << "entered set_power_level";
    disableClockDisplay = true;
    connect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_power_level(bool)));
    displayPowerLevel();
}

void Microwave::SetPowerLevelExit()
{
    qDebug() << "left set_power_level";
    disconnect(this, SIGNAL(blink_sig(bool)), this, SLOT(blink_power_level(bool)));
    disableClockDisplay = false;
}

void Microwave::DisplayTimerInitEntry()
{
    qDebug() << "entered display_timer";
    connect(this, SIGNAL(clock_sig()), this, SIGNAL(display_timer_done_sig()));
    connect(this, SIGNAL(power_level_sig()), this, SLOT(startDisplayPowerLevel2Sec()));
    disableClockDisplay = true;
    disablePowerLevel = true;
}

void Microwave::DisplayTimerInitExit()
{
    qDebug() << "left display_timer";
    disconnect(this, SIGNAL(clock_sig()), this, SIGNAL(display_timer_done_sig()));
    disconnect(ui->pb_powerLevel, SIGNAL(clicked()), this, SLOT(startDisplayPowerLevel2Sec()));
    disableClockDisplay = false;
    disablePowerLevel = false;
}

void Microwave::onTcpConnect()
{
    qDebug() << "socket connected";
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
//    connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(onBytesWritten(qint64)));
}

void Microwave::onTcpDisconnect()
{
    qDebug() << "socket disconnected";
    disconnect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
//    disconnect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(onBytesWritten(qint64)));
}

void Microwave::onReadyRead()
{
    using namespace MicrowaveMsgFormat;
    static const qint32 NonUpdateSize = sizeof(int) * 2;
    static const qint32 MessageSize = sizeof(Message);

    rxBuf.append(socket->readAll());

    //handle received data
    while(rxBuf.size() >= NonUpdateSize) {
        char * tmp {strstr(rxBuf.data(), "ppaM")};
        if(tmp) {
            *rxMessage = Message(tmp);
        }
        else {
            //clear everything except the last 3 bytes because
            // a truncated header might be there
            rxBuf = rxBuf.right(3);
        }

        Type type = static_cast<Type>(static_cast<uint32_t>(rxMessage->state) >> 24);

        switch(type) {
        case Type::STATE:
            handleState(*rxMessage);
            break;
        case Type::SIGNAL:
            handleSignal(*rxMessage);
            break;
        case Type::UPDATE:
            handleUpdate(*rxMessage);
            break;
        }

        //remove used data
        rxBuf = rxBuf.right(rxBuf.size() - MessageSize);
    }
}

void Microwave::onBytesWritten(qint64 bytes)
{
    qDebug() << bytes << "written to" << socket->peerAddress();
}

void Microwave::handleState(const MicrowaveMsgFormat::Message &msg)
{
    using namespace MicrowaveMsgFormat;
    switch(msg.state) {
    case State::DISPLAY_CLOCK:
        emit state_req_display_clock();
        break;
    case State::CLOCK_SELECT_HOUR_TENS:
        emit state_req_clock_select_left_tens();
        break;
    case State::CLOCK_SELECT_HOUR_ONES:
        emit state_req_clock_select_left_ones();
        break;
    case State::CLOCK_SELECT_MINUTE_TENS:
        emit state_req_clock_select_right_tens();
        break;
    case State::CLOCK_SELECT_MINUTE_ONES:
        emit state_req_clock_select_right_ones();
        break;
    case State::SET_COOK_TIMER:
        emit state_req_set_cook_timer();
        break;
    case State::SET_POWER_LEVEL:
        emit state_req_set_power_level();
        break;
    case State::KITCHEN_SELECT_HOUR_TENS:
        emit state_req_kitchen_select_left_tens();
        break;
    case State::KITCHEN_SELECT_HOUR_ONES:
        emit state_req_kitchen_select_left_ones();
        break;
    case State::KITCHEN_SELECT_MINUTE_TENS:
        emit state_req_kitchen_select_right_tens();
        break;
    case State::KITCHEN_SELECT_MINUTE_ONES:
        emit state_req_kitchen_select_right_ones();
        break;
    case State::DISPLAY_TIMER:
        emit state_req_display_timer();
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
    case Signal::BLINK_ON:
        emit blink_sig(true);
        break;
    case Signal::BLINK_OFF:
        emit blink_sig(false);
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
    default:
        break;
    }
}

void Microwave::handleUpdate(const MicrowaveMsgFormat::Message &msg)
{
    using namespace MicrowaveMsgFormat;
    switch(msg.update) {
    case Update::CLOCK:
            time->left_tens = static_cast<uint32_t>(msg.data[0] - '0');
            time->left_ones = static_cast<uint32_t>(msg.data[1] - '0');
            time->right_tens = static_cast<uint32_t>(msg.data[2] - '0');
            time->right_ones = static_cast<uint32_t>(msg.data[3] - '0');
        if(!disableClockDisplay) {
            displayTime();
        }
        break;
    case Update::DISPLAY_TIMER:
            time->left_tens = static_cast<uint32_t>(msg.data[0] - '0');
            time->left_ones = static_cast<uint32_t>(msg.data[1] - '0');
            time->right_tens = static_cast<uint32_t>(msg.data[2] - '0');
            time->right_ones = static_cast<uint32_t>(msg.data[3] - '0');
        if(!disableDisplayTimer) {
            displayTime();
        }
        break;
    case Update::POWER_LEVEL:
        powerLevel = static_cast<uint32_t>(((msg.data[0] - '0') * 10) + (msg.data[1] - '0'));
        if(!disablePowerLevel) {
            displayPowerLevel();
        }
        break;
    case Update::NONE:
        break;
    }
}

void Microwave::writeData()
{
    txBuf.append(reinterpret_cast<char*>(txMessage), sizeof(MicrowaveMsgFormat::Message));
    if(socket->state() == QAbstractSocket::ConnectedState) {
        const qint64 count {socket->write(txBuf)};
        if(-1 == count) {
            qDebug() << "Error occurred while writing data";
        }
    }
    txBuf.clear();
}
void Microwave::sendTimeCook()
{
    qDebug() << "time cook";
    txMessage->signal = MicrowaveMsgFormat::Signal::COOK_TIME;
    writeData();
}

void Microwave::sendPowerLevel()
{
    qDebug() << "power level";
    txMessage->signal = MicrowaveMsgFormat::Signal::POWER_LEVEL;
    writeData();
}

void Microwave::sendKitchenTimer()
{
    qDebug() << "kitchen timer";
    txMessage->signal = MicrowaveMsgFormat::Signal::KITCHEN_TIMER;
    writeData();
}

void Microwave::sendClock()
{
    qDebug() << "clock";
    txMessage->signal = MicrowaveMsgFormat::Signal::CLOCK;
    writeData();
}

void Microwave::send0()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_0;
    writeData();
}

void Microwave::send1()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_1;
    writeData();
}

void Microwave::send2()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_2;
    writeData();
}

void Microwave::send3()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_3;
    writeData();
}

void Microwave::send4()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_4;
    writeData();
}

void Microwave::send5()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_5;
    writeData();
}

void Microwave::send6()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_6;
    writeData();
}

void Microwave::send7()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_7;
    writeData();
}

void Microwave::send8()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_8;
    writeData();
}

void Microwave::send9()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::DIGIT_9;
    writeData();
}

void Microwave::sendStop()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::STOP;
    writeData();
}

void Microwave::sendStart()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::START;
    writeData();
}

void Microwave::SendStateRequest()
{
    txMessage->signal = MicrowaveMsgFormat::Signal::STATE_REQUEST;
    writeData();
}

void Microwave::displayTime()
{
    ui->left_tens->setText(QString::number(time->left_tens));
    ui->left_ones->setText(QString::number(time->left_ones));
    ui->right_tens->setText(QString::number(time->right_tens));
    ui->right_ones->setText(QString::number(time->right_ones));

    ui->colon->setText(":");
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

void Microwave::startDisplayPowerLevel2Sec()
{
    static const qint32 twoSec {2000};
    disconnect(this, SIGNAL(power_level_sig()), this, SLOT(startDisplayPowerLevel2Sec()));
    connect(timer, SIGNAL(timeout()), this, SLOT(stopDisplayPowerLevel2Sec()));
    disableDisplayTimer = true;
    timer->setSingleShot(true);
    timer->start(twoSec);
    displayPowerLevel();
}

void Microwave::stopDisplayPowerLevel2Sec()
{
    timer->stop();
    disconnect(timer, SIGNAL(timeout()), this, SLOT(stopDisplayPowerLevel2Sec()));
    disableDisplayTimer = false;
    displayTime();
    connect(this, SIGNAL(power_level_sig()), this, SLOT(startDisplayPowerLevel2Sec()));
}

void Microwave::blink_colon(const bool flag)
{
    ui->colon->setText(flag ? ":" : "");
}

void Microwave::blink_left_tens(const bool flag)
{
    ui->left_tens->setText(flag ? QString::number(time->left_tens) : "");
}

void Microwave::blink_left_ones(const bool flag)
{
    ui->left_ones->setText(flag ? QString::number(time->left_ones) : "");
}

void Microwave::blink_right_tens(const bool flag)
{
    ui->right_tens->setText(flag ? QString::number(time->right_tens) : "");
}

void Microwave::blink_right_ones(const bool flag)
{
    ui->right_ones->setText(flag ? QString::number(time->right_ones) : "");
}

void Microwave::blink_power_level(const bool flag)
{
    if(flag) {
        displayPowerLevel();
    }
    else {
        ui->colon->setText("");
        ui->left_tens->setText("");
        ui->left_ones->setText("");
        ui->right_tens->setText("");
        ui->right_ones->setText("");
    }
}

void Microwave::clock_done()
{
    emit clock_done_sig();
}

void Microwave::onStateRequestTimeout()
{
    SendStateRequest();
}

