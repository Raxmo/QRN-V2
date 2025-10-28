#version 450 core
out vec4 FragColor;
in vec2 uv;
layout(std430, binding = 0) buffer Grid
{
    float values[512][512];  // height x width
};
uniform vec2 playerpos = vec2(4.5, 4.5); // <-- is the player's position
uniform vec2 playerrot = vec2(1.0, 0.0); // <-- is a rotor, player's yaw value as rotor
uniform uint frameCount;

float playerHeight = 2.0; // <-- is the player's eye height off the ground

/*=============================================================+/
							 Structs
/+=============================================================*/

struct HitInfo
{
	vec2 uv;		// <-- uv coordinates on the face we hit
	ivec3 face;		// <-- which face we hit (x, y, or z)
	ivec2 tileID;	// <-- which tile we hit
	int tileType;	// <-- what type of tile we hit
	float dist;		// <-- distance from ray origin to hit point
	vec3 point;		// <-- point of intersection
};

/*=============================================================+/
							Functions
/+=============================================================*/
uint rotmul(uint a, uint b)
{
	uint c = 0;

	for (int i = 0; i < 32; i++) {
    uint rotated = (a << i) | (a >> (32 - i));
    uint mask = (b >> i) & 1u;
    c ^= rotated & (0u - mask);
}

	return c;
}

uint chaoticHash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

float uint_to_unit(uint x)
{
    // keep only 23 mantissa bits
    uint mantissa = x & 0x007FFFFFu;      // mask out bottom 23 bits
    uint bits = 0x3F800000u | mantissa;   // exponent=127, sign=0
    return uintBitsToFloat(bits) - 1.0;   // result [0.0, 1.0)
}

float p1DtoFloat(uint p)
{
	return uint_to_unit(chaoticHash(p));
}

float p2DtoFloat(ivec2 coord)
{
	return uint_to_unit(floatBitsToUint(p1DtoFloat(coord.r) + p1DtoFloat(coord.g)));
}

float p3DtoFloat(ivec3 coord)
{
	return uint_to_unit(
	chaoticHash(
	chaoticHash(uint(coord.x) +
	chaoticHash(uint(coord.y) +
	chaoticHash(uint(coord.z)
	)))));
}

float p4DtoFloat(ivec4 coord)
{
	return uint_to_unit(floatBitsToUint(p2DtoFloat(coord.rg) + p2DtoFloat(coord.ba)));
}

ivec2 vec2AsIvec2(vec2 v)
{
	return ivec2
	(
		floatBitsToInt(v.x),
		floatBitsToInt(v.y)
	);
}

ivec3 vec3AsIvec3(vec3 v)
{
	return ivec3
	(
		floatBitsToInt(v.x),
		floatBitsToInt(v.y),
		floatBitsToInt(v.z)
	);
}

ivec4 vec4AsIvec4(vec4 v)
{
	return ivec4
	(
		floatBitsToInt(v.r),
		floatBitsToInt(v.g),
		floatBitsToInt(v.b),
		floatBitsToInt(v.a)
	);
}	

bool IsWall(ivec2 maploc)
{
	return (values[maploc.y][maploc.x] > 0.0);
}

vec3 minMask(vec3 vec)
{
	float m = min(min(vec.x, vec.y), vec.z);
	vec3 mask = vec3(float(vec.x == m), float(vec.y == m), float(vec.z == m));
	return mask;
}

HitInfo DDACheck(vec3 ro, vec3 rd)
{
	HitInfo hitinfo;
	hitinfo.face = ivec3(-sign(rd));
	hitinfo.face.y *= -1;
	vec3 point = ro;

	float zdist = abs(playerHeight / rd.z);		// <-- distance to the floor / cieling.
	zdist = min(zdist, 1000.0);					// <-- clamp to avoid infinities

	ivec2 tileid = ivec2(floor(point.xy));
	ivec2 tstep = ivec2(sign(rd.xy));

	vec2 deltas = abs(vec2(1.0) / rd.xy);										// <-- distance between x-side and y-side checks
	vec3 sideDists = vec3(fract(1.0 - (ro.xy * vec2(tstep))) * deltas, zdist);	// <-- distance from ray origin to first x-side and y-side checks
	vec3 totdists = vec3(0.0);													// <-- total distance traveled along the ray
	totdists.z = sideDists.z;													// <-- initialize z distances

	vec3 mask = vec3(0.0);

	while(mask.z == 0.0 && !IsWall(tileid))
	{
		mask = minMask(sideDists);
		tileid += ivec2(tstep * ivec2(mask.xy));
		totdists = sideDists;
		sideDists.xy += deltas * mask.xy;
	}
	mask = minMask(totdists);
	float dist = min(totdists.z, min(totdists.x, totdists.y));
	point = ro + rd * dist;
	hitinfo.point = point;
	point *= 1.0 - mask; // <-- zero out the axis we traveled on
	point = fract(point); // <-- get the uv coords on the face we hit
	point.xy = fract(point.xy * dot(vec3(hitinfo.face), mask));
	vec2 uv = 
	point.xy * mask.z +
	point.yz * mask.x +
	point.xz * mask.y;

	hitinfo.uv = uv;
	hitinfo.tileID = tileid;
	hitinfo.tileType = int(values[tileid.y][tileid.x]);
	hitinfo.dist = dist;
	return hitinfo;
}

float getValue(HitInfo hit)
{
	float val;

	float dval = 7.0 / (hit.dist * hit.dist + 6.0);

	float wallval = max(abs(hit.uv.x-0.5), abs(hit.uv.y -0.5)) * 2.0;

	wallval = 1.0 - wallval;
	wallval *= 5.0 * sqrt(5.0);
	wallval = clamp(wallval, 0.0, 1.0);
	
	val = dval * wallval;

	return val;
}

/*=============================================================+/
							  Main
/+=============================================================*/

void main()
{
	// change the uv coords to world coords
	vec3 right = vec3(playerrot.x * playerrot.x - playerrot.y * playerrot.y, -2.0 * playerrot.x * playerrot.y, 0.0);
	vec3 forward = vec3(-right.y, right.x, 0.0);	// <-- 90 degree rotations are easy to hard-code, will need to rotate again in the virtical later. NOTE: rotate, THEN flip 90 degrees?
	vec3 up = vec3(0.0, 0.0, 1.0);					// <-- change to cross product once virtical rotation is added

	vec3 raydir = vec3((right * uv.x) + (up * uv.y) + forward);

	HitInfo hit = DDACheck(vec3(playerpos, playerHeight), normalize(raydir));

	float noise = p3DtoFloat(ivec3(vec2AsIvec2(uv), frameCount));

	float kval = getValue(hit);

	float tval = pow(noise, 1.0 / kval - 1.0);

	FragColor = vec4(vec3(tval), 1.0);
}