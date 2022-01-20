#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glad/gl.h>
#include <glfw/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>
#include <vector>

namespace
{

// Vertex definition
struct Vertex
{
  glm::vec3 position, normal;
  glm::ivec4 boneIds;    // Which bones affect this vertex (indices into the bone and bone transform array)
  glm::vec4 boneWeights; // How much each indexed bone affects this vertex, elements sum up to 1.0
};

// Bone definition
struct Bone
{
  glm::mat4 inverseBindMatrix; // Inverse bind pose bone transform (transforms from unposed bone to model space origin)
  glm::mat4 posedTransform;    // Posed bone transform in bone space (translation * rotation * scale)
  std::vector<glm::mat4> translationKeyframes, rotationKeyframes, scaleKeyframes;
  Bone* parent;
};

// Window constants
constexpr char windowTitle[] = "Poser";
constexpr int windowWidth = 640;
constexpr int windowHeight = 400;
constexpr float windowAspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);

// OpenGL constants
constexpr GLsizei shaderInfoLogLength = 512;

// File constants
constexpr char modelFileName[] = "models/silly_dancing.fbx";

// Color constants
constexpr glm::vec4 clearColor = { 0.9f, 0.4f, 0.1f, 1.0f };
constexpr glm::vec4 geometryColor = { 0.1f, 0.4f, 0.9f, 1.0f };

// Camera constants
constexpr float cameraMinDistance = 0.5f;
constexpr float cameraPositionY = 4.0f;
constexpr float cameraTargetY = 1.5f;
constexpr float cameraNear = 0.1f;
constexpr float cameraFar = 100.0f;
constexpr float cameraFov = 45.0f; // Vertical field of view in degrees

// Camera variables
bool mouseDown = false;
float cameraAngle = glm::radians(45.0f);
float cameraDistance = 5.0f;
double lastMouseX;

// Geometry variables
std::vector<Vertex> vertices;
std::vector<unsigned int> indices;

// Animation variables
std::vector<Bone> bones;
std::vector<glm::mat4> boneTransforms; // Transforms from unposed to posed bone in model space
unsigned int frameIndex = 0u;          // Current animation frame

glm::mat4 assimpToGlmMat4(const aiMatrix4x4& matrix)
{
  return glm::transpose(glm::make_mat4(&matrix.a1)); // Convert row-major (assimp) to column-major (glm)
}

int findNamedBone(const aiScene* scene, const aiString& name)
{
  const aiMesh* mesh = scene->mMeshes[0];
  for (unsigned int i = 0u; i < mesh->mNumBones; ++i)
  {
    if (mesh->mBones[i]->mName == name)
    {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void loadSkeletonNode(const aiScene* scene, const aiNode* node, Bone* parent)
{
  const int boneIndex = findNamedBone(scene, node->mName);
  if (boneIndex >= 0)
  {
    Bone& bone = bones.at(boneIndex);
    bone.parent = parent;
    parent = &bone;
  }

  // Process the children of this node recursively
  for (unsigned int i = 0u; i < node->mNumChildren; ++i)
  {
    loadSkeletonNode(scene, node->mChildren[i], parent);
  }
}

void updateAnimation()
{
  // Update the new posed transform at the current animation frame for each bone in bone space
  for (size_t i = 0u; i < bones.size(); ++i)
  {
    Bone& bone = bones.at(i);

    const glm::mat4 translation = bone.translationKeyframes.at(frameIndex % bone.translationKeyframes.size());
    const glm::mat4 rotation = bone.rotationKeyframes.at(frameIndex % bone.rotationKeyframes.size());
    const glm::mat4 scale = bone.scaleKeyframes.at(frameIndex % bone.scaleKeyframes.size());

    bone.posedTransform = translation * rotation * scale;
  }

  // Update the transform from the unposed to the posed bone for each bone in model space
  for (size_t i = 0u; i < bones.size(); ++i)
  {
    const Bone& bone = bones.at(i);

    // Find the posed bone transform in model space (transforms from the model space origin to the posed bone)
    glm::mat4 posedTransform = bone.posedTransform;
    {
      // Multiply the posed bone transforms of all previous bones in the hierarchy to find the posed bone transform
      Bone* parent = bone.parent;
      while (parent)
      {
        posedTransform = parent->posedTransform * posedTransform;
        parent = parent->parent;
      }
    }

    // Store the transform from the unposed to the posed bone in model space by transforming from the unposed bone to
    // the model space origin (through the inverse bind matrix) and then from there to the posed bone in model space
    boneTransforms.at(i) = posedTransform * bone.inverseBindMatrix;
  }
}

void cursorPositionCallback(GLFWwindow* window, double x, double y)
{
  // Tumble the camera
  if (mouseDown)
  {
    const double deltaX = x - lastMouseX;
    cameraAngle -= (static_cast<float>(deltaX * glm::pi<double>()) / windowWidth);
  }
  lastMouseX = x;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
  // Store mouse button state
  if (button == GLFW_MOUSE_BUTTON_LEFT)
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
    glEnable(GL_DEPTH_TEST);
  }

  // Load a model
  {
    Assimp::Importer importer;

    // Parse the file
    constexpr int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenNormals;
    const aiScene* scene = importer.ReadFile(modelFileName, flags);
    if (!scene)
    {
      std::cerr << "Failed to load model:\n" << importer.GetErrorString();
      glfwTerminate();
      return EXIT_FAILURE;
    }

    // Load the first mesh if there is one
    if (scene->mNumMeshes > 0)
    {
      const aiMesh* mesh = scene->mMeshes[0];

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

      // Load the vertices
      vertices.resize(static_cast<size_t>(mesh->mNumVertices));
      for (unsigned int i = 0u; i < mesh->mNumVertices; ++i)
      {
        Vertex& vertex = vertices.at(i);

        // Position
        {
          const aiVector3D& position = mesh->mVertices[i];
          vertex.position = glm::vec3(position.x, position.y, position.z);
        }

        // Normal
        {
          const aiVector3D& normal = mesh->mNormals[i];
          vertex.normal = glm::vec3(normal.x, normal.y, normal.z);
        }

        // These will be set in the next step
        vertices.at(i).boneIds = glm::ivec4(-1);
        vertices.at(i).boneWeights = glm::vec4(0.0f);
      }

      // Load the bones
      bones.resize(mesh->mNumBones);
      boneTransforms.resize(mesh->mNumBones);
      for (unsigned int i = 0u; i < mesh->mNumBones; ++i)
      {
        const aiBone* boneInfo = mesh->mBones[i];

        // Store the inverse bind matrix for this bone
        bones.at(i).inverseBindMatrix = assimpToGlmMat4(boneInfo->mOffsetMatrix);

        // Iterate through all the vertices that this bone affects
        for (unsigned int j = 0u; j < boneInfo->mNumWeights; ++j)
        {
          const aiVertexWeight& weight = boneInfo->mWeights[j];
          Vertex& affectedVertex = vertices.at(weight.mVertexId);

          // Find the first unpopulated element for this vertex
          unsigned int element = 0u;
          while (element < 3u)
          {
            if (affectedVertex.boneIds[element] < 0)
            {
              break;
            }
            ++element;
          }

          // Mark this vertex as being affected by the bone
          affectedVertex.boneIds[element] = i;
          affectedVertex.boneWeights[element] = weight.mWeight;
        }
      }
    }

    // Load the first animation
    {
      const aiAnimation* animation = scene->mAnimations[0];

      // Load the keyframes for each bone
      for (unsigned int i = 0u; i < animation->mNumChannels; ++i)
      {
        const aiNodeAnim* channel = animation->mChannels[i];

        const int boneIndex = findNamedBone(scene, channel->mNodeName);
        if (boneIndex < 0)
        {
          continue;
        }

        Bone& bone = bones.at(boneIndex);

        // Translation keyframes
        {
          bone.translationKeyframes.resize(channel->mNumPositionKeys);
          for (unsigned int j = 0u; j < channel->mNumPositionKeys; ++j)
          {
            const aiVector3D& translation = channel->mPositionKeys[j].mValue;
            bone.translationKeyframes.at(j) = glm::translate(glm::mat4(1.0f), glm::make_vec3(&translation.x));
          }
        }

        // Rotation keyframes
        {
          bone.rotationKeyframes.resize(channel->mNumRotationKeys);
          for (unsigned int j = 0u; j < channel->mNumRotationKeys; ++j)
          {
            const aiQuaternion& rotation = channel->mRotationKeys[j].mValue;
            bone.rotationKeyframes.at(j) = glm::toMat4(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
          }
        }

        // Scale keyframes
        {
          bone.scaleKeyframes.resize(channel->mNumScalingKeys);
          for (unsigned int j = 0u; j < channel->mNumScalingKeys; ++j)
          {
            const aiVector3D& scale = channel->mScalingKeys[j].mValue;
            bone.scaleKeyframes.at(j) = glm::scale(glm::mat4(1.0f), glm::make_vec3(&scale.x));
          }
        }
      }
    }

    // Load the skeleton
    loadSkeletonNode(scene, scene->mRootNode, nullptr);
  }

  // Set up geometry
  {
    // Generate and bind a vertex array to capture the following vertex and index buffer
    {
      GLuint vertexArray;
      glGenVertexArrays(1, &vertexArray);
      glBindVertexArray(vertexArray);
    }

    // Generate and fill an index buffer
    {
      GLuint indexBuffer;
      glGenBuffers(1, &indexBuffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(unsigned int) * indices.size()),
                   indices.data(), GL_STATIC_DRAW);
    }

    // Generate and fill a vertex buffer
    {
      GLuint vertexBuffer;
      glGenBuffers(1, &vertexBuffer);
      glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(Vertex) * vertices.size()), vertices.data(),
                   GL_STATIC_DRAW);
    }

    // Apply the vertex definition
    {
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, position)));

      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, normal)));

      glEnableVertexAttribArray(2);
      glVertexAttribIPointer(2, 4, GL_INT, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, boneIds)));

      glEnableVertexAttribArray(3);
      glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, boneWeights)));
    }
  }

  // Set up a shader program
  GLint viewUniformLocation, boneTransformsUniformLocation;
  {
    // Compile the vertex shader
    GLuint vertexShader;
    {
      vertexShader = glCreateShader(GL_VERTEX_SHADER);

      const GLchar* source = R"(#version 330 core
                                uniform mat4 view;
                                uniform mat4 projection;
                                uniform mat4 boneTransforms[64];
                                layout(location = 0) in vec3 inPosition;
                                layout(location = 1) in vec3 inNormal;
                                layout(location = 2) in ivec4 inBoneIds;
                                layout(location = 3) in vec4 inBoneWeights;
                                out vec3 normal;
                                void main()
                                {
                                  mat4 boneTransform = mat4(0.0);
                                  for (int i = 0; i < 4; ++i)
                                  {
                                    boneTransform += boneTransforms[inBoneIds[i]] * inBoneWeights[i];
                                  }
                                  gl_Position = projection * view * boneTransform * vec4(inPosition, 1.0);
                                  normal = normalize((boneTransform * vec4(inNormal, 0.0)).xyz);
                                })";

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
                                in vec3 normal;
                                out vec4 fragColor;
                                void main()
                                {
                                  float diffuse = dot(normal, vec3(1.0));
                                  fragColor = vec4(color.rgb * diffuse, color.a);
                                })";

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

    // Use shader program, retrieve uniform locations and set constant uniform values
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

      // Retrieve bone transforms location
      {
        boneTransformsUniformLocation = glGetUniformLocation(program, "boneTransforms");
        if (boneTransformsUniformLocation < 0)
        {
          std::cerr << "Failed to get bone transforms uniform location";
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

        const glm::mat4 projectionMatrix = glm::perspective(cameraFov, windowAspectRatio, cameraNear, cameraFar);
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

        const glm::vec4 color = glm::vec4(geometryColor.r, geometryColor.g, geometryColor.b, geometryColor.a);
        glUniform4fv(location, 1, glm::value_ptr(color));
      }
    }
  }

  // Main loop
  while (!glfwWindowShouldClose(window))
  {
    // Update
    {
      ++frameIndex;
      updateAnimation();
    }

    // Render
    {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Set view matrix uniform
      {
        glm::mat4 viewMatrix = glm::lookAt(glm::vec3(glm::sin(cameraAngle) * cameraDistance, cameraPositionY,
                                                     glm::cos(cameraAngle) * cameraDistance),
                                           glm::vec3(0.0f, cameraTargetY, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(viewUniformLocation, 1, GL_FALSE, glm::value_ptr(viewMatrix));
      }

      // Set bone transforms uniform
      glUniformMatrix4fv(boneTransformsUniformLocation, static_cast<GLsizei>(boneTransforms.size()), GL_FALSE,
                         glm::value_ptr(boneTransforms[0]));

      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

      glfwSwapBuffers(window);
    }

    glfwPollEvents();
  }

  glfwTerminate();
  return EXIT_SUCCESS;
}