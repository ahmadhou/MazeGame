#define GLEW_STATIC
#define STB_IMAGE_IMPLEMENTATION

#include <GL/glew.h>
#include "stb_image.h"
#include <GLFW/glfw3.h> 
#include <glm/glm.hpp> 
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtc/type_ptr.hpp> 
#include <vector> 
#include <string> 
#include <iostream> 
#include <array> 
#include <filesystem> 
#ifdef _WIN32 
#include <windows.h> 
#include <wincodec.h> 
#pragma comment(lib, "Windowscodecs.lib") 
#endif 
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;
using namespace glm;
static fs::path getExeDir()
{
#ifdef _WIN32 
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    return fs::path(buf).remove_filename();
#else 
    return fs::current_path();
#endif 
}
static std::string resolvePath(const char* p)
{    fs::path req(p); if (fs::exists(req))return req.string();
    fs::path cwd = fs::current_path();
    fs::path exe = getExeDir();
    const std::array<fs::path, 4> roots = { cwd, cwd / "x64" / "Debug", cwd / "Project6", exe };
    for (auto& r : roots)
    {        if (r.empty())continue; fs::path cand = r / req;
        if (fs::exists(cand))return cand.string();    }
    return std::string(p);}
static std::string readTextFile(const char* path)
{ std::string resolved = resolvePath(path);std::ifstream file(resolved);
        if (!file.is_open())
    {        std::cout << "Failed to open shader file: " << resolved << std::endl; return ""; }
        std::stringstream buffer;buffer << file.rdbuf();return buffer.str();}
#ifdef _WIN32 
static bool loadImageWithWIC(const std::string& filename, int& w, int& h, int& ch, std::vector<unsigned char>& pixels)
{    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool com = SUCCEEDED(hr) && hr != RPC_E_CHANGED_MODE;
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) || !factory)
    {        if (com)            CoUninitialize();        return false;    }
    std::wstring wpath = fs::path(filename).wstring();    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)) || !decoder)
    {        factory->Release(); if (com)    CoUninitialize();   return false;    }
    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame)
    { decoder->Release();factory->Release(); if (com)CoUninitialize();return false;  }
    UINT width = 0, height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0)
    { frame->Release(); decoder->Release();factory->Release();if (com)CoUninitialize();return false; }
    IWICFormatConverter* converter = nullptr;
    if (FAILED(factory->CreateFormatConverter(&converter)) || !converter)
    { frame->Release();decoder->Release();factory->Release();if (com)CoUninitialize();  return false; }
    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {converter->Release();frame->Release();decoder->Release();factory->Release();if (com)CoUninitialize();return false;}
    pixels.resize(width * height * 4);
    if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data())))
    {pixels.clear();converter->Release();frame->Release();decoder->Release();factory->Release();if (com)CoUninitialize();return false;}
    w = static_cast<int>(width);h = static_cast<int>(height);ch = 4;
    converter->Release();frame->Release();decoder->Release();factory->Release();if (com)CoUninitialize();return true;}
#else 
static bool loadImageWithWIC(const std::string&, int&, int&, int&, std::vector<unsigned char>&) { return false; }
#endif 
struct AABB
{    vec3 min, max;};
static bool intersects(const AABB& a, const AABB& b)
{return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&(a.min.y <= b.max.y && a.max.y >= b.min.y) &&(a.min.z <= b.max.z && a.max.z >= b.min.z);}
class Shader
{public:
    unsigned int ID = 0;Shader() = default;
    Shader(const char* vertexPath, const char* fragmentPath)
    {   std::string vertexCode = readTextFile(vertexPath);std::string fragmentCode = readTextFile(fragmentPath);
        const char* vs = vertexCode.c_str();const char* fs = fragmentCode.c_str();
        unsigned int v = compile(GL_VERTEX_SHADER, vs);unsigned int f = compile(GL_FRAGMENT_SHADER, fs);
                ID = glCreateProgram();glAttachShader(ID, v);glAttachShader(ID, f);glLinkProgram(ID);
                int success;
                glGetProgramiv(ID, GL_LINK_STATUS, &success);

                if (!success)
                {
                    char log[1024];
                    glGetProgramInfoLog(ID, 1024, NULL, log);
                    std::cout << "Program link error:\n" << log << std::endl;
                }
                glDeleteShader(v);glDeleteShader(f);   }
    void use() const { glUseProgram(ID); }
    void setMat4(const char* n, const mat4& v) const { glUniformMatrix4fv(glGetUniformLocation(ID, n), 1, GL_FALSE, value_ptr(v)); }
    void setVec3(const char* n, const vec3& v) const { glUniform3f(glGetUniformLocation(ID, n), v.x, v.y, v.z); }
    void setInt(const char* n, int value) const { glUniform1i(glGetUniformLocation(ID, n), value); }
    private:
    unsigned int compile(unsigned int t, const char* s)
    { unsigned int sh = glCreateShader(t); glShaderSource(sh, 1, &s, NULL); glCompileShader(sh);
    int success;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
        if (!success)    {        char log[1024];        glGetShaderInfoLog(sh, 1024, NULL, log);        std::cout << "Shader compile error:\n" << log << std::endl;    }    return sh;  }};
struct Camera
{vec3 pos{ 0.5f, 1.0f, 0.5f };float yaw = -90.0f, pitch = 0.0f;    vec3 front() const
    {return normalize(vec3(cos(radians(yaw)) * cos(radians(pitch)), sin(radians(pitch)), sin(radians(yaw)) * cos(radians(pitch))));}    vec3 right() const { return normalize(cross(front(), vec3(0, 1, 0))); }mat4 view() const { return lookAt(pos, pos + front(), vec3(0, 1, 0)); }};
static const std::array<std::pair<vec3, vec3>, 8> WallDefs = {
    std::pair<vec3, vec3>{{4.5f, 1.0f, -0.5f}, {5.0f, 1.0f, 0.5f}}, {{4.5f, 1.0f, 9.5f}, {5.0f, 1.0f, 0.5f}}, {{-0.5f, 1.0f, 4.5f}, {0.5f, 1.0f, 5.0f}}, {{9.5f, 1.0f, 4.5f}, {0.5f, 1.0f, 5.0f}}, {{2.0f, 1.0f, 2.0f}, {0.5f, 1.0f, 2.0f}}, {{5.0f, 1.0f, 6.0f}, {0.5f, 1.0f, 2.5f}}, {{7.5f, 1.0f, 3.5f}, {2.0f, 1.0f, 0.5f}}, {{3.5f, 1.0f, 7.5f}, {2.0f, 1.0f, 0.5f}} };
static std::vector<AABB> walls;static std::vector<vec3> collectibles = { {1.0f, 0.5f, 1.0f}, {3.0f, 0.5f, 5.0f}, {6.0f, 0.5f, 2.0f}, {8.0f, 0.5f, 7.0f} };
static std::vector<bool> collected(4, false);Camera cam;float lastX = 640, lastY = 360, elapsed = 0;bool firstMouse = true, won = false, lost = false;int score = 0;
unsigned int loadTexture(const char* p)
{    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int w, h, ch;    stbi_set_flip_vertically_on_load(true);std::string path = resolvePath(p);std::vector<unsigned char> fallback;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data && loadImageWithWIC(path, w, h, ch, fallback))    { data = fallback.data(); }
    if (data)    {GLenum f = (ch == 4) ? GL_RGBA : GL_RGB;glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);if (fallback.empty())stbi_image_free(data);}
else{unsigned char white[] = { 255, 255, 255, 255 };glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);}
    return tex;}
void processInput(GLFWwindow* window, float dt)
{    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)glfwSetWindowShouldClose(window, true);vec3 m(0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) m += cam.front();
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) m -= cam.front();
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) m -= cam.right();
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) m += cam.right(); m.y = 0;
    if (length(m) > 0.001f)
    {        vec3 next = cam.pos + normalize(m) * (3.0f * dt);
        auto check = [&](vec3 p)
            {AABB b = { p - vec3(0.15, 0.8, 0.15), p + vec3(0.15, 0.2, 0.15) };
                for (auto& w : walls)if (intersects(b, w))return true;  return false; };
        if (!check(vec3(next.x, cam.pos.y, cam.pos.z))) cam.pos.x = next.x;
        if (!check(vec3(cam.pos.x, cam.pos.y, next.z))) cam.pos.z = next.z;    }}
void mouse_callback(GLFWwindow*, double x, double y)
{    if (firstMouse)
   {        lastX = (float)x;   lastY = (float)y;        firstMouse = false;    }
    cam.yaw += ((float)x - lastX) * 0.1f;cam.pitch += (lastY - (float)y) * 0.1f;cam.pitch = clamp(cam.pitch, -89.0f, 89.0f);
    lastX = (float)x; lastY = (float)y;}
int main()
{    glfwInit();GLFWwindow* win = glfwCreateWindow(1280, 720, "MazeGame", NULL, NULL);
    glfwMakeContextCurrent(win);glewInit();glEnable(GL_DEPTH_TEST);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);glfwSetCursorPosCallback(win, mouse_callback);
        for (auto& d : WallDefs)walls.push_back({ d.first - d.second, d.first + d.second });
        Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl"); shader.use();shader.setInt("tex", 0);
        unsigned int fT = loadTexture("assets/floor.jpg"),wT = loadTexture("assets/wall.jpg");
        static const float v[] = { -0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0, 0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0, 0.5f, 0.5f, -0.5f, 0, 0, -1, 1, 1,
        0.5f, 0.5f, -0.5f, 0, 0, -1, 1, 1, -0.5f, 0.5f, -0.5f, 0, 0, -1, 0, 1, -0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0,
        -0.5f, -0.5f, 0.5f, 0, 0, 1, 0, 0, 0.5f, -0.5f, 0.5f, 0, 0, 1, 1, 0, 0.5f, 0.5f, 0.5f, 0, 0, 1, 1, 1,
        0.5f, 0.5f, 0.5f, 0, 0, 1, 1, 1, -0.5f, 0.5f, 0.5f, 0, 0, 1, 0, 1, -0.5f, -0.5f, 0.5f, 0, 0, 1, 0, 0,
        -0.5f, 0.5f, 0.5f, -1, 0, 0, 1, 0, -0.5f, 0.5f, -0.5f, -1, 0, 0, 1, 1, -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 1,
        -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 1, -0.5f, -0.5f, 0.5f, -1, 0, 0, 0, 0, -0.5f, 0.5f, 0.5f, -1, 0, 0, 1, 0,
        0.5f, 0.5f, 0.5f, 1, 0, 0, 1, 0, 0.5f, 0.5f, -0.5f, 1, 0, 0, 1, 1, 0.5f, -0.5f, -0.5f, 1, 0, 0, 0, 1,
        0.5f, -0.5f, -0.5f, 1, 0, 0, 0, 1, 0.5f, -0.5f, 0.5f, 1, 0, 0, 0, 0, 0.5f, 0.5f, 0.5f, 1, 0, 0, 1, 0,
        -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 1, 0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 1, 0.5f, -0.5f, 0.5f, 0, -1, 0, 1, 0,
        0.5f, -0.5f, 0.5f, 0, -1, 0, 1, 0, -0.5f, -0.5f, 0.5f, 0, -1, 0, 0, 0, -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 1,
        -0.5f, 0.5f, -0.5f, 0, 1, 0, 0, 1, 0.5f, 0.5f, -0.5f, 0, 1, 0, 1, 1, 0.5f, 0.5f, 0.5f, 0, 1, 0, 1, 0,
        0.5f, 0.5f, 0.5f, 0, 1, 0, 1, 0, -0.5f, 0.5f, 0.5f, 0, 1, 0, 0, 0, -0.5f, 0.5f, -0.5f, 0, 1, 0, 0, 1 };
            unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);glGenBuffers(1, &vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * 4, (void*)0);
    glEnableVertexAttribArray(0);glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(6 * 4));
    glEnableVertexAttribArray(2);
    float lT = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win))
    {float now = (float)glfwGetTime();float dt = now - lT;lT = now;processInput(win, dt);
    if (!won && !lost)
    {        AABB pB = { cam.pos - vec3(0.15, 0.8, 0.15), cam.pos + vec3(0.15, 0.2, 0.15) };
                for (int i = 0; i < 4; i++)
            if (!collected[i] && intersects(pB, { collectibles[i] - 0.2f, collectibles[i] + 0.2f }))
            {      collected[i] = true;     score++; }
                if (score == 4 && intersects(pB, { vec3(8.5, 1, 8.5) - 0.6f, vec3(8.5, 1, 8.5) + 0.6f }))
        {    won = true;       glfwSetWindowShouldClose(win, true);    }
                if ((elapsed += dt) > 90.0f) lost = true; }
    std::string title ="CGL Drone Project | Score " + std::to_string(score) + "/4 | Time " + std::to_string((int)elapsed);
        if (won)       title += " | WIN";
        if (lost)  title += " | LOSE";
            glfwSetWindowTitle(win, title.c_str());
        int w, h; glfwGetFramebufferSize(win, &w, &h);
        shader.setMat4("v", cam.view());shader.setMat4("pj", perspective(radians(60.0f), (float)w / (h ? h : 1), 0.1f, 100.0f));
        shader.setVec3("lPos", vec3(5, 5, 5));glBindVertexArray(vao);glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fT);shader.setVec3("tint", vec3(0.4f));
        mat4 m = translate(mat4(1), vec3(4.5f, -0.01f, 4.5f)) * scale(mat4(1), vec3(10, 0.02f, 10));
        shader.setMat4("m", m); glDrawArrays(GL_TRIANGLES, 0, 36);
        m = translate(mat4(1), vec3(4.5f, 2.01f, 4.5f)) * scale(mat4(1), vec3(10, 0.02f, 10));
        shader.setMat4("m", m);glDrawArrays(GL_TRIANGLES, 0, 36);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D, wT);shader.setVec3("tint", vec3(1));
        for (auto& wl : walls)
        { m = translate(mat4(1), (wl.min + wl.max) * 0.5f) * scale(mat4(1), wl.max - wl.min);shader.setMat4("m", m);glDrawArrays(GL_TRIANGLES, 0, 36); }
        for (int i = 0; i < 4; i++)
            if (!collected[i])
            { shader.setVec3("tint", vec3(1, 0.8, 0));m = translate(mat4(1), collectibles[i]) * scale(mat4(1), vec3(0.25f));
                shader.setMat4("m", m);glDrawArrays(GL_TRIANGLES, 0, 36); }
        shader.setVec3("tint", vec3(0, 1, 0.3));m = translate(mat4(1), vec3(8.5, 1, 8.5)) * scale(mat4(1), vec3(0.4f));
        shader.setMat4("m", m);glDrawArrays(GL_TRIANGLES, 0, 36);glfwSwapBuffers(win);glfwPollEvents();  } glfwTerminate();
    return 0;
}