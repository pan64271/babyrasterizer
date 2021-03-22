#include <limits>
#include <vector>

#include "tgaimage.h"
#include "geometry.h"
#include "model.h"
#include "gl.h"

struct DepthShader : public IShader
{
	// uniform variables
	Matrix uVpPV;
	// varying variables
	mat<4, 3, float> vScreenCoords;


	DepthShader() {}

	Vec4f vertex(unsigned nthvert, Vec4f worldCoord, Vec2f uv, Vec3f normal)
	{
		Vec4f screenCoord = uVpPV * worldCoord;
		screenCoord = screenCoord / screenCoord[3];
		vScreenCoords.set_col(nthvert, screenCoord);

		return screenCoord;
	}

	bool fragment(Vec3f bar, Vec3f &color)
	{
		Vec4f fragPos = vScreenCoords * bar;
		color = Vec3f(255.0f, 255.0f, 255.0f) * powf(expf(fragPos[2]-1.0f), 4.0f);

		return true;
	}
};

struct LightColor
{
	Vec3f ambient, diffuse, specular;

	LightColor(Vec3f ambi = Vec3f(), Vec3f diff = Vec3f(), Vec3f spec = Vec3f())
	{
		ambient = ambi;
		diffuse = diff;
		specular = spec;
	}
};

struct Shader : public IShader
{
	// uniform variables
	Model *uTexture;
	Matrix uModel, uVpPV, uLightVpPV;
	Vec3f uEyePos, uLightPos, uTangent, uBitangent;
	LightColor uLightColor;
	float *uShadowBuffer;
	unsigned uShadowBufferWidth;
	// varying variables
	mat<4, 3, float> vScreenCoords;
	mat<2, 3, float> vUv;
	mat<3, 3, float> vN;
	mat<3, 3, float> vLightSpacePos;
	mat<3, 3, float> vWorldCoords;


	Shader() {}

	Vec4f vertex(unsigned nthvert, Vec4f worldCoord, Vec2f uv, Vec3f normal)
	{
		Vec4f screenCoord = uVpPV * worldCoord;
		float w = screenCoord[3];

		vWorldCoords.set_col(nthvert, proj<3>(worldCoord) / w);

		screenCoord = screenCoord / w;
		screenCoord[2] = screenCoord[2] / w;
		screenCoord[3] = 1.0f / w;
		vScreenCoords.set_col(nthvert, screenCoord);

		Vec2f vertUv = uv / w;
		vUv.set_col(nthvert, vertUv);

		Vec3f vertN = normal / w;
		vN.set_col(nthvert, vertN);

		Vec4f temp = uLightVpPV * worldCoord;
		temp = temp / temp.w;
		Vec3f vertLightSpacePos = proj<3>(temp) / w;
		vLightSpacePos.set_col(nthvert, vertLightSpacePos);

		return screenCoord;
	}

	bool fragment(Vec3f bar, Vec3f &color)
	{
		// calculate w for perspective-correct interpolation
		float w = (vScreenCoords * bar)[3];
		if (fabs(w) < 1e-7) return false;
		w = 1.0f / w;

		// calculate uv for texture indexing
		Vec2f uv = vUv * bar * w;

		// calculate normal vector from tangent space
		mat<3, 3, float> TBN;
		TBN.set_col(0, uTangent);
		TBN.set_col(1, uBitangent);
		TBN.set_col(2, vN * bar * w);
		Vec3f n = (TBN * uTexture->normal(uv)).normalize();
		
		// calculate direction vectors for lattter use
		Vec3f worldCoord = vWorldCoords * bar * w;
		Vec3f lightDir = uLightPos.normalize();
		Vec3f eyeDir = (uEyePos - worldCoord).normalize();
		Vec3f half = (lightDir + eyeDir) / 2.0f;

		// ambient reflection
		Vec3f materialAmbient = uTexture->diffuse(uv).rgb();
		Vec3f ambient = uLightColor.ambient * materialAmbient;
		
		// diffuse reflection
		Vec3f materialDiffuse = uTexture->diffuse(uv).rgb();
		Vec3f diffuse = uLightColor.diffuse * (materialDiffuse * std::max(0.0f, dot(n, lightDir)));

		// specular reflection
		float materialSpecular = uTexture->specular(uv);
		Vec3f specular = uLightColor.specular * (materialSpecular * powf(std::max(0.0f, dot(n, half)), 32.0f));

		// calculate shadow
		float shadow = 1.0f;
		Vec3f lightSpacePos = vLightSpacePos * bar * w;
		if (lightSpacePos.z + 0.1f < uShadowBuffer[int(lightSpacePos.y)*uShadowBufferWidth + int(lightSpacePos.x)])
			shadow = 0.3f;

		// Blinn-Phong lighting model
		color = ambient + (diffuse + specular) * shadow;

		return true;
	}
};

const float PI = acosf(-1.0f);

const unsigned SCREEN_WIDTH = 800;
const unsigned SCREEN_HEIGHT = 800;

const unsigned SHADOW_WIDTH = 800;
const unsigned SHADOW_HEIGHT = 800;

const unsigned CNT_SAMPLE = 4;          // number of samples for every pixel
const float D_MSAA[CNT_SAMPLE][2] = {   // displacements for MSAA samples
	{0.25f, 0.25f}, {0.25f, 0.75f},
	{0.75f, 0.25f}, {0.75f, 0.75f}
};
const float D_NonMSAA[1][2] = {         // displacements for non-MSAA samples
	{0.0f, 0.0f}
};

Vec3f lightPos(1.0f, 1.0f, 1.0f);
LightColor lightColor(Vec3f(0.3f, 0.3f, 0.3f), Vec3f(1.0f, 1.0f, 1.0f), Vec3f(0.5f, 0.5f, 0.5f));

Vec3f eye(1.0f, 1.0f, 3.0f);
Vec3f center(0.0f, 0.0f, 0.0f);
Vec3f up(0.0f, 1.0f, 0.0f);

Matrix shadowMapping(Model **modelData, Matrix *modelTrans, unsigned cntModel, float *shadowBuffer, TGAImage &depth)
{
	Matrix view = lookat(lightPos, center, up);
	Matrix project = ortho(-2.0f, 2.0f, -2.0f, 2.0f, -0.01f, -10.0f);
	Matrix vp = viewport(SHADOW_WIDTH, SHADOW_HEIGHT);

	Vec3f *colorBuffer = new Vec3f[SHADOW_WIDTH * SHADOW_HEIGHT];

	for (unsigned m = 0; m < cntModel; ++m)
	{
		// create shader, set uniform variables of shader
		DepthShader depthShader;
		depthShader.uVpPV = vp * project*view;

		// rendering pipeline: calculate depth vewing from the light
		Matrix modelInverTranspose = modelTrans[m].invert_transpose();
		for (int i = 0; i < modelData[m]->nfaces(); ++i)
		{
			// vertex processing
			Vec4f screenCoords[3];
			for (int j = 0; j < 3; ++j)
			{
				Vec4f worldCoord = modelTrans[m] * embed<4>(modelData[m]->vert(i, j));
				Vec2f uv = modelData[m]->uv(i, j);
				Vec3f normal = proj<3>(modelInverTranspose * Vec4f(modelData[m]->normal(i, j), 0.0f));
				screenCoords[j] = depthShader.vertex(j, worldCoord, uv, normal);
			}

			// ransterization + fragment processing
			triangle(screenCoords, depthShader, colorBuffer, shadowBuffer, SHADOW_WIDTH, SHADOW_HEIGHT, D_NonMSAA, 1);
		}
	}
	std::cerr << "finish shadow depth buffer calculation" << std::endl;

	// write depth color to depth.tga (for debugging)
	for (unsigned x = 0; x < SHADOW_WIDTH; ++x)
	{
		for (unsigned y = 0; y < SHADOW_HEIGHT; ++y)
		{
			Vec3f color = colorBuffer[y * SHADOW_WIDTH + x];
			depth.set(x, y, TGAColor(color.x, color.y, color.z, 255));
		}
	}
	std::cerr << "finish writing depth.tga" << std::endl;
	
	// deallocate resources
	delete[] colorBuffer;
	return vp * project * view;
}

void PhongShading(Model **modelData, Matrix *modelTrans, unsigned cntModel, float *zBuffer, Matrix lightVpPV, float *shadowBuffer, TGAImage &frame)
{
	Matrix view = lookat(eye, center, up);
	Matrix project = projection(PI / 4.0f, 1.0f, -0.01f, -10.0f);
	Matrix vp = viewport(SCREEN_WIDTH, SCREEN_HEIGHT);
	Matrix PV = project * view;

	Vec3f *colorBuffer = new Vec3f[SCREEN_WIDTH * SCREEN_HEIGHT * CNT_SAMPLE];
	for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT * CNT_SAMPLE; ++i)
		colorBuffer[i] = Vec3f(0, 0, 0);

	for (unsigned m = 0; m < cntModel; ++m)
	{
		// create shader, set uniform variables of shader
		Shader PhongShader;
		PhongShader.uTexture = modelData[m];
		PhongShader.uModel = modelTrans[m];
		PhongShader.uVpPV = vp * project * view;
		PhongShader.uLightVpPV = lightVpPV;
		PhongShader.uEyePos = eye;
		PhongShader.uLightPos = lightPos;
		PhongShader.uLightColor = lightColor;
		PhongShader.uShadowBuffer = shadowBuffer;
		PhongShader.uShadowBufferWidth = SHADOW_WIDTH;

		// rendering pipeline: calculate info for each sample
		for (int i = 0; i < modelData[m]->nfaces(); ++i)
		{
			// back-face culling
			Vec3f n = cross(modelData[m]->vert(i, 1) - modelData[m]->vert(i, 0), modelData[m]->vert(i, 2) - modelData[m]->vert(i, 0)).normalize();
			n = proj<3>((view * modelTrans[m]).invert_transpose() * Vec4f(n, 0.0f));
			if (n.z <= 0.0f) continue;

			// z-axis clipping
			Matrix modelInverTranspose = modelTrans[m].invert_transpose();
			std::vector<Vertex> original, clipped;
			for (int j = 0; j < 3; j++)
			{
				Vec4f worldCoord = modelTrans[m] * embed<4>(modelData[m]->vert(i, j));
				Vec4f clipCoord = PV * worldCoord;
				Vec3f normal = proj<3>(modelInverTranspose * Vec4f(modelData[m]->normal(i, j), 0.0f));
				Vec2f uv = modelData[m]->uv(i, j);
				Vertex vertex(worldCoord, clipCoord, uv, normal);
				original.push_back(vertex);
			}
			zClip(original, clipped);

			// frustum culling (only z-axis)
			if (clipped.size() < 3) continue;

			// calculate the tangent and bitangent vectors for this triangle
			mat<2, 3, float> A;
			A[0] = proj<3>(modelTrans[m] * Vec4f(modelData[m]->vert(i, 1) - modelData[m]->vert(i, 0), 0.0f));
			A[1] = proj<3>(modelTrans[m] * Vec4f(modelData[m]->vert(i, 2) - modelData[m]->vert(i, 0), 0.0f));
			mat<2, 2, float> U;
			U[0] = modelData[m]->uv(i, 1) - modelData[m]->uv(i, 0);
			U[1] = modelData[m]->uv(i, 2) - modelData[m]->uv(i, 0);
			mat<2, 3, float> tTB = U.invert() * A;
			PhongShader.uTangent = tTB[0].normalize();
			PhongShader.uBitangent = tTB[1].normalize();

			// shade for each sub-triangle
			for (size_t j = 1; j < clipped.size() - 1; ++j)
			{
				// vertex processing
				Vec4f screenCoords[3];
				screenCoords[0] = PhongShader.vertex(0, clipped[0].worldCoord, clipped[0].uv, clipped[0].normal);
				screenCoords[1] = PhongShader.vertex(1, clipped[j].worldCoord, clipped[j].uv, clipped[j].normal);
				screenCoords[2] = PhongShader.vertex(2, clipped[j+1].worldCoord, clipped[j+1].uv, clipped[j+1].normal);

				// ransterization + fragment processing
				triangle(screenCoords, PhongShader, colorBuffer, zBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, D_MSAA, CNT_SAMPLE);
			}
		}
	}
	std::cerr << "finish shading" << std::endl;

	// write shading color to frame.tga, average the MSAA samples for each pixel
	for (unsigned x = 0; x < SCREEN_WIDTH; ++x)
	{
		for (unsigned y = 0; y < SCREEN_HEIGHT; ++y)
		{
			if (x == 362 && y == 417)
				x = x;
			Vec3f color(0.0f, 0.0f, 0.0f);
			for (unsigned i = 0; i < CNT_SAMPLE; ++i)
			{
				if (zBuffer[CNT_SAMPLE * (y*SCREEN_WIDTH + x) + i] > -std::numeric_limits<float>::max())
				{
					color = color + colorBuffer[CNT_SAMPLE * (y*SCREEN_WIDTH + x) + i];
				}
			}
			color = color / CNT_SAMPLE;
			frame.set(x, y, TGAColor(color.x, color.y, color.z, 255));
		}
	}
	std::cerr << "finish writing frame.tga" << std::endl;

	// deallocate resources
	delete[] colorBuffer;
}

int main()
{
	// allocate buffers for two passes
	float *zBuffer = new float[SCREEN_WIDTH * SCREEN_HEIGHT * CNT_SAMPLE];
	float *shadowBuffer = new float[SCREEN_WIDTH * SCREEN_HEIGHT];
	for (int i = 0; i < SCREEN_WIDTH*SCREEN_HEIGHT; ++i)
	{
		for (int j = 0; j < CNT_SAMPLE; ++j)
			zBuffer[CNT_SAMPLE * i + j] = -std::numeric_limits<float>::max();
		shadowBuffer[i] = -std::numeric_limits<float>::max();
	}

	// load model
	unsigned cntModel = 2;
	Model **modelData = new Model*[cntModel];
	modelData[0] = new Model("./obj/african_head/african_head.obj");
	modelData[1] = new Model("./obj/floor.obj");
	std::cerr << std::endl;
	
	// model tansformations for each model
	Matrix *modelTrans = new Matrix[cntModel];
	modelTrans[0] = Matrix::identity();
	modelTrans[1] = Matrix::identity();
	modelTrans[1][1][3] = -0.3f;

	// shadow pass
	TGAImage depth(SCREEN_WIDTH, SCREEN_HEIGHT, TGAImage::RGB);
	Matrix lightVpPV = shadowMapping(modelData, modelTrans, cntModel, shadowBuffer, depth);
	depth.write_tga_file("./output/depth.tga");
	std::cerr << "Shadow Pass Over" << std::endl << std::endl;

	// shading pass
	TGAImage frame(SCREEN_WIDTH, SCREEN_HEIGHT, TGAImage::RGB);
	PhongShading(modelData, modelTrans, cntModel, zBuffer, lightVpPV, shadowBuffer, frame);
	frame.write_tga_file("./output/frame.tga");
	std::cerr << "Shading Pass Over" << std::endl;

	// deallocate resources
	for (unsigned i = 0; i < cntModel; ++i)
	{
		delete modelData[i];
	}
	delete[] modelData;
	delete[] modelTrans;
	delete[] zBuffer;
	delete[] shadowBuffer;

	return 0;
}
