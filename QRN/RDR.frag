#version 450 core
out vec4 FragColor;
in vec2 uv;
layout(std430, binding = 0) buffer Grid
{
    float values[32][32];  // height x width
};
uniform vec2 playerpos = vec2(4.5, 4.5); // <-- is the player's position
uniform vec2 playerrot = vec2(1.0, 0.0); // <-- is a rotor, player's yaw value as rotor

float playerHeight = 2.0; // <-- is the player's eye height off the ground

/*=============================================================+/
							Functions
/+=============================================================*/

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

vec3 DDACheck(vec3 ro, vec3 rd)
{
	vec3 point = ro;

	float zdist = abs(playerHeight / rd.z); // <-- distance to the floor / cieling.
	zdist = min(zdist, 1000.0); // <-- clamp to avoid infinities

	ivec2 tileid = ivec2(floor(point.xy));
	ivec2 tstep = ivec2(sign(rd.xy));

	vec2 deltas = abs(vec2(1.0) / rd.xy); // <-- distance between x-side and y-side checks
	vec3 sideDists = vec3(fract(1.0 - (ro.xy * vec2(tstep))) * deltas, zdist); // <-- distance from ray origin to first x-side and y-side checks
	vec3 totdists = vec3(0.0); // <-- total distance traveled along the ray
	totdists.z = sideDists.z; // <-- initialize z distances

	vec3 mask = vec3(0.0);

	while(mask.z == 0.0 && !IsWall(tileid))
	{
		mask = minMask(sideDists);
		tileid += ivec2(tstep * ivec2(mask.xy));
		totdists = sideDists;
		sideDists.xy += deltas * mask.xy;
	}
	mask = minMask(totdists);
	vec3 uvz;
	point = ro + rd * min(totdists.z, min(totdists.x, totdists.y));
	point *= 1.0 - mask; // <-- zero out the axis we traveled on
	point = fract(point); // <-- get the uv coords on the face we hit
	vec2 uv = 
	point.xy * mask.z +
	point.yz * mask.x +
	point.xz * mask.y;

	uvz = vec3(uv, 0.0);

	return uvz; // <-- gives UV coordinates, and leaves Z as a possible index for other calculations
}

/*=============================================================+/
							  Main
/+=============================================================*/

void main()
{
	// change the uv coords to world coords
	vec3 right = vec3(playerrot.x * playerrot.x - playerrot.y * playerrot.y, -2.0 * playerrot.x * playerrot.y, 0.0);
	vec3 forward = vec3(-right.y, right.x, 0.0); // <-- 90 degree rotations are easy to hard-code, will need to rotate again in the virtical later. NOTE: rotate, THEN flip 90 degrees?
	vec3 up = vec3(0.0, 0.0, 1.0); // <-- change to cross product once virtical rotation is added

	vec3 raydir = vec3((right * uv.x) + (up * uv.y) + forward);

	FragColor = vec4(DDACheck(vec3(playerpos, playerHeight), raydir), 1.0);

	//FragColor = vec4(uv, 0.0, 1.0);
}