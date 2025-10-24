#version 450 core
out vec4 FragColor;
in vec2 uv;
uniform sampler2D tex;
uniform vec2 playerpos; // <-- is the player's position
uniform vec2 playerrot; // = vec2(1.0, 0.0); // <-- is a rotor, player's yaw value

ivec2 tsize = textureSize(tex, 0);

/*

	//directly return the pixel color from the texture
	ivec2 tpos = ivec2((uv + 1.0) / 2.0 * vec2(tsize));
	FragColor = texelFetch(tex, tpos, 0);

*/

vec2 rotate(in vec2 vec)
{
	// v1 is Rv
	vec2 v1 = vec2( 
		vec.x * playerrot.x - vec.y * playerrot.y,
		vec.x * playerrot.y + vec.y * playerrot.x
	);

	// v2 is v1R~ where R~ is the conjugate
	vec2 v2 = vec2( 
		 v1.x * playerrot.x + v1.y * playerrot.y,
		-v1.x * playerrot.y + v1.y * playerrot.x
	);
	return v2;
};

void main()
{
	// change the uv coords to world coords
	vec3 right = vec3(rotate(vec2(1.0, 0.0)), 0.0);
	vec3 forward = vec3(-right.y, right.x, 0.0); // <-- 90 degree rotations are easy to hard-code, will need to rotate again in the virtical later. NOTE: rotate, THEN flip 90 degrees?
	vec3 up = vec3(0.0, 0.0, 1.0); // <-- change to cross product once virtical rotation is added

	FragColor = vec4(right * uv.x + up * uv.y + forward, 1.0);
	
	//ivec2 tpos = ivec2((uv + 1.0) / 2.0 * vec2(tsize));
	//FragColor = texelFetch(tex, tpos, 0);
}