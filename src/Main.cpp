#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glad/gl.h>
#include <glfw/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

namespace
{

// Window constants
constexpr int windowWidth = 640;
constexpr int windowHeight = 400;
constexpr char windowTitle[] = "Poser";

// OpenGL constants
constexpr GLsizei shaderInfoLogLength = 512;

// File constants
constexpr char modelFileName[] = "models/model.blend";

// Color constants
constexpr glm::vec4 clearColor = { 0.9f, 0.4f, 0.1f, 1.0f };
constexpr glm::vec4 quadColor = { 0.1f, 0.4f, 0.9f, 1.0f };

// Camera constants
constexpr float cameraMinDistance = 0.5f;
constexpr float cameraPositionY = 1.25f;
constexpr float cameraTargetY = 0.5f;

// Camera variables
bool mouseDown = false;
float cameraAngle = glm::radians(45.0f);
float cameraDistance = 5.0f;
double lastMouseX;
glm::mat4 viewMatrix;

// Geometry variables
std::vector<glm::vec3> vertices;
std::vector<unsigned int> indices;

void cursorPositionCallback(GLFWwindow* window, double x, double y)
{
  if (mouseDown)
  // Tumble the camera
  {
    const double deltaX = x - lastMouseX;
    cameraAngle -= (static_cast<float>(deltaX * glm::pi<double>()) / windowWidth);
  }
  lastMouseX = x;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT)
  // Store mouse button state
  {
    mouseDown = (action == GLFW_PRESS);
  }
}

void scrollCallback(GLFWwindow* window, double x, double y)
{
  // Dolly the camera
  cameraDistance -= static_cast<float>(y);
  if (cameraDistance < cameraMinDistance)
  {
    cameraDistance = cameraMinDistance;
  }
}

} // namespace

int main()
{
  // Create window and load OpenGL
  GLFWwindow* window;
  {
    if (!glfwInit())
    {
      std::cerr << "Failed to initialize GLFW";
      return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, nullptr, nullptr);
    if (!window)
    {
      std::cerr << "Failed to create window";
      glfwTerminate();
      return EXIT_FAILURE;
    }

    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);

    glfwMakeContextCurrent(window);
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
      std::cerr << "Failed to load OpenGL";
      return EXIT_FAILURE;
    }

    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  }

  // Load a model
  {
    Assimp::Importer importer;

    // Parse the file
    const aiScene* scene = importer.ReadFile(modelFileName, aiProcess_Triangulate);
    if (!scene)
    {
      std::cerr << "Failed to load model:\n" << importer.GetErrorString();
      glfwTerminate();
      return EXIT_FAILURE;
    }

    if (scene->mNumMeshes > 0)
    // Load the first mesh if there is one
    {
      const aiMesh* mesh = scene->mMeshes[0];

      // Load the vertices
      vertices.resize(static_cast<size_t>(mesh->mNumVertices));
      for (unsigned int i = 0u; i < mesh->mNumVertices; ++i)
      {
        const aiVector3D& vertex = mesh->mVertices[i];
        vertices.at(i) = glm::vec3(vertex.x, vertex.y, vertex.z);
      }

      // Load the indices
      indices.resize(static_cast<size_t>(mesh->mNumFaces) * 3u);
      for (unsigned int i = 0u; i < mesh->mNumFaces; ++i)
      {
        const aiFace& face = mesh->mFaces[i];
        assert(face.mNumIndices == 3u);
        indices.at(static_cast<size_t>(i) * 3u + 0u) = face.mIndices[0];
        indices.at(static_cast<size_t>(i) * 3u + 1u) = face.mIndices[1];
        indices.at(static_cast<size_t>(i) * 3u + 2u) = face.mIndices[2];
      }
    }
  }

  // Set up some geometry
  {
    // Set up a vertex array to capture the following vertex and index buffer
    {
      GLuint vertexArray;
      glGenVertexArrays(1, &vertexArray);
      glBindVertexArray(vertexArray);
    }

    // Set up a vertex buffer
    {
      GLuint vertexBuffer;
      glGenBuffers(1, &vertexBuffer);
      glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vertices.size()), vertices.data(),
                   GL_STATIC_DRAW);

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, reinterpret_cast<void*>(sizeof(float) * 0));
    }

    // Set up an index buffer
    GLuint indexBuffer;
    {
      glGenBuffers(1, &indexBuffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(unsigned int) * indices.size()),
                   indices.data(), GL_STATIC_DRAW);
    }
  }

  // Set up a shader program
  GLint viewUniformLocation;
  {
    // Compile the vertex shader
    GLuint vertexShader;
    {
      vertexShader = glCreateShader(GL_VERTEX_SHADER);

      const GLchar* source = R"(#version 330 core
                                uniform mat4 view;
                                uniform mat4 projection;
                                in vec3 inPosition;
                                void main() { gl_Position = projection * view * vec4(inPosition, 1.0); })";

      glShaderSource(vertexShader, 1, &source, nullptr);
      glCompileShader(vertexShader);

      GLint success;
      glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
      if (!success)
      {
        GLchar infoLog[shaderInfoLogLength];
        glGetShaderInfoLog(vertexShader, shaderInfoLogLength, nullptr, infoLog);
        std::cerr << "Failed to compile vertex shader:\n" << infoLog;
        glfwTerminate();
        return EXIT_FAILURE;
      }
    }

    // Compile the fragment shader
    GLuint fragmentShader;
    {
      fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

      const GLchar* source = R"(#version 150 core
                                uniform vec4 color;
                                out vec4 fragColor;
                                void main() { fragColor = color; })";

      glShaderSource(fragmentShader, 1, &source, nullptr);
      glCompileShader(fragmentShader);

      GLint success;
      glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
      if (!success)
      {
        GLchar infoLog[shaderInfoLogLength];
        glGetShaderInfoLog(fragmentShader, shaderInfoLogLength, nullptr, infoLog);
        std::cerr << "Failed to compile fragment shader:\n" << infoLog;
        glfwTerminate();
        return EXIT_FAILURE;
      }
    }

    // Link shader program
    GLuint program;
    {
      program = glCreateProgram();

      glAttachShader(program, vertexShader);
      glAttachShader(program, fragmentShader);

      glLinkProgram(program);

      GLint success;
      glGetProgramiv(program, GL_LINK_STATUS, &success);
      if (!success)
      {
        GLchar infoLog[shaderInfoLogLength];
        glGetProgramInfoLog(program, shaderInfoLogLength, nullptr, infoLog);
        std::cerr << "Failed to link shader program:\n" << infoLog;
        glfwTerminate();
        return EXIT_FAILURE;
      }

      glDeleteShader(vertexShader);
      glDeleteShader(fragmentShader);
    }

    // Use shader program and set uniform values
    {
      glUseProgram(program);

      // Retrieve view matrix location
      {
        viewUniformLocation = glGetUniformLocation(program, "view");
        if (viewUniformLocation < 0)
        {
          std::cerr << "Failed to get view matrix uniform location";
          glfwTerminate();
          return EXIT_FAILURE;
        }
      }

      // Set projection matrix
      {
        const GLint location = glGetUniformLocation(program, "projection");
        if (location < 0)
        {
          std::cerr << "Failed to get projection matrix uniform location";
          glfwTerminate();
          return EXIT_FAILURE;
        }

        const glm::mat4 projectionMatrix =
          glm::perspective(45.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.1f, 100.0f);
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
      }

      // Set color
      {
        const GLint location = glGetUniformLocation(program, "color");
        if (location < 0)
        {
          std::cerr << "Failed to get color uniform location";
          glfwTerminate();
          return EXIT_FAILURE;
        }

        const glm::vec4 color = glm::vec4(quadColor.r, quadColor.g, quadColor.b, quadColor.a);
        glUniform4fv(location, 1, glm::value_ptr(color));
      }
    }
  }

  while (!glfwWindowShouldClose(window))
  // Main loop
  {
    // Render
    {
      glClear(GL_COLOR_BUFFER_BIT);

      // Set view matrix
      {
        viewMatrix = glm::lookAt(glm::vec3(glm::sin(cameraAngle) * cameraDistance, cameraPositionY,
                                           glm::cos(cameraAngle) * cameraDistance),
                                 glm::vec3(0.0f, cameraTargetY, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(viewUniformLocation, 1, GL_FALSE, glm::value_ptr(viewMatrix));
      }

      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

      glfwSwapBuffers(window);
    }

    glfwPollEvents();
  }

  glfwTerminate();
  return EXIT_SUCCESS;
}