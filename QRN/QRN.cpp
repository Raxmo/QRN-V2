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



GLuint texture;

GLuint playerposloc;
GLuint playerrotloc;
GLuint aspectratioloc;

std::array<double, 2> playerposraw = { 0.0, 0.0 };
std::array<double, 2> playerrotraw = { 1.0, 0.0 };
float aspectratio = 1.0f;

std::array<float, 2> lastMousePos = { 400.0f, 300.0f };

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

std::array<double, 2> getForward() // forward is (0, 1) rotated by playerrotraw
{
	return { 2.0 * playerrotraw[0] * playerrotraw[1], playerrotraw[0] * playerrotraw[0] - playerrotraw[1] * playerrotraw[1] };
}

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

        // create and bind VAO
		unsigned int VAO;
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);

		// Set ancillary setting from info
		info.self = this;
        glClearColor(info.clearColor[0], info.clearColor[1], info.clearColor[2], info.clearColor[3]);
    }

    ~Window()
    {
        glfwDestroyWindow(context);
        glfwTerminate();
	}

    void operator () ()
    {
        // Update loop
        while (!glfwWindowShouldClose(context)) {
            if (glfwGetKey(context, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(context, true);
            glfwPollEvents();

			int width, height;
            glfwGetFramebufferSize(context, &width, &height);
            glViewport(0, 0, width, height);
			aspectratio = (float)height / (float)width;


            //glClear(GL_COLOR_BUFFER_BIT);
            if(info.onUpdate) info.onUpdate();
            glfwSwapBuffers(context);
        }
    }
};

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
		std::function<void()> onUpdate;
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

Shader vertex(GL_VERTEX_SHADER, IDR_RCDATA1);
Shader fragment(GL_FRAGMENT_SHADER, IDR_RCDATA2);
ShaderProgram shaderProgram({
	.Shaders = { vertex, fragment },
    .onBuild = []()
    {
        playerposloc = glGetUniformLocation(shaderProgram.SELF, "playerpos");
        playerrotloc = glGetUniformLocation(shaderProgram.SELF, "playerrot");
		aspectratioloc = glGetUniformLocation(shaderProgram.SELF, "aspectratio");
	},
    .onInvoke = []()
    {
		glUniform2f(playerposloc, (float)playerposraw[0], (float)playerposraw[1]);
		glUniform2f(playerrotloc, (float)playerrotraw[0], (float)playerrotraw[1]);
		glUniform1f(aspectratioloc, aspectratio);
		glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    });

Shader computeShader(GL_COMPUTE_SHADER, IDR_RCDATA3);
ShaderProgram computeProgram({
    .Shaders = { computeShader },
    .onInvoke = []()
    {
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
	});

int main()
{
    try
    {
        Window::Info info;
		info.title = "My OpenGL Window";
        Window window(info);



        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 8, 8);
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

        std::cout << "Renderer" << std::endl;
        shaderProgram.Build();
		std::cout << "Compute Shader" << std::endl;
		computeProgram.Build();


        window.info.onUpdate = []()
        {
			computeProgram();
            shaderProgram();
        };

        window();
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
