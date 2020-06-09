#ifndef MICROWAVE_H
#define MICROWAVE_H

#include <QMainWindow>
#include <QByteArray>

//forward declarations
class QTcpSocket;
class QStateMachine;
class QSignalTransition;
class QState;
class QTimer;

namespace MicrowaveMsgFormat {
class Time;
class Message;
}

QT_BEGIN_NAMESPACE
namespace Ui { class Microwave; }
QT_END_NAMESPACE

class Microwave : public QMainWindow
{
    Q_OBJECT

public:
    Microwave(QWidget *parent = nullptr);
    ~Microwave();

signals:
    //signals mapped to rx Signal from the dev board
    void clock_sig();
    void cook_time_sig();
    void power_level_sig();
    void kitchen_timer_sig();
    void stop_sig();
    void start_sig();
    void blink_sig(bool);
    void select_left_tens_sig();
    void select_left_ones_sig();
    void select_right_tens_sig();
    void select_right_ones_sig();

    //signals generated from the response of the STATE_REQUEST tx signal
    void state_req_display_clock();
    void state_req_clock_select_left_tens();
    void state_req_clock_select_left_ones();
    void state_req_clock_select_right_tens();
    void state_req_clock_select_right_ones();
    void state_req_set_cook_timer();
    void state_req_set_power_level();
    void state_req_kitchen_select_left_tens();
    void state_req_kitchen_select_left_ones();
    void state_req_kitchen_select_right_tens();
    void state_req_kitchen_select_right_ones();
    void state_req_display_timer();

    //signals that result from handling other signals
    void clock_done_sig();
    void display_timer_done_sig();

private:
    Ui::Microwave *ui;
    QTcpSocket* socket;
    QByteArray txBuf;
    QByteArray rxBuf;
    QTimer* timer;

    MicrowaveMsgFormat::Message* txMessage;
    MicrowaveMsgFormat::Message* rxMessage;
    MicrowaveMsgFormat::Time* time;
    quint32 powerLevel;
    bool disableClockDisplay;
    bool disableDisplayTimer;
    bool disablePowerLevel;

    QStateMachine* sm;
    QState* InitialState;
    QState* DisplayClock;
      QState* DisplayClockInit;
      QState* SetClock;
        QState* SetClockInit;
        QState* ClockSelectHourTens;
        QState* ClockSelectHourOnes;
        QState* ClockSelectMinuteTens;
        QState* ClockSelectMinuteOnes;
    QState* SetCookTimer;
      QState* SetCookTimerInit;
    QState* SetPowerLevel;
      QState* SetPowerLevelInit;
    QState* SetKitchenTimer;
      QState* SetKitchenTimerInit;
      QState* KitchenSelectMinuteTens;
      QState* KitchenSelectMinuteOnes;
      QState* KitchenSelectSecondTens;
      QState* KitchenSelectSecondOnes;
    QState* DisplayTimer;
      QState* DisplayTimerInit;

    QSignalTransition* SetCookTimerTransition;
    QSignalTransition* SetKitchenTimerTransition;
    QSignalTransition* DisplayTimerTransition;

    void SetupDisplayClockState(QState* parent = Q_NULLPTR);
    void SetupSetCookTimerState(QState* parent = Q_NULLPTR);
    void SetupSetPowerLevelState(QState* parent = Q_NULLPTR);
    void SetupSetKitchenTimerState(QState* parent = Q_NULLPTR);
    void SetupDisplayTimerState(QState* parent = Q_NULLPTR);

    void handleState(const MicrowaveMsgFormat::Message& txMessage);
    void handleSignal(const MicrowaveMsgFormat::Message& txMessage);
    void handleUpdate(const MicrowaveMsgFormat::Message& txMessage);

    void writeData();

private slots:
    void onTcpConnect();
    void onTcpDisconnect();
    void onReadyRead();
    void onBytesWritten(qint64 bytes);

    void sendTimeCook();
    void sendPowerLevel();
    void sendKitchenTimer();
    void sendClock();
    void send0();
    void send1();
    void send2();
    void send3();
    void send4();
    void send5();
    void send6();
    void send7();
    void send8();
    void send9();
    void sendStop();
    void sendStart();
    void SendStateRequest();

    void displayTime();
    void displayPowerLevel();
    void startDisplayPowerLevel2Sec();
    void stopDisplayPowerLevel2Sec();

    void onStateRequestTimeout();

    //slots for blinking stuff
    void blink_colon(const bool flag);
    void blink_left_tens(const bool flag);
    void blink_left_ones(const bool flag);
    void blink_right_tens(const bool flag);
    void blink_right_ones(const bool flag);
    void blink_power_level(const bool flag);

    void clock_done();

    void InitialStateEntry();
    void InitialStateExit();

    void DisplayClockInitEntry();
    void DisplayClockInitExit();

    void SetClockEntry();
    void SetClockExit();

    void SelectLeftTensEntry();
    void SelectLeftTensExit();

    void SelectLeftOnesEntry();
    void SelectLeftOnesExit();

    void SelectRightTensEntry();
    void SelectRightTensExit();

    void SelectRightOnesEntry();
    void SelectRightOnesExit();

    void SetCookTimerEntry();
    void SetCookTimerExit();

    void SetPowerLevelEntry();
    void SetPowerLevelExit();

    void DisplayTimerInitEntry();
    void DisplayTimerInitExit();
};
#endif // MICROWAVE_H
