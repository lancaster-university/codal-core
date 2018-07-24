#ifndef CODAL_PHYSICS_MODEL_H
#define CODAL_PHYSICS_MODEL_H

#include "CoordinateSystem.h"
#include "ErrorNo.h"

namespace codal
{
    enum PhysicsFlag
    {
        PhysicsStayOnScreen = 0,
        PhysicsStatic,
        PhysicsNoCollide
    };

    class PhysicsBody
    {
        public:
        int width;
        int height;

        Sample3D position;
        Sample3D oldPosition;

        uint16_t flags;

        PhysicsBody(int16_t x, int16_t y, int16_t z, int width, int height): position(x,y,z)
        {
            this->flags = 0;
            this->width = width;
            this->height = height;
        }

        virtual void apply(){}

        virtual bool intersectsWith(PhysicsBody& pb) { return false;}

        virtual void collideWith(PhysicsBody&) {}

        int setFlag(PhysicsFlag s)
        {
            flags |= (1 << s);
            return DEVICE_OK;
        }

        int unsetFlag(PhysicsFlag s)
        {
            flags &= ~(1 << s);
            return DEVICE_OK;
        }
    };
}

#endif