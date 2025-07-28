#include "../ChaiLove.h"
#include <GL/gl.h>

LRESULT CALLBACK Win32WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1: /* Open */ break;
                case 2: /* Save */ break;
                case 3: PostQuitMessage(0); break; // Exit
                case 4: MessageBox(hwnd, "About!", "Help", MB_OK); break;
                case 5: ChaiLove::getInstance()->chai_editor.toggleEditMode(); break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static bool fontBitmapsInitialized = false;
static GLuint fontBase = 0;

namespace love
{
chai_debug::chai_debug()
{
    
}

chai_debug::~chai_debug()
{
    // Destructor implementation
}

void chai_debug::init() {
    int width = 1920;
    int height = 1080;
    const char* title = "Standalone Win32 Window";
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const char* CLASS_NAME = "MyWin32WindowClass";

    // Register window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = Win32WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // --- Add menu ---
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, 1, "Open");
    AppendMenu(hFileMenu, MF_STRING, 2, "Save");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, 3, "Exit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");

    HMENU hTestMenu = CreatePopupMenu();
    AppendMenu(hTestMenu, MF_STRING, 5, "Test");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTestMenu, "Test");

    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenu(hHelpMenu, MF_STRING, 4, "About");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "Help");
    // ----------------

    // Create the window
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class
        title,                          // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL,       // Parent window    
        hMenu,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL) {
        MessageBox(NULL, "Failed to create Win32 window.", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    ShowWindow(hwnd, SW_SHOW);

    // Set up a simple OpenGL context (if needed)
    hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        MessageBox(NULL, "Failed to set pixel format.", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    hglrc = wglCreateContext(hdc);
    if (hglrc == NULL || !wglMakeCurrent(hdc, hglrc)) {
        MessageBox(NULL, "Failed to create OpenGL context.", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
}

void chai_debug::update(float dt, std::vector<chaiscript::Boxed_Value> viewMatrix) {
    
    wglMakeCurrent(hdc, hglrc);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ChaiLove* cl = ChaiLove::getInstance();
    cl->chai_collisions.processDebug(dt, viewMatrix); // Example update call, adjust as needed
    cl->chai_collisions.debugDraw();


    // Draw something simple, like a triangle
    // glBegin(GL_TRIANGLES);
    // glColor3f(1.0f, 0.0f, 0.0f); // Red
    // glVertex2f(-0.5f, -0.5f);
    // glColor3f(0.0f, 1.0f, 0.0f); // Green
    // glVertex2f(0.5f, -0.5f);
    // glColor3f(0.0f, 0.0f, 1.0f); // Blue
    // glVertex2f(0.0f, 0.5f);
    // glEnd();

    // Swap buffers
    SwapBuffers(hdc);    
}

void chai_debug::pushDebugMessagef(const char* fmt, ...) {
    
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    messages.push_back(std::string(buf));
    
}

void chai_debug::pushDebugMessage(const std::string &message) {
   
    messages.push_back(message);
    
}

void chai_debug::displayDebugMessages() {
    
    wglMakeCurrent(hdc, hglrc);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (!fontBitmapsInitialized) {
        fontBase = glGenLists(96);
        HFONT hFont = CreateFontA(
            -48, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, "Consolas"
        );
        SelectObject(hdc, hFont);
        wglUseFontBitmapsA(hdc, 32, 96, fontBase);
        fontBitmapsInitialized = true;
    }

    glClear(GL_COLOR_BUFFER_BIT);   
    glColor3f(1.0f, 1.0f, 1.0f); // White text
    float yOffset = 0.0f;
    for (const auto &msg : messages) {
        glRasterPos2f(-0.95f, 0.9f - yOffset);
        glListBase(fontBase - 32);
        glCallLists((GLsizei)msg.length(), GL_UNSIGNED_BYTE, msg.c_str());
        yOffset += 0.07f; // Move down for next line
    }
    SwapBuffers(hdc);  
    messages.clear();
    
}

void chai_debug::visualizeMatrix(const glm::mat4 &matrix) {
    this->matrix = matrix;
}

void chai_debug::displayMatrix() {
    // Draw a graph of the matrix
    // Visualize the basis vectors (columns) of the matrix as RGB axes

    wglMakeCurrent(hdc, hglrc);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw origin
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glEnd();

    // Draw X (red), Y (green), Z (blue) axes from the matrix
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    // X axis (red)
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(matrix[0][0], matrix[1][0]);
    // Y axis (green)
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(matrix[0][1], matrix[1][1]);
    // Z axis (blue)
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(matrix[0][2], matrix[1][2]);
    glEnd();

    // Optionally, draw the translation (origin) as a yellow point
    glPointSize(10.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 1.0f, 0.0f);
    glVertex2f(matrix[3][0], matrix[3][1]);
    glEnd();

    SwapBuffers(hdc);
}
}