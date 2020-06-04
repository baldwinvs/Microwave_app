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
    STATE   = 0x10,
    SIGNAL  = 0x20,
    UPDATE  = 0x40,
};

enum class State : uint32_t {
    NONE                        = 0x10000000,
    DISPLAY_CLOCK               = 0x10000001,   // DEV->APP
    CLOCK_SELECT_HOUR_TENS      = 0x10000002,   // DEV->APP
    CLOCK_SELECT_HOUR_ONES      = 0x10000004,   // DEV->APP
    CLOCK_SELECT_MINUTE_TENS    = 0x10000008,   // DEV->APP
    CLOCK_SELECT_MINUTE_ONES    = 0x10000010,   // DEV->APP
    SET_COOK_TIMER_INITIAL      = 0x10000020,   // DEV->APP
    SET_COOK_TIMER_FINAL        = 0x10000040,   // DEV->APP
    SET_POWER_LEVEL             = 0x10000080,   // DEV->APP
    KITCHEN_SELECT_HOUR_TENS    = 0x10000100,   // DEV->APP
    KITCHEN_SELECT_HOUR_ONES    = 0x10000200,   // DEV->APP
    KITCHEN_SELECT_MINUTE_TENS  = 0x10000400,   // DEV->APP
    KITCHEN_SELECT_MINUTE_ONES  = 0x10000800,   // DEV->APP
    DISPLAY_TIMER_RUNNING       = 0x10000100,   // DEV->APP
    DISPLAY_TIMER_PAUSED        = 0x10000200,   // DEV->APP
};

enum class Signal : uint32_t {
    NONE            = 0x20000000,
    CLOCK           = 0x20000001,   // APP<->DEV
    COOK_TIME       = 0x20000002,   // APP<->DEV
    POWER_LEVEL     = 0x20000004,   // APP<->DEV
    KITCHEN_TIMER   = 0x20000008,   // APP<->DEV
    STOP            = 0x20000010,   // APP->DEV
    START           = 0x20000020,   // APP->DEV
    DIGIT_0         = 0x20000040,   // APP->DEV
    DIGIT_1         = 0x20000080,   // APP->DEV
    DIGIT_2         = 0x20000100,   // APP->DEV
    DIGIT_3         = 0x20000200,   // APP->DEV
    DIGIT_4         = 0x20000400,   // APP->DEV
    DIGIT_5         = 0x20000800,   // APP->DEV
    DIGIT_6         = 0x20001000,   // APP->DEV
    DIGIT_7         = 0x20002000,   // APP->DEV
    DIGIT_8         = 0x20004000,   // APP->DEV
    DIGIT_9         = 0x20008000,   // APP->DEV
    BLINK_ON        = 0x20010000,   // DEV->APP
    BLINK_OFF       = 0x20020000,   // DEV->APP
    MOD_LEFT_TENS   = 0x20040000,   // DEV->APP
    MOD_LEFT_ONES   = 0x20080000,   // DEV->APP
    MOD_RIGHT_TENS  = 0x20100000,   // DEV->APP
    MOD_RIGHT_ONES  = 0x20200000,   // DEV->APP
    STATE_REQUEST   = 0x20400000    // APP->DEV
};

enum class Update : uint32_t {
    NONE            = 0x40000000,
    CLOCK           = 0x40000001,   // DEV->APP
    DISPLAY_TIMER   = 0x40000002,   // DEV->APP
    POWER_LEVEL     = 0x40000004,   // DEV->APP
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
