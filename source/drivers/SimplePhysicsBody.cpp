#include "SimplePhysicsBody.h"
#include "CodalDmesg.h"
#include "cute_c2.h"
#include "CodalCompat.h"

using namespace codal;

int SimplePhysicsBody::gravity = 1;

SimplePhysicsBody::SimplePhysicsBody(int16_t x, int16_t y, int16_t z, int width, int height) : PhysicsBody(x, y, z, width, height)
{
    dx = 0.0;
    dy = 0.0;

    restitution = 0.1;
    // inverse_mass = 1.0;
    inverse_mass = 1.0 / 100.0;
    setPosition(x,y);
}

void SimplePhysicsBody::setPosition(int x, int y)
{
    oldPosition = position;

    position.x = (int)(x);
    position.y = (int)(y);

    rect.min = c2V(position.x, position.y);
    rect.max = c2V(position.x + width, position.y + height);
}

void SimplePhysicsBody::apply()
{
    if (flags & (1 << PhysicsStatic))
        return;

    dy += 1.0f;

    if (dy > 5.0)
        dy = 5.0;

    if (dx > 5.0)
        dx = 5.0;

    setPosition(position.x + (int)(dx), position.y + (int)(dy));
}

bool SimplePhysicsBody::intersectsWith(PhysicsBody& pb)
{
    if (this->flags & (1 << PhysicsNoCollide) || pb.flags & (1 << PhysicsNoCollide))
        return false;

    SimplePhysicsBody* spb = (SimplePhysicsBody*)&pb;

    return c2AABBtoAABB(this->rect, spb->rect);
}

void SimplePhysicsBody::collideWith(PhysicsBody& pb)
{
    SimplePhysicsBody* spb = (SimplePhysicsBody*)&pb;

    c2Manifold manifold;
    c2AABBtoAABBManifold(this->rect, spb->rect, &manifold);

    if (manifold.count > 0)
    {
        // this->setPosition(position.x - manifold.n.x * manifold.depths[0], position.y - ((manifold.n.y * manifold.depths[0]) + 1));
        // spb->setPosition(spb->position.x - manifold.n.x * manifold.depths[0], spb->position.y - ((manifold.n.y * manifold.depths[0]) + 1));

        // this->setPosition(oldPosition.x, oldPosition.y);
        // spb->setPosition(spb->oldPosition.x, spb->oldPosition.y);

        float rvx = spb->dx - this->dx;
        float rvy = spb->dy - this->dy;

        rvx *= (float)(manifold.n.x * manifold.depths[0]);
        rvy *= (float)(manifold.n.y * manifold.depths[0]);

        float contactVel = rvx + rvy;

        if (contactVel > 0.0)
            return;

        float j = /*-(1.0 +*/ (this->restitution * spb->restitution)/*)*/ * contactVel;

        j /= this->inverse_mass + spb->inverse_mass;

        float nx = j * (float)(manifold.n.x * manifold.depths[0]);
        float ny = j * (float)(manifold.n.y * manifold.depths[0]);

        if (!(this->flags & (1 << PhysicsStatic)))
        {
            DMESG("NOT STATIC %d %d %d", width, height, flags);
            this->dx -= this->inverse_mass * nx;
            this->dy += this->inverse_mass * ny;
        }

        if (!(spb->flags & (1 << PhysicsStatic)))
        {
            spb->dx += spb->inverse_mass * nx;
            spb->dy -= spb->inverse_mass * ny;
        }
        // this->rect.max.x -= manifold.n.x * manifold.depths[0];
        // this->rect.min.y -= manifold.n.y * manifold.depths[0];
    }
    // position = oldPosition;
}

void SimplePhysicsBody::setGravity(int gravity)
{
    SimplePhysicsBody::gravity = gravity;
}

