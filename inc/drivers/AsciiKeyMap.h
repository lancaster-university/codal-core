#include "ErrorNo.h"
#include "KeyMap.h"

namespace codal {
class AsciiKeyMap : public KeyMap {
  public:
    AsciiKeyMap(const KeySequence* seq, uint32_t len) : KeyMap(seq, len) {}

    virtual const KeySequence* mapCharacter(uint16_t c)
    {
        if (c >= length) return NULL;

        return &map[c];
    }
};
}  // namespace codal
