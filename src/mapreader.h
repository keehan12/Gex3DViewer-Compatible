#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "imagepacker.h"

#include <sstream>

struct file_t;
struct level_t;

struct IComponent
{
	~IComponent() = default;

	virtual void ParseData(file_t& file, level_t& level, unsigned int data) = 0;
	virtual void ExportData(std::stringstream& ss) = 0;
	virtual void RenderGUI(level_t& level, void* textureSheet) {}

protected:
	IComponent() = default;
};

struct PathPoint
{
	unsigned short speed;
	short x, y, z;
};

struct PathRotation
{
	unsigned int speed;
	float rotX, rotY, rotZ, rotW;
};

struct Path
{
	unsigned int address;
	unsigned int owner;
	std::vector<PathPoint> points;
	std::vector<PathRotation> rotations;
};

// Maybe move to own file to de-clutter
struct PathComponent : public IComponent
{
	Path path;

	virtual void ParseData(file_t& file, level_t& level, unsigned int data) override;
	virtual void ExportData(std::stringstream& ss) override {}
};

struct LevelTVComponent : public IComponent
{
	char levelType[9]{ 0 };
	unsigned char levelNum;
	unsigned char screenType;
	virtual void ParseData(file_t& file, level_t& level, unsigned int data) override;
	virtual void ExportData(std::stringstream& ss) override {}
	virtual void RenderGUI(level_t& level, void* textureSheet) override;
};

struct FlyBoxComponent : public IComponent
{
	unsigned char flyBoxType;

	virtual void ParseData(file_t& file, level_t& level, unsigned int data) override;
	virtual void ExportData(std::stringstream& ss) override {}
	virtual void RenderGUI(level_t& level, void* textureSheet) override;
};

struct QMarkComponent : public IComponent
{
	//virtual void ExportData(std::stringstream& ss) override;
};

class Object
{
	glm::vec3 position{ 0, 0, 0 };
	glm::vec3 rotation{ 0, 0, 0 };
	unsigned int address;
	bool visible = true;
	std::vector<std::unique_ptr<IComponent>> components;
public:
	Object(unsigned int addr) : address(addr) {}
	void SetVisible(bool b) { visible = b; }
	bool GetVisible() const { return visible; }
	unsigned int GetAddress() const { return address; }

	glm::vec3 GetPosition() const { return position; }
	void SetPosition(glm::vec3 v) { position = v; }

	glm::vec3 GetRotation() const { return rotation; }
	void SetRotation(glm::vec3 v) { rotation = v; }

	template<typename T>
	T& AddComponent()
	{
		components.push_back(std::make_unique<T>());
		return *(T*)components.back().get();
	}

	unsigned int raw_data[4] = { 0 };
};

struct objinstance_t
{
	glm::vec3 position{ 0, 0, 0 };
	glm::vec3 rotation{ 0, 0, 0 };
	bool isVisible = true;
	unsigned int address = 0;
	unsigned int instanceData[4] = { 0 };

	std::vector<std::unique_ptr<IComponent>> components;
	template<typename T>
	T& AddComponent()
	{
		components.push_back(std::make_unique<T>());
		return *(T*)components.back().get();
	}
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
		unsigned char optColors[4] = { 0, 0, 0, 0 };
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
	bool argb1555 = false;
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
	unsigned int baseData;
};

bool LoadLevel(const std::string& filepath, level_t& level);

inline std::string Hexify(unsigned int n)
{
	if (n == 0)
		return "0";
	std::string s;
	while (n > 0)
	{
		if (n % 0x10 > 9)
		{
			s += 'a' + ((n % 0x10) - 10);
		}
		else
		{
			s += '0' + (n % 0x10);
		}
		n >>= 4;
	}
	std::reverse(s.begin(), s.end());
	return s;
};

void AddLineToModel(std::shared_ptr<Model> model, glm::vec3 start, glm::vec3 end);