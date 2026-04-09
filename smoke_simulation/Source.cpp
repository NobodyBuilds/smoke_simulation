#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include "shader.h"
#include <vector>
#include "cuda.h"
#include "param.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include<algorithm>
#include<cmath>
#include "ui.h"
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 900;
double frameTimeSec = 0.0;
int currentW = SCR_WIDTH;
int currentH = SCR_HEIGHT;
double mousex, mousey;
double prevmousex = 0, prevmousey = 0;
double dmx = 0, dmy = 0;
const float fixeddt = 1.0f / 120.0f;

    
     int w = SCR_WIDTH / settings.tilesize;
     int h = SCR_HEIGHT / settings.tilesize;
     
     float radiusPx = 50.0f; // world-space pixels, stays consistent
  

    
std::vector<float> density(w* h, 0.0f);
std::vector<float> velocityx(w * h , 0.0f); // 2 components for velocity x
std::vector<float> velocityy(w * h , 0.0f); // 2 components for velocity y
std::vector<float> pressure(w* h, 0.0f); // 1 component for pressure
std::vector<float> Div(w* h, 0.0f); // 1 component for divergence
std::vector<float> tempdata(w* h, 0.0f);
std::vector<float> tempvx(w* h, 0.0f);
std::vector<float> tempvy(w* h, 0.0f);

int issolid(int x, int y) {
    // Simple boundary condition: solid walls at the edges
    if (x <= 0 || x >= w - 1 || y <= 0 || y >= h - 1)
        return 1; // solid
    return 0; // fluid
}

void divergence(){
    for(int y = 1; y < h - 1; y++) {      // start at 1, stop before h-1
        for (int x = 1; x < w - 1; x++) {

           int i =y*w+x;
           float vleft = issolid(x - 1, y) ? 0.0f : velocityx[i - 1];
           float vright = issolid(x + 1, y) ? 0.0f : velocityx[i + 1];
           float vtop = issolid(x, y - 1) ? 0.0f : velocityy[i - w];
           float vbottom = issolid(x, y + 1) ? 0.0f : velocityy[i + w];


            float gradx=(vright-vleft)*0.5f;
            float grady=(vbottom-vtop)*0.5f;
            float divergence=gradx+grady;
            Div[i]=divergence;

            // Store or use the divergence value as needed
        pressure[y*w+x] = 0.0f;
        }
    }
}
void solvePressure(int iterations,float d) {
    float scale = settings.density / d;

    for (int iter = 0; iter < iterations; iter++) {
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                int i = y * w + x;

                int flowtop = issolid(x, y + 1) ? 0 : 1;
                int flowbottom = issolid(x, y - 1) ? 0 : 1;
                int flowleft = issolid(x - 1, y) ? 0 : 1;
                int flowright = issolid(x + 1, y) ? 0 : 1;
                int dgecount = flowbottom + flowtop + flowleft + flowright;

                if (issolid(x, y) || dgecount == 0) { pressure[i] = 0.0f; continue; }

                float newpressure = (
                    (pressure[i - 1] * flowleft)
                    + (pressure[i + 1] * flowright)
                    + (pressure[i - w] * flowtop)
                    + (pressure[i + w] * flowbottom)
                    - scale * Div[i]                  // ← correct sign + physical scaling
                    ) / dgecount;

                float oldpressure = pressure[i];
                pressure[i] = oldpressure + (newpressure - oldpressure) * settings.sor;
            }
        }
    }
}

void project() {
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            settings.k = fixeddt / (settings.density * 2.0f);

            
                    if (issolid(x, y)) {
                        velocityx[i] = 0.0f;
                        velocityy[i] = 0.0f;
                        continue;
                    }

                    // x-component — zero if neighbor wall blocks this face
                    if (issolid(x - 1, y))
                        velocityx[i] = 0.0f;
                    else
                        velocityx[i] -= settings.k * (pressure[i + 1] - pressure[i - 1]);

                    // y-component — independent, never skipped by x-check
                    if (issolid(x, y - 1))
                        velocityy[i] = 0.0f;
                    else
                        velocityy[i] -= settings.k * (pressure[i + w] - pressure[i - w]);
                    velocityy[i] += settings.bouyancy * fixeddt; // add buoyancy force

                
        }
    }
}




// bilinear sample helper
float sample(std::vector<float>& field, float x, float y) {
    x = std::clamp(x, 0.5f, w - 1.5f);
    y = std::clamp(y, 0.5f, h - 1.5f);
    int x0 = (int)x, y0 = (int)y;
    int x1 = x0 + 1, y1 = y0 + 1;
    float tx = x - x0, ty = y - y0;
    return (1 - tx) * (1 - ty) * field[y0 * w + x0]
        + tx * (1 - ty) * field[y0 * w + x1]
        + (1 - tx) * ty * field[y1 * w + x0]
        + tx * ty * field[y1 * w + x1];
}
void advectVelocity(float dt) {
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if (issolid(x, y)) { tempvx[i] = 0; tempvy[i] = 0; continue; }
            float vx = std::clamp(velocityx[i], -1.0f / dt, 1.0f / dt);
            float vy = std::clamp(velocityy[i], -1.0f / dt, 1.0f / dt);

            float px = x - vx * dt;
            float py = y - vy * dt;
            tempvx[i] = sample(velocityx, px, py);
            tempvy[i] = sample(velocityy, px, py);
        }
    }
    std::swap(velocityx, tempvx);
    std::swap(velocityy, tempvy);
    // zero boundaries
    for (int x = 0; x < w; x++) {
        velocityx[x] = 0; velocityx[(h - 1) * w + x] = 0;
        velocityy[x] = 0; velocityy[(h - 1) * w + x] = 0;
    }
    for (int y = 0; y < h; y++) {
        velocityx[y * w] = 0; velocityx[y * w + w - 1] = 0;
        velocityy[y * w] = 0; velocityy[y * w + w - 1] = 0;
    }
}
void advectDensity(float dt) {
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if (issolid(x, y)) { tempdata[i] = 0; continue; }
            float px = x - velocityx[i] * dt;
            float py = y - velocityy[i] * dt;
            tempdata[i] = sample(density, px, py);
        }
    }
    std::swap(density, tempdata);
    for (int x = 0; x < w; x++) { density[x] = 0; density[(h - 1) * w + x] = 0; }
    for (int y = 0; y < h; y++) { density[y * w] = 0; density[y * w + w - 1] = 0; }
}
float totalerror = 0.0f;
void divergenceerror() {
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            float vleft = issolid(x - 1, y) ? 0.0f : velocityx[i - 1];
            float vright = issolid(x + 1, y) ? 0.0f : velocityx[i + 1];
            float vtop = issolid(x, y - 1) ? 0.0f : velocityy[i - w];
            float vbottom = issolid(x, y + 1) ? 0.0f : velocityy[i + w];
            float gradx = (vright - vleft) / 0.5f;
                float grady = (vbottom - vtop) / 0.5f;
            float divergence = gradx + grady;
            totalerror += std::abs(divergence);
        }
    }
	const float displayfactor = 10000.0f; // Adjust this factor to make the error more visible
    settings.diverror = (int)(totalerror / (w * h) * displayfactor);
	totalerror = 0.0f; // reset for next frame

}

void diffuse(float dt) {
    std::vector<float> tmp = density;
    float a = settings.visc * dt ;
    for (int iter = 0; iter < 20; iter++) {
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                int i = y * w + x;
                if (issolid(x, y)) continue;
                density[i] = (tmp[i] + a * (
                    density[i - 1] + density[i + 1] +
                    density[i - w] + density[i + w]
                    )) / (1 + 4 * a);
            }
        }
    }
}
void dissipate(float dt) {
    float decay = 1.0f - settings.dissipation * dt; // dissipation ~ 0.5f
    for (int i = 0; i < w * h; i++)
        density[i] *= decay;
}

void dampVelocity(float dt) {
    float damp = 1.0f - settings.damp * dt; // velDamping ~ 0.5f
    for (int i = 0; i < w * h; i++) {
        velocityx[i] *= damp;
        velocityy[i] *= damp;
    }
}

std::vector<float> vorticity(w* h, 0.0f);

void vorticityConfinement(float dt) {
    // step 1: compute curl (scalar in 2D) at each cell
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if (issolid(x, y)) { vorticity[i] = 0; continue; }
            float dvdx = (velocityy[i + 1] - velocityy[i - 1]) * 0.5f;
            float dudy = (velocityx[i + w] - velocityx[i - w]) * 0.5f;
            vorticity[i] = dvdx - dudy;
        }
    }

    // step 2: compute gradient of |vorticity| magnitude, then apply force
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int i = y * w + x;
            if (issolid(x, y)) continue;

            float dx = (fabsf(vorticity[i + 1]) - fabsf(vorticity[i - 1])) * 0.5f;
            float dy = (fabsf(vorticity[i + w]) - fabsf(vorticity[i - w])) * 0.5f;

            float len = sqrtf(dx * dx + dy * dy) + 1e-5f;
            dx /= len;
            dy /= len;

            float omega = vorticity[i];
            // force is perpendicular to gradient, scaled by omega
            velocityx[i] += settings.vorticity * dy * omega * dt;
            velocityy[i] += settings.vorticity * -dx * omega * dt;
        }
    }
}

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetShaderInfoLog(s, 1024, nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
    }
    return s;
}
static GLuint createProgram(const char* vs, const char* fs)
{
    GLuint a = compileShader(GL_VERTEX_SHADER, vs);
    GLuint b = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char buf[1024];
        glGetProgramInfoLog(p, 1024, nullptr, buf);
        std::cerr << "Program link error: " << buf << "\n";
    }
    glDeleteShader(a);
    glDeleteShader(b);
    return p;
}

void initshader() {
    if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    if (TEX) { glDeleteTextures(1, &TEX); TEX = 0; }

    float vertz[]{
      -1.0f, 1.0f, 0.0f,
     1.0f, 1.0f, 0.0f,
     1.0f,  -1.0f, 0.0f,
	 -1.0f, 1.0f, 0.0f,
	 -1.0f, -1.0f, 0.0f,
	 1.0f, -1.0f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
	glGenTextures(1, &TEX);

	glBindTexture(GL_TEXTURE_2D, TEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
  
	glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertz), vertz, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TEX);
    glBindVertexArray(0);

}


void draw() {

    glUseProgram(shaderProgram);
    glUniform2f(uresolution, (float)currentW, (float)currentH);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TEX);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
   // printf("draw\n");

}

extern "C" void restart() {
    w = currentW / (int)settings.tilesize;
    h = currentH / (int)settings.tilesize;
    settings.w = w;
    settings.h = h;
    freecuda(); unregisterbuffer();
    density.assign(w * h, 0.0f);
    velocityx.assign(w * h, 0.0f);
    velocityy.assign(w * h, 0.0f);
    pressure.assign(w * h, 0.0f);
    vorticity.assign(w * h, 0.0f);
    Div.assign(w * h, 0.0f);
    tempdata.assign(w * h, 0.0f); // ← missing
    tempvx.assign(w * h, 0.0f);   // ← missing
    tempvy.assign(w * h, 0.0f);   // ← missing
    initshader(); initcuda(w, h); registerBuffer(TEX);
    updateframe(w, h, density.data());
   
  
  // updateframe(w, h, data.data());
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "hello", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, [](GLFWwindow*, double, double dy) {
        settings.radius = std::clamp(settings.radius + (float)dy * 5.0f, 5.0f, 300.0f);
        });


    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 330";
    ImGui_ImplOpenGL3_Init(glsl_version);
    // compile shaders
    // ---------------
    shaderProgram = createProgram(vertexShaderSource, fragmentShaderSource);
    // retrieve uniform locations and bind sampler
    glUseProgram(shaderProgram);
    {
        GLint loc = glGetUniformLocation(shaderProgram, "ushader");
        if (loc >= 0) { glUniform1i(loc, 0); ushader = (GLuint)loc; }
        loc = glGetUniformLocation(shaderProgram, "uresolution");
        if (loc >= 0) { uresolution = (GLuint)loc; }
    }
    glUseProgram(0);
    // -----------
    settings.w = w;
    settings.h = h;
        initshader();
		initcuda(w, h);
		registerBuffer(TEX);
        double lastTime = glfwGetTime();
        double fpsclock = lastTime;
		float accumulator = 0.0f;
    // render loop
    while (!glfwWindowShouldClose(window))
    {
        extern double frameTimeSec;
		double currentTime = glfwGetTime();
		double frametime = currentTime - lastTime;
		lastTime = currentTime;
        frameTimeSec = frametime;

         accumulator += (float)frametime;
        float dt = (float)frametime;

        glfwGetCursorPos(window, &mousex, &mousey);
         dmx = mousex - prevmousex;
         dmy = mousey - prevmousey;
        prevmousex = mousex;
        prevmousey = mousey;
        processInput(window);

        while (accumulator >= fixeddt) {
           // test();
            advectVelocity(fixeddt);
            // 2. then make it divergence-free
            divergence();
            solvePressure(settings.itters, fixeddt);
            project();
            vorticityConfinement(fixeddt);
			//dampVelocity(fixeddt);
            // 3. then advect density along the clean velocity
            advectDensity(fixeddt);
            diffuse(fixeddt);
            dissipate(fixeddt);
            divergenceerror();
			accumulator -= fixeddt;
        }
        

        

        // input
        // -----
      

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
			updateframe(w,h,density.data());
        draw();
        ui();

     

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        double elapsed = currentTime - fpsclock;
        fpsclock = currentTime;
        settings.fps = (elapsed > 0.0) ? 1.0 / elapsed : settings.fps;
        settings.fpsTimer += (float)elapsed;
        settings.fpsCount++;
        if (settings.fps > settings.maxFps)
            settings.maxFps = (float)settings.fps;
        if (settings.fps < settings.minFps)
            settings.minFps = (float)settings.fps;
        if (settings.fpsTimer >= 0.5f)
        {
            settings.avgFps = settings.fpsCount / settings.fpsTimer;
            settings.fpsTimer = 0.f;
            settings.fpsCount = 0;
        }
        settings.fuc_ms = (settings.avgFps > 0.0f) ? 1000.0f / settings.avgFps : 0.0f;

        glfwPollEvents();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    // Cleanup OpenGL resources
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
    if (TEX) { glDeleteTextures(1, &TEX); TEX = 0; }
    if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
    
    // Cleanup CUDA resources
    unregisterbuffer();
    freecuda();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)

{

    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        restart();
    }



    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        int col = (int)mousex / settings.tilesize;
        int row = (int)(currentH - mousey) / settings.tilesize;
       settings.radiuscells = settings.radius / settings.tilesize;
        if (col >= 0 && col < w && row >= 0 && row < h) {
            int i = row * w + col;
           // float radius = 10.0f;
          
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int idx = y * w + x;
                    float dx = x - col;
                    float dy = y - row;
                    float d = (dx * dx) + (dy * dy);
                    float dist = sqrtf(d);
                  //  printf("%3f\n", dist);
                    if (dist <= settings.radiuscells && !issolid(x, y)) {
                        float nx = dx / (dist + 0.001f); // normalized direction from center
                        float ny = dy / (dist + 0.001f);
                        float mousespeed = sqrtf(dmx * dmx + dmy * dmy);
                        // inject velocity in mouse direction
                        
                       
                       // velocityx[idx] += -ny * mousespeed * 0.5f;
                       // velocityy[idx] += nx * mousespeed * 0.5f;
                        density[idx] += settings.dscale *fixeddt;
                    }
                    
                }
            }



           // data[i] = 1.0f;
          /*  divergence();
			solvePressure();
			project();
			advect(fixeddt);*/
        }
           // updateframe(w, h, data.data());
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        int col = (int)mousex / settings.tilesize;
        int row = (int)(currentH - mousey) / settings.tilesize;
        settings.radiuscells = settings.radius / settings.tilesize;
        if (col >= 0 && col < w && row >= 0 && row < h) {
            int i = row * w + col;
            // float radius = 10.0f;

            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int idx = y * w + x;
                    float dx = x - col;
                    float dy = y - row;
                    float d = (dx * dx) + (dy * dy);
                    float dist = sqrtf(d);
                    //  printf("%3f\n", dist);
                    if (dist <= settings.radiuscells) {
                        
                        float vscale =settings.vscale;
                        // left mouse — same fix as right
                        velocityx[idx] += (float)(dmx / settings.tilesize / frameTimeSec) * vscale * fixeddt;
                        velocityy[idx] -= (float)(dmy / settings.tilesize / frameTimeSec) * vscale * fixeddt;
                    }

                }
            }



            // data[i] = 1.0f;
           /*  divergence();
             solvePressure();
             project();
             advect(fixeddt);*/
        }
        // updateframe(w, h, data.data());
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    currentW = width;
    currentH = height;
    
    glViewport(0, 0, width, height);
    restart();
}