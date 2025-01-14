#include "shader.h"
#include "mapreader.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

#include <fstream>
#include <string>

#ifdef _WIN32
std::string OpenLoadPrompt(const char* filter)
{
    OPENFILENAMEA ofn{ sizeof(OPENFILENAMEA) };
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Open File...";
    char buffer[MAX_PATH + 1]{ 0 };
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    if (GetOpenFileNameA(&ofn) == TRUE)
        return buffer;
    return "";
}
std::string OpenSavePrompt(const char* filter, const std::string& def = "")
{
    OPENFILENAMEA ofn{ sizeof(OPENFILENAMEA) };
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Save File...";
    char buffer[MAX_PATH + 1] = { 0 };
    memcpy(buffer, def.data(), def.length());
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    if (GetSaveFileNameA(&ofn) == TRUE)
        return buffer;
    return "";
}
#else
std::string OpenLoadPrompt(const char* filter) {}
std::string OpenSavePrompt(const char* filter) {}
#endif

glm::vec3 g_CamPos = { 0, 0, 0 };
glm::vec2 g_CamRot = { 0, 0 };

struct Vertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
};

glm::vec3 GetUpVector()
{
    return { 0, 1, 0 };
}

glm::vec3 GetForwardVector()
{
    glm::mat4 RotationMatrix = glm::rotate(glm::mat4(1.f), glm::radians(g_CamRot.x), GetUpVector());
    RotationMatrix = glm::rotate(RotationMatrix, glm::radians(g_CamRot.y), glm::vec3(-1, 0, 0));
    return glm::vec3{ RotationMatrix * glm::vec4(0, 0, 1, 1) };
}

glm::vec3 GetRightVector()
{
    return glm::cross(GetForwardVector(), GetUpVector());
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;

    g_CamPos += GetForwardVector() * (float)yoffset;
}

struct sleveldata_t
{
    level_t level;
    GLuint texid = 0;
    bool open = false;
};

static void mouse_callback(GLFWwindow* window, double x, double y)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_RELEASE)
    {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        double dX = x - w / 2.0, dY = y - h / 2.0;
        glfwSetCursorPos(window, w / 2.0, h / 2.0);

        g_CamRot += glm::vec2{glm::radians(-5 * dX), glm::radians(-5 * dY)};
        g_CamRot.y = glm::clamp<float>(g_CamRot.y, -75.f, 75.f);
    }
}

bool wireframe = false;
bool texturesVis = true;
bool vertexCols = true;
bool noObjects = false;
bool enableBillboarding = true;

void SetWireframe(bool state)
{
    wireframe = state;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    texturesVis = vertexCols = !wireframe;
}

static void mousebtn_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        glfwSetCursorPos(window, w / 2.0, h / 2.0);
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;

    if (key == GLFW_KEY_X && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL))
    {
        SetWireframe(!wireframe);
    }
}

GLFWwindow* g_Window = nullptr;

bool cameraInvalidated = true;

glm::mat4 camera(const glm::mat4& model)
{
    static glm::mat4 Projection;
    static glm::mat4 View;
    if (cameraInvalidated)
    {
        int w, h;
        glfwGetWindowSize(g_Window, &w, &h);
        Projection = glm::perspective(glm::pi<float>() * 0.25f, w / (float)h, 0.1f, 1000.f);
        View = glm::lookAt(g_CamPos, g_CamPos + GetForwardVector() * 10.f, GetUpVector());
        cameraInvalidated = false;
    }
    
    return Projection * View * model;
}

bool IsBillboardObject(const std::string& name)
{
    if (name.empty())
        return false;

    if (name[0] == '$')
        return true;

    const char* BillboardObjectNames[] = {
        "nflame__",
        "mflame__",

        "charger_",
        "steam___",

        "cold____",

        "proxsig_",
        "@Path",

        /// COLLECTIBLES
        // Aztec 2 Step
        "gem_____",

        // I Got the Reruns
        "coltv___",

        // Trouble in Uranus
        "saucer__",

        // In Drag Net
        "badge___",

        // The Spy Who Loved Himself
        "case____",

        // Circuit Central
        "batt____", // Chips and Dips
        "led_____",
        "atom____",

        // Scream TV
        "skull___", // Thursday the 12th
        "tomb____",
        "jason___",

        // Kung-Fu Theatre
        "takeout_", // Lizard in a China Shop
        "yinyang_",
        "kabuki__",

        // Toon TV
        "carrot__",
        "spinach_",
        "plunge__",

        // Prehistory Channel
        "drum____",
        "cowhead_",
        "dino____",

        // Space Channel
        "ship____",
        "phaser__",
        "robot___"

        // Mazed and Confused (Rezop 1)
        "cd______",
        "radiate_", // Bugged Out
        "camera__",

        // No Weddings and a Funeral (Rezop 3)
        "gear____",
        "toolbox_",
        "oilcan__",
    };
    const char** pENDPTR = BillboardObjectNames + sizeof(BillboardObjectNames) / sizeof(const char*);

    return std::find_if(BillboardObjectNames, pENDPTR,
        [&name](const char* objName) {
            return name == objName;
        }) != pENDPTR;
}

struct globj_t
{
    GLuint vbo = 0;
    bool hasNoTexture = false;
    std::vector<Vertex> vertices;
    void draw(GLuint program, sleveldata_t& leveldata, objinstance_t& inst, const std::string& name)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(glm::vec3));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3) + sizeof(glm::vec4)));
        glEnableVertexAttribArray(2);

        glm::mat4 Model = glm::translate(glm::mat4(1.f), -inst.position);
        bool doBillboarding = enableBillboarding && IsBillboardObject(name);
        if (doBillboarding)
        {
            Model = glm::rotate(Model, glm::radians(g_CamRot.x-180), {0, 1, 0});
        }
        else
        {
            //Model = glm::rotate(Model, -inst.rotation.x, { 1, 0, 0 });
            Model = glm::rotate(Model, -inst.rotation.y, { 0, 1, 0 });
            //Model = glm::rotate(Model, -inst.rotation.z, { 0, 0, 1 });
        }
        glm::mat4 cam = camera(Model);
        glUniformMatrix4fv(glGetUniformLocation(program, "uCamera"), 1, false, glm::value_ptr(cam));
        glUniform1i(glGetUniformLocation(program, "uWireframe"), (wireframe << 0) | (vertexCols << 1) | ((texturesVis && !hasNoTexture) << 2));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, leveldata.texid);
        glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
        // extra hack for billboarding transparency
        // todo: hopefully remove with fixed textures
        glUniform1i(glGetUniformLocation(program, "uBillboard"), (int)doBillboarding);
        glUseProgram(program);

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
    }
};
std::vector<std::shared_ptr<globj_t>> mdls;

void CloseLevel(sleveldata_t& leveldata)
{
    glBindTexture(GL_TEXTURE_2D, 0);
    if (leveldata.texid != 0)
        glDeleteTextures(1, &leveldata.texid);
    leveldata.texid = 0;
    for (auto& tex : leveldata.level.textures)
        if (tex.deletePixels)
            delete[] tex.pixels;
    leveldata.level.textures.clear();
    leveldata.level.models.clear();
    leveldata.level.paths.clear();
    leveldata.open = false;
    mdls.clear();
}

std::shared_ptr<globj_t> createobj(std::shared_ptr<Model> model)
{
    auto ptr = std::make_shared<globj_t>();

    for (auto& p : model->polygons)
    {
        for (int i = 0; i < 3; ++i)
        {
            auto& v = model->vertices[p.vertex[i]];
            ptr->vertices.push_back({ {v.x / 1000.f, v.y / 1000.f, v.z / 1000.f}, {v.r / 255.f, v.g / 255.f, v.b / 255.f, v.a / 255.f}, p.uvs[i] });
            if (p.materialID == 0xFFFF'FFFF)
                if (model->hasNoTextures)
                {
                    auto& c = ptr->vertices.rbegin()->color;
                    c.r /= 2;
                    c.g /= 2;
                    c.b /= 2;
                }
                else
                    ptr->vertices.rbegin()->color.a = 0.f;
        }
    }

    ptr->hasNoTexture = model->hasNoTextures;

    glGenBuffers(1, &ptr->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ptr->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * ptr->vertices.size(), ptr->vertices.data(), GL_STATIC_DRAW);

    return ptr;
}

ImVec4 bgColor = { 0xBB / 255.f, 0xF6 / 255.f, 0xF7 / 255.f, 255 };

std::string levelPath, levelName;
bool OpenLevel(const char* levelPath, sleveldata_t& leveldata)
{
    CloseLevel(leveldata);
    printf("Loading level \"%s\"\n", levelPath);
    if (!LoadLevel(levelPath, leveldata.level))
    {
        ::levelPath = levelName = "";
        return false;
    }
    ::levelPath = levelPath;
    size_t fsi = ::levelPath.find_last_of("/");
    size_t bsi = ::levelPath.find_last_of("\\");
    if (fsi != std::string::npos || bsi != std::string::npos)
    {
        size_t pos = 0;
        if (fsi != std::string::npos && bsi != std::string::npos)
        {
            pos = fsi < bsi ? bsi : fsi;
        }
        else
        {
            if (fsi != std::string::npos)
                pos = fsi;
            else
                pos = bsi;
        }

        ::levelPath = ::levelPath.substr(pos + 1);
    }
    levelName = leveldata.level.name;
    leveldata.open = true;
    bgColor.x = leveldata.level.bgColor[0];
    bgColor.y = leveldata.level.bgColor[1];
    bgColor.z = leveldata.level.bgColor[2];

    for (auto& m : leveldata.level.models)
        mdls.push_back(createobj(m));

    glGenTextures(1, &leveldata.texid);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, leveldata.texid);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, leveldata.level.sheet.w, leveldata.level.sheet.h, 0, GL_RGBA, GL_FLOAT, leveldata.level.sheet.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool str_ends_with_nocase(std::string src, std::string trg)
{
    if (src.length() <= trg.length())
        return false;

    for (auto& c : trg)
        c = tolower(c);

    src = src.substr(src.length() - trg.length());
    for (auto& c : src)
        c = tolower(c);

    for (size_t i = 0; i < src.length(); ++i)
        if (src[i] != trg[i])
            return false;

    return true;
}

bool ImGui_CenteredButton(const char* label)
{
    ImGuiStyle& style = ImGui::GetStyle();

    float size = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    float avail = ImGui::GetContentRegionAvail().x;

    ImGui::Spacing();
    float off = (avail - size) * 0.5f;
    if (off > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
    return ImGui::Button(label);
};

void DumpObjects(FILE* f, sleveldata_t& leveldata);
void ExportModel(FILE* f, std::shared_ptr<Model> mdl);
void ExportTextureSheet(FILE* f, sleveldata_t& leveldata);

int main()
{
    glfwInit();
    g_Window = glfwCreateWindow(1024, 720, "Gex 2 Level Viewer", NULL, NULL);

    glfwMakeContextCurrent(g_Window);
    gladLoadGL();
    glfwSetScrollCallback(g_Window, scroll_callback);
    glfwSetCursorPosCallback(g_Window, mouse_callback);
    glfwSetMouseButtonCallback(g_Window, mousebtn_callback);
    glfwSetKeyCallback(g_Window, key_callback);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = NULL;
    ImGui::GetIO().LogFilename = NULL;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
    ImGui_ImplOpenGL3_Init();

    unsigned int program;
    if (!LoadShader(program, { "../data/shaders/basic.vert", "../data/shaders/basic.frag" }))
        if (!LoadShader(program, { "./data/shaders/basic.vert", "./data/shaders/basic.frag" }))
            return 1;

    sleveldata_t leveldata;

    std::vector<Vertex> vertices = {
        {{-10, -10, 50}, {0.5f, 0.5f, 0.5f, 0.5f}, {1, 1}},
        {{10, -10, 50}, {0.5f, 0.5f, 0.5f, 0.5f}, {0, 1}},
        {{10, 10, 50}, {0.5f, 0.5f, 0.5f, 0.5f}, {0, 0}},
        {{-10, -10, 50}, {0.5f, 0.5f, 0.5f, 0.5f}, {1, 1}},
        {{10, 10, 50}, {0.5f, 0.5f, 0.5f, 0.5f}, {0, 0}},
        {{-10, 10, 50}, {0.5f, 0.5f, 0.5f, 0.5f}, {1, 0}},
    };

    bool showTexturePanel = false;
    float textureZoomScale = 1.f;

    bool toggleObjectsMenu = false;

    while(!glfwWindowShouldClose(g_Window))
    {
        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(g_Window, &width, &height);
        glViewport(0, 0, width, height);

        if (wireframe)
            glClearColor(0, 0, 0, 1.f);
        else
            glClearColor(bgColor.x, bgColor.y, bgColor.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        // transparent textures bugged rn
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //glEnable(GL_BLEND);

        // todo: use state instead
        //if (glfwGetInputMode(g_Window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
        {
            if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
            {
                bool front = glfwGetKey(g_Window, GLFW_KEY_W) == GLFW_PRESS;
                bool back = glfwGetKey(g_Window, GLFW_KEY_S) == GLFW_PRESS;
                bool right = glfwGetKey(g_Window, GLFW_KEY_D) == GLFW_PRESS;
                bool left = glfwGetKey(g_Window, GLFW_KEY_A) == GLFW_PRESS;

                int hori = right - left;
                int forward = front - back;

                g_CamPos += ((GetForwardVector() * (float)forward) + (GetRightVector() * (float)hori)) * 0.1f;
            }
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        for (size_t i = 0; i < leveldata.level.models.size(); ++i)
        {
            if (leveldata.level.models[i]->objectVisibility)
                for (auto& inst : leveldata.level.models[i]->instances)
                {
                    if (inst.isVisible)
                        mdls[i]->draw(program, leveldata, inst, leveldata.level.models[i]->name);
                }
            if (noObjects)
                break;
        }

        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ 240, (float)height });
        if (ImGui::Begin("Camera", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::Text("Level: %s", levelName.c_str());
            ImGui::Text("File name: %s", levelPath.c_str());
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Camera:");
            ImGui::Text("  Position (%.0f, %.0f, %.0f)", g_CamPos.x * 1000.f, g_CamPos.y * 1000.f, g_CamPos.z * -1000.f);
            ImGui::Text("  Rotation (>: %.1f, ^: %.1f)", g_CamRot.x, g_CamRot.y);
            ImGui::Spacing(); 
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Stats:");
            ImGui::Text("  Polygons: %d", leveldata.level.models.empty() ? 0 : leveldata.level.models[0]->polygons.size());
            ImGui::Text("  Textures: %d", leveldata.level.textures.size() - 1);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Checkbox("Toggle Wireframe Mode?", &wireframe))
                SetWireframe(wireframe);
            ImGui::Checkbox("Toggle Vertex Color?", &vertexCols);
            ImGui::Checkbox("Toggle Textures?", &texturesVis);
            ImGui::Checkbox("Toggle Objects?", &noObjects);
            ImGui::Checkbox("Toggle Billboarding?", &enableBillboarding);
            if (ImGui_CenteredButton("Open Level (*.dfx)"))
            {
                auto path = OpenLoadPrompt("Gex 3D Level File (*.dfx)\0*.dfx\0All files (*.*)\0*.*\0");
                if (!path.empty())
                    OpenLevel(path.c_str(), leveldata);
            }

            if (ImGui_CenteredButton("Open Objects Panel"))
            {
                toggleObjectsMenu = true;
            }

            if (ImGui_CenteredButton("View Texture Atlas"))
            {
                showTexturePanel = true;
            }

            if (ImGui_CenteredButton("Reset Camera"))
            {
                g_CamPos = { 0, 0, 0 };
                g_CamRot = { 0, 0 };
            }

            ImGui::Spacing();
            ImGuiStyle& style = ImGui::GetStyle();

            ImGui::ColorPicker3("Skybox\nColor", &bgColor.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions);

            if (ImGui_CenteredButton("Export Level Geometry..."))
            {
                if (leveldata.open)
                {
                    auto path = OpenSavePrompt("Polygon File (*.ply)\0*.ply\0All files (*.*)\0*.*\0", levelPath + ".ply");
                    if (!path.empty())
                    {
                        if (!str_ends_with_nocase(path, ".ply"))
                            path += ".ply";
                        FILE* f = NULL;
                        fopen_s(&f, path.c_str(), "w");
                        if (f)
                        {
                            auto writeln = [f](const std::string& s)
                                {
                                    fwrite(s.data(), s.length(), 1, f);
                                };

                            struct PLYVertex
                            {
                                float x, y, z;
                                unsigned char r, g, b;
                                float u, v;
                            };

                            std::vector<PLYVertex> vertices;

                            std::shared_ptr<Model> models[] = { leveldata.level.models[0], leveldata.level.models[1] };

                            for (auto mdl : models)
                            {
                                const auto& v = mdl->vertices;
                                for (auto& p : mdl->polygons)
                                {
                                    if (mdl->addr == 0xFFFF'FFFF && (p.materialID == 0xFFFF'FFFF || p.flags & 0x80))
                                        continue;

                                    for (int i = 0; i < 3; ++i)
                                    {
                                        const auto& vert = v[p.vertex[i]];

                                        vertices.push_back({ vert.x / -1000.f, vert.z / 1000.f, vert.y / 1000.f, vert.r, vert.g, vert.b, p.uvs[i][0],  1.f - p.uvs[i][1] });
                                    }
                                }
                            }

                            writeln("ply\n");
                            writeln("format ascii 1.0\n");
                            writeln("comment Created from the Gex 3D Level Viewer\n");
                            writeln("element vertex " + std::to_string(vertices.size()));
                            writeln("\nproperty float x\n");
                            writeln("property float y\n");
                            writeln("property float z\n");
                            writeln("property uchar red\n");
                            writeln("property uchar green\n");
                            writeln("property uchar blue\n");
                            writeln("property float s\n");
                            writeln("property float t\n");
                            writeln("element face " + std::to_string(vertices.size() / 3));
                            writeln("\nproperty list uchar uint vertex_indices\n");
                            writeln("end_header\n");
                            char buffer[1024] = { 0 };

                            for (auto& v : vertices)
                            {
                                sprintf_s(buffer, "%f %f %f %d %d %d %f %f\n", v.x, v.y, v.z, v.r, v.g, v.b, v.u, v.v);
                                writeln(buffer);
                            }

                            for (unsigned int i = 0; i < vertices.size() / 3; ++i)
                            {
                                sprintf_s(buffer, "3 %d %d %d\n", 3 * i, 3 * i + 1, 3 * i + 2);
                                writeln(buffer);
                            }

                            fclose(f);
                        }

                    }
                }
            }

            {
                ImGui::Separator();
                static char offsetInput[32] = { 0 };
                static size_t offsetOutput = 0;

                {
                    float size = ImGui::CalcItemWidth() + style.FramePadding.x * 2.0f;
                    float avail = ImGui::GetWindowContentRegionMax().x;

                    ImGui::Spacing();
                    float off = (avail - size) * 0.5f;
                    if (off > 0.0f)
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
                }

                ImGui::InputText("##OffsetConvert", offsetInput, 31, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal);
                if (ImGui_CenteredButton("Convert to File Offset"))
                {
                    offsetOutput = 0;
                    size_t len = strnlen(offsetInput, 31);
                    for (size_t i = 0; i < len; ++i)
                    {
                        size_t n = 0;
                        if ('0' <= offsetInput[i] && offsetInput[i] <= '9')
                            n = offsetInput[i] - '0';
                        else if ('a' <= offsetInput[i] && offsetInput[i] <= 'f')
                            n = (offsetInput[i] - 'a') + 10;
                        else if ('A' <= offsetInput[i] && offsetInput[i] <= 'F')
                            n = (offsetInput[i] - 'A') + 10;

                        offsetOutput += (n << (4 * ((len - 1) - i)));

                    }

                    offsetOutput += leveldata.level.baseData;
                }
                if (ImGui_CenteredButton("Convert from File Offset"))
                {
                    offsetOutput = 0;
                    size_t len = strnlen(offsetInput, 31);
                    for (size_t i = 0; i < len; ++i)
                    {
                        size_t n = 0;
                        if ('0' <= offsetInput[i] && offsetInput[i] <= '9')
                            n = offsetInput[i] - '0';
                        else if ('a' <= offsetInput[i] && offsetInput[i] <= 'f')
                            n = (offsetInput[i] - 'a') + 10;
                        else if ('A' <= offsetInput[i] && offsetInput[i] <= 'F')
                            n = (offsetInput[i] - 'A') + 10;

                        offsetOutput += (n << (4 * ((len - 1) - i)));

                    }

                    offsetOutput -= leveldata.level.baseData;
                }
                ImGui::Text("Calculated Offset: 0x%.6X", offsetOutput);
            }

            const char* msg = "Shoutout to the /r/Gex Discord";
            float sizey = ImGui::CalcTextSize(msg).y + style.FramePadding.y * 2.0f;
            float availy = ImGui::GetContentRegionAvail().y;
            float sizex = ImGui::CalcTextSize(msg).x + style.FramePadding.x * 2.0f;
            float availx = ImGui::GetContentRegionAvail().x;

            ImGui::Spacing();
            float offy = availy - sizey;
            float offx = (availx - sizex) / 2.f;
            if (offy > 0.0f)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offy);
            if (offx > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offx);
            ImGui::Text(msg);

            ImGui::End();
        }

        if (showTexturePanel)
        {
            ImGui::SetNextWindowSize({ 512, 512 }, ImGuiCond_Appearing);
            ImGui::SetNextWindowPos({ ImGui::GetWindowWidth() / 2.f, ImGui::GetWindowHeight() / 2.f }, ImGuiCond_Appearing);
            if (ImGui::Begin("Texture Atlas", &showTexturePanel, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
            {
                ImGui::Text("Atlas Size: %lux%lu", leveldata.level.sheet.w, leveldata.level.sheet.h);
                ImGui::SameLine();
                if (ImGui::Button("Export Texture Atlas..."))
                {
                    auto path = OpenSavePrompt("TARGA File (*.TGA)\0*.TGA\0All files (*.*)\0*.*\0", levelPath + ".tga");
                    if (!path.empty())
                    {
                        if (!str_ends_with_nocase(path, ".tga"))
                            path += ".tga";

                        FILE* f = NULL;
                        fopen_s(&f, path.c_str(), "wb");
                        if (f)
                            ExportTextureSheet(f, leveldata);

                    }
                }
                ImGui::SliderFloat("Texture Zoom", &textureZoomScale, 1.f, 8.f, "%.0f");
                ImGuiStyle& style = ImGui::GetStyle();
                float ratio = 1.f;
                if (ImGui::GetContentRegionAvail().x < ImGui::GetContentRegionAvail().y)
                {
                    ratio = (ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.f) / leveldata.level.sheet.w;
                }
                else
                {
                    ratio = (ImGui::GetContentRegionAvail().y - style.FramePadding.y * 2.f) / leveldata.level.sheet.h;
                }
                auto [cx, cy] = ImGui::GetCursorPos();
                ImGui::Image((ImTextureID)leveldata.texid, { ratio * leveldata.level.sheet.w * textureZoomScale, (float)leveldata.level.sheet.h * ratio * textureZoomScale });
                ImGui::End();
            }
        }

        if (toggleObjectsMenu)
        {
            ImGui::SetNextWindowSize({ 512, 512 }, ImGuiCond_Appearing);
            ImGui::SetNextWindowPos({ ImGui::GetWindowWidth() / 2.f, ImGui::GetWindowHeight() / 2.f }, ImGuiCond_Appearing);
            if (ImGui::Begin("Objects", &toggleObjectsMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar))
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
                if (ImGui::Button("Show All"))
                {
                    for (auto& mdl : leveldata.level.models)
                        mdl->objectVisibility = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Hide All"))
                {
                    for (auto& mdl : leveldata.level.models)
                        mdl->objectVisibility = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Dump Object Info"))
                {
                    auto path = OpenSavePrompt("JSON File (*.json)\0*.json\0All files (*.*)\0*.*\0", levelPath + ".json");
                    if (!path.empty())
                    {
                        if (!str_ends_with_nocase(path, ".json"))
                            path += ".json";
                        FILE* f = NULL;
                        fopen_s(&f, path.c_str(), "w");
                        if (f)
                            DumpObjects(f, leveldata);
                    }
                }
                static char filterTextBuffer[32] = "!!";
                static std::string filterText = "!!";
                auto tolower = [](std::string s)
                    {
                        for (auto& c : s)
                            c = ::tolower(c);
                        return s;
                    };
                ImGui::Text("Object Filter");
                ImGui::SameLine();
                if (ImGui::InputText("##Object Filter", filterTextBuffer, 16, ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    filterText = filterTextBuffer;
                    filterText = tolower(filterText);
                }

                for (auto& mdl : leveldata.level.models)
                {
                    if (!filterText.empty() && filterText != "!")
                    {
                        if (filterText[0] == '!')
                        {
                            if (filterText[1] == '!')
                            {
                                if (std::string("@$").find(mdl->name[0]) != std::string::npos)
                                    continue;
                            }
                            else if (tolower(mdl->name).find(&filterText[1]) != std::string::npos)
                                continue;
                        }
                        else if (tolower(mdl->name).find(filterText) == std::string::npos)
                            continue;
                    }
                    {
                        bool colorSet = !mdl->name.empty() && mdl->name[0] == '$' || mdl->name[0] == '@';
                        if (colorSet)
                        {
                            if (mdl->name[0] == '$')
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(237, 190, 19, 255));
                            else
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(212, 46, 34, 255));
                        }
                        ImGui::Text(mdl->name.c_str());
                        if (colorSet)
                            ImGui::PopStyleColor();
                    }

                    ImGui::BeginGroup();
                    ImGui::Indent(8.f);
                    ImGui::Checkbox(("Visible?##" + std::to_string(mdl->addr)).c_str(), &mdl->objectVisibility);
                    ImGui::Text("Instances: %u", mdl->instances.size());
                    ImGui::Checkbox(("Show Instances List##" + std::to_string(mdl->addr)).c_str(), &mdl->showInstances);
                    if (ImGui::Button(("Show All##" + std::to_string(mdl->addr)).c_str()))
                    {
                        for (auto& inst : mdl->instances)
                            inst.isVisible = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(("Hide All##" + std::to_string(mdl->addr)).c_str()))
                    {
                        for (auto& inst : mdl->instances)
                            inst.isVisible = false;
                    }

                    if (ImGui::Button(("Export Model...##" + std::to_string(mdl->addr)).c_str()))
                    {
                        if (leveldata.open)
                        {
                            auto path = OpenSavePrompt("Polygon File (*.ply)\0*.ply\0All files (*.*)\0*.*\0", levelPath + ".ply");
                            if (!path.empty())
                            {
                                if (!str_ends_with_nocase(path, ".ply"))
                                    path += ".ply";

                                FILE* f = NULL;
                                fopen_s(&f, path.c_str(), "w");
                                if (f)
                                    ExportModel(f, mdl);

                            }
                        }
                    }
                    if (mdl->showInstances)
                    {
                        ImGui::Indent(8.f);
                        int __i = 0;
                        for (auto& inst : mdl->instances)
                        {
                            ImGui::Text("Pos: (%.0f, %.0f, %.0f) Rot: (%.0f, %.0f, %.0f)", -inst.position.x * 1000.f, -inst.position.y * 1000.f, inst.position.z * 1000.f, inst.rotation.x * 180 / glm::pi<float>(), inst.rotation.y * 180 / glm::pi<float>(), inst.rotation.z * 180 / glm::pi<float>());
                            if (inst.address != 0)
                            {
                                ImGui::Text("Address: 0x%.6x", inst.address);
                            }
                            ImGui::Text("Instance Data:\n");
                            for (int i = 0; i < 16; ++i)
                            {
                                ImGui::Text("%.2x", inst.instanceData[i / 4] >> (8 * (i % 4)) & 0xFF);
                                if (i == 3 || i == 7 || i == 11)
                                {
                                    ImGui::SameLine();
                                    ImGui::Text(" ");
                                }
                                if (i < 15)
                                    ImGui::SameLine();
                            }
                            ImGui::Checkbox(("Visible?##" + std::to_string(mdl->addr) + "_" + std::to_string(__i++)).c_str(), &inst.isVisible);
                            ImGui::SameLine();
                            if (ImGui::Button(("Teleport To...##" + std::to_string(mdl->addr) + "_" + std::to_string(__i++)).c_str()))
                            {
                                g_CamPos = -inst.position;
                            }

                            ImGui::Separator();
                        }
                        ImGui::Unindent(8.f);
                    }
                    ImGui::Unindent(8.f);
                    ImGui::EndGroup();

                    ImGui::Separator();
                }
                ImGui::End();
            }
        }

        ImGui::Render();
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g_Window);
        cameraInvalidated = true;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g_Window);
    glfwTerminate();

    return 0;
}

void DumpObjects(FILE* f, sleveldata_t& leveldata)
{
    fwrite("{\n", 2, 1, f);
    char buffer[1024];
    sprintf_s(buffer, "\t\"raw_objects\": {\n");
    fwrite(buffer, strnlen(buffer, 1024), 1, f);
    for (size_t i = 0; i < leveldata.level.models.size(); ++i)
    {
        auto& mdl = leveldata.level.models[i];
        std::string objName = mdl->name.c_str();
        if (objName.empty())
        {
            if (mdl->addr == 0xFFFF'FFFF)
                objName = "Level";
            else
                objName = "???";
        }
        sprintf_s(buffer, "\t\t\"%s\": [\n", objName.c_str());
        fwrite(buffer, strnlen(buffer, 1024), 1, f);
        for (size_t j = 0; j < mdl->instances.size(); ++j)
        {
            auto& inst = mdl->instances[j];
            sprintf_s(buffer, "\t\t\t{ \"pos\": [%d, %d, %d], \"rot\": [%.6f, %.6f, %.6f] }",
                (int)(inst.position.x * -1000),
                (int)(inst.position.y * -1000),
                (int)(inst.position.z * 1000),
                (inst.rotation.x),
                (inst.rotation.y),
                (inst.rotation.z)
            );
            fwrite(buffer, strnlen(buffer, 1024), 1, f);
            if ((j + 1) < mdl->instances.size())
                fwrite(",\n", 2, 1, f);
        }
        fwrite("\n\t\t]", 4, 1, f);
        if ((i + 1) < leveldata.level.models.size())
            fwrite(",\n", 2, 1, f);
    }
    fwrite("\n\t},\n", 4, 1, f);
    fwrite("\"paths\": [", 10, 1, f);
    std::string pstr;
    for (auto& path : leveldata.level.paths)
    {
        pstr += "\n\t\t{\n\t\t\t\"owner_instance\": " + std::to_string(path.owner);
        pstr += ",\n\t\t\t\"points\": [";
        for (auto& pt : path.points)
        {
            sprintf_s(buffer, "\n\t\t\t\t{\"speed\":%d, \"pos\": [%d, %d, %d]},", pt.speed, pt.x, pt.z, pt.y);
            pstr += buffer;
        }
        if (pstr.rbegin()[0] == ',')
            pstr.pop_back();
        pstr += "\n\t\t\t],\n\t\t\t\"rotations\":[";
        for (auto& pt : path.rotations)
        {
            sprintf_s(buffer, "\n\t\t\t\t{\"speed\":%d, \"rot\": [%f, %f, %f, %f]},", pt.speed, pt.rotX, pt.rotY, pt.rotZ, pt.rotW);
            pstr += buffer;
        }
        if (pstr.rbegin()[0] == ',')
            pstr.pop_back();
        pstr += "\n\t\t\t]\n\t\t},";
    }
    if (pstr.rbegin()[0] == ',')
        pstr.pop_back();
    fwrite(pstr.data(), pstr.length(), 1, f);
    fwrite("\n\t],\n", 5, 1, f);
    fwrite("\t\"pickups\": [", 13, 1, f);

    for (int i = 0; i < 3; ++i)
    {
        const char* const p = leveldata.level.pickupName[i];
        if (strncmp(p, "gameboy_", 8) == 0 || strncmp(p, "remgold_", 8) == 0)
            continue; // not real collectibles

        std::string s = "\n\t\t{\n\t\t\t\"type\": \"" + std::string(p) + "\", \"instances\": [";
        if (auto it = std::find_if(
            leveldata.level.models.begin(),
            leveldata.level.models.end(),
            [p](std::shared_ptr<Model> m)
            {
                return m->name == p;
            });
            it != leveldata.level.models.end()
                )
        {
            for (auto& inst : (*it)->instances)
            {
                sprintf_s(buffer, "\n\t\t\t\t[%d, %d, %d],",
                    (int)(inst.position.x * -1000),
                    (int)(inst.position.y * -1000),
                    (int)(inst.position.z * 1000));
                s += buffer;
            }
            if ((*it)->instances.size() > 0)
                s.pop_back();
        }
        s += "\n\t\t\t]\n\t\t},";
        fwrite(s.data(), s.length(), 1, f);
    }

    {
        std::string s = "\n\t\t{\n\t\t\t\"type\": \"cold____\", \"instances\": [";
        if (auto it = std::find_if(
            leveldata.level.models.begin(),
            leveldata.level.models.end(),
            [](std::shared_ptr<Model> m)
            {
                return m->name == "cold____";
            });
            it != leveldata.level.models.end()
                )
        {
            for (auto& inst : (*it)->instances)
            {
                sprintf_s(buffer, "\n\t\t\t\t[%d, %d, %d],",
                    (int)(inst.position.x * -1000),
                    (int)(inst.position.y * -1000),
                    (int)(inst.position.z * 1000));
                s += buffer;
            }
            if ((*it)->instances.size() > 0)
                s.pop_back();
        }
        s += "\n\t\t\t]\n\t\t}";
        fwrite(s.data(), s.length(), 1, f);
    }

    fwrite("\n\t]", 3, 1, f);
    fwrite("\n}", 2, 1, f);
    fclose(f);
}

void ExportModel(FILE* f, std::shared_ptr<Model> mdl)
{
    auto writeln = [f](const std::string& s)
        {
            fwrite(s.data(), s.length(), 1, f);
        };

    struct PLYVertex
    {
        float x, y, z;
        unsigned char r, g, b;
        float u, v;
    };

    std::vector<PLYVertex> vertices;

    const auto& v = mdl->vertices;
    for (auto& p : mdl->polygons)
    {
        if (mdl->addr == 0xFFFF'FFFF && (p.materialID == 0xFFFF'FFFF || p.flags & 0x80))
            continue;

        for (int i = 0; i < 3; ++i)
        {
            const auto& vert = v[p.vertex[i]];

            vertices.push_back({ vert.x / -1000.f, vert.z / 1000.f, vert.y / 1000.f, vert.r, vert.g, vert.b, p.uvs[i][0],  1.f - p.uvs[i][1] });
        }
    }

    writeln("ply\n");
    writeln("format ascii 1.0\n");
    writeln("comment Created from the Gex 3D Level Viewer\n");
    writeln("element vertex " + std::to_string(vertices.size()));
    writeln("\nproperty float x\n");
    writeln("property float y\n");
    writeln("property float z\n");
    writeln("property uchar red\n");
    writeln("property uchar green\n");
    writeln("property uchar blue\n");
    writeln("property float s\n");
    writeln("property float t\n");
    writeln("element face " + std::to_string(vertices.size() / 3));
    writeln("\nproperty list uchar uint vertex_indices\n");
    writeln("end_header\n");
    char buffer[1024] = { 0 };

    for (auto& v : vertices)
    {
        sprintf_s(buffer, "%f %f %f %d %d %d %f %f\n", v.x, v.y, v.z, v.r, v.g, v.b, v.u, v.v);
        writeln(buffer);
    }

    for (unsigned int i = 0; i < vertices.size() / 3; ++i)
    {
        sprintf_s(buffer, "3 %d %d %d\n", 3 * i, 3 * i + 1, 3 * i + 2);
        writeln(buffer);
    }

    fclose(f);
}

void ExportTextureSheet(FILE* f, sleveldata_t& leveldata)
{
    unsigned char tga[18];
    memset(tga, 0, 18);
    tga[2] = 2;
    tga[12] = leveldata.level.sheet.w & 0xFF;
    tga[13] = (leveldata.level.sheet.w >> 8) & 0xFF;
    tga[14] = leveldata.level.sheet.h & 0xFF;
    tga[15] = (leveldata.level.sheet.h >> 8) & 0xFF;
    tga[16] = 32;
    tga[17] = 32;

    fwrite(&tga, sizeof(tga), 1, f);
    struct _pixel { byte a, r, g, b; } pixel;
    for (unsigned int y = 0; y < leveldata.level.sheet.h; ++y)
        for (unsigned int x = 0; x < leveldata.level.sheet.w; ++x)
        {
            pixel = {
                (byte)(leveldata.level.sheet.pixels[y * leveldata.level.sheet.w + x][2] * 255),
                (byte)(leveldata.level.sheet.pixels[y * leveldata.level.sheet.w + x][1] * 255),
                (byte)(leveldata.level.sheet.pixels[y * leveldata.level.sheet.w + x][0] * 255),
                (byte)(leveldata.level.sheet.pixels[y * leveldata.level.sheet.w + x][3] * 255),
            };
            fwrite(&pixel, sizeof(_pixel), 1, f);
        }

    fclose(f);
}
