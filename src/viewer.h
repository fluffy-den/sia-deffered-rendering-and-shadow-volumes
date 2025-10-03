#pragma once

#include "camera.h"
#include "fbo.h"
#include "mesh.h"
#include "opengl.h"
#include "shader.h"

#include <iostream>

class Viewer {
public:
  //! Constructor
  Viewer();
  virtual ~Viewer();

  // gl stuff
  void init(int w, int h);
  void updateScene();
  void reshape(int w, int h);
  void updateGUI();

  // events
  void mousePressed(GLFWwindow *window, int button, int action);
  void mouseMoved(int x, int y);
  void mouseScroll(double x, double y);
  void keyPressed(int key, int action, int mods);
  void charPressed(int key);

protected:
  void loadProgram();
  void drawForward();
  void drawDeferred();
  void drawLights();

private:
  int _winWidth, _winHeight;

  Camera _cam;
  Shader _blinnPrg, _simplePrg, _gbufferPrg, _deferredPrg;
  FBO _deferredFbo;
  Mesh *_deferredQuad;

  // some geometry to render
  std::vector<Mesh *> _shapes;
  std::vector<float> _specularCoef;

  // geometrical representation of a pointlight
  std::vector<Mesh *> _pointLights;
  std::vector<Eigen::Vector3f> _lightColors;
  float _lightAngle = 0.f, _lastTime;
  bool _animate{true};

  enum ShadingMode { FORWARD, DEFERRED } _shadingMode{FORWARD};

  // mouse parameters
  Eigen::Vector2f _lastMousePos;
  int _button = -1;
};
