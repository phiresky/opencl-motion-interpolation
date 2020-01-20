#pragma once

#include "GLCommon.h"

class CGLTexture {
 private:
  void deleteTexture();
  bool createTexture();

  GLenum target;
  int width, height;
  GLuint ID;

 public:
  explicit CGLTexture(bool _rectangular = false);
  ~CGLTexture();

  GLuint getID() { return ID - 1; }
  int getWidth() { return width; }
  int getHeight() { return height; }

  bool loadPNG(const char *fileName, bool updateOnly = false);

  void bind();
};
