#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "imagepacker.h"

struct objinstance_t
{
	glm::vec3 position{ 0, 0, 0 };
	glm::vec3 rotation{ 0, 0, 0 };
	bool isVisible = true;
	unsigned int address = 0;
	unsigned int instanceData[4] = { 0 };
};

struct Model
{
	struct vertex_t
	{
		int x, y, z;
		int oX, oY, oZ;
		unsigned short normalId;
		unsigned char r, g, b, a;
	};
	struct polygon_t
	{
		size_t vertex[3];
		unsigned int materialID;
		unsigned short flags;
		glm::vec2 uvs[3];
	};
	const unsigned int addr;
	std::string name;
	std::vector<vertex_t> vertices;
	std::vector<polygon_t> polygons;
	std::vector<objinstance_t> instances;
	bool objectVisibility = true;
	bool showInstances = false;
	bool hasNoTextures = false;

	Model(unsigned int addr) : addr(addr) {}
};

struct texture_t
{
	unsigned int w, h;
	glm::vec4* pixels;
	bool deletePixels = true;
};

struct PathPoint
{
	unsigned short speed;
	short x, y, z;
};

struct Path
{
	unsigned int address;
	unsigned int owner;
	std::vector<PathPoint> points;
};

struct level_t
{
	std::vector<std::shared_ptr<Model>> models;
	std::vector<texture_t> textures;
	std::vector<Path> paths;
	ImagePacker::ImageInformationList list;
	texture_t sheet{ 0, 0, NULL };
	std::string name;
	float bgColor[3];
	char pickupName[3][9];
};

bool LoadLevel(const std::string& filepath, level_t& level);