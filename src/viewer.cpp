#include "viewer.h"
#include "camera.h"

#include <SOIL2.h>
#include <imgui.h>

using namespace Eigen;

Viewer::Viewer() {}

Viewer::~Viewer() {
  for (Mesh *s : _shapes)
    delete s;
  for (Mesh *s : _pointLights)
    delete s;
}

////////////////////////////////////////////////////////////////////////////////
// GL stuff

// initialize OpenGL context
void Viewer::init(int w, int h) {
  _winWidth = w;
  _winHeight = h;

  // set the background color, i.e., the color used
  // to fill the screen when calling glClear(GL_COLOR_BUFFER_BIT)
  glClearColor(0.f, 0.f, 0.f, 1.f);

  glEnable(GL_DEPTH_TEST);

  loadProgram();

  Mesh *quad = new Mesh();
  quad->createGrid(2, 2);
  quad->init();
  quad->transformationMatrix() = AngleAxisf(M_PI / 2.f, Vector3f(-1, 0, 0)) *
                                 Scaling(20.f, 20.f, 1.f) *
                                 Translation3f(-0.5, -0.5, -0.5);
  _shapes.push_back(quad);
  _specularCoef.push_back(0.f);

  Mesh *tw = new Mesh();
  tw->load(DATA_DIR "/models/tw.off");
  tw->init();
  _shapes.push_back(tw);
  _specularCoef.push_back(0.75);

  Mesh *sphere = new Mesh();
  sphere->load(DATA_DIR "/models/sphere.off");
  sphere->init();
  sphere->transformationMatrix() = Translation3f(0, 0, 2.f) * Scaling(0.5f);
  _shapes.push_back(sphere);
  _specularCoef.push_back(0.3f);

  _lightColors.push_back(Vector3f::Constant(0.8f));
  Mesh *light = new Mesh();
  light->createSphere(0.025f);
  light->init();
  light->transformationMatrix() =
      Translation3f(_cam.sceneCenter() +
                    _cam.sceneRadius() *
                        Vector3f(Eigen::internal::random<float>(),
                                  Eigen::internal::random<float>(0.1f, 0.5f),
                                  Eigen::internal::random<float>()));
  _pointLights.push_back(light);

  _lastTime = glfwGetTime();

  AlignedBox3f aabb;
  for (size_t i = 0; i < _shapes.size(); ++i)
    aabb.extend(_shapes[i]->boundingBox());

  _cam.setSceneCenter(aabb.center());
  _cam.setSceneRadius(aabb.sizes().maxCoeff());
  _cam.setSceneDistance(_cam.sceneRadius() * 3.f);
  _cam.setMinNear(0.1f);
  _cam.setNearFarOffsets(-_cam.sceneRadius() * 100.f,
                         _cam.sceneRadius() * 100.f);
  _cam.setScreenViewport(AlignedBox2f(Vector2f(0.0, 0.0), Vector2f(w, h)));
}

void Viewer::reshape(int w, int h) {
  _winWidth = w;
  _winHeight = h;
  _cam.setScreenViewport(AlignedBox2f(Vector2f(0.0, 0.0), Vector2f(w, h)));
  glViewport(0, 0, w, h);
}

void Viewer::drawForward() {
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  _blinnPrg.activate();
  glUniformMatrix4fv(_blinnPrg.getUniformLocation("projection_matrix"), 1,
                      GL_FALSE, _cam.computeProjectionMatrix().data());
  glUniformMatrix4fv(_blinnPrg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                      _cam.computeViewMatrix().data());
  Vector4f lightPos;
  lightPos << _pointLights[0]->transformationMatrix().translation(), 1.f;
  glUniform4fv(_blinnPrg.getUniformLocation("light_pos"), 1,
                (_cam.computeViewMatrix() * lightPos).eval().data());
  glUniform3fv(_blinnPrg.getUniformLocation("light_col"), 1,
                _lightColors[0].data());

  for (size_t i = 0; i < _shapes.size(); ++i) {
    glUniformMatrix4fv(_blinnPrg.getUniformLocation("model_matrix"), 1,
                        GL_FALSE, _shapes[i]->transformationMatrix().data());
    Matrix3f normal_matrix =
        (_cam.computeViewMatrix() * _shapes[i]->transformationMatrix())
            .linear()
            .inverse()
            .transpose();
    glUniformMatrix3fv(_blinnPrg.getUniformLocation("normal_matrix"), 1,
                        GL_FALSE, normal_matrix.data());
    glUniform1f(_blinnPrg.getUniformLocation("specular_coef"),
                _specularCoef[i]);

    _shapes[i]->draw(_blinnPrg);
  }

  _blinnPrg.deactivate();

  drawLights();
}

void Viewer::drawDeferred() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  // TODO

  drawLights();
}

void Viewer::drawLights() {
  _simplePrg.activate();
  glUniformMatrix4fv(_simplePrg.getUniformLocation("projection_matrix"), 1,
                     GL_FALSE, _cam.computeProjectionMatrix().data());
  glUniformMatrix4fv(_simplePrg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                     _cam.computeViewMatrix().data());
  for (int i = 0; i < _pointLights.size(); ++i) {
    Affine3f modelMatrix = _pointLights[i]->transformationMatrix();  
    glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1,
                       GL_FALSE, modelMatrix.data());
    glUniform3fv(_simplePrg.getUniformLocation("light_col"), 1,
                 _lightColors[i].data());

    _pointLights[i]->draw(_simplePrg);
  }
  _simplePrg.deactivate();
}

void Viewer::updateScene() {
  if (_animate && glfwGetTime() > _lastTime + 1.f / 60.f) {
    for (int i = 0; i < _pointLights.size(); ++i) {
      // update light position
      Vector3f lightPos = _pointLights[i]->transformationMatrix().translation();
      AngleAxisf rotation = AngleAxisf(_lightAngle, Vector3f::UnitY());
      _pointLights[i]->transformationMatrix().translation() = rotation * lightPos;
    }
    _lastTime = glfwGetTime();
  }

  if (_shadingMode == DEFERRED) {
    drawDeferred();
  } else {
    drawForward();
  }
}

void Viewer::loadProgram() {
  _blinnPrg.loadFromFiles(DATA_DIR "/shaders/blinn.vert",
                          DATA_DIR "/shaders/blinn.frag");
  _simplePrg.loadFromFiles(DATA_DIR "/shaders/simple.vert",
                           DATA_DIR "/shaders/simple.frag");
}

void Viewer::updateGUI() {
  ImGui::RadioButton("Forward", (int *)&_shadingMode, FORWARD);
  ImGui::SameLine();
  ImGui::RadioButton("Deferred", (int *)&_shadingMode, DEFERRED);
  ImGui::Checkbox("Animate light", &_animate);
}

////////////////////////////////////////////////////////////////////////////////
// Events

/*
   callback to manage mouse : called when user press or release mouse button
   You can change in this function the way the user
   interact with the system.
 */
void Viewer::mousePressed(GLFWwindow *window, int button, int action) {
  if (action == GLFW_PRESS) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      _cam.startRotation(_lastMousePos);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      _cam.startTranslation(_lastMousePos);
    }
    _button = button;
  } else if (action == GLFW_RELEASE) {
    if (_button == GLFW_MOUSE_BUTTON_LEFT) {
      _cam.endRotation();
    } else if (_button == GLFW_MOUSE_BUTTON_RIGHT) {
      _cam.endTranslation();
    }
    _button = -1;
  }
}

/*
   callback to manage mouse : called when user move mouse with button pressed
   You can change in this function the way the user
   interact with the system.
 */
void Viewer::mouseMoved(int x, int y) {
  if (_button == GLFW_MOUSE_BUTTON_LEFT) {
    _cam.dragRotate(Vector2f(x, y));
  } else if (_button == GLFW_MOUSE_BUTTON_RIGHT) {
    _cam.dragTranslate(Vector2f(x, y));
  }
  _lastMousePos = Vector2f(x, y);
}

void Viewer::mouseScroll(double x, double y) {
  _cam.zoom((y > 0) ? 1.1 : 1. / 1.1);
}

/*
   callback to manage keyboard interactions
   You can change in this function the way the user
   interact with the system.
 */
void Viewer::keyPressed(int key, int action, int mods) {
  if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    loadProgram();
  } else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    _animate = !_animate;
  } else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
    _shadingMode = ShadingMode((_shadingMode + 1) % 2);
  }
}

void Viewer::charPressed(int key) {}
