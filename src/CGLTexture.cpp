#include "CGLTexture.h"

#include <stdio.h>

#include <png++/png.hpp>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

CGLTexture::CGLTexture(bool _rectangular) {
  ID = 0;
  if (_rectangular)
    target = GL_TEXTURE_RECTANGLE_ARB;
  else
    target = GL_TEXTURE_2D;
}

CGLTexture::~CGLTexture() { deleteTexture(); }

bool CGLTexture::createTexture() {
  deleteTexture();

  glGenTextures(1, &ID);

  ID++;

  glBindTexture(target, ID - 1);
  glTexParameterf(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(target, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR);  // GL_LINEAR_MIPMAP_LINEAR
  glTexParameterf(target, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR);  // GL_LINEAR_MIPMAP_LINEAR

  return true;
}

void CGLTexture::deleteTexture() {
  if (ID) {
    ID--;
    glDeleteTextures(1, &ID);
    ID = 0;
  }
}

void CGLTexture::bind() {
  // printf("BINDING %d\n", ID - 1);
  glBindTexture(target, ID - 1);
}

#define TGA_RGB 2   // RGB file
#define TGA_A 3     // ALPHA file
#define TGA_RLE 10  // run-length encoded

bool CGLTexture::loadPNG(const char *filename, bool updateOnly) {
  createTexture();
  png::image<png::rgb_pixel> image(filename);
  width = image.get_width();
  height = image.get_height();
  int channels = 4;
  int stride = channels * width;
  unsigned char *data = new unsigned char[stride * height];
  for (png::uint_32 y = 0; y < height; ++y) {
    for (png::uint_32 x = 0; x < width; ++x) {
      auto pix = image.get_pixel(x, y);
      int i = y * width + x;
      data[i * channels + 0] = pix.red;
      data[i * channels + 1] = pix.green;
      data[i * channels + 2] = pix.blue;
      data[i * channels + 3] = 1;
    }
  }
  fprintf(stderr, "copied %dx%d pixels,bytes=%d\n", width, height,
          stride * height);

  glTexImage2D(target, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);

  /*int err = gluBuild2DMipmaps(target, 3, width, height, GL_RGB,
  GL_UNSIGNED_BYTE, data); if (err)
  {
          fprintf(stderr, "creating mipmap faild: %d\n", err);
          return false;
  }*/
  delete[] data;
  fprintf(stderr, "[texture]: loaded '%s'\n", filename);
  return true;
}
