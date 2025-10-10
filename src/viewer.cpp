#include "viewer.h"
#include "camera.h"

#include <SOIL2.h>
#include <cstdint>
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
  glEnable(GL_STENCIL_TEST);

  loadProgram();

  Mesh *quad = new Mesh();
  quad->createGrid(2, 2);
  quad->init();
  quad->transformationMatrix() = AngleAxisf(M_PI / 2.f, Vector3f(-1, 0, 0)) *
                                 Scaling(20.f, 20.f, 1.f) *
                                 Translation3f(-0.5, -0.5, -0.5);
  _shapes.push_back(quad);
  _specularCoef.push_back(0.f);

  Mesh *sphere = new Mesh();
  sphere->load(DATA_DIR "/models/sphere.off");
  sphere->init();
  sphere->transformationMatrix() = Translation3f(0, 0, 2.f) * Scaling(0.5f);
  _shapes.push_back(sphere);
  _specularCoef.push_back(0.3f);

  Mesh *tw = new Mesh();
  tw->load(DATA_DIR "/models/tw.off");
  tw->init();
  _shapes.push_back(tw);
  _specularCoef.push_back(0.75);

  // Army of tinky winkies
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 2; ++i) {
      Mesh *tinky = new Mesh();
      tinky->load(DATA_DIR "/models/tw.off");
      tinky->init();
      tinky->transformationMatrix() = Translation3f(-5.f + i, 0.f, -5.f + j);
      _shapes.push_back(tinky);
      _specularCoef.push_back(0.1f + 0.65f * (i / 9.f));
    }
  }

  _lightColors.push_back(Vector3f::Constant(0.8f));
  Mesh *light = new Mesh();
  light->createSphere(0.025f);
  light->init();
  light->transformationMatrix() = Translation3f(
      _cam.sceneCenter() +
      _cam.sceneRadius() * Vector3f(Eigen::internal::random<float>(),
                                    Eigen::internal::random<float>(0.1f, 0.5f),
                                    Eigen::internal::random<float>()));
  _pointLights.push_back(light);

  // Army of lights
  for (uint32_t i = 0; i < 20; ++i) {
    Mesh *lighti = new Mesh();
    lighti->createSphere(Eigen::internal::random<float>(0.025f, 0.025f));
    lighti->init();
    lighti->transformationMatrix() =
        Translation3f(_cam.sceneCenter() +
                      _cam.sceneRadius() *
                          Vector3f(Eigen::internal::random<float>(0.1f, 1.f),
                                   Eigen::internal::random<float>(0.1f, 4.f),
                                   Eigen::internal::random<float>(0.1f, 4.f)));

    _lightColors.emplace_back(
        Vector3f(Eigen::internal::random<float>(0.25f, 1.0f),
                 Eigen::internal::random<float>(0.25f, 1.0f),
                 Eigen::internal::random<float>(0.25f, 1.0f)));
    _pointLights.emplace_back(lighti);
  }

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

  _deferredFbo.init(w, h);

  _deferredQuad = new Mesh();
  _deferredQuad->createGrid(2, 2);
  _deferredQuad->init();
}

void Viewer::reshape(int w, int h) {
  _winWidth = w;
  _winHeight = h;
  _cam.setScreenViewport(AlignedBox2f(Vector2f(0.0, 0.0), Vector2f(w, h)));
  glViewport(0, 0, w, h);
}

static void _drawShadowVolumeDebug(Mesh *shadowVolume, Shader &prg,
                                   const Matrix4f &proj, const Matrix4f &view,
                                   const Matrix4f &model,
                                   bool shadowWireframe = true) {

  if (!shadowVolume)
    return;

  prg.activate();
  glUniformMatrix4fv(prg.getUniformLocation("projection_matrix"), 1, GL_FALSE,
                     proj.data());
  glUniformMatrix4fv(prg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                     view.data());
  glUniformMatrix4fv(prg.getUniformLocation("model_matrix"), 1, GL_FALSE,
                     model.data());

  // state pour visualiser le volume (dessine faces avant puis arrière)
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  if (shadowWireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // Draw front faces
  glCullFace(GL_BACK);
  shadowVolume->draw(prg);

  // Draw back faces
  glCullFace(GL_FRONT);
  shadowVolume->draw(prg);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // restore
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);
  prg.deactivate();
}
static void _drawShadowVolume(Mesh *sv, Shader &prg, const Matrix4f &proj,
                              const Matrix4f &view, const Matrix4f &model) {
  prg.activate();
  glUniformMatrix4fv(prg.getUniformLocation("projection_matrix"), 1, GL_FALSE,
                     proj.data());
  glUniformMatrix4fv(prg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                     view.data());
  glUniformMatrix4fv(prg.getUniformLocation("model_matrix"), 1, GL_FALSE,
                     model.data());

  sv->draw(prg);
  prg.deactivate();
}

void Viewer::drawForward() {
  // Clear buffers
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  // Ambient pass (base color)
  _blinnPrg.activate();
  glUniformMatrix4fv(_blinnPrg.getUniformLocation("projection_matrix"), 1,
                     GL_FALSE, _cam.computeProjectionMatrix().data());
  glUniformMatrix4fv(_blinnPrg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                     _cam.computeViewMatrix().data());

  // Use a small ambient light as base (we render ambient contribution for all
  // objects)
  glUniform1f(_blinnPrg.getUniformLocation("is_first"), 1.f);
  Vector4f tmpLightPos;
  tmpLightPos << Vector3f::Zero(), 1.f;
  glUniform4fv(_blinnPrg.getUniformLocation("light_pos"), 1,
               (_cam.computeViewMatrix() * tmpLightPos).eval().data());
  Vector3f ambientCol = Vector3f::Constant(0.05f);
  glUniform3fv(_blinnPrg.getUniformLocation("light_col"), 1, ambientCol.data());

  glEnable(GL_DEPTH_TEST);
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

  // Per-light passes using stencil shadow volumes (z-fail / Carmack's reverse)
  for (size_t l = 0; l < _pointLights.size(); ++l) {
    // Clear stencil for this light
    glClear(GL_STENCIL_BUFFER_BIT);

    // Prepare for shadow volume rendering: disable color writes and depth
    // writes, enable stencil test
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);

    // Compute light position in model space per shape when needed
    // Render shadow volumes to stencil buffer using z-fail method:
    // increment on depth fail for back faces, decrement on depth fail for
    // front faces
    _simplePrg.activate();
    glUniformMatrix4fv(_simplePrg.getUniformLocation("projection_matrix"), 1,
                       GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_simplePrg.getUniformLocation("view_matrix"), 1,
                       GL_FALSE, _cam.computeViewMatrix().data());

    glEnable(GL_CULL_FACE);
    // First: back faces, increment on depth fail (z-fail)
    glCullFace(GL_FRONT);
    glStencilOp(GL_KEEP, GL_INCR_WRAP, GL_KEEP);

    for (size_t i = 0; i < _shapes.size(); ++i) {
      Vector4f lightPos;
      lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
      // Transform light position to model space
      Vector3f lightPosModel =
          (_shapes[i]->transformationMatrix().inverse() * lightPos).head<3>();

      Mesh *sv = _shapes[i]->computeShadowVolume(lightPosModel);
      if (!sv)
        continue;

      glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1,
                         GL_FALSE,
                         _shapes[i]->transformationMatrix().matrix().data());
      sv->draw(_simplePrg);

      delete sv;
    }

    // Second: front faces, decrement on depth fail (z-fail)
    glCullFace(GL_BACK);
    glStencilOp(GL_KEEP, GL_DECR_WRAP, GL_KEEP);

    for (size_t i = 0; i < _shapes.size(); ++i) {
      Vector4f lightPos;
      lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
      Vector3f lightPosModel =
          (_shapes[i]->transformationMatrix().inverse() * lightPos).head<3>();

      Mesh *sv = _shapes[i]->computeShadowVolume(lightPosModel);
      if (!sv)
        continue;

      glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1,
                         GL_FALSE,
                         _shapes[i]->transformationMatrix().matrix().data());
      sv->draw(_simplePrg);

      delete sv;
    }

    glDisable(GL_CULL_FACE);
    _simplePrg.deactivate();

    // Restore color writes. Keep depth writes disabled and use depth test
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_EQUAL);
    glStencilFunc(GL_EQUAL, 0,
                  0xFF); // only render where stencil == 0 (lit fragments)
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // Additive light pass: render light contribution only where stencil == 0
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    _blinnPrg.activate();
    glUniformMatrix4fv(_blinnPrg.getUniformLocation("projection_matrix"), 1,
                       GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_blinnPrg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                       _cam.computeViewMatrix().data());

    // Set light uniforms (view-space position)
    Vector4f lightPosView;
    lightPosView << _pointLights[l]->transformationMatrix().translation(), 1.f;
    glUniform4fv(_blinnPrg.getUniformLocation("light_pos"), 1,
                 (_cam.computeViewMatrix() * lightPosView).eval().data());
    glUniform3fv(_blinnPrg.getUniformLocation("light_col"), 1,
                 _lightColors[l].data());
    // Mark as non-first since ambient already drawn
    glUniform1f(_blinnPrg.getUniformLocation("is_first"), 0.f);

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

    // restore state for next light
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
  }

  // DEBUG: Draw plain shadow volume if requested
  if (_dispShadowVolume == true) {
    for (size_t l = 0; l < _pointLights.size(); ++l) {
      Vector4f lightPos;
      lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
      for (size_t i = 0; i < _shapes.size(); ++i) {
        Vector3f lightPosModel =
            (_shapes[i]->transformationMatrix().inverse() * lightPos).head<3>();

        Mesh *sv = _shapes[i]->computeShadowVolume(lightPosModel);

        _drawShadowVolumeDebug(sv, _simplePrg, _cam.computeProjectionMatrix(),
                               _cam.computeViewMatrix(),
                               _shapes[i]->transformationMatrix().matrix(),
                               _dispShadowWireframe);
        delete sv;
      }
    }
  }

  drawLights();
}
void Viewer::drawDeferred() {
  // 1. GBuffer pass - render geometry data to textures
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  _deferredFbo.bind();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  _gbufferPrg.activate();

  glUniformMatrix4fv(_gbufferPrg.getUniformLocation("projection_matrix"), 1,
                     GL_FALSE, _cam.computeProjectionMatrix().data());
  glUniformMatrix4fv(_gbufferPrg.getUniformLocation("view_matrix"), 1, GL_FALSE,
                     _cam.computeViewMatrix().data());

  glEnable(GL_DEPTH_TEST);
  for (size_t i = 0; i < _shapes.size(); ++i) {
    glUniformMatrix4fv(_gbufferPrg.getUniformLocation("model_matrix"), 1,
                       GL_FALSE, _shapes[i]->transformationMatrix().data());
    Matrix3f normal_matrix =
        (_cam.computeViewMatrix() * _shapes[i]->transformationMatrix())
            .linear()
            .inverse()
            .transpose();
    glUniformMatrix3fv(_gbufferPrg.getUniformLocation("normal_matrix"), 1,
                       GL_FALSE, normal_matrix.data());
    glUniform1f(_gbufferPrg.getUniformLocation("specular_coef"),
                _specularCoef[i]);

    _shapes[i]->draw(_gbufferPrg);
  }

  _gbufferPrg.deactivate();

  _deferredFbo.unbind();

  // 2. Deferred lighting with shadows
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  // Copy depth buffer from FBO to default framebuffer for proper depth testing
  glBindFramebuffer(GL_READ_FRAMEBUFFER, _deferredFbo.id());
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, _winWidth, _winHeight, 0, 0, _winWidth, _winHeight,
                    GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Ambient pass (base lighting with no shadows)
  _deferredPrg.activate();

  Matrix4f inverse_projection = _cam.computeProjectionMatrix().inverse();
  glUniformMatrix4fv(_deferredPrg.getUniformLocation("inv_projection_matrix"),
                     1, GL_FALSE, inverse_projection.data());

  glUniform2f(_deferredPrg.getUniformLocation("resolution"),
              static_cast<float>(_winWidth), static_cast<float>(_winHeight));

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _deferredFbo.textureId(0));
  glUniform1i(_deferredPrg.getUniformLocation("color_sampler"), 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _deferredFbo.textureId(1));
  glUniform1i(_deferredPrg.getUniformLocation("normal_sampler"), 1);

  glDisable(GL_DEPTH_TEST);

  // Render ambient light contribution (always lit)
  glUniform1f(_deferredPrg.getUniformLocation("is_first"), 1.f);
  Vector4f tmpLightPos;
  tmpLightPos << Vector3f::Zero(), 1.f;
  glUniform4fv(_deferredPrg.getUniformLocation("light_pos"), 1,
               (_cam.computeViewMatrix() * tmpLightPos).eval().data());
  Vector3f ambientCol = Vector3f::Constant(0.05f);
  glUniform3fv(_deferredPrg.getUniformLocation("light_col"), 1,
               ambientCol.data());

  _deferredQuad->draw(_deferredPrg);

  _deferredPrg.deactivate();

  // Per-light passes with shadow volumes
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  for (size_t l = 0; l < _pointLights.size(); ++l) {
    // Clear stencil for this light
    glClear(GL_STENCIL_BUFFER_BIT);

    // Render shadow volumes to stencil buffer using z-fail method
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);

    _simplePrg.activate();
    glUniformMatrix4fv(_simplePrg.getUniformLocation("projection_matrix"), 1,
                       GL_FALSE, _cam.computeProjectionMatrix().data());
    glUniformMatrix4fv(_simplePrg.getUniformLocation("view_matrix"), 1,
                       GL_FALSE, _cam.computeViewMatrix().data());

    glEnable(GL_CULL_FACE);

    // Back faces: increment on depth fail (z-fail)
    glCullFace(GL_FRONT);
    glStencilOp(GL_KEEP, GL_INCR_WRAP, GL_KEEP);

    for (size_t i = 0; i < _shapes.size(); ++i) {
      Vector4f lightPos;
      lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
      Vector3f lightPosModel =
          (_shapes[i]->transformationMatrix().inverse() * lightPos).head<3>();

      Mesh *sv = _shapes[i]->computeShadowVolume(lightPosModel);
      if (!sv)
        continue;

      glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1,
                         GL_FALSE,
                         _shapes[i]->transformationMatrix().matrix().data());
      sv->draw(_simplePrg);

      delete sv;
    }

    // Front faces: decrement on depth fail (z-fail)
    glCullFace(GL_BACK);
    glStencilOp(GL_KEEP, GL_DECR_WRAP, GL_KEEP);

    for (size_t i = 0; i < _shapes.size(); ++i) {
      Vector4f lightPos;
      lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
      Vector3f lightPosModel =
          (_shapes[i]->transformationMatrix().inverse() * lightPos).head<3>();

      Mesh *sv = _shapes[i]->computeShadowVolume(lightPosModel);
      if (!sv)
        continue;

      glUniformMatrix4fv(_simplePrg.getUniformLocation("model_matrix"), 1,
                         GL_FALSE,
                         _shapes[i]->transformationMatrix().matrix().data());
      sv->draw(_simplePrg);

      delete sv;
    }

    glDisable(GL_CULL_FACE);
    _simplePrg.deactivate();

    // Restore rendering and use stencil test to only light non-shadowed areas
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glStencilFunc(GL_EQUAL, 0, 0xFF); // only render where stencil == 0 (lit)
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // Render light contribution with deferred shader
    _deferredPrg.activate();

    glUniformMatrix4fv(_deferredPrg.getUniformLocation("inv_projection_matrix"),
                       1, GL_FALSE, inverse_projection.data());
    glUniform2f(_deferredPrg.getUniformLocation("resolution"),
                static_cast<float>(_winWidth), static_cast<float>(_winHeight));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _deferredFbo.textureId(0));
    glUniform1i(_deferredPrg.getUniformLocation("color_sampler"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _deferredFbo.textureId(1));
    glUniform1i(_deferredPrg.getUniformLocation("normal_sampler"), 1);

    glUniform1f(_deferredPrg.getUniformLocation("is_first"), 0.f);

    Vector4f lightPos;
    lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
    glUniform4fv(_deferredPrg.getUniformLocation("light_pos"), 1,
                 (_cam.computeViewMatrix() * lightPos).eval().data());
    glUniform3fv(_deferredPrg.getUniformLocation("light_col"), 1,
                 _lightColors[l].data());

    _deferredQuad->draw(_deferredPrg);

    _deferredPrg.deactivate();

    glDisable(GL_STENCIL_TEST);
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  // DEBUG: Draw shadow volumes if requested
  if (_dispShadowVolume == true) {
    for (size_t l = 0; l < _pointLights.size(); ++l) {
      Vector4f lightPos;
      lightPos << _pointLights[l]->transformationMatrix().translation(), 1.f;
      for (size_t i = 0; i < _shapes.size(); ++i) {
        Vector3f lightPosModel =
            (_shapes[i]->transformationMatrix().inverse() * lightPos).head<3>();

        Mesh *sv = _shapes[i]->computeShadowVolume(lightPosModel);

        _drawShadowVolumeDebug(sv, _simplePrg, _cam.computeProjectionMatrix(),
                               _cam.computeViewMatrix(),
                               _shapes[i]->transformationMatrix().matrix(),
                               _dispShadowWireframe);
        delete sv;
      }
    }
  }

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
      _pointLights[i]->transformationMatrix().translation() =
          rotation * lightPos;
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
  printf("Loading: %s", DATA_DIR "/shaders/blinn.*\n");
  _blinnPrg.loadFromFiles(DATA_DIR "/shaders/blinn.vert",
                          DATA_DIR "/shaders/blinn.frag");

  printf("Loading: %s", DATA_DIR "/shaders/simple.*\n");
  _simplePrg.loadFromFiles(DATA_DIR "/shaders/simple.vert",
                           DATA_DIR "/shaders/simple.frag");

  printf("Loading: %s", DATA_DIR "/shaders/gbuffer.*\n");
  _gbufferPrg.loadFromFiles(DATA_DIR "/shaders/gbuffer.vert",
                            DATA_DIR "/shaders/gbuffer.frag");

  printf("Loading: %s", DATA_DIR "/shaders/deffered.*\n");
  _deferredPrg.loadFromFiles(DATA_DIR "/shaders/deferred.vert",
                             DATA_DIR "/shaders/deferred.frag");
}

void Viewer::updateGUI() {
  ImGui::RadioButton("Forward", (int *)&_shadingMode, FORWARD);
  ImGui::SameLine();
  ImGui::RadioButton("Deferred", (int *)&_shadingMode, DEFERRED);
  ImGui::Checkbox("Animate light", &_animate);
  ImGui::Checkbox("Display: Shadow Volumes", &_dispShadowVolume);
  ImGui::Checkbox("Display: Shadow Wireframe", &_dispShadowWireframe);
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
