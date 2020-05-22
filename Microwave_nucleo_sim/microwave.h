#ifndef MICROWAVE_H
#define MICROWAVE_H

#include <QMainWindow>

//forward declarations
class QUdpSocket;
class QStateMachine;
class QState;
class QTimer;

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

    //states that modify stuff
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
    void next_digit_sig();
    void clock_done_sig();
    void digit_entered(quint32);
    void stop_sig();
    void start_sig();
    void start_display_timer_sig();
    void display_timer_done_sig();

private:
    Ui::Microwave *ui;
    QUdpSocket* inSocket;
    QUdpSocket* outSocket;
    QTimer* minuteTimer;
    QTimer* secondTimer;

    static const quint32 maxCookTimes {2};

    Time clockTime;
    Time proposedClockTime;
    Time displayTime[maxCookTimes];
    quint32 powerLevel;
    bool setting_clock; //maybe
    bool set_cook_time_initial_digit;
    bool isCooking;
    quint32 currentCookIndex;
    MOD_STATE currentModState;

    QStateMachine* sm;
    QState* display_clock;
    QState* set_cook_time;
    QState* set_power_level;
    QState* display_timer;

    QState* create_display_clock_state(QState* parent = Q_NULLPTR);
    QState* create_set_cook_time_state(QState* parent = Q_NULLPTR);
    QState* create_set_power_level_state(QState* parent = Q_NULLPTR);
    QState* create_set_kitchen_timer_state(QState* parent = Q_NULLPTR) {Q_UNUSED(parent); return Q_NULLPTR;}
    QState* create_display_timer_state(QState* parent = Q_NULLPTR);

private slots:
    void readDatagram();

    void incrementClockTime();
    void decrementTimer();
    void add30Seconds(Time& time);

    void send_current_clock();  //CURRENT_CLOCK + clockTime
    void send_current_cook();   //CURRENT_COOK + cookTime       //no usage right now
    void send_current_power();  //CURRENT_POWER + powerLevel
    void send_current_timer();  //CURRENT_TIMER + displayTime
    void send_cook_time_cmd();
    void send_clock_cmd();
    void send_stop_cmd();
    void send_digit_cmd(const quint32 digit);

    void accept_clock();
    void decline_clock();

    void start_timer();
    void stop_timer();

    void update_left_tens(quint32 digit);
    void update_left_ones(quint32 digit);
    void update_right_tens(quint32 digit);
    void update_right_ones(quint32 digit);

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

    void cook_timer_initial_entry();
    void cook_timer_initial_exit();

    void set_power_level_entry();
    void set_power_level_exit();

    void display_timer_entry();
    void display_timer_exit();
};
#endif // MICROWAVE_H
