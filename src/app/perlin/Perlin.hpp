#pragma once

#define GLM_FORCE_RADIANS
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>


/**
 * @brief Perlin noise generator
 * 
 * @details This class is a Perlin noise generator. It can generate 1D, 2D and 3D Perlin noise.
 * It will return a value in the range [-1, 1].
 * 
 * @note To understand how to tune the noise generation, i highly recommend to read this,
 * 	http://campi3d.com/External/MariExtensionPack/userGuide5R8/Understandingsomebasicnoiseterms.html
 * and to understand the noise generation, i recommend to read this,
 * 	https://eev.ee/blog/2016/05/29/perlin-noise/
 */
class Perlin
{
public:

	/**
	 * @brief Construct a new Perlin object
	 * 
	 * @param seed The seed to use to generate the noise,
	 * 	the same params with the same seed will always return the same value
	 */
	Perlin(unsigned int seed);

	/**
	 * @brief Construct a new Perlin object
	 * 
	 * @details This constructor allows to tune the noise generation, it is highly recommended to read this,
	 *  http://campi3d.com/External/MariExtensionPack/userGuide5R8/Understandingsomebasicnoiseterms.html
	 * 
	 * @param seed Same as default constructor
	 * @param octaves Default is 1
	 * @param frequency Default is 1
	 * @param persistence Default is 0.5
	 * @param lacunarity Default is 2.0
	 */
	Perlin(
		unsigned int seed,
		int octaves,
		float frequency,
		float persistence,
		float lacunarity);

	/**
	 * @brief generate 1D Perlin noise in the range [-1, 1]
	 * 
	 * @param x a point in the 1D space
	 * @return a value in the range [-1, 1]
	 */
	float			noise(const float & x) const;

	/**
	 * @brief generate 2D Perlin noise in the range [-1, 1]
	 * 
	 * @param v a point in the 2D space
	 * @return a value in the range [-1, 1]
	 */
	float			noise(const glm::vec2 & v) const;

	/**
	 * @brief generate 3D Perlin noise in the range [-1, 1]
	 * 
	 * @param v a point in the 3D space
	 * @return a value in the range [-1, 1]
	 */
	float			noise(const glm::vec3 & v) const;
private:
	unsigned int	_seed;
	int 			_octaves = 1;
	float				_frequency = 1;
	float			_persistence = 0.5;
	float			_lacunarity = 2.0;


	float 			insideNoise(const glm::vec3 & v, const unsigned int & seed) const;
	float			interpolate(
		const float & v1,
		const float & v2,
		const float & v3,
		const float & v4,
		const float & v5,
		const float & v6,
		const float & v7,
		const float & v8,
		const glm::vec3 & t) const;

	/**
	 * @brief a hash function using murmurhash
	 * 
	 * @param x 
	 * @param seed 
	 * @return unsigned int 
	 */
	unsigned int	hash(unsigned int x, const unsigned int & seed) const;

	/**
	 * @brief a vector hash function using murmurhash
	 * 
	 * @param v 
	 * @param seed 
	 * @return unsigned int 
	 */
	unsigned int 	hash(const glm::uvec3 & v, const unsigned int & seed) const;

	uint_fast8_t   testHash(const glm::uvec3 & v, const unsigned int & seed) const; 
	/**
	 * @brief Get one of the 16 predefined vectors ( see improved perlin articles for explanations )
	 * 
	 * @param v 
	 * @param seed 
	 * @return glm::vec3 
	 */
	glm::vec3		getHashedGradient(const glm::uvec3 & v, const unsigned int & seed) const;

	/**
	 * @brief Generates a pseudo-random unit vector
	 * 
	 * @param v 
	 * @param seed 
	 * @return glm::vec2 
	 */
	glm::vec2		getAngleGradient(const glm::uvec2 & v, const unsigned int & seed) const;
};
