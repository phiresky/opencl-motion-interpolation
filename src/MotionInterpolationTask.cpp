#include "CLUtil.h"

#ifdef min  // these macros are defined under windows, but collide with our math
            // utility
#undef min
#endif
#ifdef max
#undef max
#endif

#include <fstream>
#include <future>
#include <png++/error.hpp>
#include <sstream>
#include <string>

#include "CL/cl_gl.h"
#include "HLSLEx.h"
#include "MotionInterpolationTask.h"
#include "util.h"

using namespace std;

string applypattern(string &pattern, int i) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), pattern.c_str(), i);
  string buffasstr = buffer;
  return buffasstr;
}

MotionInterpolationTask::MotionInterpolationTask(MotionInterpolationArgs args)
    : m_MovieClipPattern(args.clip),
      m_FrameStart(args.framestart),
      m_LocalWorkSize(cl::NDRange(args.localSize, args.localSize)),
      m_UseDiamond(args.useDiamond),
      m_MVecIters(args.mvecIters) {
  for (int i = 0; i < 255; i++) m_KeyboardMask[i] = false;
}

MotionInterpolationTask::~MotionInterpolationTask() { ReleaseResources(); }

void MotionInterpolationTask::SetMVMatrix() {
  // set the modelview matrix
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(m_TranslateX, m_TranslateY, m_TranslateZ);
  glRotatef(m_RotateY, 0.0, 1.0, 0.0);
  glRotatef(m_RotateX, 1.0, 0.0, 0.0);
}

bool MotionInterpolationTask::InitResources(cl::Device Device,
                                            cl::Context Context) {
  string empty = applypattern(m_MovieClipPattern, m_FrameStart);
  string paths[] = {empty, applypattern(m_MovieClipPattern, m_FrameStart + 1)};
  int i = 0;
  for (auto &img : m_images) {
    img.texture = new CGLTexture();
    img.frameIdx = i > 1 ? 0 : m_FrameStart + i;
    if (!img.texture->loadPNG((i > 1 ? empty : paths[i]).c_str())) {
      cout << "Failed to load png" << endl;
      return false;
    }
    // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_ImgResX = img.texture->getWidth();
    m_ImgResY = img.texture->getHeight();

    // create cloth model
    img.clothModel = CTriMesh::CreatePlane(2, 2, (float)m_ImgResX / m_ImgResY);
    if (!img.clothModel) {
      cout << "Failed to create cloth." << endl;
      return false;
    }
    if (!img.clothModel->CreateGLResources()) {
      cout << "Failed to create cloth OpenGL resources" << endl;
      return false;
    }

    i++;
  }

  // PreloadImages(10);

  ///////////////////////////////////////////////////////////
  // shader programs

  m_VSCloth = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
  m_PSCloth = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

  if (!CreateShaderFromFile("meshtextured.vert", m_VSCloth)) return false;

  if (!CreateShaderFromFile("meshtextured.frag", m_PSCloth)) return false;

  m_ProgRenderCloth = glCreateProgramObjectARB();
  glAttachObjectARB(m_ProgRenderCloth, m_VSCloth);
  glAttachObjectARB(m_ProgRenderCloth, m_PSCloth);
  if (!LinkGLSLProgram(m_ProgRenderCloth)) return false;
  CHECK_FOR_OGL_ERROR();

  m_VSMesh = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
  m_PSMesh = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

  if (!CreateShaderFromFile("mesh.vert", m_VSMesh)) return false;

  if (!CreateShaderFromFile("mesh.frag", m_PSMesh)) return false;

  m_ProgRenderMesh = glCreateProgramObjectARB();
  glAttachObjectARB(m_ProgRenderMesh, m_VSMesh);
  glAttachObjectARB(m_ProgRenderMesh, m_PSMesh);
  if (!LinkGLSLProgram(m_ProgRenderMesh)) return false;
  CHECK_FOR_OGL_ERROR();

  GLuint diffuseSampler =
      glGetUniformLocationARB(m_ProgRenderCloth, "texDiffuse");
  glUseProgramObjectARB(m_ProgRenderCloth);
  glUniform1i(diffuseSampler, 0);
  CHECK_FOR_OGL_ERROR();

  SetMVMatrix();

  /////////////////////////////////////////////////////////////
  // OpenCL resources

  cl_int clError = 0;
  for (int i = 0; i < m_images.size(); i++) {
    auto &image = m_images[i];
    // img = clCreateBuffer(Context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, nullptr,
    // &clError);
    int flags =
        CL_MEM_READ_WRITE;  // i == 2 ? CL_MEM_WRITE_ONLY : CL_MEM_READ_ONLY;
    image.clImage = cl::ImageGL(Context, flags, GL_TEXTURE_2D, 0,
                                image.texture->getID(), &clError);
    image.clBuffer = cl::Buffer(Context, CL_MEM_READ_WRITE,
                                sizeof(cl_float) * 4 * m_ImgResX * m_ImgResY);
    V_RETURN_FALSE_CL(clError, "createtexteure");
  }

  string programCode = util::loadFileToString("motion.cl");
  stringstream compileOptions;
  compileOptions << " -D TILE_X=" << m_LocalWorkSize[0]
                 << " -D TILE_Y=" << m_LocalWorkSize[1]
                 << " -D USE_DIAMOND=" << m_UseDiamond;
  m_Program = cl::Program(Context, programCode);
  try {
    m_Program.build(compileOptions.str().c_str());
  } catch (...) {
    cl_int buildErr = CL_SUCCESS;
    auto buildInfo = m_Program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(&buildErr);
    for (auto &pair : buildInfo) {
      std::cerr << pair.second << std::endl << std::endl;
    }
    throw;
  }
  m_kernelEstimateMotion =
      new cl::KernelFunctor<RR_KERNEL_ARG, int>(m_Program, "EstimateMotion");
  m_kernelShiftVectors =
      new cl::KernelFunctor<RR_KERNEL_ARG, float>(m_Program, "ShiftVectors");
  m_kernelRenderFrame =
      new cl::KernelFunctor<RR_KERNEL_ARG, float>(m_Program, "RenderFrame");
  m_kernelRenderFrameBothDirs = new cl::KernelFunctor<RR_KERNEL_ARG, float>(
      m_Program, "RenderFrameBothDirs");
  m_kernelNormalizeMVecs =
      new cl::KernelFunctor<RR_KERNEL_ARG>(m_Program, "NormalizeMVecs");
  return true;
}

CGLTexture *loadPngNow(std::string m_MovieClipPattern, int frameIdx) {
  try {
    string buffer = applypattern(m_MovieClipPattern, frameIdx);
    std::ifstream _infile(buffer);
    if (!_infile.good()) return nullptr;
    auto texture = new CGLTexture();
    texture->loadPNG(buffer.c_str(), true);
    return texture;
  } catch (png::std_error e) {
    return nullptr;
  }
}
void MotionInterpolationTask::PreloadImages(int count) {
  printf("loading %d future images\n", count);
  /*auto pattern = m_MovieClipPattern;
  auto functor = [pattern, this](int idx) -> CGLTexture * {
          auto png = loadPngNow(pattern, idx);
          std::lock_guard<std::mutex> guard(this->m_FrameTextures_Mutex);
          m_FrameTextures[idx] = png;
  };*/
  for (int i = 1; i < count; i++) {
    auto fix = m_FrameStart + i;
    if (!m_FrameTextures.count(fix)) {
      auto png = loadPngNow(m_MovieClipPattern, fix);
      m_FrameTextures[fix] = png;
    }
  }
  printf("load done\n", count);
  /*
                  for (int i = 2; i < 100; i++)
                          std::async(std::launch::async, functor, 1);*/
}
void MotionInterpolationTask::LoadMarchImage(cl::Context Context) {
  float simulSecs;
  float simulMod = modf(m_simulationTime, &simulSecs);
  if ((int)simulSecs == m_MarchOffset + m_MarchIdx) return;
  m_MarchIdx = simulSecs - m_MarchOffset;
  int start_img = m_FrameStart;
  int leftFrameIdx = m_MarchIdx + start_img;
  cl_int clError = 0;
  int i = 0;
  if (m_images[1].frameIdx == m_MarchIdx) {
    // reuse image
    std::swap(m_images[0], m_images[1]);
    i = 1;
  }
  for (; i < 2; i++) {
    auto &image = m_images[i];
    image.frameIdx = leftFrameIdx + i;
    if (m_FrameTextures.count(image.frameIdx))
      image.texture = m_FrameTextures[image.frameIdx];
    else {
      auto txt = loadPngNow(m_MovieClipPattern, image.frameIdx);
      if (txt != nullptr) {
        image.texture = txt;
        m_FrameTextures[image.frameIdx] = image.texture;
      } else {
        int bef = m_MarchOffset;
        m_MarchOffset = (int)m_simulationTime;
        printf("error loading png, restarting from start\n");
        if (bef != m_MarchOffset) {
          LoadMarchImage(Context);
        }
        return;
      }
    }

    // img = clCreateBuffer(Context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, nullptr,
    // &clError);
    int flags =
        CL_MEM_READ_WRITE;  // i == 2 ? CL_MEM_WRITE_ONLY : CL_MEM_READ_ONLY;
    image.clImage = cl::ImageGL(Context, flags, GL_TEXTURE_2D, 0,
                                image.texture->getID(), &clError);
    image.clBuffer = cl::Buffer(Context, CL_MEM_READ_WRITE,
                                sizeof(cl_float) * 4 * m_ImgResX * m_ImgResY);
    V_RETURN_CL(clError, "createtexteure");
  }
}

void MotionInterpolationTask::ReleaseResources() {
  for (auto &x : m_images) {
    if (x.clothModel) {
      delete x.clothModel;
      x.clothModel = NULL;
    }
  }

  delete m_kernelEstimateMotion;
  delete m_kernelShiftVectors;
  delete m_kernelRenderFrame;
  delete m_kernelRenderFrameBothDirs;
  delete m_kernelNormalizeMVecs;
  // delete m_Program;

  SAFE_RELEASE_GL_SHADER(m_PSCloth);
  SAFE_RELEASE_GL_SHADER(m_VSCloth);
  SAFE_RELEASE_GL_SHADER(m_VSMesh);
  SAFE_RELEASE_GL_SHADER(m_PSMesh);

  SAFE_RELEASE_GL_PROGRAM(m_ProgRenderCloth);
  SAFE_RELEASE_GL_PROGRAM(m_ProgRenderMesh);
}

double msTaken = 0;
int framesRendered = 0;

void MotionInterpolationTask::ComputeGPU(cl::Context &Context,
                                         cl::CommandQueue &CommandQueue) {
  // get global work size
  cl::NDRange global =
      cl::NDRange(CLUtil::GetGlobalWorkSize(m_ImgResX, m_LocalWorkSize[0]),
                  CLUtil::GetGlobalWorkSize(m_ImgResY, m_LocalWorkSize[1]));

  glFinish();

  auto start = std::chrono::steady_clock::now();

  float simulSecs;
  float simulMod = modf(m_simulationTime, &simulSecs);
  if (m_MarchMode) {
    LoadMarchImage(Context);
  }

  bool once = false;
  if (!once || m_FrameCounter == 0) {
    std::vector<cl::Memory> cl_imgs;
    for (auto &img : m_images) cl_imgs.push_back(img.clImage);

    V_RETURN_CL(CommandQueue.enqueueAcquireGLObjects(&cl_imgs), "acq");
    for (auto &img : m_images) {
      V_RETURN_CL(CommandQueue.enqueueCopyImageToBuffer(
                      img.clImage, img.clBuffer, {0, 0, 0},
                      {m_ImgResX, m_ImgResY, 1}, 0),
                  "img->buf");
    }
    auto enqueue = cl::EnqueueArgs(CommandQueue, global, m_LocalWorkSize);
    for (bool right = false; true; right = true) {
      int ofs = right ? 4 : 0;
      float theta = right ? 1.0f - simulMod : simulMod;
      cl::Buffer img_1 = m_images[right ? 1 : 0].clBuffer;
      cl::Buffer img_2 = m_images[right ? 0 : 1].clBuffer;
      cl::Buffer diffImg = m_images[ofs + 2].clBuffer;
      cl::Buffer mVecs = m_images[ofs + 3].clBuffer;
      cl::Buffer mVecsShifted = m_images[ofs + 4].clBuffer;
      cl::Buffer rendered = m_images[ofs + 5].clBuffer;
#define CARGS                                                                \
  enqueue, m_ImgResX, m_ImgResY, img_1, img_2, diffImg, mVecs, mVecsShifted, \
      rendered
      for (int i = 0; i < m_MVecIters; i++) {
        // estimate motion
        // printf("EstimateMotion %d\n", i);

        cl_int error;
        auto &fn = *m_kernelEstimateMotion;
        auto event = fn(CARGS, i, error);
        // cl_int ee = event.getProfilingInfo();
        // printf("ee %d", ee);
        V_RETURN_CL(error, "EstimateMotion");
        // CommandQueue.finish();
        // printf("EstimateMotion done\n");
      };
      {
        // printf("ShiftVectors\n");
        // shift motion vectors
        cl_int error;
        auto &fn = *m_kernelShiftVectors;
        fn(CARGS, theta, error);
        V_RETURN_CL(error, "ShiftVectors");
        // printf("ShiftVectors done\n");
      };
      {
        // CommandQueue.finish();
        // normalize motion vector image
        cl_int error;
        auto &fn = *m_kernelNormalizeMVecs;
        fn(CARGS, error);
        V_RETURN_CL(error, "NormalizeMVecs");
      };
      {
        // render frame
        cl_int error;
        auto &fn = *m_kernelRenderFrame;
        fn(CARGS, theta, error);
        V_RETURN_CL(error, "RenderFrame");
      };
      if (right) break;
    }
    {
      // render FBFBW frame
      cl_int error;
      int l = 0;
      int r = 4;
      float theta = simulMod;
      cl::Buffer img_1 = m_images[0].clBuffer;
      cl::Buffer img_2 = m_images[1].clBuffer;
      cl::Buffer unused = m_images[2].clBuffer;
      cl::Buffer mVecsShifted = m_images[l + 4].clBuffer;
      cl::Buffer mVecsRevShifted = m_images[r + 4].clBuffer;
      cl::Buffer rendered = m_images[10].clBuffer;
      auto &fn = *m_kernelRenderFrameBothDirs;
      fn(enqueue, m_ImgResX, m_ImgResY, img_1, img_2, unused, mVecsShifted,
         mVecsRevShifted, rendered, theta, error);
      V_RETURN_CL(error, "RenderFrameBothDirs");
    }
    for (auto &img : m_images) {
      // printf("copying\n");
      V_RETURN_CL(CommandQueue.enqueueCopyBufferToImage(
                      img.clBuffer, img.clImage, 0, {0, 0, 0},
                      {m_ImgResX, m_ImgResY, 1}),
                  "buf->img");
    }
    V_RETURN_CL(CommandQueue.enqueueReleaseGLObjects(&cl_imgs), "release");
    // printf("COMPGPU done\n");
  }
  CommandQueue.finish();
  {
    auto finish = std::chrono::steady_clock::now();
    std::chrono::duration<float> fs = finish - start;
    msTaken += fs.count();
    framesRendered += 1;
    if (framesRendered % 20 == 0) {
      printf("Average Compute FPS: %.1f\n", framesRendered / msTaken);
    }
  }

  m_FrameCounter++;
  m_PrevElapsedTime = m_ElapsedTime;
  m_ElapsedTime = 0;
}

void MotionInterpolationTask::RenderImg(Triforce &img, float x, float y) {
  glPushMatrix();
  glTranslatef(-(float)m_ImgResX / m_ImgResY / 2, 0.5f, 0.5f);

  float gap = 0.01;
  float xofs = (float)m_ImgResX / m_ImgResY + gap;
  float yofs = 0.5 + gap;
  glTranslatef(x * xofs - xofs / 2, 0, -yofs * (2 * y - 1));
  img.texture->bind();

  // render wireframe cloth
  if (m_InspectCloth) {
    glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_BACK, GL_LINE);
    glUseProgramObjectARB(m_ProgRenderMesh);
    glColor3f(1.0f, 1.0f, 1.0f);
    if (img.clothModel) img.clothModel->DrawGL(GL_TRIANGLES);

    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
  }

  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT, GL_FILL);
  glPolygonMode(GL_BACK, GL_FILL);
  glUseProgramObjectARB(m_ProgRenderCloth);
  if (img.clothModel) img.clothModel->DrawGL(GL_TRIANGLES);

  if (m_InspectCloth) {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0, 0);
  }
  /*if (i == 2)
          {
                  // draw arrows
                  glBindTexture(GL_TEXTURE_2D, 0);
                  glUseProgramObjectARB(0);
                  glLineWidth(2);
                  glColor3f(1, 1, 1);

                  for (int x = 8; x < m_ImgResX; x += 16)
                  {
                          for (int y = 8; y < m_ImgResY; y += 16)
                          {
                                  glBegin(GL_LINES);
                                  float xf = (float)x / m_ImgResY;
                                  float yf = -(float)y / m_ImgResY;
                                  glVertex3f(xf, -0.01, yf);
                                  glVertex3f(xf, -0.01, yf);
                                  glEnd();
                          }
                  }
          }*/
  glPopMatrix();
}

void MotionInterpolationTask::Render() {
  glCullFace(GL_BACK);

  // enable depth test & depth write
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE0);
  if (m_DisplayDebugMode == 1) {
    int i = 0;
    for (auto &img : m_images) {
      int x = i % 2;
      int y = i / 2;
      RenderImg(img, x, y);
      i++;
    }
  } else if (m_DisplayDebugMode == 2) {
    RenderImg(m_images[0], 0, 0);
    RenderImg(m_images[1], 1, 0);
    RenderImg(m_images[2], 0, 1);
    RenderImg(m_images[6], 1, 1);
    RenderImg(m_images[10], 0.5, 2);
  } else if (m_DisplayDebugMode == 3) {
    RenderImg(m_images[0], 0, 0);
    RenderImg(m_images[1], 1, 0);
    RenderImg(m_images[4], 0, 1);
    RenderImg(m_images[8], 1, 1);
    RenderImg(m_images[10], 0.5, 2);
  } else if (m_DisplayDebugMode == 4) {
    RenderImg(m_images[0], 0.5, 0);
    RenderImg(m_images[10], 0.5, 1);
  }
}

void MotionInterpolationTask::OnKeyboard(int Key, int KeyAction) {
  if (Key >= 255) return;
  m_KeyboardMask[Key] = true;

  if (KeyAction == GLFW_PRESS) {
    if (Key == GLFW_KEY_I) {
      m_InspectCloth = !m_InspectCloth;
    }
    if (Key == GLFW_KEY_M) {
      m_MarchMode = !m_MarchMode;
      m_MarchOffset = (int)m_simulationTime;
    }
    if (Key == GLFW_KEY_F) {
      m_FastMode = (m_FastMode + 1) % m_FastModes.size();
    }
    if (Key == GLFW_KEY_P) {
      fprintf(stderr, "rot=%f,%f,%f, trans=%f,%f,%f\n", m_RotateX, m_RotateY,
              0.f, m_TranslateX, m_TranslateY, m_TranslateZ);
    }
    if (Key >= GLFW_KEY_0 && Key <= GLFW_KEY_9) {
      m_DisplayDebugMode = Key - GLFW_KEY_0;
    }
    if (Key == GLFW_KEY_L) {
      PreloadImages(100);
    }
  } else if (KeyAction == GLFW_RELEASE) {
    m_KeyboardMask[Key] = false;
  }
}

void MotionInterpolationTask::OnMouse(int Button, int State) {
  if (State == GLFW_PRESS) {
    m_Buttons |= 1 << Button;
  } else if (State == GLFW_RELEASE) {
    m_Buttons = 0;
  }
}

void MotionInterpolationTask::OnMouseMove(int X, int Y) {
  int dx, dy;
  dx = X - m_PrevX;
  dy = Y - m_PrevY;

  if (m_Buttons & 1) {
    m_RotateX += dy * 0.2f;
    m_RotateY += dx * 0.2f;
  }
  /*if (m_Buttons & 4)
  {
          m_SpherePos.z += dy * 0.002f;
  }*/
  m_PrevX = X;
  m_PrevY = Y;

  SetMVMatrix();
}

void MotionInterpolationTask::OnIdle(float ElapsedTime) {
  // move camera
  if (m_KeyboardMask[GLFW_KEY_E]) m_TranslateZ += 0.5f * ElapsedTime;
  if (m_KeyboardMask[GLFW_KEY_Q]) m_TranslateZ -= 0.5f * ElapsedTime;
  if (m_KeyboardMask[GLFW_KEY_D]) m_TranslateX -= 0.5f * ElapsedTime;
  if (m_KeyboardMask[GLFW_KEY_A]) m_TranslateX += 0.5f * ElapsedTime;
  if (m_KeyboardMask[GLFW_KEY_S]) m_TranslateY += 0.5f * ElapsedTime;
  if (m_KeyboardMask[GLFW_KEY_W]) m_TranslateY -= 0.5f * ElapsedTime;

  /*if (m_TranslateZ > 0)
          m_TranslateZ = 0;*/

  m_ElapsedTime += ElapsedTime;
  m_simulationTime += ElapsedTime * (m_FastModes[m_FastMode]);

  SetMVMatrix();
}

void MotionInterpolationTask::OnWindowResized(int Width, int Height) {
  // viewport
  glViewport(0, 0, Width, Height);

  // projection
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0f, (GLfloat)Width / (GLfloat)Height, 0.1f, 10.0f);
}

///////////////////////////////////////////////////////////////////////////////
