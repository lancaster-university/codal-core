#include "SimplePhysicsBody.h"
#include "CodalDmesg.h"
#include "cute_c2.h"
#include "CodalCompat.h"

using namespace codal;

int SimplePhysicsBody::gravity = 1;

SimplePhysicsBody::SimplePhysicsBody(int16_t x, int16_t y, int16_t z, int width, int height) : PhysicsBody(x, y, z, width, height)
{
    velocity.x = 0.0;
    velocity.y = 0.0;

    restitution = 0.1;
    // inverse_mass = 1.0;
    inverse_mass = 1.0 / 100.0;
    setPosition(x,y);
}

void SimplePhysicsBody::setPosition(int x, int y)
{
    oldPosition = position;

    position.x = x;
    position.y = y;

    actualPosition.x = (float) x;
    actualPosition.y = (float) y;

    rect.min = c2V(position.x, position.y);
    rect.max = c2V(position.x + width, position.y + height);
}

void SimplePhysicsBody::apply()
{
    if (flags & (1 << PhysicsStatic))
        return;

    setPosition(position.x + (int)(velocity.x), position.y + (int)(velocity.y));
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

    // this will be updated in the future to take in a generic type.
    c2AABBtoAABBManifold(this->rect, spb->rect, &manifold);

    // are we properly intersecting yet?
    if (manifold.count > 0)
    {
        // calculate relative velocity
        c2v rel = c2Sub(spb->velocity, this->velocity);
        float contactVel = c2Dot(rel, manifold.n);

        // if we are moving apart already, skip computation
        if (contactVel <= 0.0)
        {
            // calculate the energy to apply to the normal given by the manifold (manifold.n == the normal vector at intersection)
            // the restitution dictates the energy retained... the  < the restitution value the < the bounce.
            float j = -(1.0 + min(this->restitution, spb->restitution)) * contactVel;
            j /= this->inverse_mass + spb->inverse_mass;

            // apply our energy to our normal
            c2v impulse = c2Mulvs(manifold.n, j);

            // update velocities
            this->velocity = c2Mulvs(impulse, this->inverse_mass);
            spb->velocity = c2Mulvs(impulse, spb->inverse_mass);
        }

        // we're intersecting, prevent sinking (with slight adjustments)
        static float percent = 0.2; // usually 20% to 80%
        static float slop = 0.01; // usually 0.01 to 0.1

        // here we calculate the depth, and how much depth is tolerable...
        c2v correction = c2Mulvs(manifold.n, (max(manifold.depths[0] - slop, 0.0f ) / (this->inverse_mass + spb->inverse_mass)) * percent);

        // move them apart by our inverse mass
        c2v aCorrect = c2Mulvs(correction, this->inverse_mass);
        c2v bCorrect = c2Mulvs(correction, spb->inverse_mass);

        // update our positions.
        this->actualPosition = c2Sub(this->actualPosition, aCorrect);
        spb->actualPosition = c2Add(spb->actualPosition, bCorrect);

        this->position.x = (int)this->actualPosition.x;
        this->position.y = (int)this->actualPosition.y;
        spb->position.x = (int)spb->actualPosition.x;
        spb->position.y = (int)spb->actualPosition.y;
    }
}

void SimplePhysicsBody::setGravity(int gravity)
{
    SimplePhysicsBody::gravity = gravity;
}

