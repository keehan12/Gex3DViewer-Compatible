#include "mapreader.h"
#include "glideconstants.h"
#include <bit>
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp> // glm::pi
#include <unordered_map>
#include <algorithm>

struct file_t
{
	using data_t = unsigned char;
	data_t* data;
	size_t size;
	unsigned int baseOffset = 0;

	template<typename T>
	T Read(size_t offset, bool moveOffset = false)
	{
		T data = _Read<T>(baseOffset + offset);
		if constexpr (std::endian::native == std::endian::big && sizeof(T) > 1)
		{
			using byte = unsigned char;
			for (int i = 0; i < sizeof(T) >> 1; ++i)
			{
				static_cast<byte*>(&data)[i] ^= static_cast<byte*>(&data)[sizeof(T) - i - 1];
				static_cast<byte*>(&data)[sizeof(T) - i - 1] ^= static_cast<byte*>(&data)[i];
				static_cast<byte*>(&data)[i] ^= static_cast<byte*>(&data)[sizeof(T) - i - 1];
			}
		}
		if (moveOffset)
			baseOffset += sizeof(T) + offset;
		return data;
	}

	void Close()
	{
		delete[] data;
		data = nullptr;
		size = 0;
		baseOffset = 0;
	}

	~file_t()
	{
		Close();
	}
	
private:


	template<typename T>
	T _Read(size_t offset)
	{
#ifdef DEBUG
		if (offset >= size)
		{
			printf("READ OUT OF BOUNDS! %x >= %x!\n", offset, size);
		}
#endif
		return *(T*)(data + offset);
	}
};

void CreateCube(std::shared_ptr<Model> model)
{
	model->vertices.push_back({ -100, -100, -100,-100, -100, -100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({  100, -100, -100, 100, -100, -100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({  100,  100, -100, 100,  100, -100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({ -100,  100, -100,-100,  100, -100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({ -100, -100,  100,-100, -100,  100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({  100, -100,  100, 100, -100,  100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({  100,  100,  100, 100,  100,  100, 0, 128, 128, 128, 255 });
	model->vertices.push_back({ -100,  100,  100,-100,  100,  100, 0, 128, 128, 128, 255 });
	model->polygons.push_back({ {0, 1, 2}, 0, 0, {{0, 0}, {1, 0}, {1, 1}} });
	model->polygons.push_back({ {0, 2, 3}, 0, 0, {{0, 0}, {1, 1}, {0, 1}} });
	model->polygons.push_back({ {5, 4, 7}, 0, 0, {{0, 0}, {1, 0}, {1, 1}} });
	model->polygons.push_back({ {5, 7, 6}, 0, 0, {{0, 0}, {1, 1}, {0, 1}} });
	model->polygons.push_back({ {1, 5, 6}, 0, 0, {{0, 0}, {1, 0}, {1, 1}} });
	model->polygons.push_back({ {1, 6, 2}, 0, 0, {{0, 0}, {1, 1}, {0, 1}} });
	model->polygons.push_back({ {4, 0, 3}, 0, 0, {{0, 0}, {1, 0}, {1, 1}} });
	model->polygons.push_back({ {4, 3, 7}, 0, 0, {{0, 0}, {1, 1}, {0, 1}} });
	model->polygons.push_back({ {3, 2, 6}, 0, 0, {{0, 0}, {1, 0}, {1, 1}} });
	model->polygons.push_back({ {3, 6, 7}, 0, 0, {{0, 0}, {1, 1}, {0, 1}} });
	model->polygons.push_back({ {0, 1, 5}, 0, 0, {{0, 0}, {1, 0}, {1, 1}} });
	model->polygons.push_back({ {0, 5, 4}, 0, 0, {{0, 0}, {1, 1}, {0, 1}} });
}

static std::vector<texture_t> customImages;

namespace ECustomImageType
{
	enum
	{
		CUSTOM_IMAGE_BASE = 0x100000,
		INFO_SPAWN = CUSTOM_IMAGE_BASE + 0,
		INFO_PROXSIG,
		INFO_UNKNOWN,
		INFO_POINT,
		INFO_COLD
	};
}

ImagePacker::ImageInformation_t* FindImageInfoById(ImagePacker::ImageInformationList& list, int id)
{
	if (auto it = std::find_if(list.begin(), list.end(), [id](const ImagePacker::ImageInformation_t& it)
		{
			return id == (int)it.userdata;
		}); it != list.end())
	{
		return &*it;
	}

	return nullptr;
}

void CreateSpriteObject(level_t& level, std::shared_ptr<Model> model, const std::string& name, int customId, int scale = 5)
{
	model->name = name;

	model->vertices.push_back({ -100 * scale,  100 * scale, 0, -100 * scale,   100 * scale, 0, 0, 128, 128, 128, 255 });
	model->vertices.push_back({  100 * scale,  100 * scale, 0,  100 * scale,   100 * scale, 0, 0, 128, 128, 128, 255 });
	model->vertices.push_back({  100 * scale, -100 * scale, 0,  100 * scale,  -100 * scale, 0, 0, 128, 128, 128, 255 });
	model->vertices.push_back({ -100 * scale, -100 * scale, 0, -100 * scale,  -100 * scale, 0, 0, 128, 128, 128, 255 });

	model->polygons.push_back({ {2, 1, 0}, 0, 0, {{1, 1}, {1, 0}, {0, 0}} });
	model->polygons.push_back({ {3, 2, 0}, 0, 0, {{0, 1}, {1, 1}, {0, 0}} });

	if ((size_t)(customId - ECustomImageType::CUSTOM_IMAGE_BASE) < customImages.size())
	{
		if (auto info = FindImageInfoById(level.list, customId))
		{
			for (int i = 0; i < 2; ++i)
			{
				auto& poly = model->polygons[i];
				for (int j = 0; j < 3; ++j)
				{
					poly.uvs[j].x *= info->width;
					poly.uvs[j].y *= info->height;
					poly.uvs[j].x += info->x;
					poly.uvs[j].y += info->y;
					poly.uvs[j].x /= (float)level.sheet.w;
					poly.uvs[j].y /= (float)level.sheet.h;
				}
			}
		}
	}
}

bool ReadFile(const std::string& filepath, file_t& file)
{
	FILE* f = NULL;
	fopen_s(&f, filepath.c_str(), "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		file.size = ftell(f);
		fseek(f, 0, SEEK_SET);
		file.data = new file_t::data_t[file.size];
		fread(file.data, file.size, 1, f);
		fclose(f);
		return true;
	}

	file.data = NULL;
	return false;
}

using byte = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using i16 = signed short;
using i32 = signed int;
using addr_t = u32;

struct levelext_t
{
	u32 dataOffset;
	addr_t modelAddress;
	u32 nObjects;
	addr_t objAddress;
	u32 nSkybox;
	addr_t skyboxAddress;
	float skyboxRGB[3];
};

struct geo_t
{
	bool isLevel;
	addr_t bspAddress;
	u32 vertexCount;
	u32 polygonCount;
	u32 vertexColorCount;
	u32 boneCount;
	addr_t vertexAddress;
	addr_t polygonAddress;
	addr_t vertexColorAddress;
	addr_t materialAddress;
	addr_t boneAddress;
	addr_t textureAnimAddress;
};


void ReadVertices(file_t& dfx, level_t& level, levelext_t& levelData, geo_t& geo, std::shared_ptr<Model> model)
{
	dfx.baseOffset = levelData.dataOffset + geo.vertexAddress;
	for (u32 i = 0; i < geo.vertexCount; ++i)
	{
		model->vertices.push_back(
			{
				dfx.Read<i16>(i * 12 + 0), dfx.Read<i16>(i * 12 + 4), (short)-dfx.Read<i16>(i * 12 + 2),
				dfx.Read<i16>(i * 12 + 0), dfx.Read<i16>(i * 12 + 4), (short)-dfx.Read<i16>(i * 12 + 2),
				dfx.Read<u16>(i * 12 + 6),
				dfx.Read<byte>(i * 12 + 8), dfx.Read<byte>(i * 12 + 9), dfx.Read<byte>(i * 12 + 10), dfx.Read<byte>(i * 12 + 11)
			});

		if (!geo.isLevel)
		{
			model->vertices.rbegin()[0].r = 128;
			model->vertices.rbegin()[0].g = 128;
			model->vertices.rbegin()[0].b = 128;
			model->vertices.rbegin()[0].a = 255;
		}
	}
}

void ReadPolygons(file_t& dfx, level_t& level, levelext_t& levelData, geo_t& geo, std::shared_ptr<Model> model)
{
	dfx.baseOffset = levelData.dataOffset + geo.polygonAddress;
	bool hasTexturedFace = geo.isLevel;
	for (u32 i = 0; i < geo.polygonCount; ++i)
	{
		Model::polygon_t polygon;
		const byte stride = geo.isLevel ? 0x14 : 0x0C;
		polygon.vertex[0] = dfx.Read<u16>(stride * i + 0);
		polygon.vertex[1] = dfx.Read<u16>(stride * i + 2);
		polygon.vertex[2] = dfx.Read<u16>(stride * i + 4);
		polygon.flags = dfx.Read<byte>(stride * i + 7);
		//if (polygon.flags & 1)
		////if (geo.textureAnimAddress != NULL)
		//{
		//	printf("Flags: ");
		//	for (int i = (sizeof(polygon.flags) * 8) - 1; i >= 0; --i)
		//		printf("%c", ((polygon.flags >> i) & 1) + '0');
		//	printf("\n");
		//}

		if (geo.isLevel)
		{
			addr_t materialAddr = dfx.Read<addr_t>(stride * i + 0x10);
			if (materialAddr != 0xFFFF && (polygon.flags & 0x80) != 0x80)
			{
				auto currOffset = dfx.baseOffset;
				dfx.baseOffset = levelData.dataOffset + materialAddr;
				polygon.uvs[0] = { dfx.Read<byte>(0) / 255.f, dfx.Read<byte>(1) / 255.f };
				polygon.uvs[1] = { dfx.Read<byte>(4) / 255.f, dfx.Read<byte>(5) / 255.f };
				polygon.uvs[2] = { dfx.Read<byte>(8) / 255.f, dfx.Read<byte>(9) / 255.f };
				polygon.materialID = dfx.Read<u16>(6) % 0x1000;


				if (auto info = FindImageInfoById(level.list, polygon.materialID))
				{
					for (int j = 0; j < 3; ++j)
					{
						polygon.uvs[j].x *= info->width;
						polygon.uvs[j].y *= info->height;
						polygon.uvs[j].x += info->x;
						polygon.uvs[j].y += info->y;
						polygon.uvs[j].x /= (float)level.sheet.w;
						polygon.uvs[j].y /= (float)level.sheet.h;
					}
				}

				dfx.baseOffset = currOffset;
			}
			else
			{
				polygon.uvs[0].x = polygon.uvs[0].y = 0;
				polygon.uvs[1].x = polygon.uvs[1].y = 0;
				polygon.uvs[2].x = polygon.uvs[2].y = 0;
				polygon.materialID = 0xFFFFFFFF;
			}

			//if (materialAddr == 0xFFFF)
			//	polygon.flags |= 0x8000;
			//level.models[0]->vertices.push_back(
			//	{
			//		dfx.Read<i16>(i * 12 + 0), dfx.Read<i16>(i * 12 + 2), dfx.Read<i16>(i * 12 + 4),
			//		dfx.Read<u16>(i * 12 + 6),
			//		dfx.Read<byte>(i * 12 + 8), dfx.Read<byte>(i * 12 + 9), dfx.Read<byte>(i * 12 + 10), dfx.Read<byte>(i * 12 + 11)
			//	});
		}
		else
		{
			if ((polygon.flags & 0x02) == 0x02)
			{
				hasTexturedFace = true;
				addr_t materialAddr = dfx.Read<addr_t>(stride * i + 8);
				auto currOffset = dfx.baseOffset;
				dfx.baseOffset = levelData.dataOffset + materialAddr;
				polygon.materialID = ((polygon.flags & 8) == 0) ? dfx.Read<u16>(6) : geo.textureAnimAddress;
				//if (materialAddr >= 0x1000)
				//	printf("Material: (%X)|(%X) > %s\n", polygon.materialID / 0x1000, polygon.materialID % 0x1000, (polygon.flags & 8) ? "true" : "false");
				if (model->name == "charger_" || model->name == "batt____" || model->name == "launch__")
				{
					printf("MAT: %d, FLG: %x\n", polygon.materialID, polygon.flags);
				}
				if ((polygon.flags & 8) == 8)
				{
					//printf("  -8POLY: 0x%X\n", polygon.materialID);
					//was = dfx.baseOffset;
					//dfx.baseOffset = levelData.dataOffset + geo.textureAnimAddress;
					//u32 count = dfx.Read<u32>(0, true);
					////loop:
					//u32 index = 0 % count;
					//dfx.baseOffset = levelData.dataOffset + dfx.Read<addr_t>(0xC * index) + 0x10;
					////nSubframes
					////polygon.uvs[0] = { dfx.Read<byte>(0) / 255.f, dfx.Read<byte>(1) / 255.f };
					////polygon.uvs[1] = { dfx.Read<byte>(4) / 255.f, dfx.Read<byte>(5) / 255.f };
					////polygon.uvs[2] = { dfx.Read<byte>(8) / 255.f, dfx.Read<byte>(9) / 255.f };
					//polygon.materialID = dfx.Read<u16>(6/* + 0x70*/);

					//if (auto info = FindImageInfoById(level.list, polygon.materialID))
					//{
					//	for (int j = 0; j < 3; ++j)
					//	{
					//		polygon.uvs[j].x *= info->width;
					//		polygon.uvs[j].y *= info->height;
					//		polygon.uvs[j].x += info->x;
					//		polygon.uvs[j].y += info->y;
					//		polygon.uvs[j].x /= (float)level.sheet.w;
					//		polygon.uvs[j].y /= (float)level.sheet.h;
					//	}
					//}
					//dfx.baseOffset = was;
				}
				
				polygon.materialID = dfx.Read<u16>(6) % 0x1000;
				polygon.uvs[0] = { dfx.Read<byte>(0) / 255.f, dfx.Read<byte>(1) / 255.f };
				polygon.uvs[1] = { dfx.Read<byte>(4) / 255.f, dfx.Read<byte>(5) / 255.f };
				polygon.uvs[2] = { dfx.Read<byte>(8) / 255.f, dfx.Read<byte>(9) / 255.f };

				if (auto info = FindImageInfoById(level.list, polygon.materialID))
				{
					for (int j = 0; j < 3; ++j)
					{
						polygon.uvs[j].x *= info->width;
						polygon.uvs[j].y *= info->height;
						polygon.uvs[j].x += info->x;
						polygon.uvs[j].y += info->y;
						polygon.uvs[j].x /= (float)level.sheet.w;
						polygon.uvs[j].y /= (float)level.sheet.h;
					}
				}
				else
				{
					printf("Can't find texture info for material 0x%X (%lu)\n", polygon.materialID, polygon.materialID);
				}
				

				dfx.baseOffset = currOffset;
			}
			else
			{
				polygon.materialID = 0xFFFFFFFF;
			}
		}
		model->polygons.push_back(polygon);
	}
	model->hasNoTextures = !hasTexturedFace;
}

void ReadSkybox(file_t& dfx, level_t& level, levelext_t& levelData, geo_t& geo, std::shared_ptr<Model> model)
{
	u32 was = dfx.baseOffset;
	dfx.baseOffset = levelData.dataOffset + levelData.skyboxAddress;

	model->name = "@Skybox";
	model->instances.push_back({ {0, 0, 0}, {0, 0, 0} });

	for (u32 i = 0; i < levelData.nSkybox; ++i)
	{
		u32 vertexOffset = model->vertices.size();
		u16 nPoly = dfx.Read<u16>(2, true);
		addr_t vertAddr = dfx.Read<addr_t>(0, true);
		addr_t polyAddr = dfx.Read<addr_t>(0, true);
		u32 nVerts = dfx.Read<u32>(8, true);

		u32 offset = dfx.baseOffset;
		dfx.baseOffset = levelData.dataOffset + vertAddr;
		for (u32 v = 0; v < nVerts; ++v)
		{
			int x = (short)(dfx.Read<i16>(0, true)) * 20;
			int y = (short)(dfx.Read<i16>(0, true)) * 20;
			int z = (short)(dfx.Read<i16>(0, true)) * 20;
			u16 n = dfx.Read<u16>(0, true);
			model->vertices.push_back(Model::vertex_t{ x, z, -y, x, z, -y, n, 128, 128, 128, 255 });
		}

		dfx.baseOffset = levelData.dataOffset + polyAddr;
		for (u32 p = 0; p < nPoly; ++p)
		{
			Model::polygon_t poly;
			poly.vertex[0] = dfx.Read<u16>(0, true) + vertexOffset;
			poly.vertex[1] = dfx.Read<u16>(0, true) + vertexOffset;
			poly.vertex[2] = dfx.Read<u16>(0, true) + vertexOffset;
			poly.flags = dfx.Read<u16>(0, true) + vertexOffset;

			addr_t materialAddress = dfx.Read<addr_t>(0, true);
			u32 tempOffset = dfx.baseOffset;
			dfx.baseOffset = levelData.dataOffset + materialAddress;
			poly.uvs[0].x = dfx.Read<byte>(0, true) / 255.f;
			poly.uvs[0].y = dfx.Read<byte>(0, true) / 255.f;
			poly.uvs[1].x = dfx.Read<byte>(2, true) / 255.f;
			poly.uvs[1].y = dfx.Read<byte>(0, true) / 255.f;
			poly.materialID = dfx.Read<u16>(0, true) % 0x1000;
			poly.uvs[2].x = dfx.Read<byte>(0, true) / 255.f;
			poly.uvs[2].y = dfx.Read<byte>(0, true) / 255.f;
			dfx.baseOffset = tempOffset;

			if (auto info = FindImageInfoById(level.list, poly.materialID))
			{
				for (int j = 0; j < 3; ++j)
				{
					poly.uvs[j].x *= info->width;
					poly.uvs[j].y *= info->height;
					poly.uvs[j].x += info->x;
					poly.uvs[j].y += info->y;
					poly.uvs[j].x /= (float)level.sheet.w;
					poly.uvs[j].y /= (float)level.sheet.h;
				}
			}

			model->polygons.push_back(poly);
		}

		dfx.baseOffset = offset;
	}

	dfx.baseOffset = was;
}

void ReadLevelGeometry(file_t& dfx, level_t& level, levelext_t& levelData, addr_t geometryAddress)
{
	geo_t geo;
	dfx.baseOffset += geometryAddress;
	geo.isLevel = true;
	geo.bspAddress = dfx.Read<addr_t>(0);
	geo.vertexCount = dfx.Read<u32>(0x18);
	geo.polygonCount = dfx.Read<u32>(0x1C);
	geo.vertexColorCount = dfx.Read<u32>(0x20);
	geo.vertexAddress = dfx.Read<addr_t>(0x24);
	geo.polygonAddress = dfx.Read<addr_t>(0x28);
	geo.vertexColorAddress = dfx.Read<addr_t>(0x2C);
	geo.materialAddress = dfx.Read<addr_t>(0x30);
	level.models.push_back(std::make_shared<Model>(0xFFFF'FFFF));
	(*level.models.rbegin())->name = "@Level";

	ReadVertices(dfx, level, levelData, geo, level.models[0]);
	ReadPolygons(dfx, level, levelData, geo, level.models[0]);

	auto skybox = std::make_shared<Model>(levelData.skyboxAddress);
	ReadSkybox(dfx, level, levelData, geo, skybox);
	level.models.push_back(skybox);

	dfx.baseOffset = levelData.dataOffset;
}

void ReadObjectGeometry(file_t& dfx, level_t& level, levelext_t& levelData, addr_t modelAddr)
{
	dfx.baseOffset = modelAddr + levelData.dataOffset;
	auto model = std::make_shared<Model>(modelAddr);
	level.models.push_back(model);
	addr_t modelNameAddr = dfx.Read<addr_t>(0x24);
	char name[9] = { 0 };
	memcpy(name, dfx.data + levelData.dataOffset + modelNameAddr, 8);
	printf("Reading %s model data...\n", name);
	model->name = name;

	if (model->name == "proxsig_")
	{
		CreateSpriteObject(level, model, "proxsig_", ECustomImageType::INFO_PROXSIG, 2);
		return;
	} else if (model->name == "cold____")
	{
		CreateSpriteObject(level, model, "cold____", ECustomImageType::INFO_COLD, 1);
		return;
	}

	u16 objCount = dfx.Read<u16>(8);
	addr_t objStartAddr = dfx.Read<u32>(12);
	for (u16 i = 0; i < objCount; ++i)
	{
		dfx.baseOffset = 0;
		dfx.baseOffset = levelData.dataOffset + dfx.Read<addr_t>(levelData.dataOffset + objStartAddr + i * 4);

		geo_t geo;

		geo.isLevel = false;
		geo.vertexCount = dfx.Read<u16>(0, true);
		geo.vertexAddress = dfx.Read<addr_t>(2, true);
		geo.polygonCount = dfx.Read<u16>(8, true);
		geo.polygonAddress = dfx.Read<addr_t>(2, true);
		geo.boneCount = dfx.Read<u16>(0, true);
		geo.boneAddress = dfx.Read<addr_t>(2, true);
		geo.textureAnimAddress = dfx.Read<addr_t>(0, true);

		if (geo.textureAnimAddress != NULL)
		{
			printf("Animated textures detected (0x%X)!\n", geo.textureAnimAddress);
			dfx.baseOffset = levelData.dataOffset + geo.textureAnimAddress;
			//was = dfx.baseOffset;
			//dfx.baseOffset = levelData.dataOffset + geo.textureAnimAddress;
			u32 count = dfx.Read<u32>(0, true);
			for (u32 i = 0; i < count; ++i)
			{
				u32 offset = dfx.baseOffset;
				addr_t matAddr = dfx.Read<addr_t>(0);
				u32 nSubframe = dfx.Read<addr_t>(4);
				for (u32 j = 0; j < nSubframe; ++j)
				{
					//printf("  matAddr: 0x%X\n", matAddr);
				}
				dfx.baseOffset = offset + 0xC;
			}
			////loop:
			//u32 index = 0 % count;
			//dfx.baseOffset = levelData.dataOffset + dfx.Read<addr_t>(0xC * index) + 0x10;
			////nSubframes
			////polygon.uvs[0] = { dfx.Read<byte>(0) / 255.f, dfx.Read<byte>(1) / 255.f };
			////polygon.uvs[1] = { dfx.Read<byte>(4) / 255.f, dfx.Read<byte>(5) / 255.f };
			////polygon.uvs[2] = { dfx.Read<byte>(8) / 255.f, dfx.Read<byte>(9) / 255.f };
			//polygon.materialID = dfx.Read<u16>(6/* + 0x70*/);

			//if (auto info = FindImageInfoById(level.list, polygon.materialID))
			//{
			//	for (int j = 0; j < 3; ++j)
			//	{
			//		polygon.uvs[j].x *= info->width;
			//		polygon.uvs[j].y *= info->height;
			//		polygon.uvs[j].x += info->x;
			//		polygon.uvs[j].y += info->y;
			//		polygon.uvs[j].x /= (float)level.sheet.w;
			//		polygon.uvs[j].y /= (float)level.sheet.h;
			//	}
			//}
			//dfx.baseOffset = was;
		}

		ReadVertices(dfx, level, levelData, geo, model);
		ReadPolygons(dfx, level, levelData, geo, model);
	}
}

std::shared_ptr<Model> CreatePathPointObject(level_t& level, addr_t addr)
{
	std::shared_ptr<Model> pathPoint = std::make_shared<Model>(addr);
	CreateSpriteObject(level, pathPoint, "$PathPoint", ECustomImageType::INFO_POINT, 1);
	level.models.push_back(pathPoint);
	return pathPoint;
}

void AddDiamondToModel(std::shared_ptr<Model> model, glm::vec3 pos, float scale = 1.f)
{
	Model::vertex_t v;
	v.r = v.a = 255;
	v.g = v.b = 0;

	auto addPoint = [&v, model](const glm::vec3& p)
		{
			v.x = v.oX = (int)p.x;
			v.y = v.oY = (int)p.y;
			v.z = v.oZ = (int)p.z;
			model->vertices.push_back(v);
		};

	addPoint(pos + glm::vec3{  scale, 0, 0 });
	addPoint(pos + glm::vec3{ -scale, 0, 0 });
	addPoint(pos + glm::vec3{ 0,  scale, 0 });
	addPoint(pos + glm::vec3{ 0, -scale, 0 });
	addPoint(pos + glm::vec3{ 0, 0,  scale });
	addPoint(pos + glm::vec3{ 0, 0, -scale });

	model->polygons.push_back({
		model->vertices.size() - 2,
		model->vertices.size() - 6,
		model->vertices.size() - 4,
		});

	model->polygons.push_back({
		model->vertices.size() - 6,
		model->vertices.size() - 1,
		model->vertices.size() - 4,
		});

	model->polygons.push_back({
		model->vertices.size() - 1,
		model->vertices.size() - 5,
		model->vertices.size() - 4,
		});

	model->polygons.push_back({
		model->vertices.size() - 5,
		model->vertices.size() - 2,
		model->vertices.size() - 4,
		});

	// ---

	model->polygons.push_back({
		model->vertices.size() - 2,
		model->vertices.size() - 3,
		model->vertices.size() - 6,
		});

	model->polygons.push_back({
		model->vertices.size() - 6,
		model->vertices.size() - 3,
		model->vertices.size() - 1,
		});

	model->polygons.push_back({
		model->vertices.size() - 1,
		model->vertices.size() - 3,
		model->vertices.size() - 5,
		});

	model->polygons.push_back({
		model->vertices.size() - 5,
		model->vertices.size() - 3,
		model->vertices.size() - 2,
		});
}

void AddLineToModel(std::shared_ptr<Model> model, glm::vec3 start, glm::vec3 end)
{
	const glm::vec3 direction = glm::normalize(end - start);
	if (direction.x == 0 && direction.y == 0 && direction.z == 0 || isnan(direction.x) || isnan(direction.y) || isnan(direction.z))
		return; // 0-vector or invalid

	glm::vec3 normal = glm::normalize(glm::cross(direction, { 0, 1, 0 }));
	if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) // happens when the direction vector points up or down?
		normal = { 1, 0, 0 };

	const glm::vec3 perp = glm::normalize(glm::cross(direction, normal));
	Model::vertex_t v;
	v.r = 0;
	v.b = v.g = v.a = 255;

	auto addPoint = [&v, model](const glm::vec3& p)
		{
			v.x = v.oX = (int)p.x;
			v.y = v.oY = (int)p.y;
			v.z = v.oZ = (int)p.z;
			model->vertices.push_back(v);
		};

	addPoint(start + normal * 25.f);
	addPoint(start - normal * 25.f + perp * 25.f);
	addPoint(start - normal * 25.f - perp * 25.f);
	addPoint(end);

	model->polygons.push_back({
		model->vertices.size() - 3,
		model->vertices.size() - 1,
		model->vertices.size() - 4,
		});

	model->polygons.push_back({
		model->vertices.size() - 1,
		model->vertices.size() - 2,
		model->vertices.size() - 4,
		});

	model->polygons.push_back({
		model->vertices.size() - 1,
		model->vertices.size() - 3,
		model->vertices.size() - 2,
		});
}

void ReadMovingPlatform(file_t& dfx, level_t& level, levelext_t& levelData, addr_t ownerAddr, addr_t platformAddr)
{
	dfx.baseOffset = platformAddr + levelData.dataOffset;
	addr_t pathStart = dfx.Read<addr_t>(0);
	addr_t rotsAddr = dfx.Read<addr_t>(4);
	if (pathStart == 0)
		return;
	
	dfx.baseOffset = levelData.dataOffset + pathStart;
	addr_t pointsAddr = dfx.Read<addr_t>(0);
	u16 nPoints = dfx.Read<u16>(4);
	level.paths.push_back({dfx.baseOffset, ownerAddr});
	dfx.baseOffset = levelData.dataOffset + pointsAddr;
	for (u16 i = 0; i < nPoints; ++i)
	{
		level.paths.rbegin()->points.push_back({
			dfx.Read<u16>(i * 0x20 + 0),
			dfx.Read<i16>(i * 0x20 + 2),
			dfx.Read<i16>(i * 0x20 + 4),
			dfx.Read<i16>(i * 0x20 + 6)
		});
	}

	if (rotsAddr == 0)
		return;

	dfx.baseOffset = levelData.dataOffset + rotsAddr;
	u16 nRots = dfx.Read<u16>(4);
	dfx.baseOffset = levelData.dataOffset + dfx.Read<addr_t>(0);
	constexpr float c_PI_2_FROM_1024 = glm::pi<float>() / 2048.f;
	for (u16 i = 0; i < nRots; ++i)
	{
		level.paths.rbegin()->rotations.push_back({
			dfx.Read<u16>(i * 10 + 0),
			dfx.Read<i16>(i * 10 + 2) * (1.f / 0x1000),
			dfx.Read<i16>(i * 10 + 4) * (1.f / 0x1000),
			dfx.Read<i16>(i * 10 + 6) * (1.f / 0x1000),
			dfx.Read<i16>(i * 10 + 8) * (-1.f / 0x1000)
			});
	}
}

void ReadObjectInstance(file_t& dfx, level_t& level, levelext_t& levelData, addr_t instanceAddr)
{
	dfx.baseOffset = 0;
	addr_t modelAddr = dfx.Read<addr_t>(instanceAddr);
	u32 modelIndex = 0;
	for (auto& m : level.models)
	{
		if (m->addr == modelAddr)
			break;
		++modelIndex;
	}

	if (modelIndex == level.models.size())
	{
		ReadObjectGeometry(dfx, level, levelData, modelAddr);
		if (level.models[modelIndex]->hasNoTextures)
		{
			dfx.baseOffset = instanceAddr;
		}
	}
	
	dfx.baseOffset = instanceAddr;
	constexpr float c_PI_2_FROM_1024 = glm::pi<float>() / 2048.f;
	glm::vec3 rot = { dfx.Read<i16>(10) * c_PI_2_FROM_1024, dfx.Read<i16>(12) * -c_PI_2_FROM_1024, dfx.Read<i16>(14) * c_PI_2_FROM_1024 };
	glm::vec3 pos = { -dfx.Read<i16>(16) * 0.001f, -dfx.Read<i16>(20) * 0.001f, dfx.Read<i16>(18) * 0.001f };
	level.models[modelIndex]->instances.push_back({ pos, rot, true, instanceAddr,
		{
			dfx.Read<u32>(0x20),
			dfx.Read<u32>(0x24),
			dfx.Read<u32>(0x28),
			dfx.Read<u32>(0x2C),
		}
	});

	// Custom parsing for some stuff
	std::vector<const char*> listOfPlatformTypes = {
		"mplat___",
		"flttblb_",
		"flttbl__",
		"finplat_",
		"tbplat__",
		"cart____",

		"fltdesk_",
		"fltchst_",

		"tube____",
		"tubegls_",

		"kplat___",
		"kplatb__",
		"kplatc__",
		"kplatd__",

		"kswing__",
		"kngdmnd_",
		"const___",

		"jimbloc_",
		"jplat___",
		"frocket_",
		"jimboat_",
		"logturn_",
		"jimplts_",
		"bldrgen_",

		"flyplat_",
		"jimplat_",

		"qsauc___",
		"rocket__",
		"astplta_",
		"darkshp_",

		"darksop_",
		"splat___",
		"astpltb_",
		"poop____", // why, these are just space platforms
		"poopqq__",
		"poopz___",
		"qplat___",
		"qsmall__",

		"qelev___",
		"qpad____",
		"qdoor___",
		"qbars___",
		"qsdoor__",
		"qssdoor_",
		"discoff_",
		"lvltv___",
		"jaw_____",

		"aztcflr_",
		"aztcwl__",
		"aztcbs__",
		"aztcbks_",
		"trndor__",
		"gengen__",
		"rockplt_",

		"bee_____",
		"rzstart_",
		"rzbrain_",
		"follow__",
		"node____",
		"pulse___",
		"mspider_",
		"rebggen_",
		"rezsoul_",
		"choppa__",
		"tankb___",

		"hrblock_",
		"hrswtch_",
		"reza____",
		"skel____",
		"hhelev__",

		"draga___",
		"kbgen___",
		"moo_____",
		"shark___",

		"blastx__",
		"blasty__",
		"tvgen___",
		"tvgurny_",
		"mutant__",
		"scorp___",
		"sewertp_",
		"casdraw_",

		"@Path",
	};
	for(auto& n : listOfPlatformTypes)
		if (level.models[modelIndex]->name == n)
		{
			ReadMovingPlatform(dfx, level, levelData, instanceAddr, dfx.Read<addr_t>(0x28));
			break;
		}

	//if (level.models[modelIndex]->name == "qmark___")
	//{
	//	auto& inst = *level.models[modelIndex]->instances.rbegin();
	//	dfx.baseOffset = levelData.dataOffset + *(u32*)&inst.instanceData[0];
	//	u16 nStrs = dfx.Read<u16>(0xA);
	//	auto off = dfx.baseOffset;

	//	for (u16 i = 0; i < nStrs; ++i)
	//	{
	//		dfx.baseOffset = off;
	//		dfx.baseOffset = levelData.dataOffset + dfx.Read<u32>(0x10 + 0x8 * i);
	//		int j = 0;
	//		while (j < 255)
	//		{
	//			if (dfx.Read<char>(j) == '\0')
	//				break;
	//			++j;
	//		}
	//		char buffer[256] = { 0 };
	//		memcpy_s(buffer, 255, dfx.data + dfx.baseOffset, j);
	//		inst.qmarkText += std::string(buffer) + "\n";
	//	}
	//	inst.qmarkText.pop_back();
	//}
}

struct GexTex_t
{
	struct TexInfo_t
	{
		GrLOD_t smallLod;
		GrLOD_t largeLod;
		GrAspectRatio_t aspectRatio;
		GrTextureFormat_t format;
	} info;
	struct NCCTable_t
	{
		FxU8 yRGB[16];
		FxI16 iRGB[4][3];
		FxI16 qRGB[4][3];
		FxU32 packed_data[12];
	} ncctable;
	FxU32 smallLodBytes;
	FxU32 largeLodBytes;
};

auto GetImageSizeFromTexture(GrLOD_t lod, GrAspectRatio_t aspect)
{
	struct size
	{
		u32 w, h;
	};

	const FxU32 magicNum = 256U >> lod;

	switch (aspect)
	{
	case GrAspectRatio_t::GR_ASPECT_8x1:
		return size{ magicNum, magicNum >> 3 };

	case GrAspectRatio_t::GR_ASPECT_4x1:
		return size{ magicNum, magicNum >> 2 };

	case GrAspectRatio_t::GR_ASPECT_2x1:
		return size{ magicNum, magicNum >> 1 };

	case GrAspectRatio_t::GR_ASPECT_1x2:
		return size{ magicNum >> 1, magicNum };

	case GrAspectRatio_t::GR_ASPECT_1x4:
		return size{ magicNum >> 2, magicNum };

	case GrAspectRatio_t::GR_ASPECT_1x8:
		return size{ magicNum >> 3, magicNum };

	default:
	case GrAspectRatio_t::GR_ASPECT_1x1:
		return size{ magicNum, magicNum };
	}
}

glm::vec4* ConvertARGB4444(file_t& vfx, const GexTex_t& tex)
{
	auto [w, h] = GetImageSizeFromTexture(tex.info.largeLod, tex.info.aspectRatio);

	glm::vec4* buffer = new glm::vec4[w * h];

	for (size_t i = 0; i < tex.largeLodBytes / 2; ++i)
	{
		FxU16 pixel_data = vfx.Read<FxU16>(0, true);
#pragma warning(push)
#pragma warning(disable: 6386)
		buffer[i] = {
			(FxU8)(((pixel_data & 0x0F00) >> 8) * 0x11),
			(FxU8)(((pixel_data & 0x00F0) >> 4) * 0x11),
			(FxU8)(((pixel_data & 0x000F)) * 0x11),
			(FxU8)(((pixel_data & 0xF000) >> 12) * 0x11)
		};
#pragma warning(pop)
	}

	return buffer;
}

glm::vec4* ConvertARGB1555(file_t& vfx, const GexTex_t& tex)
{
	auto [w, h] = GetImageSizeFromTexture(tex.info.largeLod, tex.info.aspectRatio);

	glm::vec4* buffer = new glm::vec4[w * h];

	for (size_t i = 0; i < tex.largeLodBytes / 2; ++i)
	{
		FxU16 pixel_data = vfx.Read<FxU16>(0, true);
#pragma warning(push)
#pragma warning(disable: 6386)
		buffer[i] = {
			(FxU8)(((pixel_data >> 10) & 0x1F) * 0x08),
			(FxU8)(((pixel_data >> 5) & 0x1F) * 0x08),
			(FxU8)(((pixel_data) & 0x1F) * 0x8),
			(FxU8)(((pixel_data & 0x8000) ? 0xFF : 0x00))
		};
#pragma warning(pop)
	}

	return buffer;
}

glm::vec4* ConvertYIQ422(file_t& vfx, const GexTex_t& tex)
{
	auto [w, h] = GetImageSizeFromTexture(tex.info.largeLod, tex.info.aspectRatio);

	glm::vec4* buffer = new glm::vec4[w * h];

	GexTex_t::NCCTable_t ncc;
	const GexTex_t::NCCTable_t* ncc1 = &tex.ncctable;
	memcpy(&ncc, ncc1, sizeof(GexTex_t::NCCTable_t));

	for (int i = 0; i < 4; ++i)
	{
		if (ncc.iRGB[i][0] & 0x100)
			ncc.iRGB[i][0] |= 0xff00;
		if (ncc.iRGB[i][1] & 0x100)
			ncc.iRGB[i][1] |= 0xff00;
		if (ncc.iRGB[i][2] & 0x100)
			ncc.iRGB[i][2] |= 0xff00;

		if (ncc.qRGB[i][0] & 0x100)
			ncc.qRGB[i][0] |= 0xff00;
		if (ncc.qRGB[i][1] & 0x100)
			ncc.qRGB[i][1] |= 0xff00;
		if (ncc.qRGB[i][2] & 0x100)
			ncc.qRGB[i][2] |= 0xff00;
	}

	for (size_t i = 0; i < tex.largeLodBytes; ++i)
	{
		FxU8 in = vfx.Read<FxU8>(0, true);

		FxI32 R = (FxI32)ncc.yRGB[in >> 4] + ncc.iRGB[(in >> 2) & 0x3][0]
			+ ncc.qRGB[(in) & 0x3][0];

		FxI32 G = (FxI32)ncc.yRGB[in >> 4] + ncc.iRGB[(in >> 2) & 0x3][1]
			+ ncc.qRGB[(in) & 0x3][1];

		FxI32 B = (FxI32)ncc.yRGB[in >> 4] + ncc.iRGB[(in >> 2) & 0x3][2]
			+ ncc.qRGB[(in) & 0x3][2];

		R = ((R < 0) ? 0 : ((R > 255) ? 255 : R));
		G = ((G < 0) ? 0 : ((G > 255) ? 255 : G));
		B = ((B < 0) ? 0 : ((B > 255) ? 255 : B));

#pragma warning(push)
#pragma warning(disable: 6386)
		buffer[i] = {
			(FxU8)(R), (FxU8)(G), (FxU8)(B), 0xFF
		};
#pragma warning(pop)

		++in;
	}

	return buffer;
}

glm::vec4* ReadTexture(file_t& vfx, const GexTex_t& tex)
{
	switch (tex.info.format)
	{
	case GrTextureFormat_t::GR_TEXFMT_ARGB_4444:
		return ConvertARGB4444(vfx, tex);

	case GrTextureFormat_t::GR_TEXFMT_ARGB_1555:
		return ConvertARGB1555(vfx, tex);

	case GrTextureFormat_t::GR_TEXFMT_YIQ_422:
		return ConvertYIQ422(vfx, tex);

	default:
		printf("Unknown type: %d\n", tex.info.format);
		return NULL;
	}
}

void BlitTex(texture_t& dst, const texture_t& src, int x, int y)
{
	for (u32 yi = 0; yi < src.h; ++yi)
	{
		for (u32 xi = 0; xi < src.w; ++xi)
		{
			dst.pixels[(y + yi) * dst.w + (x + xi)] = src.pixels[yi * src.w + xi];
		}
	}
}

void LoadTextures(const std::string& filepath, level_t& level)
{
	file_t vfx;
	if (!ReadFile(filepath, vfx))
		return;

	u32 numTex = vfx.Read<u32>(0);

	vfx.baseOffset = 4;
	GexTex_t gexTex;
	for (u32 i = 0; i < numTex; ++i)
	{
		gexTex.info.smallLod = vfx.Read<GrLOD_t>(0, true);
		gexTex.info.largeLod = vfx.Read<GrLOD_t>(0, true);
		gexTex.info.aspectRatio = vfx.Read<GrAspectRatio_t>(0, true);
		gexTex.info.format = vfx.Read<GrTextureFormat_t>(0, true);
		(void)vfx.Read<FxU32>(0, true); // addr
		for (int i = 0; i < 16; ++i)
			gexTex.ncctable.yRGB[i] = vfx.Read<FxU8>(0, true);

		for (int y = 0; y < 4; ++y)
			for (int x = 0; x < 3; ++x)
				gexTex.ncctable.iRGB[y][x] = vfx.Read<FxI16>(0, true);

		for (int y = 0; y < 4; ++y)
			for (int x = 0; x < 3; ++x)
				gexTex.ncctable.qRGB[y][x] = vfx.Read<FxI16>(0, true);

		for (int i = 0; i < 12; ++i)
			gexTex.ncctable.packed_data[i] = vfx.Read<FxU32>(0, true);

		gexTex.smallLodBytes = vfx.Read<FxU32>(0, true);
		gexTex.largeLodBytes = vfx.Read<FxU32>(0, true);

		auto t = ReadTexture(vfx, gexTex);
		auto [w, h] = GetImageSizeFromTexture(gexTex.info.largeLod, gexTex.info.aspectRatio);
		if (t != NULL)
		{
			for (u32 ii = 0; ii < w * h; ++ii)
			{
				auto& pixel = t[ii];
				pixel.r /= 255.f;
				pixel.g /= 255.f;
				pixel.b /= 255.f;
				pixel.a /= 255.f;
			}
		}
		if (auto info = FindImageInfoById(level.list, i))
		{
			BlitTex(level.sheet, texture_t{ w, h, t }, info->x, info->y);
		}
		//delete[] t;
		level.textures.push_back(texture_t{ w, h, t });
	}
	
	for(size_t i = 0; i < customImages.size(); ++i)
	{
		if (auto info = FindImageInfoById(level.list, ECustomImageType::CUSTOM_IMAGE_BASE + i))
		{
			BlitTex(level.sheet, customImages[i], info->x, info->y);
		}
		level.textures.push_back(customImages[i]);
	}
}

void LoadCustomImages()
{
	if (!customImages.empty())
		return;

	file_t file;
	std::string rootType = "..";
	if (!ReadFile("../data/images/spawn.png", file))
	{
		rootType = ".";
		if (!ReadFile("./data/images/spawn.png", file))
			return; // Failed to open both files
	}

	file.Close();

	const std::vector<std::string> filesToLoad = {
		"/data/images/spawn.bin",
		"/data/images/proxsig.bin",
		"/data/images/unknown.bin",
		"/data/images/point.bin",
		"/data/images/cold.bin"
	};

	for (auto& fileName : filesToLoad)
	{
		if (ReadFile(rootType + fileName, file))
		{
			texture_t image;
			image.w = file.Read<u32>(0);
			image.h = file.Read<u32>(4);

			const unsigned short nPalette = file.Read<u16>(8);
			std::vector<glm::vec4> palette;
			for (int i = 0; i < nPalette; ++i)
				palette.push_back({
					file.Read<byte>(i * 4 + 10 + 3) / 255.f,
					file.Read<byte>(i * 4 + 10 + 2) / 255.f,
					file.Read<byte>(i * 4 + 10 + 1) / 255.f,
					file.Read<byte>(i * 4 + 10 + 0) / 255.f,
				});

			const byte compression = file.Read<byte>(10 + 4 * nPalette);
			enum CompressionFormat
			{
				NONE = 0,
				RLE = 1
			};

			image.pixels = new glm::vec4[image.w * image.h];
			image.deletePixels = false;

			switch (compression)
			{
			case NONE:
				for (u32 y = 0; y < image.h; ++y)
					for (u32 x = 0; x < image.w; ++x)
					{
						size_t index = y * image.w + x;
						image.pixels[index] = palette[file.Read<byte>(10 + 4 * nPalette + 1 + index)];
					}
				break;

			case RLE:
			{
				const u32 len = file.Read<u32>(10 + 4 * nPalette + 1, true);
				u32 index = 0;
				int count = 0;
				byte pid = 0;
				for(u32 entryIndex = 0; entryIndex < len / 2; ++entryIndex)
				{
					count = file.Read<byte>(0);
					pid = file.Read<byte>(1, true);
					for (int i = 0; i < count; ++i)
					{
#pragma warning(suppress: 6386) // No real buffer overrun
						image.pixels[index++] = palette[pid];
					}
				}
				break;
			}
			}

			customImages.push_back(image);
			file.Close();
		}
	}
}

bool GetTextureInformation(const std::string& filepath, ImagePacker::ImageInformationList& list)
{
	file_t f;
	if (ReadFile(filepath, f))
	{
		u32 nFiles = f.Read<u32>(0, true);
		for (u32 i = 0; i < nFiles; ++i)
		{
			GrLOD_t lod = f.Read<GrLOD_t>(sizeof(GrLOD_t), true);
			GrAspectRatio_t asp = f.Read<GrAspectRatio_t>(0, true);
			// Skip data
			(void)f.Read<u32>(f.Read<u32>(0x7C, true) - 4, true);

			auto [w, h] = GetImageSizeFromTexture(lod, asp);
			list.push_back({ (int)w, (int)h, (void*)i });
		}
		LoadCustomImages();
		for(size_t i = 0; i < customImages.size(); ++i)
			list.push_back({ (int)customImages[i].w, (int)customImages[i].h, (void*)(ECustomImageType::CUSTOM_IMAGE_BASE + i)});
		return true;
	}
	return false;
}

std::string GetLevelName(const std::string& levelStr, u32 dataOffsetRaw)
{
	const std::unordered_map<std::string, std::string> QuickMap = {
		{ "spy_____", "The Spy Who Loved Himself" },
		{ "nypd____", "In Drag Net" },
		{ "gillig__", "Gilligex Isle" },
		{ "mooshu__", "Mooshu Pork" },
		{ "gexzil__", "Gexzilla Vs. Mecharez" },
		{ "final___", "Channel Z" },
		{ "train___", "Poltergex" },
		{ "junk____", "I Got the Reruns" },
		{ "aztec___", "Aztec 2 Step" },
		{ "lost____", "Trouble in Uranus" }
	};

	if (auto it = QuickMap.find(levelStr); it != QuickMap.end())
		return it->second;

	struct LevelInfo_t
	{
		std::string name;
		u32 value;
	};

	const std::unordered_map<std::string, std::unordered_map<u32, std::string>> QuickMap2 = {
		{ "looney__",
			{
				{ 0x00006208, "Out of Toon" },
				{ 0x0000204E, "OoT67" },
				{ 0x00005C2D, "Fine Tooning" },
				//{ 0x00000000, "OoT70" } // Can't actually be loaded, no data
				{ 0x0000275A, "OoT88" }
			}
		},
		{ "circuit_",
			{
				{ 0x00002411, "Chips and Dips" },
				{ 0x0000694F, "www.dotcom.com" },
				{ 0x00007242, "Honey I Shrunk the Gecko" },
			}
		},
		{ "horror__",
			{
				{ 0x00006BE3, "Frankensteinfeld" },
				{ 0x0000737B, "Smellraiser" },
				{ 0x000051F3, "Texas Chainsaw Manicure" },
				{ 0x00002B27, "Thursday the 12th" }
			}
		},
		{ "kungfu__",
			{
				{ 0x00006FC1, "Samurai Night Fever" },
				{ 0x00006C72, "Mao Tse Tongue" },
				{ 0x00002F95, "Lizard in a China Shop" }
			}
		},
		{ "scifi___",
			{
				{ 0x00005501, "The Umpire Strikes Out" },
				{ 0x0000469E, "Pain in the Asteroids" }
			}
		},
		{ "rezop___",
			{
				{ 0x00006A05, "Mazed and Confused" },
				{ 0x00000BBD, "Bugged Out" },
				{ 0x00007552, "No Weddings and a Funeral" }
			}
		},
		{ "prehst__",
			{
				{ 0x000045B0, "Pangaea 90210" },
				{ 0x000040DE, "This Old Cave" },
				{ 0x00005DB3, "Lava Daba Doo" }
			}
		},
		{ "map_____",
			{
				{ 0x000071A0, "The Media Dimension" },
				{ 0x00006414, "Main Menu" },
				{ 0x00004D0A, "Credits Menu" }
			}
		}
	};

	if (auto it1 = QuickMap2.find(levelStr); it1 != QuickMap2.end())
	{
		if (auto it2 = it1->second.find(dataOffsetRaw); it2 != it1->second.end())
		{
			return it2->second;
		}
	}

	return "Unknown Level";
}

void ApplyPathModels(level_t& level)
{

	for (auto& p : level.paths)
	{
		if (p.points.size() == 0)
			continue;

		auto mdl = std::make_shared<Model>(p.address);
		mdl->name += "@Path-" + Hexify(p.address);
		level.models.push_back(mdl);
		mdl->instances.push_back({ {0, 0, 0}, {0, 0, 0} });
		mdl->hasNoTextures = true;
		for (size_t i = 0; (i + 1) < p.points.size(); ++i)
		{
			AddLineToModel(mdl, glm::vec3{
					p.points[i].x,
					p.points[i].z,
					-p.points[i].y,
				}, glm::vec3{
					p.points[i + 1].x,
					p.points[i + 1].z,
					-p.points[i + 1].y,
				});

			AddDiamondToModel(mdl, glm::vec3{
					p.points[i].x,
					p.points[i].z,
					-p.points[i].y,
				}, 50.f);
		}

		AddDiamondToModel(mdl, glm::vec3{
				p.points.rbegin()->x,
				p.points.rbegin()->z,
				-p.points.rbegin()->y,
			}, 50.f);
	}
}

bool LoadLevel(const std::string& filepath, level_t& level)
{
	file_t dfx;
	if (!ReadFile(filepath, dfx))
		return false;

	levelext_t levelData;

	const std::string vfxPath = filepath.substr(0, filepath.find_last_of(".")) + ".vfx";
	if (level.sheet.pixels)
	{
		delete[] level.sheet.pixels;
		level.sheet.pixels = NULL;
	}
	level.list.clear();
	if (GetTextureInformation(vfxPath, level.list))
	{
		if (int size = ImagePacker::GeneratePackedList(level.list, 256); size != 0)
		{
			printf("Sheet generated at %dx%d\n", size, size);
			level.sheet = { (unsigned int)size, (unsigned int)size, new glm::vec4[size * size] };
			if (level.sheet.pixels)
			{
				for (int y = 0; y < size; ++y)
				{
					for (int x = 0; x < size; ++x)
					{
						if (((x % 128) == (x % 64) && (y % 128) == (y % 64)) || ((x % 128) != (x % 64) && (y % 128) != (y % 64)))
							level.sheet.pixels[x + y * size] = { 1, 0, 1, 1 };
						else
							level.sheet.pixels[x + y * size] = { 0.5, 0, 0.5, 1 };
					}
				}
				LoadTextures(vfxPath, level);
			}
		}
	}

	level.baseData = levelData.dataOffset = ((dfx.Read<u32>(0) + 0x200) >> 9) << 11;
	dfx.baseOffset = levelData.dataOffset;
	
	levelData.modelAddress = dfx.Read<addr_t>(0x3C);
	levelData.nObjects = dfx.Read<u32>(0x78);
	levelData.objAddress = dfx.Read<addr_t>(0x7C);
	levelData.nSkybox = dfx.Read<u32>(0x20);
	levelData.skyboxAddress = dfx.Read<u32>(0x24);
	level.bgColor[0] = dfx.Read<byte>(68) / 255.f;
	level.bgColor[1] = dfx.Read<byte>(69) / 255.f;
	level.bgColor[2] = dfx.Read<byte>(70) / 255.f;

	ReadLevelGeometry(dfx, level, levelData, dfx.Read<addr_t>(0));

	std::shared_ptr<Model> misc = std::make_shared<Model>(0);
	CreateSpriteObject(level, misc, "@Path", ECustomImageType::INFO_UNKNOWN, 1);
	level.models.push_back(misc);

	// Create spawn point
	dfx.baseOffset = levelData.dataOffset;
	std::shared_ptr<Model> spawn = std::make_shared<Model>(levelData.dataOffset + 0x28);
	CreateSpriteObject(level, spawn, "$Spawn", ECustomImageType::INFO_SPAWN);
	level.models.push_back(spawn);
	spawn->instances.push_back({});
	spawn->instances[0].position = { dfx.Read<i16>(0x28) * -0.001f, dfx.Read<i16>(0x2C) * -0.001f, dfx.Read<i16>(0x2A) * 0.001f };

	size_t currModelIndex = level.models.size();

	for (u32 i = 0; i < levelData.nObjects; ++i)
	{
		ReadObjectInstance(dfx, level, levelData, levelData.dataOffset + levelData.objAddress + 0x30 * i);
	}

	// By treating the level as a model, we need to give it an instance
	level.models[0]->instances.push_back({});

	dfx.baseOffset = 0;
	std::string s;
	s.resize(8);
	memcpy(s.data(), dfx.data + levelData.dataOffset + 0xE0, 8);
	level.name = GetLevelName(s, dfx.Read<u32>(0));

	ApplyPathModels(level);

	std::sort(level.models.begin() + currModelIndex, level.models.end(), [](std::shared_ptr<Model> a, std::shared_ptr<Model> b)
		{
			return a->name < b->name;
		});

	memcpy(level.pickupName[0], dfx.data + levelData.dataOffset + 0xEC, 8);
	memcpy(level.pickupName[1], dfx.data + levelData.dataOffset + 0xF8, 8);
	memcpy(level.pickupName[2], dfx.data + levelData.dataOffset + 0x104, 8);
	level.pickupName[0][8] = level.pickupName[1][8] = level.pickupName[2][8] = '\0';

	return true;
}