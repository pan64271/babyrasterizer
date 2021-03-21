#pragma once

#include <vector>

#include "geometry.h"
#include "tgaimage.h"
#include "model.h"

// interface for shader struct
struct IShader
{
	virtual Vec4f vertex(unsigned nthvert, Vec4f worldCoord, Vec2f uv, Vec3f normal) = 0;
	virtual bool fragment(Vec3f bar, Vec3f &color) = 0;
};

// struct for clipping parameter
struct Vertex
{
	Vec3f normal;
	Vec4f worldCoord, clipCoord;
	Vec2f uv;

	Vertex(Vec4f worldCoord = Vec4f(), Vec4f clipCoord = Vec4f(), Vec2f uv = Vec2f(), Vec3f normal = Vec3f())
		: worldCoord(worldCoord), clipCoord(clipCoord), uv(uv), normal(normal) {}
};

// functions for viewing transformation
Matrix lookat(Vec3f eye, Vec3f center, Vec3f up);
Matrix projection(double fov, double ratio, double n, double f);
Matrix perspective(double n, double f);
Matrix ortho(float l, float r, float b, float t, float n, float f);
Matrix viewport(unsigned width, unsigned height);

// functions for rasterization
Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P);
void triangle(Vec4f *screenCoords, IShader &shader, Vec3f *colorBuffer, float *zBuffer, unsigned width, unsigned height, const float d[][2], unsigned cntSample);

// functions for clipping
void zClip(const std::vector<Vertex> &original, std::vector<Vertex> &result);
void singleFaceZClip(const std::vector<Vertex> &original, std::vector<Vertex> &result);
void pushIntersection(std::vector<Vertex> &result, Vertex now, Vertex next);
