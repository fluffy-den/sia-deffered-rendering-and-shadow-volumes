#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "opengl.h"
#include "viewer.h"

Viewer *v;

int WIDTH = 800;
int HEIGHT = 600;
bool showMenu{true};

static void char_callback(GLFWwindow *window, unsigned int key) {
  v->charPressed(key);
}

static void scroll_callback(GLFWwindow *window, double x, double y) {
  if (!ImGui::GetIO().WantCaptureMouse)
    v->mouseScroll(x, y);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    if ((key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    } else if ((key == GLFW_KEY_M) && action == GLFW_PRESS) {
      showMenu = !showMenu;
    }

    v->keyPressed(key, action, mods);
  }
}

void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  if (!ImGui::GetIO().WantCaptureMouse)
    v->mousePressed(window, button, action);
}

void cursorPos_callback(GLFWwindow *window, double x, double y) {
  if (!ImGui::GetIO().WantCaptureMouse)
    v->mouseMoved(x, y);
}

void reshape_callback(GLFWwindow *window, int width, int height) {
  v->reshape(width, height);
}

// initialize GLFW framework
GLFWwindow *initGLFW() {
  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, (GLint)GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "GL Viewer", NULL, NULL);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }
  glfwMakeContextCurrent(window);
  glbinding::Binding::initialize(glfwGetProcAddress);
  std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
            << std::endl;

  // callbacks
  glfwSetKeyCallback(window, key_callback);
  glfwSetCharCallback(window, char_callback);
  glfwSetFramebufferSizeCallback(window, reshape_callback);
  glfwSetCursorPosCallback(window, cursorPos_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // Disable vsync
  glfwSwapInterval(0);

  return window;
}

static void error_callback(int error, const char *description) {
  fputs(description, stderr);
}

bool initializeDearImGui(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();

  // setup platform/renderer bindings
  if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
    return false;
  }
  if (!ImGui_ImplOpenGL3_Init("#version 150")) {
    return false;
  }

  // Setup ImGui style
  ImGui::StyleColorsDark();

  return true;
}

int main(int argc, char **argv) {
  glfwSetErrorCallback(error_callback);

  GLFWwindow *window = initGLFW();
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  v = new Viewer();
  v->init(w, h);

  // Initialize Dear ImGui
  if (!initializeDearImGui(window)) {
    std::cerr << "Dear ImGui initialization failed" << std::endl;
    return EXIT_FAILURE;
  }

  while (!glfwWindowShouldClose(window)) {
    // update and render the scene
    v->updateScene();

    // update and render the GUI
    if (showMenu) {
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      if (ImGui::Begin("Viewer")) {
        ImGui::Text("%.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        ImGui::Separator();
        v->updateGUI();
      }
      ImGui::End();
      ImGui::EndFrame();
      ImGui::Render();

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup ImGui
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  delete v;

  glfwDestroyWindow(window);
  glfwTerminate();
  exit(EXIT_SUCCESS);
}
