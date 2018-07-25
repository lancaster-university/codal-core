#include "GameEngine.h"
#include "EventModel.h"
#include "Timer.h"
#include "CodalDmesg.h"
#include "Display.h"

using namespace codal;

GameEngine::GameEngine(Image& displayBuffer, uint16_t id) : displayBuffer(displayBuffer)
{
    DMESG("GE CONS");
    memset(sprites, 0, GAME_ENGINE_MAX_SPRITES * sizeof(Sprite*));
    system_timer_event_every(4, id, GAME_ENGINE_EVT_UPDATE);

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(DEVICE_ID_DISPLAY, DISPLAY_EVT_RENDER_START, this, &GameEngine::update, MESSAGE_BUS_LISTENER_IMMEDIATE);
}

int GameEngine::setDisplayBuffer(Image& i)
{
    this->displayBuffer = i;
    return DEVICE_OK;
}

int GameEngine::add(Sprite& s)
{
    int i = 0;
    for (i = 0; i < GAME_ENGINE_MAX_SPRITES; i++)
    {
        if (sprites[i] == NULL)
        {
            sprites[i] = &s;
            break;
        }
    }

    if (i == GAME_ENGINE_MAX_SPRITES)
        return DEVICE_NO_RESOURCES;

    return DEVICE_OK;
}

int GameEngine::remove(Sprite& s)
{
    int i = 0;
    for (i = 0; i < GAME_ENGINE_MAX_SPRITES; i++)
    {
        if (sprites[i] == &s)
        {
            sprites[i] = NULL;
            break;
        }
    }

    if (i == GAME_ENGINE_MAX_SPRITES)
        return DEVICE_INVALID_PARAMETER;

    return DEVICE_OK;
}

void GameEngine::update(Event)
{
    displayBuffer.clear();

    for (int i = 0; i < GAME_ENGINE_MAX_SPRITES; i++)
    {
        if (sprites[i] == NULL)
            continue;

        sprites[i]->body.apply();
    }

    for (int i = 0; i < GAME_ENGINE_MAX_SPRITES; i++)
    {
        if (sprites[i] == NULL)
            continue;

        for (int j = i + 1; j < GAME_ENGINE_MAX_SPRITES; j++)
        {
            if (sprites[j] == NULL || sprites[j] == sprites[i])
                continue;

            if (sprites[i]->body.intersectsWith(sprites[j]->body))
            {
                DMESG("COLLISION: %p %p", &sprites[j]->body, &sprites[i]->body);
                sprites[i]->body.collideWith(sprites[j]->body);
                // sprites[j]->body.collideWith(sprites[i]->body);
            }
        }
    }

    for (int i = 0; i < GAME_ENGINE_MAX_SPRITES; i++)
        sprites[i]->draw(displayBuffer);
}

