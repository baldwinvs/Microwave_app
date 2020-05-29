#include "MicrowaveMessageFormat.h"
#include <cstring>
#include <cassert>
#include <cstdio>

int main()
{
    Microwave::Message msg1;
    msg1.hdr = Microwave::Header::APP;
    msg1.messageType = Microwave::Type::SIGNAL;
    msg1.signal = Microwave::Signal::DIGIT_3;

    char data[sizeof(Microwave::Time)] = {};
    memcpy(&msg1.data, data, sizeof(Microwave::Time));
    Microwave::Message msg2(reinterpret_cast<char*>(&msg1));

    assert(msg1 == msg2);

    return 0;
}