#include "Perlin.hpp"
#include "math_utils.hpp"
#include <iostream>

Perlin::Perlin(unsigned int seed)
	: _seed(seed)
{
}


Perlin::Perlin(unsigned int seed, int octaves, float frequency, float persistence, float lacunarity)
	: _seed(seed), _octaves(octaves), _frequency(frequency), _persistence(persistence), _lacunarity(lacunarity)
{
}

static inline glm::vec3 smoothstep(const glm::vec3 & t)
{
	// 6t^5 - 15t^4 + 10t^3
	//https://www.geogebra.org/solver/fr/results/6t%5E5%20-%2015t%5E4%20%2B%2010t%5E3?from=google
	//s curve function to smooth the interpolation
	return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
}

unsigned int Perlin::hash(unsigned int x, const unsigned int & seed) const
{
	//this is a murmur hash
	static const unsigned int m = 0x5bd1e995U;
	unsigned int hash = seed;

	x *= m;
	x ^= x >> 24;
	x *= m;
	hash *= m;
	hash ^= x;

	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;
	return hash;
}

unsigned int Perlin::hash(const glm::uvec3 & v, const unsigned int & seed) const
{
	static const unsigned int m = 0x5bd1e995U;

	unsigned int hash = seed;
	unsigned int k = v.x;

	//first vector element
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	//second vector element
	k = v.y;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	//third vector element
	k = v.z;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;

	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;
	return hash;
}

glm::vec2 Perlin::getAngleGradient(const glm::uvec2 & v, const unsigned int & seed) const
{
	static const unsigned int m = 0x5bd1e995U;

	unsigned int hash = seed;
	unsigned int k = v.x;

	//first vector element
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	//second vector element
	k = v.y;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;

	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;

	float angle = (hash % 360) * (M_PI / 180);
	return glm::vec2(glm::cos(angle), glm::sin(angle));
}

uint_fast8_t Perlin::testHash(const glm::uvec3 & v, const unsigned int & seed) const
{
	static uint8_t perm[] = { 151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
	};

	uint8_t x = (v.x & 0xFF) ^ seed;
	uint8_t y = (v.y & 0xFF) ^ seed;
	uint8_t z = (v.z & 0xFF) ^ seed;

	return perm[(perm[(perm[x] ^ y)] ^ z)];
}

glm::vec3 Perlin::getHashedGradient(const glm::uvec3 & v, const unsigned int & seed) const
{
	// unsigned int h = hash(v, seed);
	uint_fast8_t h = testHash(v, seed);

	switch (h & 0xF)
	{
	case 0:
        return glm::vec3(1, 1, 0);
    case 1:
        return glm::vec3(-1, 1, 0);
    case 2:
        return glm::vec3(1, -1, 0);
    case 3:
        return glm::vec3(-1, -1, 0);
    case 4:
        return glm::vec3(1, 0, 1);
    case 5:
        return glm::vec3(-1, 0, 1);
    case 6:
        return glm::vec3(1, 0, -1);
    case 7:
        return glm::vec3(-1, 0, -1);
    case 8:
        return glm::vec3(0, 1, 1);
    case 9:
        return glm::vec3(0, -1, 1);
    case 10:
        return glm::vec3(0, 1, -1);
    case 11:
        return glm::vec3(0, -1, -1);
    case 12:
        return glm::vec3(1, 1, 0);
    case 13:
        return glm::vec3(-1, 1, 0);
    case 14:
        return glm::vec3(0, -1, 1);
    case 15:
        return glm::vec3(0, -1, -1);
    }
	std::cout << "oopsie\n";
	return glm::vec3(0);
}

float Perlin::interpolate(
	const float & v1,
	const float & v2,
	const float & v3,
	const float & v4,
	const float & v5,
	const float & v6,
	const float & v7,
	const float & v8,
	const glm::vec3 & t) const
{
	float x1 = glm::mix(v1, v2, t.x);
	float x2 = glm::mix(v3, v4, t.x);
	float x3 = glm::mix(v5, v6, t.x);
	float x4 = glm::mix(v7, v8, t.x);

	float y1 = glm::mix(x1, x2, t.y);
	float y2 = glm::mix(x3, x4, t.y);

	return glm::mix(y1, y2, t.z);
}

float Perlin::insideNoise(const glm::vec3 & v, const unsigned int & seed) const
{
	glm::vec3 floor_position = glm::floor(v);
	glm::vec3 unit_position = v - floor_position;
	glm::uvec3 cell_position(floor_position);

	const float value1 = glm::dot(getHashedGradient(cell_position, seed), unit_position);
	const float value2 = glm::dot(getHashedGradient(cell_position + glm::uvec3(1, 0, 0), seed), unit_position - glm::vec3(1, 0, 0));
	const float value3 = glm::dot(getHashedGradient(cell_position + glm::uvec3(0, 1, 0), seed), unit_position - glm::vec3(0, 1, 0));
	const float value4 = glm::dot(getHashedGradient(cell_position + glm::uvec3(1, 1, 0), seed), unit_position - glm::vec3(1, 1, 0));
	const float value5 = glm::dot(getHashedGradient(cell_position + glm::uvec3(0, 0, 1), seed), unit_position - glm::vec3(0, 0, 1));
	const float value6 = glm::dot(getHashedGradient(cell_position + glm::uvec3(1, 0, 1), seed), unit_position - glm::vec3(1, 0, 1));
	const float value7 = glm::dot(getHashedGradient(cell_position + glm::uvec3(0, 1, 1), seed), unit_position - glm::vec3(0, 1, 1));
	const float value8 = glm::dot(getHashedGradient(cell_position + glm::uvec3(1, 1, 1), seed), unit_position - glm::vec3(1, 1, 1));

	return interpolate(value1, value2, value3, value4, value5, value6, value7, value8, smoothstep(unit_position));
}

float Perlin::noise(const float & x) const
{
	return noise(glm::vec3(x, 0.5f, 0.5f));
}

float Perlin::noise(const glm::vec2 & v) const
{
	return mapRange(noise(glm::vec3(v, 0.5f)), -0.8f, 0.8f, -1.0f, 1.0f);
}

float Perlin::noise(const glm::vec3 & v) const
{
	float total = 0;
	float maxValue = 0;
	float amplitude = 1;
	float current_frequency = _frequency;

	for (int i = 0; i < _octaves; i++)
	{
		total += insideNoise(v * current_frequency, _seed) * amplitude;
		maxValue += amplitude;
		amplitude *= _persistence;
		current_frequency *= _lacunarity;
	}

	return total / maxValue;
}
