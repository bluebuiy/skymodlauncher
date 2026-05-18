

#include "imgui.h"
#include "modmgr.h"
#include "prochelper.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#define GL_SILENCE_DEPRECATION

#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void printHelp()
{
    std::cout << "Skyim mod management for Linux\n";
    std::cout << "  -d path         specify the directory to use\n";
    std::cout << "  -c path         specify a specific configuration file to use\n";
    std::cout << "  -s              verbose\n";
    std::cout << "  -e exec         invoke a preconfigured executable in the injected filesystem\n";
    std::cout << "  -x exec         invoke arbitrary command line, inside the injected filesystem (not in a shell!)\n";
    std::cout << "  -r              do not unshare (because it's already been done). only affects -e, -x.\n";
    std::cout << std::endl;
}

// Main code
int main(int argc, char** argv)
{
    ModMgr config;

    std::string configPath;
    std::string initDir;
    std::string immediateExec;
    std::string shArgs;
    bool initDirFlag = false;
    bool initConfigFlag = false;
    bool verbose = true;
    bool execArgs = false;
    bool execProg = false;
    bool invokedRaw = true;

    int opt = 0;

    while ((opt = getopt(argc, argv, "d:c:p:e:x:s?hr")) != -1)
    {
        switch (opt)
        {
            case 'd':
            {
                initDir = optarg;
                initDirFlag = true;
                break;
            }
            case 'c':
            {
                configPath = optarg;
                initConfigFlag = true;
                break;
            }
            case 'e':
            {
                immediateExec = optarg;
                execProg = true;
                break;
            }
            case 's':
            {
                verbose = true;
                break;
            }
            case 'x':
            {
                shArgs = optarg;
                execArgs = true;
                break;
            }
            case '?':
            case 'h':
            {
                printHelp();
                exit(0);
                break;
            }
            case 'r':
            {
                invokedRaw = false;
                break;
            }
            default:
            {
                std::cout << "Invalid arguments" << std::endl;
                exit(1);
                break;
            }
        }
    }

    if (initDirFlag && initConfigFlag)
    {
        std::cout << "Use only one of -d or -c" << std::endl;
        return 1;
    }

    if (execProg && execArgs)
    {
        std::cout << "Use only one of -x or -e" << std::endl;
        return 1;
    }

    if (!initDirFlag && !initConfigFlag)
    {
        configPath = "./config.json";
        std::cout << "Initializing in current directory" << std::endl;
    }
    else if (initDirFlag)
    {
        configPath = std::filesystem::path(initDir) / "config.json";
    }
    else if (initConfigFlag)
    {
        // config path already set 
    }

    config.verbose = verbose;

    bool loadExisting = execArgs || execProg;

    if (!configPath.empty() && !LoadModMgr(config, configPath, !loadExisting))
    {
        std::cout << "Failed to load config" << std::endl;
        return 1;
    }

    std::cout << "--------------" << std::endl;
    if (execArgs)
    {
        if (!shArgs.empty())
        {
            std::cout << "Executing " << immediateExec << std::endl;

            if (invokedRaw)
            {
                std::string confPath;
                if (auto o = WordExpand(shellFix(config.config.configPath)))
                {
                    confPath = *o;
                }
                else
                {
                    std::cout << "Failed to resolve config path" << std::endl;
                    return 1;
                }
                std::vector<std::string> launchArgs = {"-c", confPath, "-x", shArgs};
                ExecToolProgram ex;
                ex.args = launchArgs;
                ForkInvoke(&ex);
            }
            else
            {
                auto subShArgs = ReplaceEnvVariables(config, shArgs, true);
                if (!subShArgs)
                {
                    std::cout << "Failed varibale substitution:\n" << shArgs << std::endl;
                    exit(1);
                }
                auto cmd = ShellSplit(*subShArgs);
                for (auto&& arg : cmd)
                {
                    auto exparg = WordExpand(shellFix(arg));
                    if (!exparg)
                    {
                        std::cout << "Failed to resolve string" << std::endl;
                        exit(1);
                    }
                    arg = *exparg;
                }
                std::cout << "args: " << cmd.size() << std::endl;
                InvokeProcess(config, cmd);
            }

            return 0;
        }
        else
        {
            std::cout << "No command! doing nothing." << std::endl;
            return 0;
        }
    }

    if (execProg)
    {
        if (invokedRaw)
        {
            std::string confPath;
            if (auto o = WordExpand(shellFix(config.config.configPath)))
            {
                confPath = *o;
            }
            else
            {
                std::cout << "Failed to resolve config path" << std::endl;
                return 1;
            }
            std::vector<std::string> launchArgs = {"-c", confPath, "-e", immediateExec};
            ExecToolProgram ex;
            ex.args = launchArgs;
            ForkInvoke(&ex);
        }
        else
        {
            // envornment already set up
            std::cout << "Executing tool " << immediateExec << std::endl;
            if (!immediateExec.empty())
            {
                InvokeTool(config, immediateExec);
                return 0;
            }
        }
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Skyrim Mod Launcher", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state
    ImVec4 clear_color = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
    bool focused = true;

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(50);
            continue;
        }
        else if (!focused)
        {
            ImGui_ImplGlfw_Sleep(50);
        }

        if (glfwGetWindowAttrib(window, GLFW_FOCUSED))
        {
            focused = true;
        }
        else
        {
            focused = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderModMgr(config);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    SaveModMgr(config);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}




