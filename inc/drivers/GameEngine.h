#ifndef CODAL_GAME_ENGINE
#define CODAL_GAME_ENGINE

#include "CodalComponent.h"
#include "CodalConfig.h"
#include "Sprite.h"
#include "Event.h"
#include "PhysicsBody.h"

#define GAME_ENGINE_MAX_SPRITES         20
#define GAME_ENGINE_EVT_UPDATE          2

namespace codal
{
    class GameEngine : public CodalComponent
    {
        Image& displayBuffer;

        protected:
        Sprite* sprites[GAME_ENGINE_MAX_SPRITES];

        public:
        GameEngine(Image& displayBuffer, uint16_t id = DEVICE_ID_GAME_ENGINE);

        int setDisplayBuffer(Image& i);

        int add(Sprite& s);
        int remove(Sprite& s);

        void update(Event);
    };
}

#endif