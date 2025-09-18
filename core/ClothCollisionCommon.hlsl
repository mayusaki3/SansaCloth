#ifndef SANSA_CLOTH_COLLISION_COMMON
#define SANSA_CLOTH_COLLISION_COMMON

struct Capsule { float3 a; float3 b; float r; };

float sdCapsule(float3 p, Capsule cap)
{
    float3 pa = p - cap.a;
    float3 ba = cap.b - cap.a;
    float  denom = max(dot(ba, ba), 1e-6);
    float  h  = saturate(dot(pa, ba) / denom);
    float3 q  = pa - h * ba;
    return length(q) - cap.r;
}

#endif
