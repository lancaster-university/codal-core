#include "SimplePhysicsBody.h"
#include "CodalDmesg.h"

using namespace codal;

int SimplePhysicsBody::gravity = 1;

SimplePhysicsBody::SimplePhysicsBody(int16_t x, int16_t y, int16_t z, int width, int height) : PhysicsBody(x, y, z, width, height)
{
    velocity = 0;
    mass = 1;
}

void SimplePhysicsBody::apply()
{
    if (flags & (1 << PhysicsStatic))
        return;

    oldPosition = position;
    position.y += gravity;
}

bool SimplePhysicsBody::intersectsWith(PhysicsBody& pb)
{
    if (this->flags & (1 << PhysicsNoCollide) || pb.flags & (1 << PhysicsNoCollide))
        return false;

    // DMESG("INTER");
    // if (pb.position.y > 160)
    //     while(1);

    int ax1 = this->position.x;
    int ay1 = this->position.y;

    int ax2 = this->position.x + width;
    int ay2 = this->position.y + height;

    int bx1 = pb.position.x;
    int by1 = pb.position.y;

    int bx2 = pb.position.x + pb.width;
    int by2 = pb.position.y + pb.height;
    // DMESG("a: (%d,%d) (%d,%d)",ax1, ay1, ax2, ay2);
    // DMESG("b: (%d,%d) (%d,%d)",bx1, by1, bx2, by2);
    // while(1);
    // DMESG("chk : %d %d %d %d",ax1 < bx2,  ax2 > bx1, ay1 < by2, ay2 > by1);

    return (ax1 < bx2 && ax2 > bx1 && ay1 < by2 && ay2 > by1);
}

void SimplePhysicsBody::collideWith(PhysicsBody& pb)
{
    // if we don't move, we don't collide.
    if (this->flags & (1 << PhysicsStatic))
    {
        DMESG("STATIC %d %d %d", width, height, flags);
        return;
    }
    DMESG("SETTING OLD");
    velocityRelative = this->velocity - pb.velocity;

    position = oldPosition;
}

void SimplePhysicsBody::setGravity(int gravity)
{
    SimplePhysicsBody::gravity = gravity;
}

