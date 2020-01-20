#include "GLCommon.h"

#include <iostream>

void LoadProgram(const char *Path, char **pSource, size_t *SourceSize) {
  FILE *pFileStream = NULL;

// open the OpenCL source code file
#ifdef _WIN32  // Windows version
  if (fopen_s(&pFileStream, Path, "rb") != 0) {
    cout << "File not found: " << Path;
    return;
  }
#else  // Linux version
  pFileStream = fopen(Path, "rb");
  if (pFileStream == 0) {
    std::cout << "File not found: " << Path << std::endl;
    return;
  }
#endif

  // get the length of the source code
  fseek(pFileStream, 0, SEEK_END);
  *SourceSize = ftell(pFileStream);
  fseek(pFileStream, 0, SEEK_SET);

  *pSource = new char[*SourceSize + 1];
  fread(*pSource, *SourceSize, 1, pFileStream);
  fclose(pFileStream);
  (*pSource)[*SourceSize] = '\0';
}

bool CreateShaderFromFile(const char *Path, GLhandleARB shader) {
  char *sourceCode = NULL;

  size_t sourceLength;

  LoadProgram(Path, &sourceCode, &sourceLength);

  if (sourceCode == NULL) return false;

  glShaderSourceARB(shader, 1, (const char **)&sourceCode, NULL);

  if (!CompileGLSLShader(shader)) {
    delete[] sourceCode;
    std::cout << "Failed to compile shader: " << Path << std::endl;
    return false;
  }

  delete[] sourceCode;
  return true;
}

bool CompileGLSLShader(GLhandleARB obj) {
  GLint success;
  glCompileShaderARB(obj);
  glGetShaderiv(obj, GL_COMPILE_STATUS, &success);

  if (success == GL_FALSE) {
    int infoLogLength = 0;
    char infoLog[1024];
    std::cout << "There were compile errors:" << std::endl;
    glGetShaderInfoLog(obj, 1024, &infoLogLength, infoLog);
    if (infoLogLength > 0) std::cout << infoLog << std::endl;

    return false;
  }

  return true;
}

bool LinkGLSLProgram(GLhandleARB program) {
  GLint success;

  glLinkProgramARB(program);

  CHECK_FOR_OGL_ERROR();
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  CHECK_FOR_OGL_ERROR();

  if (success == GL_FALSE) {
    int infoLogLength = 0;
    char infoLog[1024];
    std::cout << "There were link errors:" << std::endl;
    glGetProgramInfoLog(program, 1024, &infoLogLength, infoLog);
    if (infoLogLength > 0) std::cout << infoLog << std::endl;

    return false;
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
