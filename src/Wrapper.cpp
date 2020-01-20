#include "Wrapper.h"
#include "MotionInterpolationTask.h"

#include <iostream>
#include <string>

#include "GLCommon.h"

#include "CLUtil.h"
#include <CL/cl_gl.h>
#include "util.h"

#ifdef __linux__
#include <GL/glx.h>
#endif

using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Wrapper

Wrapper *Wrapper::s_pSingletonInstance = NULL;

Wrapper *Wrapper::GetSingleton()
{
	if (!s_pSingletonInstance)
		s_pSingletonInstance = new Wrapper();

	return s_pSingletonInstance;
}

Wrapper::Wrapper()
{
}

Wrapper::~Wrapper()
{
	if (m_pCurrentTask)
	{
		delete m_pCurrentTask;
		m_pCurrentTask = nullptr;
	}
}

bool Wrapper::EnterMainLoop(int argc, char **argv)
{
	if (argc < 6)
	{
		printf("usage:   ./MotionInterpolation framepattern startframe blocksize use-diamond-pattern mveciters\n");
		printf("example: ./MotionInterpolation \"Assets/ball-crop/ball%%04d.png\" 174 16 0 1\n");
		return false;
	}
	util::set_exe_path(argv[0]);
	m_pCurrentTask = new MotionInterpolationTask(MotionInterpolationArgs{
		.clip = argv[1],
		.framestart = std::stoi(argv[2]),
		.localSize = std::stoi(argv[3]),
		.useDiamond = bool(std::stoi(argv[4])),
		.mvecIters = std::stoi(argv[5])});
	// create CL context with GL context sharing
	if (InitGL(argc, argv) && InitCLContext())
	{
		if (m_pCurrentTask)
		{
			if (!m_pCurrentTask->InitResources(m_CLDevice, m_CLContext))
			{
				return false;
			}
		}

		// the main event loop...
		while (!glfwWindowShouldClose(m_Window))
		{
			OnIdle();
			Render();

			glfwPollEvents();
		}
		printf("releasing resources\n");

		// currently broken (causes hang)
		//if (m_pCurrentTask)
		//	m_pCurrentTask->ReleaseResources();
	}
	else
	{
		cerr << "Failed to create GL and CL context, terminating..." << endl;
	}

	CleanupGL();

	return true;
}

bool Wrapper::DoCompute()
{
	if (m_pCurrentTask)
		m_pCurrentTask->ComputeGPU(m_CLContext, *m_CLCommandQueue);

	return true;
}

#define PRINT_INFO(title, buffer, bufferSize, maxBufferSize, expr) \
	{                                                              \
		expr;                                                      \
		buffer[bufferSize] = '\0';                                 \
		std::cout << title << ": " << buffer << std::endl;         \
	}

bool Wrapper::InitCLContext()
{
	// 1. get all platform IDs

	std::vector<cl_platform_id> platformIds;
	const cl_uint c_MaxPlatforms = 16;
	platformIds.resize(c_MaxPlatforms);

	cl_uint countPlatforms;
	V_RETURN_FALSE_CL(clGetPlatformIDs(c_MaxPlatforms, &platformIds[0], &countPlatforms), "Failed to get CL platform ID");
	platformIds.resize(countPlatforms);

	// 2. find all available GPU devices
	std::vector<cl_device_id> deviceIds;
	const int maxDevices = 16;
	deviceIds.resize(maxDevices);
	int countAllDevices = 0;

	cl_device_type deviceType = CL_DEVICE_TYPE_GPU;

	for (size_t i = 0; i < platformIds.size(); i++)
	{
		// Getting the available devices.
		cl_uint countDevices;
		clGetDeviceIDs(platformIds[i], deviceType, 1, &deviceIds[countAllDevices], &countDevices);
		countAllDevices += countDevices;
	}
	deviceIds.resize(countAllDevices);

	if (countAllDevices == 0)
	{
		std::cout << "No device of the selected type with OpenCL support was found.";
		return false;
	}
	// Choosing the first available device.
	m_CLDevice = deviceIds[0];
	clGetDeviceInfo(m_CLDevice(), CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &m_CLPlatform, NULL);

	// Printing platform and device data.
	const int maxBufferSize = 1024;
	char buffer[maxBufferSize];
	size_t bufferSize;
	std::cout << "OpenCL platform:" << std::endl
			  << std::endl;
	PRINT_INFO("Name", buffer, bufferSize, maxBufferSize, clGetPlatformInfo(m_CLPlatform, CL_PLATFORM_NAME, maxBufferSize, (void *)buffer, &bufferSize));
	PRINT_INFO("Vendor", buffer, bufferSize, maxBufferSize, clGetPlatformInfo(m_CLPlatform, CL_PLATFORM_VENDOR, maxBufferSize, (void *)buffer, &bufferSize));
	PRINT_INFO("Version", buffer, bufferSize, maxBufferSize, clGetPlatformInfo(m_CLPlatform, CL_PLATFORM_VERSION, maxBufferSize, (void *)buffer, &bufferSize));
	PRINT_INFO("Profile", buffer, bufferSize, maxBufferSize, clGetPlatformInfo(m_CLPlatform, CL_PLATFORM_PROFILE, maxBufferSize, (void *)buffer, &bufferSize));
	std::cout << std::endl
			  << "Device:" << std::endl
			  << std::endl;
	PRINT_INFO("Name", buffer, bufferSize, maxBufferSize, clGetDeviceInfo(m_CLDevice(), CL_DEVICE_NAME, maxBufferSize, (void *)buffer, &bufferSize));
	PRINT_INFO("Vendor", buffer, bufferSize, maxBufferSize, clGetDeviceInfo(m_CLDevice(), CL_DEVICE_VENDOR, maxBufferSize, (void *)buffer, &bufferSize));
	PRINT_INFO("Driver version", buffer, bufferSize, maxBufferSize, clGetDeviceInfo(m_CLDevice(), CL_DRIVER_VERSION, maxBufferSize, (void *)buffer, &bufferSize));
	cl_ulong localMemorySize;
	clGetDeviceInfo(m_CLDevice(), CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &localMemorySize, &bufferSize);
	std::cout << "Local memory size: " << localMemorySize << " Byte" << std::endl;
	std::cout << std::endl
			  << "******************************" << std::endl
			  << std::endl;

	cl_int clError;

// Define OS-specific context properties and create the OpenCL context
#if defined(__APPLE__)
	CGLContextObj kCGLContext = CGLGetCurrentContext();
	CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
	cl_context_properties props[] =
		{
			CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup,
			0};
#else
#ifndef _WIN32
	cl_context_properties props[] =
		{
			CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
			CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
			CL_CONTEXT_PLATFORM, (cl_context_properties)m_CLPlatform,
			0};
#else // Win32
	cl_context_properties props[] =
		{
			CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
			CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
			CL_CONTEXT_PLATFORM, (cl_context_properties)m_CLPlatform,
			0};
#endif
#endif

	m_CLContext = cl::Context(m_CLDevice, props, NULL, NULL, &clError);

	V_RETURN_FALSE_CL(clError, "Failed to create OpenCL context.");

	// Finally, create a command queue. All the asynchronous commands to the device will be issued
	// from the CPU into this queue. This way the host program can continue the execution until some results
	// from that device are needed.

	m_CLCommandQueue = new cl::CommandQueue(m_CLContext, m_CLDevice, CL_QUEUE_PROFILING_ENABLE, &clError);
	V_RETURN_FALSE_CL(clError, "Failed to create the command queue in the context");

	return true;
}

bool Wrapper::InitGL(int, char **)
{
	if (!glfwInit())
		exit(EXIT_FAILURE);

	m_Window = glfwCreateWindow(m_WindowWidth, m_WindowHeight, "CL Visual Computing Demo", NULL, NULL);
	if (!m_Window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(m_Window);

	glfwSetKeyCallback(m_Window, OnKeyboardCallback);
	glfwSetMouseButtonCallback(m_Window, OnMouseCallback);
	glfwSetCursorPosCallback(m_Window, OnMouseMoveCallback);
	glfwSetWindowSizeCallback(m_Window, OnWindowResizedCallback);
	// glfwSwapInterval(0); // disable VSYNC

	// initialize necessary OpenGL extensions
	glewInit();
	if (!glewIsSupported("GL_VERSION_2_0 GL_ARB_pixel_buffer_object"))
	{
		cout << "Missing OpenGL extension: GL_VERSION_2_0 GL_ARB_pixel_buffer_object" << endl;
		return false;
	}

	// default initialization
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// viewport
	glViewport(0, 0, m_WindowWidth, m_WindowHeight);

	// projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, (GLfloat)m_WindowWidth / (GLfloat)m_WindowHeight, 0.1f, 10.0f);

	// set view matrix
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -2.0f);

	glEnable(GL_TEXTURE_2D);

	cout << "OpenGL context initialized." << endl;

	return true;
}

void Wrapper::CleanupGL()
{
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

void Wrapper::Render()
{
	if (m_pCurrentTask)
		m_pCurrentTask->Render();

	glfwSwapBuffers(m_Window);
}

void Wrapper::OnIdle()
{
	if (m_PrevTime.time_since_epoch() == 0ns)
	{
		m_PrevTime = std::chrono::steady_clock::now();
	}
	else
	{
		auto finish = std::chrono::steady_clock::now();
		std::chrono::duration<float> fs = finish - m_PrevTime;
		double elapsedTime = fs.count();

		// threshold elapsed time
		// (a too large value might blow up the physics simulation.
		//  this happens for example when dragging the window,
		//  when our application stops receiving messages for a while.
		if (elapsedTime > 0.05f)
			elapsedTime = 0.05f;

		m_PrevTime = finish;

		if (m_pCurrentTask)
			m_pCurrentTask->OnIdle(elapsedTime);

		DoCompute();
	}
}

void Wrapper::OnWindowResizedCallback(GLFWwindow *pWindow, int Width, int Height)
{
	GetSingleton()->OnWindowResized(pWindow, Width, Height);
}

void Wrapper::OnWindowResized(GLFWwindow *, int Width, int Height)
{
	if (m_pCurrentTask)
		m_pCurrentTask->OnWindowResized(Width, Height);
}

void Wrapper::OnKeyboardCallback(GLFWwindow *pWindow, int Key, int ScanCode, int Action, int Mods)
{
	GetSingleton()->OnKeyboard(pWindow, Key, ScanCode, Action, Mods);
}

void Wrapper::OnKeyboard(GLFWwindow *, int Key, int, int Action, int)
{
	if (Key == GLFW_KEY_ESCAPE && Action == GLFW_PRESS)
		glfwSetWindowShouldClose(m_Window, GL_TRUE);

	if (m_pCurrentTask)
		m_pCurrentTask->OnKeyboard(Key, Action);
}

void Wrapper::OnMouseCallback(GLFWwindow *pWindow, int Button, int State, int Mods)
{
	GetSingleton()->OnMouse(pWindow, Button, State, Mods);
}

void Wrapper::OnMouse(GLFWwindow *, int Button, int State, int)
{
	if (m_pCurrentTask)
		m_pCurrentTask->OnMouse(Button, State);
}

void Wrapper::OnMouseMoveCallback(GLFWwindow *pWindow, double X, double Y)
{
	GetSingleton()->OnMouseMove(pWindow, X, Y);
}

void Wrapper::OnMouseMove(GLFWwindow *, double X, double Y)
{
	if (m_pCurrentTask)
		m_pCurrentTask->OnMouseMove((int)X, (int)Y);
}

///////////////////////////////////////////////////////////////////////////////
