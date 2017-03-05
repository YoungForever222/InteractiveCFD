#include <GLEW/glew.h>
#include <GLUT/freeglut.h>

#include <stdio.h>
#include <iostream>
#include <ostream>
#include <fstream>
#include <time.h>
#include <algorithm>

#include "main.h"

const int g_leftPanelWidth(350);
const int g_leftPanelHeight(500);


class Command
{
public:
    Command();

    void Start();
    void Track();
    void End();
};


class Window
{
    static Panel* m_windowPanel;
    static Panel* m_currentPanel;
    static int m_previousMouseX;
    static int m_previousMouseY;
    static int m_currentMouseButton;
public:
    Window();
    Window(const int width, const int height);

    Panel* GetWindowPanel();
    static float GetFloatCoordX(const int x);
    static float GetFloatCoordY(const int y);
    void InitializeGL();
    static void timerEvent(int value);
    static void Resize(const int width, const int height);

    static void MouseButton(const int button, const int state,
        const int x, const int y);

    static void MouseMotion(const int x, const int y);
    static void Keyboard(const unsigned char key,
        const int /*x*/, const int /*y*/);
    static void MouseWheel(const int button, const int direction,
        const int x, const int y);

    static void DrawLoop();
    void InitializeGLUT(int argc, char **argv);
    void Display();
};

Panel* Window::m_currentPanel = NULL;
int Window::m_previousMouseX = 0;
int Window::m_previousMouseY = 0;
int Window::m_currentMouseButton = 0;
Panel* Window::m_windowPanel = new Panel;

Window::Window()
{
}
Window::Window(const int width, const int height)
{
}


Panel* Window::GetWindowPanel()
{
    return m_windowPanel;
}

float Window::GetFloatCoordX(const int x)
{
    int width = m_windowPanel->GetWidth();
    return static_cast<float>(x)/width*2.f - 1.f;
}
float Window::GetFloatCoordY(const int y)
{
    int height = m_windowPanel->GetHeight();
    return static_cast<float>(y)/height*2.f - 1.f;
}

void Window::InitializeGL()
{
    glEnable(GL_LIGHT0);
    glewInit();
    glViewport(0,0,800,600);
}

void Window::timerEvent(int value)
{
    if (glutGetWindow())
    {
        glutPostRedisplay();
        glutTimerFunc(REFRESH_DELAY, timerEvent, 0);
    }
}

void Window::Resize(const int width, const int height)
{
    float scaleUp = m_windowPanel->GetPanel("Graphics")->GetGraphicsManager()->GetScaleFactor();
    int windowWidth = m_windowPanel->GetWidth();
    int windowHeight = m_windowPanel->GetHeight();
    UpdateDomainDimensionsBasedOnWindowSize(g_leftPanelHeight, g_leftPanelWidth,
        windowWidth, windowHeight, scaleUp);

    //theMouse.m_winW = windowWidth;
    //theMouse.m_winH = windowHeight;

    RectInt rect = { 200, 100, width, height };
    m_windowPanel->SetSize_Absolute(rect);
    rect = { 0, windowHeight - g_leftPanelHeight, g_leftPanelWidth, g_leftPanelHeight };
    m_windowPanel->GetPanel("CDV")->SetSize_Absolute(rect);
    rect = { g_leftPanelWidth, 0, windowWidth - g_leftPanelWidth, windowHeight };
    m_windowPanel->GetPanel("Graphics")->SetSize_Absolute(rect);
    m_windowPanel->UpdateAll();

    glViewport(0, 0, windowWidth, windowHeight);

    //UpdateDeviceImage();

}
void Window::MouseButton(const int button, const int state,
    const int x, const int y)
{
    int windowHeight = m_windowPanel->GetHeight();
    float xf = GetFloatCoordX(x);
    float yf = GetFloatCoordY(windowHeight-y);
    Window::m_currentPanel = GetPanelThatPointIsIn(m_windowPanel, xf, yf);
    std::cout << Window::m_currentPanel->GetName();

    Window::m_currentPanel->ClickDown();
    m_currentMouseButton = button;
    m_previousMouseX = x;
    m_previousMouseY = y;
}
void Window::MouseMotion(const int x, const int y)
{
    float dx = GetFloatCoordX(x) - GetFloatCoordX(m_previousMouseX);
    float dy = GetFloatCoordY(y) - GetFloatCoordY(m_previousMouseY);

    if (m_currentPanel != NULL)
    {
        m_currentPanel->Drag(x, y, dx, -dy, m_currentMouseButton);
    }
    m_previousMouseX = x;
    m_previousMouseY = y;
}
void Window::Keyboard(const unsigned char key,
    const int /*x*/, const int /*y*/)
{
    
}
void Window::MouseWheel(const int button, const int direction,
    const int x, const int y)
{

}

void Window::DrawLoop()
{
    Resize(m_windowPanel->GetWidth(), m_windowPanel->GetHeight());
    glOrtho(-1, 1, -1, 1, -100, 20);
    m_windowPanel->DrawAll();

    glutSwapBuffers();
}

void Window::InitializeGLUT(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB|GLUT_DEPTH|GLUT_DOUBLE);
    int width = m_windowPanel->GetWidth();
    int height = m_windowPanel->GetHeight();
    glutInitWindowSize(width,height);
    glutInitWindowPosition(50,30);

    glutCreateWindow("New Window management");

    glutReshapeFunc(&Window::Resize);
    glutMouseFunc(&Window::MouseButton);
    glutMotionFunc(&Window::MouseMotion);
    glutKeyboardFunc(&Window::Keyboard);
    glutMouseWheelFunc(&Window::MouseWheel);

    glutDisplayFunc(&Window::DrawLoop);
    glutTimerFunc(REFRESH_DELAY, &Window::timerEvent, 0);

}

void Window::Display()
{
    glutMainLoop();
}




int main(int argc, char **argv)
{
    int initialWindowWidth = 1200;
    int initialWindowHeight = g_leftPanelHeight+100;
    Window window(initialWindowWidth,initialWindowHeight);
    Panel* windowPanel = window.GetWindowPanel();

    SetUpWindow(*windowPanel);

    window.InitializeGLUT(argc, argv);
    window.InitializeGL();

    GraphicsManager* graphicsManager = window.GetWindowPanel()
        ->GetPanel("Graphics")->GetGraphicsManager();
    Graphics* graphics = graphicsManager->GetGraphics();
    SetUpGLInterop(*windowPanel);
    SetUpCUDA(*windowPanel);

    window.Display();


    return 0;
}