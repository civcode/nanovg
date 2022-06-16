//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#endif

#include <GLFW/glfw3.h>

#include "nanovg.h"
#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"
#include "perf.h"

float zoom = 1.0;
float zoom_prev = 1.0;
int isTranslationSet = 0;

NVGcontext *vg = NULL;

void renderPattern(NVGcontext *vg, NVGLUframebuffer *fb, float t, float pxRatio) {
    int winWidth, winHeight;
    int fboWidth, fboHeight;
    int pw, ph, x, y;
    float s = 20.0f;
    float sr = (cosf(t) + 1) * 0.5f;
    float r = s * 0.6f * (0.2f + 0.8f * sr);

    if (fb == NULL)
        return;

    nvgImageSize(vg, fb->image, &fboWidth, &fboHeight);
    winWidth = (int)(fboWidth / pxRatio);
    winHeight = (int)(fboHeight / pxRatio);

    // Draw some stuff to an FBO as a test
    nvgluBindFramebufferGL3(fb);
    glViewport(0, 0, fboWidth, fboHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

    pw = (int)ceilf(winWidth / s);
    ph = (int)ceilf(winHeight / s);

    nvgBeginPath(vg);
    for (y = 0; y < ph; y++) {
        for (x = 0; x < pw; x++) {
            float cx = (x + 0.5f) * s;
            float cy = (y + 0.5f) * s;
            nvgCircle(vg, cx, cy, r);
        }
    }
    nvgFillColor(vg, nvgRGBA(220, 160, 0, 200));
    nvgFill(vg);

    nvgEndFrame(vg);
    nvgluBindFramebufferGL3(NULL);
}

int loadFonts(NVGcontext *vg) {
    int font;
    font = nvgCreateFont(vg, "sans", "../example/Roboto-Regular.ttf");
    if (font == -1) {
        printf("Could not add font regular.\n");
        return -1;
    }
    font = nvgCreateFont(vg, "sans-bold", "../example/Roboto-Bold.ttf");
    if (font == -1) {
        printf("Could not add font bold.\n");
        return -1;
    }
    return 0;
}

void errorcb(int error, const char *desc) { printf("GLFW error %d: %s\n", error, desc); }

static void key(GLFWwindow *window, int key, int scancode, int action, int mods) {
    NVG_NOTUSED(scancode);
    NVG_NOTUSED(mods);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if (key == GLFW_KEY_Q && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

static void mouseScroll(GLFWwindow *window, double xoffset, double yoffset) {
    printf("scroll: [%.2f, %.2f]\n", xoffset, yoffset);
    zoom_prev = zoom;
    zoom += yoffset / 2;
    if (zoom < 0.5) {
        zoom = 0.5;
    }
    if (zoom > 5.0) {
        zoom = 5.0;
    }
    printf("zoom = %f\n", zoom);
    isTranslationSet = 1;

    // double mx;
    // double my;
    // glfwGetCursorPos(window, &mx, &my);

    // nvgScale(vg, zoom, zoom);
    // nvgTranslate(vg, mx*(1.0/zoom-1.0), my*(1.0/zoom-1.0));
}

static void mouseButton(GLFWwindow *window, int button, int action, int mods) {
    double mx;
    double my;
    glfwGetCursorPos(window, &mx, &my);

    if (action == GLFW_PRESS) {
        printf("cursos pos: [%.2f, %.2f]\n", mx, my);
        printf("mouse button: [%d, %d, %d]\n", button, action, mods);
    }

    if (action == GLFW_PRESS) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                printf("MOUSE_BUTTON_LEFT pressed\n");
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                printf("MOUSE_BUTTON_RIGHT pressed\n");
                break;
            default:
                break; 
        }
    }

}

typedef struct vec2_ {
    float x;
    float y;
} vec2;

typedef struct grid_ {
    float x;
    float y;
    float w;
    float h;
    float d;
    float s;
    vec2 o;
} grid;

int main() {
    GLFWwindow *window;
    //NVGcontext *vg = NULL;
    GPUtimer gpuTimer;
    PerfGraph fps, cpuGraph, gpuGraph;
    double prevt = 0, cpuTime = 0;
    NVGLUframebuffer *fb = NULL;
    int winWidth, winHeight;
    int fbWidth, fbHeight;
    float pxRatio;

    if (!glfwInit()) {
        printf("Failed to init GLFW.");
        return -1;
    }

    initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
    initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");

    glfwSetErrorCallback(errorcb);
#ifndef _WIN32 // don't require this on win32, and works with more cards
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);

#ifdef DEMO_MSAA
    glfwWindowHint(GLFW_SAMPLES, 4);
#endif
    window = glfwCreateWindow(1000, 600, "NanoVG", NULL, NULL);
    //	window = glfwCreateWindow(1000, 600, "NanoVG", glfwGetPrimaryMonitor(),
    // NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    //GLFWimage icon;
    //icon.height = 50;
    //icon.width = 50;


    glfwSetKeyCallback(window, key);
    glfwSetScrollCallback(window, mouseScroll);
    glfwSetMouseButtonCallback(window, mouseButton);

    glfwMakeContextCurrent(window);

#ifdef DEMO_MSAA
    vg = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_DEBUG);
#else
    vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
#endif
    if (vg == NULL) {
        printf("Could not init nanovg.\n");
        return -1;
    }

    // Create hi-dpi FBO for hi-dpi screens.
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    // Calculate pixel ration for hi-dpi devices.
    pxRatio = (float)fbWidth / (float)winWidth;

    // The image pattern is tiled, set repeat on x and y.
    fb = nvgluCreateFramebufferGL3(vg, (int)(100 * pxRatio), (int)(100 * pxRatio),
                                   NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY);
    if (fb == NULL) {
        printf("Could not create FBO.\n");
        return -1;
    }

    if (loadFonts(vg) == -1) {
        printf("Could not load fonts\n");
        return -1;
    }

    glfwSwapInterval(0);

    initGPUTimer(&gpuTimer);

    glfwSetTime(0);
    prevt = glfwGetTime();

    grid grid_0 = {.x = 100, 
                    .y = 100, 
                    .w = 300, 
                    .h = 200, 
                    .d = 10,
                    .s = 1,
                    .o.x = 0,
                    .o.y = 0};
    //grid_0.o.x = grid_0.x;
    //grid_0.o.y = grid_0.y;

    while (!glfwWindowShouldClose(window)) {
        double mx, my, t, dt;
        float gpuTimes[3];
        int i, n;
        static double mx_prev = 0;
        static double my_prev = 0;

        t = glfwGetTime();
        dt = t - prevt;
        prevt = t;

        startGPUTimer(&gpuTimer);

        glfwGetCursorPos(window, &mx, &my);
        if (mx != mx_prev || my != my_prev) {
            //printf("cursor: [%.2f, %.2f]\n", mx, my);
            mx_prev = mx;
            my_prev = my;
        }
        glfwGetWindowSize(window, &winWidth, &winHeight);
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        // Calculate pixel ration for hi-dpi devices.
        pxRatio = (float)fbWidth / (float)winWidth;

        glfwPollEvents();

        renderPattern(vg, fb, t, pxRatio);

        // Update and render
        glViewport(0, 0, fbWidth, fbHeight);
        // glViewport(fbWidth/2, fbHeight/2, fbWidth, fbHeight);
        // glScissor(200, 200, 100, 100);
        // glEnable(GL_SCISSOR_TEST);
        // glViewport(1*fbWidth/3, 1*fbHeight/3, fbWidth/3, fbHeight/3);
        // glViewport(1*fbWidth/3, 1*fbHeight/3, fbWidth*2, fbHeight*2);
        // glViewport(100, 3, fbWidth, fbHeight);

        // clear view
        glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // glMatrixMode(GL_PROJECTION);
        // glLoadIdentity();
        // glScalef(1.5f, 1.5f, 1.0f);
        // glOrtho(-100, 100, -100, 100, -100, 100);
        // glMatrixMode(GL_MODELVIEW);
        // glLoadIdentity();
        // glScalef(2.0f, 2.0f, 2.0f);

        nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

        // Use the FBO as image pattern.
        if (fb != NULL) {
            //nvgSave(vg);

            // cursor
            nvgSave(vg);
            nvgTranslate(vg, mx, my); //move offset of CS to [mx. my]
            nvgScale(vg, zoom, zoom);
            nvgBeginPath(vg);
            nvgMoveTo(vg, 0, 0);
            nvgLineTo(vg, 20, 0);
            nvgMoveTo(vg, 0, 0);
            nvgLineTo(vg, 0, 20);
            nvgCircle(vg, 20, 20, 2);
            nvgStrokeColor(vg, nvgRGBAf(1,1,1,1));
            nvgStrokeWidth(vg, 2);
            nvgStroke(vg);
            nvgClosePath(vg);
            nvgRestore(vg);

            // red circle
            float px = 100;
            float py = 100;
            nvgSave(vg);
            nvgBeginPath(vg);
            nvgCircle(vg, px, py, 10);
            nvgStrokeColor(vg, nvgRGBA(220, 0, 0, 255));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
            nvgClosePath(vg);
            nvgRestore(vg);

            float s = zoom;
            nvgSave(vg);
            //nvgTranslate(vg, px, py);
            nvgTranslate(vg, mx, my);
            nvgScale(vg, s, s);
            nvgTranslate(vg, -mx/s, -my/s);
            nvgBeginPath(vg);
            nvgCircle(vg, 0, 0, 10);
            nvgStrokeColor(vg, nvgRGBA(220, 0, 0, 255));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
            nvgClosePath(vg);
            nvgRestore(vg);


            vec2 p0 = {0, 0};

            float d = 10.0f;
            float w = 100.0f;
            float h = 50.0f;

            static double mx_;
            static double my_;
            static double zoom_;
            static double tx, ty;
            static double dx, dy;
            static vec2 offset;
            static vec2 t;
            glfwGetCursorPos(window, &mx, &my);
            if (isTranslationSet == 1) {
                //get offset
                //offset.x = (grid_0.x/zoom_prev)-mx;
                //offset.y = (grid_0.y/zoom_prev)-my;
                //offset.x = grid_0.o.x - (mx*zoom);
                //offset.y = grid_0.o.y - (my*zoom);
                float z = zoom/zoom_prev;
                t.x = mx + z * (grid_0.o.x - mx);
                t.y = my + z * (grid_0.o.y - my);
                grid_0.o = t;


                
                printf("offset = [%.2f, %.2f]\n", offset.x, offset.y); 
                printf("zoom_prev = %f\n", zoom_prev);
                dx = (mx_-mx*zoom_prev);
                dy = (my_-my*zoom_prev);
                dx = (mx-mx_*zoom_prev);
                dy = (my-my_*zoom_prev);
                mx_ = mx;
                my_ = my;
                tx = (mx_-w*s);
                ty = (my_-h*s); 
                tx = (dx);
                ty = (dy); 
                //tx = (mx_*s);
                //ty = (my_*s); 
                isTranslationSet = 0;
            }


            // 1. get offset
            // 2. 


            nvgSave(vg);

            //grid_0.x = 0;
            //nvgTranslate(vg, (mx_-w*s), (my_-h*s));
            //nvgTranslate(vg, tx, ty);
            //nvgTranslate(vg, mx_, my_);
            nvgTranslate(vg, t.x, t.y);
            nvgScale(vg, zoom, zoom);

            nvgBeginPath(vg);
            nvgRect(vg, grid_0.x, grid_0.y, grid_0.d, grid_0.d);
            nvgFillColor(vg, nvgRGBAf(0,0,0,1));
            nvgFill(vg);
            nvgClosePath(vg);

            nvgBeginPath(vg);
            for (i=0; i<grid_0.h/grid_0.d+1; i++) {
                //nvgMoveTo(vg, cx, cy+i*d);
                //nvgLineTo(vg, cx+w, cy+i*d);
                nvgMoveTo(vg, grid_0.x, grid_0.y+i*grid_0.d);
                nvgLineTo(vg, grid_0.x+grid_0.w, grid_0.y+i*grid_0.d);
            }
            for (i=0; i<grid_0.w/grid_0.d+1; i++) {
                nvgMoveTo(vg, grid_0.x+i*grid_0.d, grid_0.y);
                nvgLineTo(vg, grid_0.x+i*grid_0.d, grid_0.y+grid_0.h);
            }
            nvgStrokeColor(vg, nvgRGBAf(1,1,1,1));
            nvgStrokeWidth(vg, 0.5);
            nvgStroke(vg);
            nvgClosePath(vg);



            nvgRestore(vg);


            // Bezier Curve
            /*
            nvgSave(vg);
            nvgBeginPath(vg);
            nvgMoveTo(vg, 50.0f, 50.0f);
            nvgQuadTo(vg, 75.0f, 100.0f, 100.0f, 50.0f);
            nvgStrokeColor(vg, nvgRGBAf(1,1,1,1));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
            nvgClosePath(vg);

            nvgBeginPath(vg);
            nvgMoveTo(vg, 50.0f, 150.0f);
            nvgQuadTo(vg, 100.0f, 160.0f, 100.0f, 150.0f);
            nvgQuadTo(vg, 100.0f, 160.0f, 150.0f, 250.0f);
            nvgStrokeColor(vg, nvgRGBAf(1,1,1,1));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
            nvgClosePath(vg);

            nvgRestore(vg);
            */

            nvgSave(vg);
            nvgTranslate(vg, mx_, my_);
            nvgRestore(vg);
        }

        renderGraph(vg, 5, 5, &fps);
        renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph);
        if (gpuTimer.supported)
            renderGraph(vg, 5 + 200 + 5 + 200 + 5, 5, &gpuGraph);

        nvgEndFrame(vg);

        // Measure the CPU time taken excluding swap buffers (as the swap may
        // wait for GPU)
        cpuTime = glfwGetTime() - t;

        updateGraph(&fps, dt);
        updateGraph(&cpuGraph, cpuTime);

        // We may get multiple results.
        n = stopGPUTimer(&gpuTimer, gpuTimes, 3);
        for (i = 0; i < n; i++)
            updateGraph(&gpuGraph, gpuTimes[i]);

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    nvgluDeleteFramebufferGL3(fb);

    nvgDeleteGL3(vg);

    printf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
    printf("          CPU Time: %.2f ms\n", getGraphAverage(&cpuGraph) * 1000.0f);
    printf("          GPU Time: %.2f ms\n", getGraphAverage(&gpuGraph) * 1000.0f);

    glfwTerminate();
    return 0;
}
