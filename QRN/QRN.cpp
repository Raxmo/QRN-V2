#include <iostream>
#include <stdexcept>
#include <functional>
#include <array>
#include <string>
#include <Windows.h>
#include <vector>
#include <unordered_map>
#include <math.h>
#include <numbers>
#include <chrono>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "resource.h"
#include <algorithm>
/*=============================================================================+/
									TODO List
/+=============================================================================*/
/*
	[ ] level generation
    [ ] render in static
        [X] full hit info
		[X] hash function for random generation
			[X] 1D hash
			[X] 2D hash
			[X] 3D hash
            [X] 4D hash
            
*/

/*=============================================================================+/
								 Global Variables
/+=============================================================================*/

GLuint tilemaploc;

GLuint playerposloc;
GLuint playerrotloc;
GLuint aspectratioloc;
GLuint framecountloc;

std::array<double, 2> playerposraw = { 4.5, 4.5 };
std::array<double, 2> playerrotraw = { 1.0, 0.0 }; // rotor representing no rotation
float aspectratio = 1.0f;

std::array<double, 2> lastMousePos = { 400.0f, 300.0f };

double BaseSensitivity = 2.0;
double mouseSensitivity = 0.002;
double fps = 60.0;
double deltaTime = 0.05; // <-- independant of framerate.

std::array<double, 2> movementInput = { 0.0, 0.0 }; // x is strafe, y is forward
double movementSpeed = 4.0; // units per second

std::array<float, 512*512> mapdata;

double playerRadius = 0.95;

unsigned int frameCount;

/*=============================================================================+/
	                            Utility Functions
/+=============================================================================*/

// Rotor multiplication for 2D rotors
std::array<double, 2> RotMult(const std::array<double, 2>& a, const std::array<double, 2>& b)
{
    return {
        a[0] * b[0] - a[1] * b[1],
        a[0] * b[1] + a[1] * b[0]
    };
};

std::array<double, 2> Normalize(const std::array<double, 2>& v)
{
    double len = std::sqrt(v[0] * v[0] + v[1] * v[1]);
    if (len == 0.0) return { 0.0, 0.0 };
    return { v[0] / len, v[1] / len };
};

std::array<double, 2> NormalizeByMag(const std::array<double, 2>& v, double mag)
{
    double len = std::sqrt(v[0] * v[0] + v[1] * v[1]);
    if (len == 0.0) return { 0.0, 0.0 };
    return { v[0] / len * mag, v[1] / len * mag };
};

double Magnatude(const std::array<double, 2>& v)
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1]);
};

std::array<double, 2> getForward() // forward is (0, 1) rotated by playerrotraw
{
	return { 2.0 * playerrotraw[0] * playerrotraw[1], playerrotraw[0] * playerrotraw[0] - playerrotraw[1] * playerrotraw[1] };
}

/*=============================================================================+/
                                  Callbacks
/+=============================================================================*/

bool firstMouse = true;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastMousePos = { xpos, ypos };
        firstMouse = false;
	}
	double xoffset = (xpos) - lastMousePos[0]; // <-- horizontal mouse movement
    double yoffset = (ypos) - lastMousePos[1]; // <-- while not used, will likely be needed in future
    lastMousePos = { (xpos), (ypos) };

    // create delta rotor from mouse movement
    std::array<double, 2> rotChange = { 1.0, xoffset * mouseSensitivity };
    playerrotraw = RotMult(rotChange, playerrotraw); // <-- rotor multiplication
	playerrotraw = Normalize(playerrotraw); // <-- normalize to prevent floating point 
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    aspectratio = (float)height / (float)width;
    mouseSensitivity = BaseSensitivity / (double)max(height, width);
}

// here is the unordered map of key codes to lamdas
std::unordered_map<int, std::function<void()>> onPressFunctions = 
{
    {GLFW_KEY_ESCAPE, []()
    {
        // close window
        glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
	}},
    {GLFW_KEY_W, []()
    {
		movementInput[1] += 1.0;
    }},
    {GLFW_KEY_S, []()
    {
		movementInput[1] -= 1.0;
    }},
    {GLFW_KEY_A, []()
	{
        movementInput[0] -= 1.0;
    }},
    {GLFW_KEY_D, []()
    {
        movementInput[0] += 1.0;
    }}
};
std::unordered_map<int, std::function<void()>> onReleaseFunctions = 
{
    {GLFW_KEY_W, []()
	{
        movementInput[1] -= 1.0;
    }},
    {GLFW_KEY_S, []()
    {
        movementInput[1] += 1.0;
    }},
    {GLFW_KEY_A, []()
    {
        movementInput[0] += 1.0;
    }},
    {GLFW_KEY_D, []()
    {
        movementInput[0] -= 1.0;
    }}
};

// Here is the Key Callback
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    switch (action)
    {
    case GLFW_PRESS:
        if(onPressFunctions.contains(key))onPressFunctions[key]();
        break;
    case GLFW_RELEASE:
        if(onReleaseFunctions.contains(key))onReleaseFunctions[key]();
        break;
    default:
        return;
    }
}

/*==============================================================================+/
								  Window Stuffs
/+==============================================================================*/

GLuint VAO;
struct Window
{
    struct Info
    {
        const char* title = "Window Title";
        int width = 800;
        int height = 600;
		std::array<double, 4> clearColor = { 0.1, 0.2, 0.3, 1.0 };
        std::function<void()> onUpdate;
        std::function<void()> onRender;
		Window* self = nullptr;

	}info;

    GLFWwindow* context = nullptr;
    double lastTime = 0.0f;

	Window(Window::Info i) : info(i)
    {
        // Initialize GLFW
        if (!glfwInit()) {
			throw std::runtime_error("GLFW Initialization Failed");
        }
        // Tell GLFW we want an OpenGL 4.5 Core context
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Create a windowed mode window and its OpenGL context
        context = glfwCreateWindow(info.width, info.height, info.title, nullptr, nullptr);
        if (!context) {
            glfwTerminate();
			throw std::runtime_error("GLFW Window Creation Failed");
        } glfwMakeContextCurrent(context);

        // Load OpenGL functions using GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			glfwDestroyWindow(context);
			glfwTerminate();
			throw std::runtime_error("GLAD Initialization Failed");
        }

        // debugging error handling
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity,
            GLsizei length, const GLchar* message, const void* userParam) {
                std::cerr << "GL Debug: " << message << std::endl;
            }, nullptr);

        // create and bind VAO
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);

		// Set ancillary setting from info
		info.self = this;
        glClearColor(info.clearColor[0], info.clearColor[1], info.clearColor[2], info.clearColor[3]);

		// Set input callbacks
		glfwSetCursorPosCallback(context, cursor_position_callback);
		glfwSetFramebufferSizeCallback(context, framebuffer_size_callback);
        glfwSetKeyCallback(context, key_callback);
		// maximize window AFTER setting framebuffer size callback
		glfwMaximizeWindow(context);

        // hide and capture cursor
        glfwSetInputMode(context, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		lastTime = glfwGetTime();
    }

    ~Window()
    {
        glfwDestroyWindow(context);
        glfwTerminate();
	}

    void operator () ()
    {
        double frametime = 0.0;
        // Update loop
        while (!glfwWindowShouldClose(context)) {
            if (glfwGetKey(context, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(context, true);

            glfwPollEvents();
			double currentTime = glfwGetTime();
			deltaTime = currentTime - lastTime;

            if(info.onUpdate) info.onUpdate();

			lastTime = currentTime;
			frametime += deltaTime;
            if(frametime < (1.0 / fps)) continue;
			if(info.onRender) info.onRender();
			glfwSwapBuffers(context);
            glFinish();
			frametime -= 1.0 / fps;
        }
    }
};

/*=============================================================================+/
							Shader and Shader Program
/+=============================================================================*/

std::string TextFromResource(int resourceID)
{
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hRes) throw std::runtime_error("Resource Not Found");
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) throw std::runtime_error("Failed to Load Resource");
    DWORD dataSize = SizeofResource(NULL, hRes);
    const char* pData = static_cast<const char*>(LockResource(hData));
    if (!pData) throw std::runtime_error("Failed to Lock Resource");
	return std::string(pData, dataSize);
};

struct Shader
{
    unsigned int Type;
	int RCID;

	Shader(unsigned int type, int rcid) : Type(type), RCID(rcid) {}

    unsigned int Compile() const
    {
        std::string source = TextFromResource(RCID);
        unsigned int shader = glCreateShader(Type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        int success;
        char infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cout << (Type == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
                << " Shader Compilation Failed:\n" << infoLog << std::endl;
        }
        return shader;
	}
};

struct ShaderProgram
{
    struct Info
    {
        std::vector<Shader> Shaders;
		std::unordered_map<std::string, int> items;
	    std::function<void()> onBuild;
        std::function<void()> onDestroy;
		std::function<void()> onUpdate; // <-- subscribe to update event?
        std::function<void()> onInvoke;

    }info;
    unsigned int SELF = 0;
    void Build()
    {
        unsigned int program = glCreateProgram();
        for (const auto& shader : info.Shaders) {
            unsigned int shaderID = shader.Compile();
            glAttachShader(program, shaderID);
            glDeleteShader(shaderID);
        }
        glLinkProgram(program);
        int success;
        char infoLog[512];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cout << "Shader Program Linking Failed:\n" << infoLog << std::endl;
        }

        SELF = program;
		if (info.onBuild) info.onBuild();
    }

    void operator ()()
    {
		glUseProgram(SELF);
		if (info.onInvoke) info.onInvoke();
    }

    ShaderProgram(ShaderProgram::Info info) : info(info) {}
    ~ShaderProgram()
    {
        glDeleteProgram(SELF);
		if (info.onDestroy) info.onDestroy();
    }
};

/*=============================================================================+/
							  Shader deffinitions
/+=============================================================================*/

// Main rendering shader
Shader vertex(GL_VERTEX_SHADER, IDR_RCDATA1);
Shader fragment(GL_FRAGMENT_SHADER, IDR_RCDATA2);
ShaderProgram shaderProgram({
	.Shaders = { vertex, fragment },
    .onBuild = []()
    {
        playerposloc = glGetUniformLocation(shaderProgram.SELF, "playerpos");
        playerrotloc = glGetUniformLocation(shaderProgram.SELF, "playerrot");
        aspectratioloc = glGetUniformLocation(shaderProgram.SELF, "aspectratio");
        framecountloc = glGetUniformLocation(shaderProgram.SELF, "frameCount");
        frameCount = (unsigned int)(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
        ).count()) & (GLuint)(-1);
    },
    .onInvoke = []()
    {
		glUniform2f(playerposloc, (float)playerposraw[0], (float)playerposraw[1]);
		glUniform2f(playerrotloc, (float)playerrotraw[0], (float)playerrotraw[1]);
		glUniform1f(aspectratioloc, aspectratio);
        glUniform1ui(framecountloc, frameCount++);
        glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    });

// Map generation compute shader
Shader mapGenShader(GL_COMPUTE_SHADER, IDR_RCDATA3);
ShaderProgram mapGenProgram({
    .Shaders = { mapGenShader },
    .onBuild = []()
    {
        glGenBuffers(1, &tilemaploc);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, tilemaploc);
        
        // Allocate GPU memory, but don’t upload any data
        glBufferData(GL_SHADER_STORAGE_BUFFER, 512 * 512 * sizeof(float), nullptr, GL_DYNAMIC_COPY);
        
        // Bind to binding point 0
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tilemaploc);
        glUseProgram(mapGenProgram.SELF);
        glDispatchCompute(512/8, 512/8, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); 
        float* gpuData = (float*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (!gpuData) {
            throw std::runtime_error("Failed to map SSBO!");
        }
        std::memcpy(mapdata.data(), gpuData, 512 * 512 * sizeof(float));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
	});

/*=============================================================================+/
								  Main Function
/+=============================================================================*/

int main()
{
    try
    {
        Window::Info info;
		info.title = "QRN";
        info.onUpdate = []()
        {
			std::array<double, 2> forward = getForward();
			std::array<double, 2> right = { forward[1], -forward[0] }; // perpendicular to forward

            std::array<double, 2> movement = {
                movementInput[0] * right[0] + movementInput[1] * forward[0],
                movementInput[0] * right[1] + movementInput[1] * forward[1]
			};

			movement = Normalize(movement);

			playerposraw[0] += movement[0] * deltaTime * movementSpeed;
			playerposraw[1] += movement[1] * deltaTime * movementSpeed;

            for (int y = playerposraw[1] - playerRadius; y < playerposraw[1] + playerRadius; y++)
            {
                for (int x = playerposraw[0] - playerRadius; x < playerposraw[0] + playerRadius; x++)
                {
					float tileValue = mapdata[ y * 512 + (x)];
					if (tileValue < 0.5f) continue; // not a wall, go next

                    std::array<double, 2> closestPoint =
                    {
                        std::clamp(playerposraw[0], (double)(x), (double)(x + 1)),
                        std::clamp(playerposraw[1], (double)(y), (double)(y + 1))
					};

                    std::array<double, 2> difference =
                    {
                        playerposraw[0] - closestPoint[0],
                        playerposraw[1] - closestPoint[1]
                    };
					double distance = Magnatude(difference);
                    if (distance >= playerRadius) continue; // no colision

					std::array<double, 2> correctionDir = NormalizeByMag(difference, playerRadius);
					playerposraw[0] = correctionDir[0] + closestPoint[0];
					playerposraw[1] = correctionDir[1] + closestPoint[1];
                }
			}
			
        };
        info.onRender = []()
        {
            glClear(GL_COLOR_BUFFER_BIT);
            shaderProgram();
		};
		Window window(info); // <-- sets up OpenGL context

		mapGenProgram.Build();

        shaderProgram.Build();

        window();
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}