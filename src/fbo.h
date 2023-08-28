#pragma once

#include "opengl.h"

class FBO {
public:
  ~FBO();
  void init(int width, int height);
  void clear();
  void bind();
  void unbind();
  void savePNG(const std::string &name, int i);
  void checkFBOAttachment();
  int width() const { return _width; }
  int height() const { return _height; }
  inline GLuint id() { return _fboId; }
  inline GLuint textureId(int i) {
    assert(i < 2);
    return _buffers[i];
  }

private:
  GLuint _fboId;
  GLuint _buffers[3];
  int _width, _height;
};
