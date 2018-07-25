#include "Radio.h"

using namespace codal;

Radio* Radio::instance = NULL;

Radio::Radio(uint16_t id)
{
    this->id = id;

    if (Radio::instance == NULL)
        Radio::instance = this;
}