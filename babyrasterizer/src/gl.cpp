#include <cmath>
#include <algorithm>
#include <iostream>
#include <cassert>

#include "gl.h"

Matrix lookat(Vec3f eye, Vec3f center, Vec3f up)
{
	Vec3f z = (eye - center).normalize();
	Vec3f x = cross(up, z).normalize();
	Vec3f y = cross(z, x).normalize();

	Matrix rotate = Matrix::identity();
	Matrix translate = Matrix::identity();
	for (int i = 0; i < 3; ++i)
	{
		rotate[0][i] = x[i];
		rotate[1][i] = y[i];
		rotate[2][i] = z[i];
		translate[i][3] = -eye[i];
	}
	return rotate * translate;
}

Matrix projection(double fov, double ratio, double n, double f)
{
	float t = (-n) * tanf(fov / 2);
	float r = t * ratio;
	return ortho(-r, r, -t, t, n, f) * perspective(n, f);
}

Matrix perspective(double n, double f)
{
	Matrix ret;
	ret[0][0] = n;
	ret[1][1] = n;
	ret[2][2] = n + f;
	ret[2][3] = -f * n;
	ret[3][2] = 1.0f;
	return ret;
}

Matrix ortho(float l, float r, float b, float t, float n, float f)
{
	Matrix ret = Matrix::identity();
	ret[0][0] = 2.0f / (r - l);
	ret[1][1] = 2.0f / (t - b);
	ret[2][2] = 2.0f / (n - f);
	ret[0][3] = (l + r) / (l - r);
	ret[1][3] = (b + t) / (b - t);
	ret[2][3] = (n + f) / (f - n);
	return ret;
}

Matrix viewport(unsigned width, unsigned height)
{
	Matrix ret = Matrix::identity();
	ret[0][0] = width / 2.0f;
	ret[1][1] = height / 2.0f;
	ret[0][3] = (width - 1) / 2.0f;
	ret[1][3] = (height - 1) / 2.0f;
	return ret;
}

Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P)
{
	Vec3f t[2];
	for (int i = 0; i < 2; i++)
	{
		t[i][0] = B[i] - A[i];
		t[i][1] = C[i] - A[i];
		t[i][2] = A[i] - P[i];
	}
	Vec3f u = cross(t[0], t[1]);

	if (fabs(u.z) < 1e-5) return Vec3f(-1.0f, 0.0f, 0.0f);
	return Vec3f(1.0f - (u.x + u.y) / u.z, u.x / u.z, u.y / u.z);
}

void triangle(Vec4f *screenCoords, IShader &shader, Vec3f *colorBuffer, float *zBuffer, unsigned width, unsigned height, const float d[][2], unsigned cntSample)
{
	// find the minimum bounding box for the triangle on the screen
	Vec2i bboxmin(width - 1, height - 1);
	Vec2i bboxmax(0.0f, 0.0f);
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			bboxmin[j] = std::min(bboxmin[j], int(screenCoords[i][j]));
			bboxmax[j] = std::max(bboxmax[j], int(screenCoords[i][j]));
		}
	}
	// clipping for the x-axis and y-axis
	bboxmin[0] = std::max(0, bboxmin[0]);
	bboxmin[1] = std::max(0, bboxmin[1]);
	bboxmax[0] = std::min(int(width - 1), bboxmax[0]);
	bboxmax[1] = std::min(int(height - 1), bboxmax[1]);

	// rasterize the triangle
	Vec2f A = proj<2>(screenCoords[0]), B = proj<2>(screenCoords[1]), C = proj<2>(screenCoords[2]);
	for (int x = bboxmin.x; x <= bboxmax.x; ++x)
	{
		for (int y = bboxmin.y; y <= bboxmax.y; ++y)
		{
			// calculate color and depth for every sample of a pixel
			Vec3f barMiddle, color;
			bool covered = false;
			for (int i = 0; i < cntSample; ++i)
			{
				Vec2f sample(x + d[i][0], y + d[i][1]);
				Vec3f barSample = barycentric(A, B, C, sample);
				float w = screenCoords[0].w * barSample.x + screenCoords[1].w * barSample.y + screenCoords[2].w * barSample.z;
				w = 1.0f / w;
				float z = screenCoords[0].z * barSample.x + screenCoords[1].z * barSample.y + screenCoords[2].z * barSample.z;
				z = z * w;
				unsigned idx = cntSample * (y*width + x) + i;
				if (barSample.x < 0 || barSample.y < 0 || barSample.z < 0) continue;

				if (barSample.x < 0 || barSample.y < 0 || barSample.z < 0 || z < zBuffer[idx]) continue;

				if (!covered) // calculate the color only once for each pixel
				{
					barMiddle = barycentric(A, B, C, Vec2f(x + 0.5f, y + 0.5f));
					if (!shader.fragment(barMiddle, color)) break;
					covered = true;
				}
				colorBuffer[idx] = color;
				zBuffer[idx] = z;
			}
		}
	}
}

void zClip(const std::vector<Vertex> &original, std::vector<Vertex> &result)
{
	std::vector<Vertex> intermediate;
	singleFaceZClip(original, intermediate);
	for (auto &vertex : intermediate)
	{
		vertex.clipCoord.z = -vertex.clipCoord.z;
	}

	singleFaceZClip(intermediate, result);
	for (auto &vertex : result)
	{
		vertex.clipCoord.z = -vertex.clipCoord.z;
	}
}

void singleFaceZClip(const std::vector<Vertex> &original, std::vector<Vertex> &result)
{
	for (unsigned i = 0; i < original.size(); ++i)
	{
		Vertex now = original[i], next = original[(i + 1) % original.size()];
		float checkNow = now.clipCoord.z / now.clipCoord.w, checkNext = next.clipCoord.z / next.clipCoord.w;
		if (checkNow <= 1.0f)
		{
			result.push_back(now);
		}
		if ((checkNow < 1.0f && checkNext > 1.0f) || (checkNow > 1.0f && checkNext < 1.0f))
		{
			pushIntersection(result, now, next);
		}
	}
}

void pushIntersection(std::vector<Vertex> &result, Vertex now, Vertex next)
{
	// calculate the t for equation: w0 + t*(w1-w0) = z0 + t*(z1-z0)
	float t0 = now.clipCoord.w - now.clipCoord.z;
	float t1 = next.clipCoord.w - next.clipCoord.z;
	float t = t0 / (t0 - t1);

	float z = 1.0f / now.clipCoord.w + t * (1.0f / next.clipCoord.w - 1.0f / now.clipCoord.w);
	z = 1.0f / z;
	Vertex inter;
	inter.clipCoord = now.clipCoord + (next.clipCoord - now.clipCoord) * t;
	inter.worldCoord = now.worldCoord + (next.worldCoord - now.worldCoord) * t;
	inter.worldCoord = inter.worldCoord * z;
	inter.normal = now.normal + (next.normal * now.normal) * t;
	inter.normal = inter.normal * z;
	inter.uv = now.uv + (next.uv - now.uv) * t;
	inter.uv = inter.uv * z;
	result.push_back(inter);
}
