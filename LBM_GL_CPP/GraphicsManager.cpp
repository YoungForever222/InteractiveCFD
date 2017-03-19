#include "GraphicsManager.h"
#include "Panel.h"
#include "Layout.h"
#include "kernel.h"
#include "Mouse.h"
#include "Domain.h"
#include "cuda_runtime.h"
#include "cuda_gl_interop.h"
#include "helper_cuda.h"
#include "helper_cuda_gl.h"
#include <glm/gtc/type_ptr.hpp>
#include <GLEW/glew.h>
#include <algorithm>


CudaLbm::CudaLbm()
{
    m_domain = new Domain;
    m_isPaused = false;
    m_timeStepsPerFrame = TIMESTEPS_PER_FRAME / 2;
}

CudaLbm::CudaLbm(int maxX, int maxY)
{
    m_maxX = maxX;
    m_maxY = maxY;
}

Domain* CudaLbm::GetDomain()
{
    return m_domain;
}

float* CudaLbm::GetFA()
{
    return m_fA_d;
}

float* CudaLbm::GetFB()
{
    return m_fB_d;
}

int* CudaLbm::GetImage()
{
    return m_Im_d;
}

float* CudaLbm::GetFloorTemp()
{
    return m_FloorTemp_d;
}

Obstruction* CudaLbm::GetDeviceObst()
{
    return m_obst_d;
}

Obstruction* CudaLbm::GetHostObst()
{
    return &m_obst_h[0];
}

float CudaLbm::GetInletVelocity()
{
    return m_inletVelocity;
}

float CudaLbm::GetOmega()
{
    return m_omega;
}

void CudaLbm::SetInletVelocity(float velocity)
{
    m_inletVelocity = velocity;
}

void CudaLbm::SetOmega(float omega)
{
    m_omega = omega;
}

void CudaLbm::SetPausedState(bool isPaused)
{
    m_isPaused = isPaused;
}

bool CudaLbm::IsPaused()
{
    return m_isPaused;
}

int CudaLbm::GetTimeStepsPerFrame()
{
    return m_timeStepsPerFrame;
}

void CudaLbm::SetTimeStepsPerFrame(const int timeSteps)
{
    m_timeStepsPerFrame = timeSteps;
}



void CudaLbm::AllocateDeviceMemory()
{
    size_t memsize_lbm, memsize_int, memsize_float, memsize_inputs;

    int domainSize = ceil(MAX_XDIM / BLOCKSIZEX)*BLOCKSIZEX*ceil(MAX_YDIM / BLOCKSIZEY)*BLOCKSIZEY;
    memsize_lbm = domainSize*sizeof(float)*9;
    memsize_int = domainSize*sizeof(int);
    memsize_float = domainSize*sizeof(float);
    memsize_inputs = sizeof(m_obst_h);

    float* fA_h = new float[domainSize * 9];
    float* fB_h = new float[domainSize * 9];
    float* floor_h = new float[domainSize];
    int* im_h = new int[domainSize];

    cudaMalloc((void **)&m_fA_d, memsize_lbm);
    cudaMalloc((void **)&m_fB_d, memsize_lbm);
    cudaMalloc((void **)&m_FloorTemp_d, memsize_float);
    cudaMalloc((void **)&m_Im_d, memsize_int);
    cudaMalloc((void **)&m_obst_d, memsize_inputs);
}

void CudaLbm::DeallocateDeviceMemory()
{
    cudaFree(m_fA_d);
    cudaFree(m_fB_d);
    cudaFree(m_Im_d);
    cudaFree(m_FloorTemp_d);
    cudaFree(m_obst_d);
}

void CudaLbm::InitializeDeviceMemory()
{
    int domainSize = ceil(MAX_XDIM / BLOCKSIZEX)*BLOCKSIZEX*ceil(MAX_YDIM / BLOCKSIZEY)*BLOCKSIZEY;
    size_t memsize_lbm, memsize_float, memsize_inputs;
    memsize_lbm = domainSize*sizeof(float)*9;
    memsize_float = domainSize*sizeof(float);

    float* f_h = new float[domainSize*9];
    for (int i = 0; i < domainSize * 9; i++)
    {
        f_h[i] = 0;
    }
    cudaMemcpy(m_fA_d, f_h, memsize_lbm, cudaMemcpyHostToDevice);
    cudaMemcpy(m_fB_d, f_h, memsize_lbm, cudaMemcpyHostToDevice);
    delete[] f_h;
    float* floor_h = new float[domainSize];
    for (int i = 0; i < domainSize; i++)
    {
        floor_h[i] = 0;
    }
    cudaMemcpy(m_FloorTemp_d, floor_h, memsize_float, cudaMemcpyHostToDevice);
    delete[] floor_h;

    UpdateDeviceImage();

    for (int i = 0; i < MAXOBSTS; i++)
    {
        m_obst_h[i].r1 = 0;
        m_obst_h[i].x = 0;
        m_obst_h[i].y = -1000;
        m_obst_h[i].state = Obstruction::REMOVED;
    }	
    m_obst_h[0].r1 = 6.5;
    m_obst_h[0].x = 30;// g_xDim*0.2f;
    m_obst_h[0].y = 42;// g_yDim*0.3f;
    m_obst_h[0].u = 0;// g_yDim*0.3f;
    m_obst_h[0].v = 0;// g_yDim*0.3f;
    m_obst_h[0].shape = Obstruction::SQUARE;
    m_obst_h[0].state = Obstruction::NEW;

    m_obst_h[1].r1 = 4.5;
    m_obst_h[1].x = 30;// g_xDim*0.2f;
    m_obst_h[1].y = 100;// g_yDim*0.3f;
    m_obst_h[1].u = 0;// g_yDim*0.3f;
    m_obst_h[1].v = 0;// g_yDim*0.3f;
    m_obst_h[1].shape = Obstruction::VERTICAL_LINE;
    m_obst_h[1].state = Obstruction::NEW;

    memsize_inputs = sizeof(m_obst_h);
    cudaMemcpy(m_obst_d, m_obst_h, memsize_inputs, cudaMemcpyHostToDevice);
}

void CudaLbm::UpdateDeviceImage()
{
    int domainSize = ceil(MAX_XDIM / BLOCKSIZEX)*BLOCKSIZEX*ceil(MAX_YDIM / BLOCKSIZEY)*BLOCKSIZEY;
    int* im_h = new int[domainSize];
    for (int i = 0; i < domainSize; i++)
    {
        int x = i%MAX_XDIM;
        int y = i/MAX_XDIM;
        im_h[i] = ImageFcn(x, y);
    }
    size_t memsize_int = domainSize*sizeof(int);
    cudaMemcpy(m_Im_d, im_h, memsize_int, cudaMemcpyHostToDevice);
    delete[] im_h;
}

int CudaLbm::ImageFcn(const int x, const int y){
    int xDim = GetDomain()->GetXDim();
    int yDim = GetDomain()->GetYDim();
    if (x < 0.1f)
        return 3;//west
    else if ((xDim - x) < 1.1f)
        return 2;//east
    else if ((yDim - y) < 1.1f)
        return 11;//11;//xsymmetry top
    else if (y < 0.1f)
        return 12;//12;//xsymmetry bottom
    return 0;
}

Graphics::Graphics()
{
    m_shaderProgram = new ShaderProgram;
    m_computeProgram = new ShaderProgram;
}

void Graphics::CreateCudaLbm()
{
    m_cudaLbm = new CudaLbm;
}

CudaLbm* Graphics::GetCudaLbm()
{
    return m_cudaLbm;
}

cudaGraphicsResource* Graphics::GetCudaSolutionGraphicsResource()
{
    return m_cudaGraphicsResource;
}

void Graphics::CreateVboForCudaInterop(unsigned int size)
{
    cudaGLSetGLDevice(gpuGetMaxGflopsDeviceId());
    CreateVbo(size, cudaGraphicsMapFlagsWriteDiscard);
    CreateElementArrayBuffer();
}

void Graphics::CleanUpGLInterOp()
{
    DeleteVbo();
    DeleteElementArrayBuffer();
}

void Graphics::CreateVbo(unsigned int size, unsigned int vboResFlags)
{
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    cudaGraphicsGLRegisterBuffer(&m_cudaGraphicsResource, m_vbo, vboResFlags);
}

void Graphics::DeleteVbo()
{
    cudaGraphicsUnregisterResource(m_cudaGraphicsResource);
    glBindBuffer(1, m_vbo);
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
    m_vbo = 0;
    m_vao = 0;
}

void Graphics::CreateElementArrayBuffer()
{
    const int numberOfElements = (MAX_XDIM - 1)*(MAX_YDIM - 1);
    const int numberOfNodes = MAX_XDIM*MAX_YDIM;
    GLuint* elementIndices = new GLuint[numberOfElements * 4 * 2];
    for (int j = 0; j < MAX_YDIM-1; j++){
        for (int i = 0; i < MAX_XDIM-1; i++){
            //going clockwise, since y orientation will be flipped when rendered
            elementIndices[j*(MAX_XDIM-1)*4+i*4+0] = (i)+(j)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*4+i*4+1] = (i+1)+(j)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*4+i*4+2] = (i+1)+(j+1)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*4+i*4+3] = (i)+(j+1)*MAX_XDIM;
        }
    }
    for (int j = 0; j < MAX_YDIM-1; j++){
        for (int i = 0; i < MAX_XDIM-1; i++){
            //going clockwise, since y orientation will be flipped when rendered
            elementIndices[numberOfElements*4+j*(MAX_XDIM-1)*4+i*4+0] = numberOfNodes+(i)+(j)*MAX_XDIM;
            elementIndices[numberOfElements*4+j*(MAX_XDIM-1)*4+i*4+1] = numberOfNodes+(i+1)+(j)*MAX_XDIM;
            elementIndices[numberOfElements*4+j*(MAX_XDIM-1)*4+i*4+2] = numberOfNodes+(i+1)+(j+1)*MAX_XDIM;
            elementIndices[numberOfElements*4+j*(MAX_XDIM-1)*4+i*4+3] = numberOfNodes+(i)+(j+1)*MAX_XDIM;
        }
    }
    glGenBuffers(1, &m_elementArrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementArrayBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*numberOfElements*4*2, elementIndices, GL_DYNAMIC_DRAW);
    free(elementIndices);
}

void Graphics::DeleteElementArrayBuffer(){
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &m_elementArrayBuffer);
}

void Graphics::CreateShaderStorageBuffer(const unsigned int sizeInInts, const std::string name)
{
    GLuint temp;
    glGenBuffers(1, &temp);
    GLint* data = new GLint[sizeInInts];
    for (int i = 0; i < sizeInInts; i++)
    {
        data[i] = 0;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeInInts*sizeof(GLint), data, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, temp);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_ssbos.push_back(Ssbo{ temp, name });
}

GLuint Graphics::GetShaderStorageBuffer(const std::string name)
{
    for (std::vector<Ssbo>::iterator it = m_ssbos.begin(); it != m_ssbos.end(); ++it)
    {
        if (it->m_name == name)
        {
            return it->m_id;
        }
    }
}

GLuint Graphics::GetElementArrayBuffer()
{
    return m_elementArrayBuffer;
}

GLuint Graphics::GetVbo()
{
    return m_vbo;
}

ShaderProgram* Graphics::GetShaderProgram()
{
    return m_shaderProgram;
}

ShaderProgram* Graphics::GetComputeProgram()
{
    return m_computeProgram;
}

void Graphics::CompileShaders()
{
    GetShaderProgram()->Initialize();
    GetShaderProgram()->CreateShader("VertexShader.glsl", GL_VERTEX_SHADER);
    GetShaderProgram()->CreateShader("FragmentShader.glsl", GL_FRAGMENT_SHADER);
    GetComputeProgram()->Initialize();
    GetComputeProgram()->CreateShader("ComputeShader.glsl", GL_COMPUTE_SHADER);
}

void Graphics::AllocateStorageBuffers()
{
    CreateShaderStorageBuffer(MAX_XDIM*MAX_YDIM, "Floor");
}

void Graphics::RenderVbo(bool renderFloor, Domain &domain, glm::mat4 modelMatrix, glm::mat4 projectionMatrix)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBindVertexArray(m_vao);
    //Draw solution field
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementArrayBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 16, (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableClientState(GL_VERTEX_ARRAY);

    int yDimVisible = domain.GetYDimVisible();
    if (renderFloor)
    {
        //Draw floor
        glDrawElements(GL_QUADS, (MAX_XDIM - 1)*(yDimVisible - 1)*4, GL_UNSIGNED_INT, 
            BUFFER_OFFSET(sizeof(GLuint)*4*(MAX_XDIM - 1)*(MAX_YDIM - 1)));
    }
    //Draw water surface
    glDrawElements(GL_QUADS, (MAX_XDIM - 1)*(yDimVisible - 1)*4 , GL_UNSIGNED_INT, (GLvoid*)0);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindVertexArray(0);
}

void Graphics::RunComputeShader(const float3 cameraPosition)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_vbo);
    const GLuint ssbo_floor = GetShaderStorageBuffer("Floor");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_floor);
    ShaderProgram* const shader = GetComputeProgram();

    shader->Use();

    const GLint maxXDimLocation = glGetUniformLocation(shader->GetId(), "maxXDim");
    const GLint maxYDimLocation = glGetUniformLocation(shader->GetId(), "maxYDim");
    const GLint xDimVisibleLocation = glGetUniformLocation(shader->GetId(), "xDimVisible");
    const GLint yDimVisibleLocation = glGetUniformLocation(shader->GetId(), "yDimVisible");
    const GLint cameraPositionLocation = glGetUniformLocation(shader->GetId(), "cameraPosition");
    glUniform1i(maxXDimLocation, MAX_XDIM);
    glUniform1i(maxYDimLocation, MAX_YDIM);
    Domain domain = *m_cudaLbm->GetDomain();
    glUniform1i(xDimVisibleLocation, domain.GetXDimVisible());
    glUniform1i(yDimVisibleLocation, domain.GetYDimVisible());
    glUniform3f(cameraPositionLocation, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    const GLuint phongLighting = glGetSubroutineIndex(shader->GetId(), GL_COMPUTE_SHADER,
        "PhongLighting");
    const GLuint collectLight = glGetSubroutineIndex(shader->GetId(), GL_COMPUTE_SHADER,
        "ComputeFloorLightIntensitiesFromMeshDeformation");
    const GLuint applyLights = glGetSubroutineIndex(shader->GetId(), GL_COMPUTE_SHADER,
        "ApplyCausticLightingToFloor");
    glUniformSubroutinesuiv(GL_COMPUTE_SHADER, 1, &phongLighting);

    glDispatchCompute(MAX_XDIM, MAX_YDIM, 2);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUniformSubroutinesuiv(GL_COMPUTE_SHADER, 1, &collectLight);
    glDispatchCompute(MAX_XDIM, MAX_YDIM, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUniformSubroutinesuiv(GL_COMPUTE_SHADER, 1, &applyLights);
    glDispatchCompute(MAX_XDIM, MAX_YDIM, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    shader->Unset();//

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Graphics::RenderVboUsingShaders(bool renderFloor, Domain &domain, glm::mat4 modelMatrix,
    glm::mat4 projectionMatrix)
{
    ShaderProgram* shader = GetShaderProgram();

    shader->Use();
    GLint modelMatrixLocation = glGetUniformLocation(shader->GetId(), "modelMatrix");
    GLint projectionMatrixLocation = glGetUniformLocation(shader->GetId(), "projectionMatrix");
    glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    RenderVbo(renderFloor, domain, modelMatrix, projectionMatrix);

    shader->Unset();
    glBindVertexArray(0);
}


GraphicsManager::GraphicsManager(Panel* panel)
{
    m_parent = panel;
    m_graphics = new Graphics;
    m_graphics->CreateCudaLbm();
    m_obstructions = m_graphics->GetCudaLbm()->GetHostObst();
    m_rotate = { 60.f, 0.f, 30.f };
    m_translate = { 0.f, 0.8f, 0.2f };
}

float3 GraphicsManager::GetRotationTransforms()
{
    return m_rotate;
}

float3 GraphicsManager::GetTranslationTransforms()
{
    return m_translate;
}

void GraphicsManager::SetCurrentObstSize(const float size)
{
    m_currentObstSize = size;
}

void GraphicsManager::SetCurrentObstShape(const Obstruction::Shape shape)
{
    m_currentObstShape = shape;
}

ViewMode GraphicsManager::GetViewMode()
{
    return m_viewMode;
}

void GraphicsManager::SetViewMode(const ViewMode viewMode)
{
    m_viewMode = viewMode;
}

float GraphicsManager::GetContourMinValue()
{
    return m_contourMinValue;
}

float GraphicsManager::GetContourMaxValue()
{
    return m_contourMaxValue;
}

void GraphicsManager::SetContourMinValue(const float contourMinValue)
{
    m_contourMinValue = contourMinValue;
}

void GraphicsManager::SetContourMaxValue(const float contourMaxValue)
{
    m_contourMaxValue = contourMaxValue;
}

ContourVariable GraphicsManager::GetContourVar()
{
    return m_contourVar;
}

void GraphicsManager::SetContourVar(const ContourVariable contourVar)
{
    m_contourVar = contourVar;
}


Obstruction::Shape GraphicsManager::GetCurrentObstShape()
{
    return m_currentObstShape;
}

void GraphicsManager::SetObstructionsPointer(Obstruction* obst)
{
    m_obstructions = m_graphics->GetCudaLbm()->GetHostObst();
}

float GraphicsManager::GetScaleFactor()
{
    return m_scaleFactor;
}

void GraphicsManager::SetScaleFactor(const float scaleFactor)
{
    m_scaleFactor = scaleFactor;
}

CudaLbm* GraphicsManager::GetCudaLbm()
{
    return m_graphics->GetCudaLbm();
}

Graphics* GraphicsManager::GetGraphics()
{
    return m_graphics;
}

void GraphicsManager::CenterGraphicsViewToGraphicsPanel(const int leftPanelWidth)
{
    Panel* rootPanel = m_parent->GetRootPanel();
    float scaleUp = rootPanel->GetSlider("Slider_Resolution")->m_sliderBar1->GetValue();
    SetScaleFactor(scaleUp);

    int windowWidth = rootPanel->GetWidth();
    int windowHeight = rootPanel->GetHeight();

    int xDimVisible = GetCudaLbm()->GetDomain()->GetXDimVisible();
    int yDimVisible = GetCudaLbm()->GetDomain()->GetYDimVisible();

    float xTranslation = -((static_cast<float>(windowWidth)-xDimVisible*scaleUp)*0.5
        - static_cast<float>(leftPanelWidth)) / windowWidth*2.f;
    float yTranslation = -((static_cast<float>(windowHeight)-yDimVisible*scaleUp)*0.5)
        / windowHeight*2.f;

    //get view transformations
    float3 cameraPosition = { m_translate.x, 
        m_translate.y, - m_translate.z };

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glTranslatef(xTranslation,yTranslation,0.f);
    glScalef((static_cast<float>(xDimVisible*scaleUp) / windowWidth),
        (static_cast<float>(yDimVisible*scaleUp) / windowHeight), 1.f);


    if (m_viewMode == ViewMode::TWO_DIMENSIONAL)
    {
        glOrtho(-1,1,-1,static_cast<float>(yDimVisible)/xDimVisible*2.f-1.f,-100,20);
    }
    else
    {
        gluPerspective(45.0, static_cast<float>(xDimVisible) / yDimVisible, 0.1, 10.0);
        glTranslatef(m_translate.x, m_translate.y, -2+m_translate.z);
        glRotatef(-m_rotate.x,1,0,0);
        glRotatef(m_rotate.z,0,0,1);
    }
}

void GraphicsManager::SetUpGLInterop()
{
    unsigned int solutionMemorySize = MAX_XDIM*MAX_YDIM * 4 * sizeof(float);
    unsigned int floorSize = MAX_XDIM*MAX_YDIM * 4 * sizeof(float);
    Graphics* graphics = GetGraphics();
    graphics->CreateVboForCudaInterop(solutionMemorySize+floorSize);
}

void GraphicsManager::SetUpShaders()
{
    Graphics* graphics = GetGraphics();
    graphics->CompileShaders();
    graphics->AllocateStorageBuffers();
}

void GraphicsManager::SetUpCuda()
{
    float4 rayCastIntersect{ 0, 0, 0, 1e6 };

    cudaMalloc((void **)&m_rayCastIntersect_d, sizeof(float4));
    cudaMemcpy(m_rayCastIntersect_d, &rayCastIntersect, sizeof(float4), cudaMemcpyHostToDevice);

    CudaLbm* cudaLbm = GetCudaLbm();
    cudaLbm->AllocateDeviceMemory();
    cudaLbm->InitializeDeviceMemory();

    float u = m_parent->GetRootPanel()->GetSlider("Slider_InletV")->m_sliderBar1->GetValue();

    float* fA_d = cudaLbm->GetFA();
    float* fB_d = cudaLbm->GetFB();
    int* im_d = cudaLbm->GetImage();
    float* floor_d = cudaLbm->GetFloorTemp();
    Obstruction* obst_d = cudaLbm->GetDeviceObst();

    Graphics* graphics = GetGraphics();
    cudaGraphicsResource* cudaSolutionField = graphics->GetCudaSolutionGraphicsResource();
    float4 *dptr;
    cudaGraphicsMapResources(1, &cudaSolutionField, 0);
    size_t num_bytes,num_bytes2;
    cudaGraphicsResourceGetMappedPointer((void **)&dptr, &num_bytes, cudaSolutionField);

    Domain* domain = cudaLbm->GetDomain();
    InitializeDomain(dptr, fA_d, im_d, u, *domain);
    InitializeDomain(dptr, fB_d, im_d, u, *domain);

    InitializeFloor(dptr, floor_d, *domain);

    cudaGraphicsUnmapResources(1, &cudaSolutionField, 0);
}

void GraphicsManager::RunCuda()
{
    // map OpenGL buffer object for writing from CUDA
    CudaLbm* cudaLbm = GetCudaLbm();
    Graphics* graphics = GetGraphics();
    cudaGraphicsResource* vbo_resource = graphics->GetCudaSolutionGraphicsResource();
    Panel* rootPanel = m_parent->GetRootPanel();

    float4 *dptr;

    cudaGraphicsMapResources(1, &vbo_resource, 0);
    size_t num_bytes;
    cudaGraphicsResourceGetMappedPointer((void **)&dptr, &num_bytes, vbo_resource);

    UpdateLbmInputs();
    float u = cudaLbm->GetInletVelocity();
    float omega = cudaLbm->GetOmega();

    float* floorTemp_d = cudaLbm->GetFloorTemp();
    Obstruction* obst_d = cudaLbm->GetDeviceObst();
    Obstruction* obst_h = cudaLbm->GetHostObst();

    Domain* domain = cudaLbm->GetDomain();
    MarchSolution(cudaLbm);
    UpdateSolutionVbo(dptr, cudaLbm, m_contourVar, m_contourMinValue, m_contourMaxValue, m_viewMode);
 
    SetObstructionVelocitiesToZero(obst_h, obst_d);
    float3 cameraPosition = { m_translate.x, m_translate.y, - m_translate.z };

//    if (ShouldRenderFloor())
//    {
//        LightSurface(dptr, obst_d, cameraPosition, *domain);
//    }
    LightFloor(dptr, floorTemp_d, obst_d, cameraPosition, *domain);
    CleanUpDeviceVBO(dptr, *domain);

    // unmap buffer object
    cudaGraphicsUnmapResources(1, &vbo_resource, 0);

}

void GraphicsManager::RunComputeShader()
{
    GetGraphics()->RunComputeShader(m_translate);
}

void GraphicsManager::RenderVbo()
{
    CudaLbm* cudaLbm = GetCudaLbm();
    GetGraphics()->RenderVbo(ShouldRenderFloor(), *cudaLbm->GetDomain(),
        GetModelMatrix(), GetProjectionMatrix());
}

void GraphicsManager::RenderVboUsingShaders()
{
    CudaLbm* cudaLbm = GetCudaLbm();
    GetGraphics()->RenderVboUsingShaders(ShouldRenderFloor(), *cudaLbm->GetDomain(),
        GetModelMatrix(), GetProjectionMatrix());
}

bool GraphicsManager::ShouldRenderFloor()
{
    if (m_viewMode == ViewMode::THREE_DIMENSIONAL || m_contourVar == ContourVariable::WATER_RENDERING)
    {
        return true;
    }
    return false;
}


void GraphicsManager::GetSimCoordFromMouseCoord(int &xOut, int &yOut, const int mouseX, const int mouseY)
{
    int xDimVisible = GetCudaLbm()->GetDomain()->GetXDimVisible();
    int yDimVisible = GetCudaLbm()->GetDomain()->GetYDimVisible();
    float xf = intCoordToFloatCoord(mouseX, m_parent->GetRootPanel()->GetWidth());
    float yf = intCoordToFloatCoord(mouseY, m_parent->GetRootPanel()->GetHeight());
    RectFloat coordsInRelFloat = RectFloat(xf, yf, 1.f, 1.f) / m_parent->GetRectFloatAbs();
    float graphicsToSimDomainScalingFactorX = static_cast<float>(xDimVisible) /
        std::min(static_cast<float>(m_parent->GetRectIntAbs().m_w), MAX_XDIM*m_scaleFactor);
    float graphicsToSimDomainScalingFactorY = static_cast<float>(yDimVisible) /
        std::min(static_cast<float>(m_parent->GetRectIntAbs().m_h), MAX_YDIM*m_scaleFactor);
    xOut = floatCoordToIntCoord(coordsInRelFloat.m_x, m_parent->GetRectIntAbs().m_w)*
        graphicsToSimDomainScalingFactorX;
    yOut = floatCoordToIntCoord(coordsInRelFloat.m_y, m_parent->GetRectIntAbs().m_h)*
        graphicsToSimDomainScalingFactorY;
}

void GraphicsManager::GetSimCoordFromFloatCoord(int &xOut, int &yOut, 
    const float xf, const float yf)
{
    int xDimVisible = GetCudaLbm()->GetDomain()->GetXDimVisible();
    int yDimVisible = GetCudaLbm()->GetDomain()->GetYDimVisible();    
    RectFloat coordsInRelFloat = RectFloat(xf, yf, 1.f, 1.f) / m_parent->GetRectFloatAbs();
    float graphicsToSimDomainScalingFactorX = static_cast<float>(xDimVisible) /
        std::min(static_cast<float>(m_parent->GetRectIntAbs().m_w), MAX_XDIM*m_scaleFactor);
    float graphicsToSimDomainScalingFactorY = static_cast<float>(yDimVisible) /
        std::min(static_cast<float>(m_parent->GetRectIntAbs().m_h), MAX_YDIM*m_scaleFactor);
    xOut = floatCoordToIntCoord(coordsInRelFloat.m_x, m_parent->GetRectIntAbs().m_w)*
        graphicsToSimDomainScalingFactorX;
    yOut = floatCoordToIntCoord(coordsInRelFloat.m_y, m_parent->GetRectIntAbs().m_h)*
        graphicsToSimDomainScalingFactorY;
}

void GraphicsManager::GetMouseRay(float3 &rayOrigin, float3 &rayDir,
    const int mouseX, const int mouseY)
{
    double x, y, z;
    gluUnProject(mouseX, mouseY, 0.0f, m_modelMatrix, m_projectionMatrix, m_viewport, &x, &y, &z);
    //printf("Origin: %f, %f, %f\n", x, y, z);
    rayOrigin.x = x;
    rayOrigin.y = y;
    rayOrigin.z = z;
    gluUnProject(mouseX, mouseY, 1.0f, m_modelMatrix, m_projectionMatrix, m_viewport, &x, &y, &z);
    rayDir.x = x-rayOrigin.x;
    rayDir.y = y-rayOrigin.y;
    rayDir.z = z-rayOrigin.z;
    float mag = sqrt(rayDir.x*rayDir.x + rayDir.y*rayDir.y + rayDir.z*rayDir.z);
    rayDir.x /= mag;
    rayDir.y /= mag;
    rayDir.z /= mag;
}

int GraphicsManager::GetSimCoordFrom3DMouseClickOnObstruction(int &xOut, int &yOut,
    const int mouseX, const int mouseY)
{
    int xDimVisible = GetCudaLbm()->GetDomain()->GetXDimVisible();
    int yDimVisible = GetCudaLbm()->GetDomain()->GetYDimVisible();
    float3 rayOrigin, rayDir;
    GetMouseRay(rayOrigin, rayDir, mouseX, mouseY);
    int returnVal = 0;

    // map OpenGL buffer object for writing from CUDA
    cudaGraphicsResource* cudaSolutionField = m_graphics->GetCudaSolutionGraphicsResource();
    float4 *dptr;
    cudaGraphicsMapResources(1, &cudaSolutionField, 0);
    size_t num_bytes;
    cudaGraphicsResourceGetMappedPointer((void **)&dptr, &num_bytes, cudaSolutionField);

    float3 selectedCoordF;
    Obstruction* obst_d = GetCudaLbm()->GetDeviceObst();
    Domain* domain = GetCudaLbm()->GetDomain();
    int rayCastResult = RayCastMouseClick(selectedCoordF, dptr, m_rayCastIntersect_d, 
        rayOrigin, rayDir, obst_d, *domain);
    if (rayCastResult == 0)
    {
        m_currentZ = selectedCoordF.z;

        xOut = (selectedCoordF.x + 1.f)*0.5f*xDimVisible;
        yOut = (selectedCoordF.y + 1.f)*0.5f*xDimVisible;
    }
    else
    {
        returnVal = 1;
    }

    cudaGraphicsUnmapResources(1, &cudaSolutionField, 0);

    return returnVal;
}

void GraphicsManager::GetSimCoordFromMouseRay(int &xOut, int &yOut,
    const int mouseX, const int mouseY)
{
    GetSimCoordFromMouseRay(xOut, yOut, mouseX, mouseY, m_currentZ);
}

void GraphicsManager::GetSimCoordFromMouseRay(int &xOut, int &yOut,
    const float mouseXf, const float mouseYf, const float planeZ)
{
    int mouseX = floatCoordToIntCoord(mouseXf, m_parent->GetRootPanel()->GetWidth());
    int mouseY = floatCoordToIntCoord(mouseYf, m_parent->GetRootPanel()->GetHeight());
    GetSimCoordFromMouseRay(xOut, yOut, mouseX, mouseY, planeZ);
}

void GraphicsManager::GetSimCoordFromMouseRay(int &xOut, int &yOut, 
    const int mouseX, const int mouseY, const float planeZ)
{
    int xDimVisible = GetCudaLbm()->GetDomain()->GetXDimVisible();
    float3 rayOrigin, rayDir;
    GetMouseRay(rayOrigin, rayDir, mouseX, mouseY);

    float t = (planeZ - rayOrigin.z)/rayDir.z;
    float xf = rayOrigin.x + t*rayDir.x;
    float yf = rayOrigin.y + t*rayDir.y;

    xOut = (xf + 1.f)*0.5f*xDimVisible;
    yOut = (yf + 1.f)*0.5f*xDimVisible;
}

void GraphicsManager::ClickDown(Mouse mouse)
{
    int mod = glutGetModifiers();
    int mouseX = mouse.GetX();
    int mouseY = mouse.GetY();
    int simX, simY;
    if (mouse.m_lmb == 1)
    {
        m_currentObstId = -1;
        int simX, simY;
        if (GetSimCoordFrom3DMouseClickOnObstruction(simX, simY, mouseX, mouseY) == 0)
        {
            m_currentObstId = FindClosestObstructionId(simX, simY);
        }
    }
    else if (mouse.m_rmb == 1)
    {
        m_currentObstId = -1;
        m_currentZ = -0.5f;
        GetSimCoordFromMouseRay(simX, simY, mouseX, mouseY);
        AddObstruction(simX, simY);
    }
    else if (mouse.m_mmb == 1 && mod != GLUT_ACTIVE_CTRL)
    {
        m_currentObstId = -1;
        if (GetSimCoordFrom3DMouseClickOnObstruction(simX, simY, mouseX, mouseY) == 0)
        {            
            m_currentObstId = FindClosestObstructionId(simX, simY);
            RemoveObstruction(simX, simY);
        }

    }
}

void GraphicsManager::Drag(const int xi, const int yi, const float dxf,
    const float dyf, const int button)
{
    if (button == GLUT_LEFT_BUTTON)
    {
        MoveObstruction(xi,yi,dxf,dyf);
    }
    else if (button == GLUT_MIDDLE_BUTTON)
    {
        int mod = glutGetModifiers();
        if (mod == GLUT_ACTIVE_CTRL)
        {
            m_translate.x += dxf;
            m_translate.y += dyf;
        }
        else
        {
            m_rotate.x += dyf*45.f;
            m_rotate.z += dxf*45.f;
        }

    }
}

void GraphicsManager::Pan(const float dx, const float dy)
{
    m_translate.x += dx;
    m_translate.y += dy;
}

void GraphicsManager::Rotate(const float dx, const float dy)
{
    m_rotate.x += dy;
    m_rotate.z += dx;
}

int GraphicsManager::PickObstruction(const float mouseXf, const float mouseYf)
{
    int mouseX = floatCoordToIntCoord(mouseXf, m_parent->GetRootPanel()->GetWidth());
    int mouseY = floatCoordToIntCoord(mouseYf, m_parent->GetRootPanel()->GetHeight());
    return PickObstruction(mouseX, mouseY);
}

int GraphicsManager::PickObstruction(const int mouseX, const int mouseY)
{
    int simX, simY;
    if (GetSimCoordFrom3DMouseClickOnObstruction(simX, simY, mouseX, mouseY) == 0)
    {
        return FindClosestObstructionId(simX, simY);
    }
    return -1;
}


void GraphicsManager::MoveObstruction(int obstId, const float mouseXf, const float mouseYf,
    const float dxf, const float dyf)
{
    int simX1, simY1, simX2, simY2;
    int xi, yi, dxi, dyi;
    int windowWidth = m_parent->GetRootPanel()->GetRectIntAbs().m_w;
    int windowHeight = m_parent->GetRootPanel()->GetRectIntAbs().m_h;
    xi = floatCoordToIntCoord(mouseXf, windowWidth);
    yi = floatCoordToIntCoord(mouseYf, windowHeight);
    dxi = dxf*static_cast<float>(windowWidth) / 2.f;
    dyi = dyf*static_cast<float>(windowHeight) / 2.f;
    GetSimCoordFromMouseRay(simX1, simY1, xi-dxi, yi-dyi);
    GetSimCoordFromMouseRay(simX2, simY2, xi, yi);
    Obstruction obst = m_obstructions[obstId];
    obst.x += simX2-simX1;
    obst.y += simY2-simY1;
    float u = std::max(-0.1f,std::min(0.1f,static_cast<float>(simX2-simX1) / (TIMESTEPS_PER_FRAME)));
    float v = std::max(-0.1f,std::min(0.1f,static_cast<float>(simY2-simY1) / (TIMESTEPS_PER_FRAME)));
    obst.u = u;
    obst.v = v;
    obst.state = Obstruction::ACTIVE;
    m_obstructions[obstId] = obst;
    Obstruction* obst_d = GetCudaLbm()->GetDeviceObst();
    UpdateDeviceObstructions(obst_d, obstId, obst);  
}


void GraphicsManager::Wheel(const int button, const int dir, const int x, const int y)
{
    if (dir > 0){
        m_translate.z -= 0.3f;
    }
    else
    {
        m_translate.z += 0.3f;
    }
}

void GraphicsManager::Zoom(const int dir, const float mag)
{
    if (dir > 0){
        m_translate.z -= mag;
    }
    else
    {
        m_translate.z += mag;
    }   
}

void GraphicsManager::AddObstruction(const int simX, const int simY)
{
    Obstruction obst = { m_currentObstShape, simX, simY, m_currentObstSize, 0, 0, 0, Obstruction::NEW  };
    int obstId = FindUnusedObstructionId();
    m_obstructions[obstId] = obst;
    Obstruction* obst_d = GetCudaLbm()->GetDeviceObst();
    UpdateDeviceObstructions(obst_d, obstId, obst);
}

void GraphicsManager::RemoveObstruction(const int simX, const int simY)
{
    int obstId = FindObstructionPointIsInside(simX,simY,1.f);
    RemoveSpecifiedObstruction(obstId);
}

void GraphicsManager::RemoveSpecifiedObstruction(const int obstId)
{
    if (obstId >= 0)
    {
        m_obstructions[obstId].state = Obstruction::REMOVED;
        Obstruction* obst_d = GetCudaLbm()->GetDeviceObst();
        UpdateDeviceObstructions(obst_d, obstId, m_obstructions[obstId]);
    }
}

void GraphicsManager::MoveObstruction(const int xi, const int yi,
    const float dxf, const float dyf)
{
    int xDimVisible = GetCudaLbm()->GetDomain()->GetXDimVisible();
    int yDimVisible = GetCudaLbm()->GetDomain()->GetYDimVisible();
    if (m_currentObstId > -1)
    {
        int simX1, simY1, simX2, simY2;
        int dxi, dyi;
        int windowWidth = m_parent->GetRootPanel()->GetRectIntAbs().m_w;
        int windowHeight = m_parent->GetRootPanel()->GetRectIntAbs().m_h;
        dxi = dxf*static_cast<float>(windowWidth) / 2.f;
        dyi = dyf*static_cast<float>(windowHeight) / 2.f;
        GetSimCoordFromMouseRay(simX1, simY1, xi-dxi, yi-dyi);
        GetSimCoordFromMouseRay(simX2, simY2, xi, yi);
        Obstruction obst = m_obstructions[m_currentObstId];
        obst.x += simX2-simX1;
        obst.y += simY2-simY1;
        float u = std::max(-0.1f,std::min(0.1f,static_cast<float>(simX2-simX1) / (TIMESTEPS_PER_FRAME)));
        float v = std::max(-0.1f,std::min(0.1f,static_cast<float>(simY2-simY1) / (TIMESTEPS_PER_FRAME)));
        obst.u = u;
        obst.v = v;
        obst.state = Obstruction::ACTIVE;
        m_obstructions[m_currentObstId] = obst;
        Obstruction* obst_d = GetCudaLbm()->GetDeviceObst();
        UpdateDeviceObstructions(obst_d, m_currentObstId, obst);
    }
}

int GraphicsManager::FindUnusedObstructionId()
{
    for (int i = 0; i < MAXOBSTS; i++)
    {
        if (m_obstructions[i].state == Obstruction::REMOVED || 
            m_obstructions[i].state == Obstruction::INACTIVE)
        {
            return i;
        }
    }
    MessageBox(0, "Object could not be added. You are currently using the maximum number of objects.",
        "Error", MB_OK);
    return 0;
}


int GraphicsManager::FindClosestObstructionId(const int simX, const int simY)
{
    float dist = 999999999999.f;
    int closestObstId = -1;
    for (int i = 0; i < MAXOBSTS; i++)
    {
        if (m_obstructions[i].state != Obstruction::REMOVED)
        {
            float newDist = GetDistanceBetweenTwoPoints(simX, simY, m_obstructions[i].x, m_obstructions[i].y);
            if (newDist < dist)
            {
                dist = newDist;
                closestObstId = i;
            }
        }
    }
    return closestObstId;
}

int GraphicsManager::FindObstructionPointIsInside(const int simX, const int simY,
    const float tolerance)
{
    float dist = 999999999999.f;
    int closestObstId = -1;
    for (int i = 0; i < MAXOBSTS; i++)
    {
        if (m_obstructions[i].state != Obstruction::REMOVED)
        {
            float newDist = GetDistanceBetweenTwoPoints(simX, simY, m_obstructions[i].x,
                m_obstructions[i].y);
            if (newDist < dist && newDist < m_obstructions[i].r1+tolerance)
            {
                dist = newDist;
                closestObstId = i;
            }
        }
    }
    //printf("closest obst: %i", closestObstId);
    return closestObstId;
}

bool GraphicsManager::IsInClosestObstruction(const int mouseX, const int mouseY)
{
    int simX, simY;
    GetSimCoordFromMouseCoord(simX, simY, mouseX, mouseY);
    int closestObstId = FindClosestObstructionId(simX, simY);
    float dist = GetDistanceBetweenTwoPoints(simX, simY, m_obstructions[closestObstId].x, 
        m_obstructions[closestObstId].y);
    return (dist < m_obstructions[closestObstId].r1);
}

void GraphicsManager::UpdateViewTransformations()
{
    glGetIntegerv(GL_VIEWPORT, m_viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, m_modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, m_projectionMatrix);
}

void GraphicsManager::UpdateGraphicsInputs()
{
    Panel* rootPanel = m_parent->GetRootPanel();
    m_contourMinValue = Layout::GetCurrentContourSliderValue(*rootPanel, 1);
    m_contourMaxValue = Layout::GetCurrentContourSliderValue(*rootPanel, 2);
    m_currentObstSize = Layout::GetCurrentSliderValue(*rootPanel, "Slider_Size");
}

void GraphicsManager::UpdateLbmInputs()
{
    Panel* rootPanel = m_parent->GetRootPanel();
    float u = rootPanel->GetSlider("Slider_InletV")->m_sliderBar1->GetValue();
    float omega = rootPanel->GetSlider("Slider_Visc")->m_sliderBar1->GetValue();
    CudaLbm* cudaLbm = GetCudaLbm();
    cudaLbm->SetInletVelocity(u);
    cudaLbm->SetOmega(omega);
}


glm::vec4 GraphicsManager::GetViewportMatrix()
{
    return glm::make_vec4(m_viewport);
}

glm::mat4 GraphicsManager::GetModelMatrix()
{
    return glm::make_mat4(m_modelMatrix);
}

glm::mat4 GraphicsManager::GetProjectionMatrix()
{
    return glm::make_mat4(m_projectionMatrix);
}



float GetDistanceBetweenTwoPoints(const float x1, const float y1,
    const float x2, const float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}