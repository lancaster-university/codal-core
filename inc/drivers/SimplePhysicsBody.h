#ifndef CODAL_SIMPLE_PHYSICS_MODEL_H
#define CODAL_SIMPLE_PHYSICS_MODEL_H

#include "PhysicsBody.h"
#include "cute_c2.h"

namespace codal
{
    class SimplePhysicsBody : public PhysicsBody
    {
        static int gravity;

        public:
        c2v velocity;
        float inverse_mass;
        float restitution;

        c2v actualPosition;

        c2AABB rect;

        SimplePhysicsBody(int16_t x, int16_t y, int16_t z, int width, int height);

        void setPosition(int x, int y);

        virtual void apply();

        virtual bool intersectsWith(PhysicsBody& pb);

        virtual void collideWith(PhysicsBody&);

        virtual void setGravity(int gravity);
    };
}

#endif