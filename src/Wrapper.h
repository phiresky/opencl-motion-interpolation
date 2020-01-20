#pragma once
#include "GLCommon.h"
#include "CLUtil.h"

#include "MotionInterpolationTask.h"

class Wrapper
{
public:
	virtual ~Wrapper();

	static Wrapper *GetSingleton();

	//! We overload this method to define OpenGL callbacks and to support CL-GL context sharing
	virtual bool EnterMainLoop(int argc, char **argv);

	virtual bool DoCompute();

	virtual bool InitGL(int argc, char **argv);

	virtual void CleanupGL();

	// GLFW callback functions
	// NOTE: these have to be static, as GLFW does not support ptrs to member functions
	static void OnKeyboardCallback(GLFWwindow *Window, int Key, int ScanCode, int Action, int Mods);

	static void OnMouseCallback(GLFWwindow *Window, int Button, int State, int Mods);
	static void OnMouseMoveCallback(GLFWwindow *window, double X, double Y);

	static void OnWindowResizedCallback(GLFWwindow *window, int Width, int Height);

protected:
	// ctor is protected to enforce singleton pattern
	Wrapper();

	virtual bool InitCLContext();

	virtual void Render();

	virtual void OnKeyboard(GLFWwindow *pWindow, int Key, int ScanCode, int Action, int Mods);

	virtual void OnMouse(GLFWwindow *pWindow, int Button, int State, int Mods);
	virtual void OnMouseMove(GLFWwindow *pWindow, double X, double Y);

	virtual void OnIdle();

	virtual void OnWindowResized(GLFWwindow *pWindow, int Width, int Height);

protected:
	GLFWwindow *m_Window = nullptr;
	cl_platform_id m_CLPlatform;
	cl::Device m_CLDevice;
	cl::Context m_CLContext;
	cl::CommandQueue *m_CLCommandQueue;
	int m_WindowWidth = 1024;
	int m_WindowHeight = 768;

	MotionInterpolationTask *m_pCurrentTask;

	static Wrapper *s_pSingletonInstance;

	// for the physical simulation we need to keep track of the time between frames rendered
	int m_FrameTimer;

	std::chrono::steady_clock::time_point m_PrevTime;
};
