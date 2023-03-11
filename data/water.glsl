#version 330

#pragma vertex_shader

in vec4 vertex;
in vec3 normal;
in vec2 velocity;
in vec2 info;	// waves, distanceToEdge

uniform mat4  transform;
uniform mat4  modelMatrix;

out vec2 texcoord;
out vec3 worldNormal;
out vec3 worldPos;
out vec4 data;

void main() {
	gl_Position = transform * vertex;
	texcoord = vertex.xz;
	worldNormal = mat3(modelMatrix) * normal;
	worldPos = (modelMatrix * vertex).xyz;
	data = vec4(velocity, info);
}


#pragma fragment_shader

in vec2 texcoord;
in vec3 worldNormal;
in vec3 worldPos;
in vec4 data;
out vec4 fragment;

uniform sampler2D water;
uniform vec3 lightDirection;
uniform float time;

vec4 sampleWater(vec2 coord, vec2 dir, float speed, float time, out float weight) {
	float t = fract(time);
	weight = 1.0 - abs(t * 2.0 - 1.0);	// Triangle wave
	return texture2D(water, coord + dir * speed * t);
}

void main() {

	// Flow
	float distort = 0.1; // Maximum distortion
	float speed = length(data.xy);
	if(speed == 0) speed = 0.0001;
	vec2 direction = data.xy / speed;
	float t = time * distort;
	speed /= distort;

	vec3 weight;
	vec4 ca = sampleWater(texcoord.st, direction, speed, t, weight.x);
	vec4 cb = sampleWater(texcoord.st + vec2(0.1, 0.3), direction, speed, t+0.33, weight.y);
	vec4 cc = sampleWater(texcoord.st + vec2(0.4, 0.7), direction, speed, t-0.33, weight.z);
	weight /= dot(weight, vec3(1.0));
	fragment = ca*weight.x + cb*weight.y + cc*weight.z;

	fragment.r = 1 - data.w * 0.01;



	float l = dot( normalize(worldNormal), normalize(lightDirection));
	float s = (l+1)/1.3*0.2+0.1; // dark side lighting

	fragment.rgb *= max(s, l);
}


