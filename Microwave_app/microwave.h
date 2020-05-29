#ifndef MICROWAVE_H
#define MICROWAVE_H

#include <QMainWindow>

//forward declarations
class QUdpSocket;
class QStateMachine;
class QState;
class QTimer;

namespace MicrowaveMsgFormat { class Message; }

QT_BEGIN_NAMESPACE
namespace Ui { class Microwave; }
QT_END_NAMESPACE

class Microwave : public QMainWindow
{
    Q_OBJECT

public:
    Microwave(QWidget *parent = nullptr);
    ~Microwave();

    class Time
    {
    public:
        inline Time()
            : left_tens{},
              left_ones{},
              right_tens{},
              right_ones{}
        {}

        quint32 left_tens;
        quint32 left_ones;
        quint32 right_tens;
        quint32 right_ones;
    };

    enum class MOD_STATE : uint32_t {
        NONE,
        SET_CLOCK,
        SET_COOK,
        SET_POWER,
        SET_KITCHEN
    };

signals:
    void clock_sig();
    void cook_time_sig();
    void power_level_sig();
    void kitchen_timer_sig();
    void next_digit_sig();
    void clock_done_sig();
    void stop_sig();
    void start_sig();
    void select_left_tens_sig();
    void select_left_ones_sig();
    void select_right_tens_sig();
    void select_right_ones_sig();
    void display_timer_done_sig();
    void blink_sig();

private:
    Ui::Microwave *ui;
    QUdpSocket* inSocket;
    QUdpSocket* outSocket;
    QTimer* blinkTimer;
    QTimer* colonBlinkTimer;
    MicrowaveMsgFormat::Message* msg;

    Time time;
    quint32 powerLevel;
    bool blinkage;
    bool colon_blink;
    bool setting_clock;
    MOD_STATE currentModState;

    QStateMachine* sm;
    QState* display_clock;
    QState* set_cook_time;
    QState* set_power_level;
    QState* set_kitchen_timer;
    QState* display_timer;

    QState* create_display_clock_state(QState* parent = Q_NULLPTR);
    QState* create_set_cook_time_state(QState* parent = Q_NULLPTR);
    QState* create_set_power_level_state(QState* parent = Q_NULLPTR);
    QState* create_set_kitchen_timer_state(QState* parent = Q_NULLPTR);
    QState* create_display_timer_state(QState* parent = Q_NULLPTR);

    void handleState(const MicrowaveMsgFormat::Message& msg);
    void handleSignal(const MicrowaveMsgFormat::Message& msg);
    void handleUpdate(const MicrowaveMsgFormat::Message& msg);

    void writeData(const MicrowaveMsgFormat::Message* const msg);

private slots:
    void readDatagram();

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
    void sendCurrentClockReq();

    void displayClock();
    void displayPowerLevel();

    void blink_colon();

    void blink_left_tens();
    void blink_left_ones();
    void blink_right_tens();
    void blink_right_ones();

    void accept_clock();
    void decline_clock();

    void display_clock_entry();
    void display_clock_exit();

    void set_clock_entry();
    void set_clock_exit();

    void select_hour_tens_entry();
    void select_hour_tens_exit();

    void select_hour_ones_entry();
    void select_hour_ones_exit();

    void select_minute_tens_entry();
    void select_minute_tens_exit();

    void select_minute_ones_entry();
    void select_minute_ones_exit();

    void set_cook_time_entry();
    void set_cook_time_exit();

    void set_power_level_entry();
    void set_power_level_exit();

    void display_timer_entry();
    void display_timer_exit();
};
#endif // MICROWAVE_H
