#include <glad/gl.h>
#include <glfw/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <iostream>

namespace
{
constexpr int windowWidth = 640;
constexpr int windowHeight = 400;
constexpr char windowTitle[] = "Poser";

constexpr GLsizei shaderInfoLogLength = 512;

constexpr std::array vertices = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f };
constexpr std::array indices = { 0u, 1u, 2u, 3u };

constexpr glm::vec4 clearColor = { 0.9f, 0.4f, 0.1f, 1.0f };
constexpr glm::vec4 quadColor = { 0.1f, 0.4f, 0.9f, 1.0f };
} // namespace

int main()
{
  // Load window and OpenGL
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

    glfwMakeContextCurrent(window);

    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
      std::cerr << "Failed to load OpenGL";
      return EXIT_FAILURE;
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
      glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, reinterpret_cast<void*>(sizeof(float) * 0));
    }

    // Set up an index buffer
    GLuint indexBuffer;
    {
      glGenBuffers(1, &indexBuffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);
    }
  }

  // Set up a shader program
  {
    // Compile the vertex shader
    GLuint vertexShader;
    {
      vertexShader = glCreateShader(GL_VERTEX_SHADER);

      const GLchar* source = R"(#version 330 core
                                uniform mat4 world;
                                uniform mat4 projection;
                                in vec3 inPosition;
                                void main() { gl_Position = projection * world * vec4(inPosition, 1.0); })";

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

      // Set world matrix
      {
        const GLint location = glGetUniformLocation(program, "world");
        if (location < 0)
        {
          std::cerr << "Failed to get world matrix uniform location";
          glfwTerminate();
          return EXIT_FAILURE;
        }

        const glm::mat4 worldMatrix =
          glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 0.0f)), glm::vec3(100.0f));
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(worldMatrix));
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
          glm::ortho(0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.0f, -1.0f, 1.0f);
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
  {
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLE_STRIP, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();

  return EXIT_SUCCESS;
}