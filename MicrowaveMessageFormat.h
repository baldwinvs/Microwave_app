#ifndef MICROWAVE_MESSAGE_FORMAT_H
#define MICROWAVE_MESSAGE_FORMAT_H

#include <cstring>
#include <cstdint>

namespace MicrowaveMsgFormat {

enum class Destination : uint32_t {
    APP = 0x4D617070,   // DEV->APP
    DEV = 0x4D646576    // APP->DEV
};

enum class Type : uint8_t {
    STATE   = 0x40,
    SIGNAL  = 0x50,
    UPDATE  = 0x60,
};

enum class State : uint32_t {
    NONE                        = 0x404D3030,
    DISPLAY_CLOCK,              // DEV->APP
    CLOCK_SELECT_HOUR_TENS,     // DEV->APP
    CLOCK_SELECT_HOUR_ONES,     // DEV->APP
    CLOCK_SELECT_MINUTE_TENS,   // DEV->APP
    CLOCK_SELECT_MINUTE_ONES,   // DEV->APP
    SET_COOK_TIMER,             // DEV->APP
    SET_POWER_LEVEL,            // DEV->APP
    KITCHEN_SELECT_HOUR_TENS,   // DEV->APP
    KITCHEN_SELECT_HOUR_ONES,   // DEV->APP
    KITCHEN_SELECT_MINUTE_TENS, // DEV->APP
    KITCHEN_SELECT_MINUTE_ONES, // DEV->APP
    DISPLAY_TIMER,              // DEV->APP
};

enum class Signal : uint32_t {
    NONE            = 0x504D3030,
    CLOCK,          // APP<->DEV
    COOK_TIME,      // APP<->DEV
    POWER_LEVEL,    // APP<->DEV
    KITCHEN_TIMER,  // APP<->DEV
    STOP,           // APP->DEV
    START,          // APP->DEV
    DIGIT_0,        // APP->DEV
    DIGIT_1,        // APP->DEV
    DIGIT_2,        // APP->DEV
    DIGIT_3,        // APP->DEV
    DIGIT_4,        // APP->DEV
    DIGIT_5,        // APP->DEV
    DIGIT_6,        // APP->DEV
    DIGIT_7,        // APP->DEV
    DIGIT_8,        // APP->DEV
    DIGIT_9,        // APP->DEV
    BLINK_ON,       // DEV->APP
    BLINK_OFF,      // DEV->APP
    MOD_LEFT_TENS,  // DEV->APP
    MOD_LEFT_ONES,  // DEV->APP
    MOD_RIGHT_TENS, // DEV->APP
    MOD_RIGHT_ONES, // DEV->APP
    STATE_REQUEST   // APP->DEV
};

enum class Update : uint32_t {
    NONE            = 0x604D3030,
    CLOCK,          // DEV->APP
    DISPLAY_TIMER,  // DEV->APP
    POWER_LEVEL,    // DEV->APP
};

class Time {
public:
    Time() = default;
    ~Time() = default;

    Time(const Time&) = default;
    Time& operator=(const Time&) = default;

    Time(Time&&) = default;
    Time& operator=(Time&&) = default;

    bool operator==(const Time& rhs)
    {
        return 0 == memcmp(this, &rhs, sizeof(Time));
    }
    bool operator!=(const Time& rhs)
    {
        return 0 != memcmp(this, &rhs, sizeof(Time));
    }
    void clear()
    {
        left_tens = 0;
        left_ones = 0;
        right_tens = 0;
        right_ones = 0;
    }
    uint32_t left_tens;
    uint32_t left_ones;
    uint32_t right_tens;
    uint32_t right_ones;
};

class Message
{
public:
    Message() = default;
    Message(const char* rxData)
        : data{}
    {
        memcpy(this, rxData, sizeof(Message));
    }

    ~Message() = default;

    Message(const Message&) = default;
    Message& operator=(const Message&) = default;

    Message(Message&&) = default;
    Message& operator=(Message&&) = default;

    bool operator==(const Message& rhs)
    {
        return 0 == memcmp(this, &rhs, sizeof(Message));
    }
    bool operator!=(const Message& rhs)
    {
        return 0 != memcmp(this, &rhs, sizeof(Message));
    }

    Destination dst;
    union {
        State state;
        Signal signal;
        Update update;
    };
    char data[sizeof(Time)];
};

} // namespace MicrowaveMsgFormat

#endif // MICROWAVE_MESSAGE_FORMAT_H
