#pragma once

#define IMG_CUNT 11

#include "CTriMesh.h"
#include "CGLTexture.h"
#include <array>
#include <unordered_map>

#define RR_KERNEL_ARG uint, uint, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer

struct MotionInterpolationArgs
{
	std::string clip;
	int framestart;
	int localSize;
	bool useDiamond;
	int mvecIters;
};
struct Triforce
{
	CTriMesh *clothModel;
	CGLTexture *texture;
	cl::ImageGL clImage;
	cl::Buffer clBuffer;
	int frameIdx;
};

class MotionInterpolationTask
{
public:
	MotionInterpolationTask(MotionInterpolationArgs);

	virtual ~MotionInterpolationTask();

	// IComputeTask
	virtual bool InitResources(cl::Device Device, cl::Context Context);
	virtual void LoadMarchImage(cl::Context Context);

	virtual void ReleaseResources();

	virtual void ComputeGPU(cl::Context &Context, cl::CommandQueue &CommandQueue);
	// Not implemented!
	virtual void ComputeCPU(){};
	virtual bool ValidateResults() { return false; };

	// IGUIEnabledComputeTask
	virtual void Render();

	virtual void RenderImg(Triforce &img, float x, float y);

	virtual void OnKeyboard(int Key, int Action);

	virtual void OnMouse(int Button, int State);

	virtual void OnMouseMove(int X, int Y);

	virtual void OnIdle(float ElapsedTime);

	virtual void OnWindowResized(int Width, int Height);

	virtual void SetMVMatrix();

	virtual void PreloadImages(int count);

protected:
	unsigned int m_ImgResX = 0;
	unsigned int m_ImgResY = 0;
	unsigned int m_FrameCounter = 0;

	std::array<Triforce, IMG_CUNT> m_images;

	// OpenGL variables
	GLhandleARB m_VSCloth = 0;
	GLhandleARB m_PSCloth = 0;
	GLhandleARB m_ProgRenderCloth = 0;

	GLhandleARB m_VSMesh = 0;
	GLhandleARB m_PSMesh = 0;
	GLhandleARB m_ProgRenderMesh = 0;

	// OpenCL variables

	cl::Program m_Program;
	cl::KernelFunctor<RR_KERNEL_ARG, int> *m_kernelEstimateMotion;
	cl::KernelFunctor<RR_KERNEL_ARG, float> *m_kernelShiftVectors;
	cl::KernelFunctor<RR_KERNEL_ARG, float> *m_kernelRenderFrame;
	cl::KernelFunctor<RR_KERNEL_ARG, float> *m_kernelRenderFrameBothDirs;
	cl::KernelFunctor<RR_KERNEL_ARG> *m_kernelNormalizeMVecs;

	float m_ElapsedTime = 0.0f;
	float m_PrevElapsedTime = 0.0f;
	float m_simulationTime = 0.0f;

	std::array<bool, 255> m_KeyboardMask;
	std::string m_MovieClipPattern;
	bool m_InspectCloth = false;
	bool m_MarchMode = true;
	int m_FastMode = 0;
	int m_MVecIters = 1;
	std::array<int, 9> m_FastModes = {1, 2, 3, 6, 8, 12, 24, 24, 24};
	int m_FrameStart = 0;
	int m_MarchIdx = 0;
	int m_MarchOffset = 0;
	bool m_UseDiamond = false;
	int m_DisplayDebugMode = 1;
	std::unordered_map<int, CGLTexture *> m_FrameTextures;
	std::mutex m_FrameTextures_Mutex;

	// mouse
	int m_Buttons = 0;
	int m_PrevX = 0;
	int m_PrevY = 0;

	//for camera handling
	float m_RotateX = -90.0f;
	float m_RotateY = 0.0f;
	float m_TranslateX = 0.0f;
	float m_TranslateY = 0.516f;
	float m_TranslateZ = -2.18f;

	cl::NDRange m_LocalWorkSize;
};
