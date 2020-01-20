#pragma once

#include <CL/opencl.h>

#include <string>
#include <iostream>
#include <algorithm>
#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <CL/cl2.hpp>

//! Utility class for frequently-needed OpenCL tasks
// TO DO: replace this with a nicer OpenCL wrapper
class CLUtil
{
public:
    //! Determines the OpenCL global work size given the number of data elements and threads per workgroup
    static size_t GetGlobalWorkSize(size_t DataElemCount, size_t LocalWorkSize);

    //! Builds a CL program
    static cl_program BuildCLProgramFromMemory(cl_device_id Device, cl_context Context, const std::string &SourceCode, const std::string &CompileOptions = "");

    static void PrintBuildLog(cl_program Program, cl_device_id Device);

    static const char *GetCLErrorString(cl_int CLErrorCode);
};

// Some useful shortcuts for handling pointers and validating function calls
#define V_RETURN_FALSE_CL(expr, errmsg)                                                                  \
    do                                                                                                   \
    {                                                                                                    \
        cl_int e = (expr);                                                                               \
        if (CL_SUCCESS != e)                                                                             \
        {                                                                                                \
            std::cerr << "Error: " << errmsg << " [" << CLUtil::GetCLErrorString(e) << "]" << std::endl; \
            return false;                                                                                \
        }                                                                                                \
    } while (0)
#define V_RETURN_0_CL(expr, errmsg)                                                                      \
    do                                                                                                   \
    {                                                                                                    \
        cl_int e = (expr);                                                                               \
        if (CL_SUCCESS != e)                                                                             \
        {                                                                                                \
            std::cerr << "Error: " << errmsg << " [" << CLUtil::GetCLErrorString(e) << "]" << std::endl; \
            return 0;                                                                                    \
        }                                                                                                \
    } while (0)
#define V_RETURN_CL(expr, errmsg)                                                                        \
    do                                                                                                   \
    {                                                                                                    \
        cl_int e = (expr);                                                                               \
        if (CL_SUCCESS != e)                                                                             \
        {                                                                                                \
            std::cerr << "Error: " << errmsg << " [" << CLUtil::GetCLErrorString(e) << "]" << std::endl; \
            return;                                                                                      \
        }                                                                                                \
    } while (0)

#define SAFE_DELETE(ptr) \
    do                   \
    {                    \
        if (ptr)         \
        {                \
            delete ptr;  \
            ptr = NULL;  \
        }                \
    } while (0)
#define SAFE_DELETE_ARRAY(x) \
    do                       \
    {                        \
        if (x)               \
        {                    \
            delete[] x;      \
            x = NULL;        \
        }                    \
    } while (0)

#define SAFE_RELEASE_KERNEL(ptr)  \
    do                            \
    {                             \
        if (ptr)                  \
        {                         \
            clReleaseKernel(ptr); \
            ptr = NULL;           \
        }                         \
    } while (0)
#define SAFE_RELEASE_PROGRAM(ptr)  \
    do                             \
    {                              \
        if (ptr)                   \
        {                          \
            clReleaseProgram(ptr); \
            ptr = NULL;            \
        }                          \
    } while (0)
#define SAFE_RELEASE_MEMOBJECT(ptr)  \
    do                               \
    {                                \
        if (ptr)                     \
        {                            \
            clReleaseMemObject(ptr); \
            ptr = NULL;              \
        }                            \
    } while (0)
#define SAFE_RELEASE_SAMPLER(ptr)  \
    do                             \
    {                              \
        if (ptr)                   \
        {                          \
            clReleaseSampler(ptr); \
            ptr = NULL;            \
        }                          \
    } while (0)

#define ARRAYLEN(a) (sizeof(a) / sizeof(a[0]))
