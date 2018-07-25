#include "cute_c2.h"

int c2Collided(const void* A, const c2x* ax, C2_TYPE typeA, const void* B, const c2x* bx, C2_TYPE typeB)
{
    switch (typeA)
    {
    case C2_CIRCLE:
        switch (typeB)
        {
        case C2_CIRCLE:  return c2CircletoCircle(*(c2Circle*)A, *(c2Circle*)B);
        case C2_AABB:    return c2CircletoAABB(*(c2Circle*)A, *(c2AABB*)B);
        case C2_CAPSULE: return c2CircletoCapsule(*(c2Circle*)A, *(c2Capsule*)B);
        case C2_POLY:    return c2CircletoPoly(*(c2Circle*)A, (const c2Poly*)B, bx);
        default:         return 0;
        }
        break;

    case C2_AABB:
        switch (typeB)
        {
        case C2_CIRCLE:  return c2CircletoAABB(*(c2Circle*)B, *(c2AABB*)A);
        case C2_AABB:    return c2AABBtoAABB(*(c2AABB*)A, *(c2AABB*)B);
        case C2_CAPSULE: return c2AABBtoCapsule(*(c2AABB*)A, *(c2Capsule*)B);
        case C2_POLY:    return c2AABBtoPoly(*(c2AABB*)A, (const c2Poly*)B, bx);
        default:         return 0;
        }
        break;

    case C2_CAPSULE:
        switch (typeB)
        {
        case C2_CIRCLE:  return c2CircletoCapsule(*(c2Circle*)B, *(c2Capsule*)A);
        case C2_AABB:    return c2AABBtoCapsule(*(c2AABB*)B, *(c2Capsule*)A);
        case C2_CAPSULE: return c2CapsuletoCapsule(*(c2Capsule*)A, *(c2Capsule*)B);
        case C2_POLY:    return c2CapsuletoPoly(*(c2Capsule*)A, (const c2Poly*)B, bx);
        default:         return 0;
        }
        break;

    case C2_POLY:
        switch (typeB)
        {
        case C2_CIRCLE:  return c2CircletoPoly(*(c2Circle*)B, (const c2Poly*)A, ax);
        case C2_AABB:    return c2AABBtoPoly(*(c2AABB*)B, (const c2Poly*)A, ax);
        case C2_CAPSULE: return c2CapsuletoPoly(*(c2Capsule*)B, (const c2Poly*)A, ax);
        case C2_POLY:    return c2PolytoPoly((const c2Poly*)A, ax, (const c2Poly*)B, bx);
        default:         return 0;
        }
        break;

    default:
        return 0;
    }
}

void c2Collide(const void* A, const c2x* ax, C2_TYPE typeA, const void* B, const c2x* bx, C2_TYPE typeB, c2Manifold* m)
{
    m->count = 0;

    switch (typeA)
    {
    case C2_CIRCLE:
        switch (typeB)
        {
        case C2_CIRCLE:  c2CircletoCircleManifold(*(c2Circle*)A, *(c2Circle*)B, m); break;
        case C2_AABB:    c2CircletoAABBManifold(*(c2Circle*)A, *(c2AABB*)B, m); break;
        case C2_CAPSULE: c2CircletoCapsuleManifold(*(c2Circle*)A, *(c2Capsule*)B, m); break;
        case C2_POLY:    c2CircletoPolyManifold(*(c2Circle*)A, (const c2Poly*)B, bx, m); break;
        }
        break;

    case C2_AABB:
        switch (typeB)
        {
        case C2_CIRCLE:  c2CircletoAABBManifold(*(c2Circle*)B, *(c2AABB*)A, m); m->n = c2Neg(m->n); break;
        case C2_AABB:    c2AABBtoAABBManifold(*(c2AABB*)A, *(c2AABB*)B, m); break;
        case C2_CAPSULE: c2AABBtoCapsuleManifold(*(c2AABB*)A, *(c2Capsule*)B, m); break;
        case C2_POLY:    c2AABBtoPolyManifold(*(c2AABB*)A, (const c2Poly*)B, bx, m); break;
        }
        break;

    case C2_CAPSULE:
        switch (typeB)
        {
        case C2_CIRCLE:  c2CircletoCapsuleManifold(*(c2Circle*)B, *(c2Capsule*)A, m); m->n = c2Neg(m->n); break;
        case C2_AABB:    c2AABBtoCapsuleManifold(*(c2AABB*)B, *(c2Capsule*)A, m); m->n = c2Neg(m->n); break;
        case C2_CAPSULE: c2CapsuletoCapsuleManifold(*(c2Capsule*)A, *(c2Capsule*)B, m); break;
        case C2_POLY:    c2CapsuletoPolyManifold(*(c2Capsule*)A, (const c2Poly*)B, bx, m); break;
        }
        break;

    case C2_POLY:
        switch (typeB)
        {
        case C2_CIRCLE:  c2CircletoPolyManifold(*(c2Circle*)B, (const c2Poly*)A, ax, m); m->n = c2Neg(m->n); break;
        case C2_AABB:    c2AABBtoPolyManifold(*(c2AABB*)B, (const c2Poly*)A, ax, m); m->n = c2Neg(m->n); break;
        case C2_CAPSULE: c2CapsuletoPolyManifold(*(c2Capsule*)B, (const c2Poly*)A, ax, m); m->n = c2Neg(m->n); break;
        case C2_POLY:    c2PolytoPolyManifold((const c2Poly*)A, ax, (const c2Poly*)B, bx, m); break;
        }
        break;
    }
}

int c2CastRay(c2Ray A, const void* B, const c2x* bx, C2_TYPE typeB, c2Raycast* out)
{
    switch (typeB)
    {
    case C2_CIRCLE:  return c2RaytoCircle(A, *(c2Circle*)B, out);
    case C2_AABB:    return c2RaytoAABB(A, *(c2AABB*)B, out);
    case C2_CAPSULE: return c2RaytoCapsule(A, *(c2Capsule*)B, out);
    case C2_POLY:    return c2RaytoPoly(A, (const c2Poly*)B, bx, out);
    }

    return 0;
}

#define C2_GJK_ITERS 20

typedef struct
{
    float radius;
    int count;
    c2v verts[C2_MAX_POLYGON_VERTS];
} c2Proxy;

typedef struct
{
    c2v sA;
    c2v sB;
    c2v p;
    float u;
    int iA;
    int iB;
} c2sv;

typedef struct
{
    c2sv a, b, c, d;
    float div;
    int count;
} c2Simplex;

static C2_INLINE void c2MakeProxy(const void* shape, C2_TYPE type, c2Proxy* p)
{
    switch (type)
    {
    case C2_CIRCLE:
    {
        c2Circle* c = (c2Circle*)shape;
        p->radius = c->r;
        p->count = 1;
        p->verts[0] = c->p;
    }	break;

    case C2_AABB:
    {
        c2AABB* bb = (c2AABB*)shape;
        p->radius = 0;
        p->count = 4;
        c2BBVerts(p->verts, bb);
    }	break;

    case C2_CAPSULE:
    {
        c2Capsule* c = (c2Capsule*)shape;
        p->radius = c->r;
        p->count = 2;
        p->verts[0] = c->a;
        p->verts[1] = c->b;
    }	break;

    case C2_POLY:
    {
        c2Poly* poly = (c2Poly*)shape;
        p->radius = 0;
        p->count = poly->count;
        for (int i = 0; i < p->count; ++i) p->verts[i] = poly->verts[i];
    }	break;
    }
}

static C2_INLINE int c2Support(const c2v* verts, int count, c2v d)
{
    int imax = 0;
    float dmax = c2Dot(verts[0], d);

    for (int i = 1; i < count; ++i)
    {
        float dot = c2Dot(verts[i], d);
        if (dot > dmax)
        {
            imax = i;
            dmax = dot;
        }
    }

    return imax;
}

#define C2_BARY(n, x) c2Mulvs(s->n.x, (den * s->n.u))
#define C2_BARY2(x) c2Add(C2_BARY(a, x), C2_BARY(b, x))
#define C2_BARY3(x) c2Add(c2Add(C2_BARY(a, x), C2_BARY(b, x)), C2_BARY(c, x))

static C2_INLINE c2v c2L(c2Simplex* s)
{
    float den = 1.0f / s->div;
    switch (s->count)
    {
    case 1: return s->a.p;
    case 2: return C2_BARY2(p);
    case 3: return C2_BARY3(p);
    default: return c2V(0, 0);
    }
}

static C2_INLINE void c2Witness(c2Simplex* s, c2v* a, c2v* b)
{
    float den = 1.0f / s->div;
    switch (s->count)
    {
    case 1: *a = s->a.sA; *b = s->a.sB; break;
    case 2: *a = C2_BARY2(sA); *b = C2_BARY2(sB); break;
    case 3: *a = C2_BARY3(sA); *b = C2_BARY3(sB); break;
    default: *a = c2V(0, 0); *b = c2V(0, 0);
    }
}

static C2_INLINE c2v c2D(c2Simplex* s)
{
    switch (s->count)
    {
    case 1: return c2Neg(s->a.p);
    case 2:
    {
        c2v ab = c2Sub(s->b.p, s->a.p);
        if (c2Det2(ab, c2Neg(s->a.p)) > 0) return c2Skew(ab);
        return c2CCW90(ab);
    }
    case 3:
    default: return c2V(0, 0);
    }
}

static C2_INLINE void c22(c2Simplex* s)
{
    c2v a = s->a.p;
    c2v b = s->b.p;
    float u = c2Dot(b, c2Norm(c2Sub(b, a)));
    float v = c2Dot(a, c2Norm(c2Sub(a, b)));

    if (v <= 0)
    {
        s->a.u = 1.0f;
        s->div = 1.0f;
        s->count = 1;
    }

    else if (u <= 0)
    {
        s->a = s->b;
        s->a.u = 1.0f;
        s->div = 1.0f;
        s->count = 1;
    }

    else
    {
        s->a.u = u;
        s->b.u = v;
        s->div = u + v;
        s->count = 2;
    }
}

static C2_INLINE void c23(c2Simplex* s)
{
    c2v a = s->a.p;
    c2v b = s->b.p;
    c2v c = s->c.p;

    float uAB = c2Dot(b, c2Norm(c2Sub(b, a)));
    float vAB = c2Dot(a, c2Norm(c2Sub(a, b)));
    float uBC = c2Dot(c, c2Norm(c2Sub(c, b)));
    float vBC = c2Dot(b, c2Norm(c2Sub(b, c)));
    float uCA = c2Dot(a, c2Norm(c2Sub(a, c)));
    float vCA = c2Dot(c, c2Norm(c2Sub(c, a)));
    float area = c2Det2(c2Norm(c2Sub(b, a)), c2Norm(c2Sub(c, a)));
    float uABC = c2Det2(b, c) * area;
    float vABC = c2Det2(c, a) * area;
    float wABC = c2Det2(a, b) * area;

    if (vAB <= 0 && uCA <= 0)
    {
        s->a.u = 1.0f;
        s->div = 1.0f;
        s->count = 1;
    }

    else if (uAB <= 0 && vBC <= 0)
    {
        s->a = s->b;
        s->a.u = 1.0f;
        s->div = 1.0f;
        s->count = 1;
    }

    else if (uBC <= 0 && vCA <= 0)
    {
        s->a = s->c;
        s->a.u = 1.0f;
        s->div = 1.0f;
        s->count = 1;
    }

    else if (uAB > 0 && vAB > 0 && wABC <= 0)
    {
        s->a.u = uAB;
        s->b.u = vAB;
        s->div = uAB + vAB;
        s->count = 2;
    }

    else if (uBC > 0 && vBC > 0 && uABC <= 0)
    {
        s->a = s->b;
        s->b = s->c;
        s->a.u = uBC;
        s->b.u = vBC;
        s->div = uBC + vBC;
        s->count = 2;
    }

    else if (uCA > 0 && vCA > 0 && vABC <= 0)
    {
        s->b = s->a;
        s->a = s->c;
        s->a.u = uCA;
        s->b.u = vCA;
        s->div = uCA + vCA;
        s->count = 2;
    }

    else
    {
        s->a.u = uABC;
        s->b.u = vABC;
        s->c.u = wABC;
        s->div = uABC + vABC + wABC;
        s->count = 3;
    }
}

#include <float.h>

// Please see http://box2d.org/downloads/ under GDC 2010 for Erin's demo code
// and PDF slides for documentation on the GJK algorithm.
float c2GJK(const void* A, C2_TYPE typeA, const c2x* ax_ptr, const void* B, C2_TYPE typeB, const c2x* bx_ptr, c2v* outA, c2v* outB, int use_radius)
{
    c2x ax;
    c2x bx;
    if (typeA != C2_POLY || !ax_ptr) ax = c2xIdentity();
    else ax = *ax_ptr;
    if (typeB != C2_POLY || !bx_ptr) bx = c2xIdentity();
    else bx = *bx_ptr;

    c2Proxy pA;
    c2Proxy pB;
    c2MakeProxy(A, typeA, &pA);
    c2MakeProxy(B, typeB, &pB);

    c2Simplex s;
    s.a.iA = 0;
    s.a.iB = 0;
    s.a.sA = c2Mulxv(ax, pA.verts[0]);
    s.a.sB = c2Mulxv(bx, pB.verts[0]);
    s.a.p = c2Sub(s.a.sB, s.a.sA);
    s.a.u = 1.0f;
    s.div = 1.0f;
    s.count = 1;

    c2sv* verts = &s.a;
    int saveA[3], saveB[3];
    int save_count = 0;
    float d0 = FLT_MAX;
    float d1 = FLT_MAX;
    int iter = 0;
    int hit = 0;
    while (iter < C2_GJK_ITERS)
    {
        save_count = s.count;
        for (int i = 0; i < save_count; ++i)
        {
            saveA[i] = verts[i].iA;
            saveB[i] = verts[i].iB;
        }

        switch (s.count)
        {
        case 1: break;
        case 2: c22(&s); break;
        case 3: c23(&s); break;
        }

        if (s.count == 3)
        {
            hit = 1;
            break;
        }

        c2v p = c2L(&s);
        d1 = c2Dot(p, p);

        if (d1 > d0) break;
        d0 = d1;

        c2v d = c2D(&s);
        if (c2Dot(d, d) < FLT_EPSILON * FLT_EPSILON) break;

        int iA = c2Support(pA.verts, pA.count, c2MulrvT(ax.r, c2Neg(d)));
        c2v sA = c2Mulxv(ax, pA.verts[iA]);
        int iB = c2Support(pB.verts, pB.count, c2MulrvT(bx.r, d));
        c2v sB = c2Mulxv(bx, pB.verts[iB]);

        ++iter;

        int dup = 0;
        for (int i = 0; i < save_count; ++i)
        {
            if (iA == saveA[i] && iB == saveB[i])
            {
                dup = 1;
                break;
            }
        }
        if (dup) break;

        c2sv* v = verts + s.count;
        v->iA = iA;
        v->sA = sA;
        v->iB = iB;
        v->sB = sB;
        v->p = c2Sub(v->sB, v->sA);
        ++s.count;
    }

    c2v a, b;
    c2Witness(&s, &a, &b);
    float dist = c2Len(c2Sub(a, b));

    if (hit)
    {
        a = b;
        dist = 0;
    }

    else if (use_radius)
    {
        float rA = pA.radius;
        float rB = pB.radius;

        if (dist > rA + rB && dist > FLT_EPSILON)
        {
            dist -= rA + rB;
            c2v n = c2Norm(c2Sub(b, a));
            a = c2Add(a, c2Mulvs(n, rA));
            b = c2Sub(b, c2Mulvs(n, rB));
        }

        else
        {
            c2v p = c2Mulvs(c2Add(a, b), 0.5f);
            a = p;
            b = p;
            dist = 0;
        }
    }

    if (outA) *outA = a;
    if (outB) *outB = b;
    return dist;
}

int c2Hull(c2v* verts, int count)
{
    if (count <= 2) return 0;
    count = c2Min(C2_MAX_POLYGON_VERTS, count);

    int right = 0;
    float xmax = verts[0].x;
    for (int i = 1; i < count; ++i)
    {
        float x = verts[i].x;
        if (x > xmax)
        {
            xmax = x;
            right = i;
        }

        else if (x == xmax)
        if (verts[i].y < verts[right].y) right = i;
    }

    int hull[C2_MAX_POLYGON_VERTS];
    int out_count = 0;
    int index = right;

    while (1)
    {
        hull[out_count] = index;
        int next = 0;

        for (int i = 1; i < count; ++i)
        {
            if (next == index)
            {
                next = i;
                continue;
            }

            c2v e1 = c2Sub(verts[next], verts[hull[out_count]]);
            c2v e2 = c2Sub(verts[i], verts[hull[out_count]]);
            float c = c2Det2(e1, e2);
            if(c < 0) next = i;
            if (c == 0 && c2Dot(e2, e2) > c2Dot(e1, e1)) next = i;
        }

        ++out_count;
        index = next;
        if (next == right) break;
    }

    c2v hull_verts[C2_MAX_POLYGON_VERTS];
    for (int i = 0; i < out_count; ++i) hull_verts[i] = verts[hull[i]];
    memcpy(verts, hull_verts, sizeof(c2v) * out_count);
    return out_count;
}

void c2Norms(c2v* verts, c2v* norms, int count)
{
    for (int  i = 0; i < count; ++i)
    {
        int a = i;
        int b = i + 1 < count ? i + 1 : 0;
        c2v e = c2Sub(verts[b], verts[a]);
        norms[i] = c2Norm(c2CCW90(e));
    }
}

void c2MakePoly(c2Poly* p)
{
    p->count = c2Hull(p->verts, p->count);
    c2Norms(p->verts, p->norms, p->count);
}

int c2CircletoCircle(c2Circle A, c2Circle B)
{
    c2v c = c2Sub(B.p, A.p);
    float d2 = c2Dot(c, c);
    float r2 = A.r + B.r;
    r2 = r2 * r2;
    return d2 < r2;
}

int c2CircletoAABB(c2Circle A, c2AABB B)
{
    c2v L = c2Clampv(A.p, B.min, B.max);
    c2v ab = c2Sub(A.p, L);
    float d2 = c2Dot(ab, ab);
    float r2 = A.r * A.r;
    return d2 < r2;
}

int c2AABBtoAABB(c2AABB A, c2AABB B)
{
    int d0 = B.max.x < A.min.x;
    int d1 = A.max.x < B.min.x;
    int d2 = B.max.y < A.min.y;
    int d3 = A.max.y < B.min.y;
    return !(d0 | d1 | d2 | d3);
}

// see: http://www.randygaul.net/2014/07/23/distance-point-to-line-segment/
int c2CircletoCapsule(c2Circle A, c2Capsule B)
{
    c2v n = c2Sub(B.b, B.a);
    c2v ap = c2Sub(A.p, B.a);
    float da = c2Dot(ap, n);
    float d2;

    if (da < 0) d2 = c2Dot(ap, ap);
    else
    {
        float db = c2Dot(c2Sub(A.p, B.b), n);
        if (db < 0)
        {
            c2v e = c2Sub(ap, c2Mulvs(n, (da / c2Dot(n, n))));
            d2 = c2Dot(e, e);
        }
        else
        {
            c2v bp = c2Sub(A.p, B.b);
            d2 = c2Dot(bp, bp);
        }
    }

    float r = A.r + B.r;
    return d2 < r * r;
}

int c2AABBtoCapsule(c2AABB A, c2Capsule B)
{
    if (c2GJK(&A, C2_AABB, 0, &B, C2_CAPSULE, 0, 0, 0, 1)) return 0;
    return 1;
}

int c2CapsuletoCapsule(c2Capsule A, c2Capsule B)
{
    if (c2GJK(&A, C2_CAPSULE, 0, &B, C2_CAPSULE, 0, 0, 0, 1)) return 0;
    return 1;
}

int c2CircletoPoly(c2Circle A, const c2Poly* B, const c2x* bx)
{
    if (c2GJK(&A, C2_CIRCLE, 0, B, C2_POLY, bx, 0, 0, 1)) return 0;
    return 1;
}

int c2AABBtoPoly(c2AABB A, const c2Poly* B, const c2x* bx)
{
    if (c2GJK(&A, C2_AABB, 0, B, C2_POLY, bx, 0, 0, 1)) return 0;
    return 1;
}

int c2CapsuletoPoly(c2Capsule A, const c2Poly* B, const c2x* bx)
{
    if (c2GJK(&A, C2_CAPSULE, 0, B, C2_POLY, bx, 0, 0, 1)) return 0;
    return 1;
}

int c2PolytoPoly(const c2Poly* A, const c2x* ax, const c2Poly* B, const c2x* bx)
{
    if (c2GJK(A, C2_POLY, ax, B, C2_POLY, bx, 0, 0, 1)) return 0;
    return 1;
}

int c2RaytoCircle(c2Ray A, c2Circle B, c2Raycast* out)
{
    c2v p = B.p;
    c2v m = c2Sub(A.p, p);
    float c = c2Dot(m, m) - B.r * B.r;
    float b = c2Dot(m, A.d);
    float disc = b * b - c;
    if (disc < 0) return 0;

    float t = -b - c2Sqrt(disc);
    if (t >= 0 && t <= A.t)
    {
        out->t = t;
        c2v impact = c2Impact(A, t);
        out->n = c2Norm(c2Sub(impact, p));
        return 1;
    }
    return 0;
}

int c2RaytoAABB(c2Ray A, c2AABB B, c2Raycast* out)
{
    c2v inv = c2V(1.0f / A.d.x, 1.0f / A.d.y);
    c2v d0 = c2Mulvv(c2Sub(B.min, A.p), inv);
    c2v d1 = c2Mulvv(c2Sub(B.max, A.p), inv);
    c2v v0 = c2Minv(d0, d1);
    c2v v1 = c2Maxv(d0, d1);
    float lo = c2Hmax(v0);
    float hi = c2Hmin(v1);

    if (hi >= 0 && hi >= lo && lo <= A.t)
    {
        c2v c = c2Mulvs(c2Add(B.min, B.max), 0.5f);
        c = c2Sub(c2Impact(A, lo), c);
        c2v abs_c = c2Absv(c);
        if (abs_c.x > abs_c.y) out->n = c2V(c2Sign(c.x), 0);
        else out->n = c2V(0, c2Sign(c.y));
        out->t = lo;
        return 1;
    }
    return 0;
}

int c2RaytoCapsule(c2Ray A, c2Capsule B, c2Raycast* out)
{
    c2m M;
    M.y = c2Norm(c2Sub(B.b, B.a));
    M.x = c2CCW90(M.y);

    // rotate capsule to origin, along Y axis
    // rotate the ray same way
    c2v yBb = c2MulmvT(M, c2Sub(B.b, B.a));
    c2v yAp = c2MulmvT(M, c2Sub(A.p, B.a));
    c2v yAd = c2MulmvT(M, A.d);
    c2v yAe = c2Add(yAp, c2Mulvs(yAd, A.t));

    if (yAe.x * yAp.x < 0 || c2Min(c2Abs(yAe.x), c2Abs(yAp.x)) < B.r)
    {
        float c = yAp.x > 0 ? B.r : -B.r;
        float d = (yAe.x - yAp.x);
        float t = (c - yAp.x) / d;
        float y = yAp.y + (yAe.y - yAp.y) * t;

        // hit bottom half-circle
        if (y < 0)
        {
            c2Circle C;
            C.p = B.a;
            C.r = B.r;
            return c2RaytoCircle(A, C, out);
        }

        // hit top-half circle
        else if (y > yBb.y)
        {
            c2Circle C;
            C.p = B.b;
            C.r = B.r;
            return c2RaytoCircle(A, C, out);
        }

        // hit the middle of capsule
        else
        {
            out->n = c > 0 ? M.x : c2Skew(M.y);
            out->t = t * A.t;
            return 1;
        }
    }

    return 0;
}

int c2RaytoPoly(c2Ray A, const c2Poly* B, const c2x* bx_ptr, c2Raycast* out)
{
    c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
    c2v p = c2MulxvT(bx, A.p);
    c2v d = c2MulrvT(bx.r, A.d);
    float lo = 0;
    float hi = A.t;
    int index = ~0;

    // test ray to each plane, tracking lo/hi times of intersection
    for (int i = 0; i < B->count; ++i)
    {
        float num = c2Dot(B->norms[i], c2Sub(B->verts[i], p));
        float den = c2Dot(B->norms[i], d);
        if (den == 0 && num < 0) return 0;
        else
        {
            if (den < 0 && num < lo * den)
            {
                lo = num / den;
                index = i;
            }
            else if (den > 0 && num < hi * den) hi = num / den;
        }
        if (hi < lo) return 0;
    }

    if (index != ~0)
    {
        out->t = lo;
        out->n = c2Mulrv(bx.r, B->norms[index]);
        return 1;
    }

    return 0;
}

void c2CircletoCircleManifold(c2Circle A, c2Circle B, c2Manifold* m)
{
    m->count = 0;
    c2v d = c2Sub(B.p, A.p);
    float d2 = c2Dot(d, d);
    float r = A.r + B.r;
    if (d2 < r * r)
    {
        float l = c2Sqrt(d2);
        c2v n = l != 0 ? c2Mulvs(d, 1.0f / l) : c2V(0, 1.0f);
        m->count = 1;
        m->depths[0] = r - l;
        m->contact_points[0] = c2Sub(B.p, c2Mulvs(n, B.r));
        m->n = n;
    }
}

void c2CircletoAABBManifold(c2Circle A, c2AABB B, c2Manifold* m)
{
    m->count = 0;
    c2v L = c2Clampv(A.p, B.min, B.max);
    c2v ab = c2Sub(L, A.p);
    float d2 = c2Dot(ab, ab);
    float r2 = A.r * A.r;
    if (d2 < r2)
    {
        // shallow (center of circle not inside of AABB)
        if (d2 != 0)
        {
            float d = c2Sqrt(d2);
            c2v n = c2Norm(ab);
            m->count = 1;
            m->depths[0] = A.r - d;
            m->contact_points[0] = c2Add(A.p, c2Mulvs(n, d));
            m->n = n;
        }

        // deep (center of circle inside of AABB)
        // clamp circle's center to edge of AABB, then form the manifold
        else
        {
            c2v mid = c2Mulvs(c2Add(B.min, B.max), 0.5f);
            c2v e = c2Mulvs(c2Sub(B.max, B.min), 0.5f);
            c2v d = c2Sub(A.p, mid);
            c2v abs_d = c2Absv(d);

            float x_overlap = e.x - abs_d.x;
            float y_overlap = e.y - abs_d.y;

            float depth;
            c2v n;

            if (x_overlap < y_overlap)
            {
                depth = x_overlap;
                n = c2V(1.0f, 0);
                n = c2Mulvs(n, d.x < 0 ? 1.0f : -1.0f);
            }

            else
            {
                depth = y_overlap;
                n = c2V(0, 1.0f);
                n = c2Mulvs(n, d.y < 0 ? 1.0f : -1.0f);
            }

            m->count = 1;
            m->depths[0] = A.r + depth;
            m->contact_points[0] = c2Sub(A.p, c2Mulvs(n, depth));
            m->n = n;
        }
    }
}

void c2CircletoCapsuleManifold(c2Circle A, c2Capsule B, c2Manifold* m)
{
    m->count = 0;
    c2v a, b;
    float r = A.r + B.r;
    float d = c2GJK(&A, C2_CIRCLE, 0, &B, C2_CAPSULE, 0, &a, &b, 0);
    if (d < r)
    {
        c2v n;
        if (d == 0) n = c2Norm(c2Skew(c2Sub(B.b, B.a)));
        else n = c2Norm(c2Sub(b, a));

        m->count = 1;
        m->depths[0] = r - d;
        m->contact_points[0] = c2Sub(b, c2Mulvs(n, B.r));
        m->n = n;
    }
}

void c2AABBtoAABBManifold(c2AABB A, c2AABB B, c2Manifold* m)
{
    m->count = 0;
    c2v mid_a = c2Mulvs(c2Add(A.min, A.max), 0.5f);
    c2v mid_b = c2Mulvs(c2Add(B.min, B.max), 0.5f);
    c2v eA = c2Absv(c2Mulvs(c2Sub(A.max, A.min), 0.5f));
    c2v eB = c2Absv(c2Mulvs(c2Sub(B.max, B.min), 0.5f));
    c2v d = c2Sub(mid_b, mid_a);

    // calc overlap on x and y axes
    float dx = eA.x + eB.x - c2Abs(d.x);
    if (dx < 0) return;
    float dy = eA.y + eB.y - c2Abs(d.y);
    if (dy < 0) return;

    c2v n;
    float depth;
    c2v p;

    // x axis overlap is smaller
    if (dx < dy)
    {
        depth = dx;
        if (d.x < 0)
        {
            n = c2V(-1.0f, 0);
            p = c2Sub(mid_a, c2V(eA.x, 0));
        }
        else
        {
            n = c2V(1.0f, 0);
            p = c2Add(mid_a, c2V(eA.x, 0));
        }
    }

    // y axis overlap is smaller
    else
    {
        depth = dy;
        if (d.y < 0)
        {
            n = c2V(0, -1.0f);
            p = c2Sub(mid_a, c2V(0, eA.y));
        }
        else
        {
            n = c2V(0, 1.0f);
            p = c2Add(mid_a, c2V(0, eA.y));
        }
    }

    m->count = 1;
    m->contact_points[0] = p;
    m->depths[0] = depth;
    m->n = n;
}

void c2AABBtoCapsuleManifold(c2AABB A, c2Capsule B, c2Manifold* m)
{
    m->count = 0;
    c2Poly p;
    c2BBVerts(p.verts, &A);
    p.count = 4;
    c2Norms(p.verts, p.norms, 4);
    c2CapsuletoPolyManifold(B, &p, 0, m);
}

void c2CapsuletoCapsuleManifold(c2Capsule A, c2Capsule B, c2Manifold* m)
{
    m->count = 0;
    c2v a, b;
    float r = A.r + B.r;
    float d = c2GJK(&A, C2_CAPSULE, 0, &B, C2_CAPSULE, 0, &a, &b, 0);
    if (d < r)
    {
        c2v n;
        if (d == 0) n = c2Norm(c2Skew(c2Sub(A.b, A.a)));
        else n = c2Norm(c2Sub(b, a));

        m->count = 1;
        m->depths[0] = r - d;
        m->contact_points[0] = c2Sub(b, c2Mulvs(n, B.r));
        m->n = n;
    }
}

static C2_INLINE c2h c2PlaneAt(const c2Poly* p, const int i)
{
    c2h h;
    h.n = p->norms[i];
    h.d = c2Dot(p->norms[i], p->verts[i]);
    return h;
}

void c2CircletoPolyManifold(c2Circle A, const c2Poly* B, const c2x* bx_tr, c2Manifold* m)
{
    m->count = 0;
    c2v a, b;
    float d = c2GJK(&A, C2_CIRCLE, 0, B, C2_POLY, bx_tr, &a, &b, 0);

    // shallow, the circle center did not hit the polygon
    // just use a and b from GJK to define the collision
    if (d != 0)
    {
        c2v n = c2Sub(b, a);
        float l = c2Dot(n, n);
        if (l < A.r * A.r)
        {
            l = c2Sqrt(l);
            m->count = 1;
            m->contact_points[0] = b;
            m->depths[0] = A.r - l;
            m->n = c2Mulvs(n, 1.0f / l);
        }
    }

    // Circle center is inside the polygon
    // find the face closest to circle center to form manifold
    else
    {
        c2x bx = bx_tr ? *bx_tr : c2xIdentity();
        float sep = -FLT_MAX;
        int index = ~0;
        c2v local = c2MulxvT(bx, A.p);

        for (int i = 0; i < B->count; ++i)
        {
            c2h h = c2PlaneAt(B, i);
            d = c2Dist(h, local);
            if (d > A.r) return;
            if (d > sep)
            {
                sep = d;
                index = i;
            }
        }

        c2h h = c2PlaneAt(B, index);
        c2v p = c2Project(h, local);
        m->count = 1;
        m->contact_points[0] = c2Mulxv(bx, p);
        m->depths[0] = A.r - sep;
        m->n = c2Neg(c2Mulrv(bx.r, B->norms[index]));
    }
}

// Forms a c2Poly and uses c2PolytoPolyManifold
void c2AABBtoPolyManifold(c2AABB A, const c2Poly* B, const c2x* bx, c2Manifold* m)
{
    m->count = 0;
    c2Poly p;
    c2BBVerts(p.verts, &A);
    p.count = 4;
    c2Norms(p.verts, p.norms, 4);
    c2PolytoPolyManifold(&p, 0, B, bx, m);
}

// clip a segment to a plane
static int c2Clip(c2v* seg, c2h h)
{
    c2v out[2];
    int sp = 0;
    float d0, d1;
    if ((d0 = c2Dist(h, seg[0])) < 0) out[sp++] = seg[0];
    if ((d1 = c2Dist(h, seg[1])) < 0) out[sp++] = seg[1];
    if (d0 * d1 <= 0) out[sp++] = c2Intersect(seg[0], seg[1], d0, d1);
    seg[0] = out[0]; seg[1] = out[1];
    return sp;
}

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4204) // nonstandard extension used: non-constant aggregate initializer
#endif

// clip a segment to the "side planes" of another segment.
// side planes are planes orthogonal to a segment and attached to the
// endpoints of the segment
static int c2SidePlanes(c2v* seg, c2x x, const c2Poly* p, int e, c2h* h)
{
    c2v ra = c2Mulxv(x, p->verts[e]);
    c2v rb = c2Mulxv(x, p->verts[e + 1 == p->count ? 0 : e + 1]);
    c2v in = c2Norm(c2Sub(rb, ra));
    c2h left = { c2Neg(in), c2Dot(c2Neg(in), ra) };
    c2h right = { in, c2Dot(in, rb) };
    if (c2Clip(seg, left) < 2) return 0;
    if (c2Clip(seg, right) < 2) return 0;
    if (h) {
        h->n = c2CCW90(in);
        h->d = c2Dot(c2CCW90(in), ra);
    }
    return 1;
}

static void c2KeepDeep(c2v* seg, c2h h, c2Manifold* m)
{
    int cp = 0;
    for (int i = 0; i < 2; ++i)
    {
        c2v p = seg[i];
        float d = c2Dist(h, p);
        if (d < 0)
        {
            m->contact_points[cp] = p;
            m->depths[cp] = -d;
            ++cp;
        }
    }
    m->count = cp;
    m->n = h.n;
}

static C2_INLINE c2v c2CapsuleSupport(c2Capsule A, c2v dir)
{
    float da = c2Dot(A.a, dir);
    float db = c2Dot(A.b, dir);
    if (da > db) return c2Add(A.a, c2Mulvs(dir, A.r));
    else return c2Add(A.b, c2Mulvs(dir, A.r));
}

static void c2AntinormalFace(c2Capsule cap, const c2Poly* p, c2x x, int* face_out, c2v* n_out)
{
    float sep = -FLT_MAX;
    int index = ~0;
    c2v n = c2V(0, 0);
    for (int i = 0; i < p->count; ++i)
    {
        c2h h = c2Mulxh(x, c2PlaneAt(p, i));
        c2v n0 = c2Neg(h.n);
        c2v s = c2CapsuleSupport(cap, n0);
        float d = c2Dist(h, s);
        if (d > sep)
        {
            sep = d;
            index = i;
            n = n0;
        }
    }
    *face_out = index;
    *n_out = n;
}

void c2CapsuletoPolyManifold(c2Capsule A, const c2Poly* B, const c2x* bx_ptr, c2Manifold* m)
{
    m->count = 0;
    c2v a, b;
    float d = c2GJK(&A, C2_CAPSULE, 0, B, C2_POLY, bx_ptr, &a, &b, 0);

    // deep, treat as segment to poly collision
    if (d == 0)
    {
        c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
        c2v n;
        int index;
        c2AntinormalFace(A, B, bx, &index, &n);
        c2v seg[2] = { c2Add(A.a, c2Mulvs(n, A.r)), c2Add(A.b, c2Mulvs(n, A.r)) };
        c2h h;
        if (!c2SidePlanes(seg, bx, B, index, &h)) return;
        c2KeepDeep(seg, h, m);
    }

    // shallow, use GJK results a and b to define manifold
    else if (d < A.r)
    {
        c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
        c2v ab = c2Sub(b, a);
        int face_case = 0;

        for (int i = 0; i < B->count; ++i)
        {
            c2v n = c2Mulrv(bx.r, B->norms[i]);
            if (c2Parallel(ab, n, 5.0e-3f))
            {
                face_case = 1;
                break;
            }
        }

        // 1 contact
        if (!face_case)
        {
            m->count = 1;
            m->contact_points[0] = b;
            m->depths[0] = A.r - d;
            m->n = c2Mulvs(ab, 1.0f / d);
        }

        // 2 contacts if laying on a polygon face nicely
        else
        {
            c2v n;
            int index;
            c2AntinormalFace(A, B, bx, &index, &n);
            c2v seg[2] = { c2Add(A.a, c2Mulvs(n, A.r)), c2Add(A.b, c2Mulvs(n, A.r)) };
            c2h h;
            if (!c2SidePlanes(seg, bx, B, index, &h)) return;
            c2KeepDeep(seg, h, m);
        }
    }
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

static float c2CheckFaces(const c2Poly* A, c2x ax, const c2Poly* B, c2x bx, int* face_index)
{
    c2x b_in_a = c2MulxxT(ax, bx);
    c2x a_in_b = c2MulxxT(bx, ax);
    float sep = -FLT_MAX;
    int index = ~0;

    for (int i = 0; i < A->count; ++i)
    {
        c2h h = c2PlaneAt(A, i);
        int idx = c2Support(B->verts, B->count, c2Mulrv(a_in_b.r, c2Neg(h.n)));
        c2v p = c2Mulxv(b_in_a, B->verts[idx]);
        float d = c2Dist(h, p);
        if (d > sep)
        {
            sep = d;
            index = i;
        }
    }

    *face_index = index;
    return sep;
}

static C2_INLINE void c2Incident(c2v* incident, const c2Poly* ip, c2x ix, const c2Poly* rp, c2x rx, int re)
{
    c2v n = c2MulrvT(ix.r, c2Mulrv(rx.r, rp->norms[re]));
    int index = ~0;
    float min_dot = FLT_MAX;
    for (int i = 0; i < ip->count; ++i)
    {
        float dot = c2Dot(n, ip->norms[i]);
        if (dot < min_dot)
        {
            min_dot = dot;
            index = i;
        }
    }
    incident[0] = c2Mulxv(ix, ip->verts[index]);
    incident[1] = c2Mulxv(ix, ip->verts[index + 1 == ip->count ? 0 : index + 1]);
}

// Please see Dirk Gregorius's 2013 GDC lecture on the Separating Axis Theorem
// for a full-algorithm overview. The short description is:
    // Test A against faces of B, test B against faces of A
    // Define the reference and incident shapes (denoted by r and i respectively)
    // Define the reference face as the axis of minimum penetration
    // Find the incident face, which is most anti-normal face
    // Clip incident face to reference face side planes
    // Keep all points behind the reference face
void c2PolytoPolyManifold(const c2Poly* A, const c2x* ax_ptr, const c2Poly* B, const c2x* bx_ptr, c2Manifold* m)
{
    m->count = 0;
    c2x ax = ax_ptr ? *ax_ptr : c2xIdentity();
    c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
    int ea, eb;
    float sa, sb;
    if ((sa = c2CheckFaces(A, ax, B, bx, &ea)) >= 0) return;
    if ((sb = c2CheckFaces(B, bx, A, ax, &eb)) >= 0) return;

    const c2Poly* rp,* ip;
    c2x rx, ix;
    int re;
    float kRelTol = 0.95f, kAbsTol = 0.01f;
    int flip;
    if (sa * kRelTol > sb + kAbsTol)
    {
        rp = A; rx = ax;
        ip = B; ix = bx;
        re = ea;
        flip = 0;
    }
    else
    {
        rp = B; rx = bx;
        ip = A; ix = ax;
        re = eb;
        flip = 1;
    }

    c2v incident[2];
    c2Incident(incident, ip, ix, rp, rx, re);
    c2h rh;
    if (!c2SidePlanes(incident, rx, rp, re, &rh)) return;
    c2KeepDeep(incident, rh, m);
    if (flip) m->n = c2Neg(m->n);
}