#ifndef CODAL_SIMPLE_PHYSICS_MODEL_H
#define CODAL_SIMPLE_PHYSICS_MODEL_H

#include "PhysicsBody.h"

namespace codal
{
    class SimplePhysicsBody : public PhysicsBody
    {
        static int gravity;

        public:
        float mass;
        float velocity;

        SimplePhysicsBody(int16_t x, int16_t y, int16_t z, int width, int height);

        virtual void apply();

        virtual bool intersectsWith(PhysicsBody& pb);

        virtual void collideWith(PhysicsBody&);

        virtual void setGravity(int gravity);
    };
}

#endif