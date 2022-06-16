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

#ifdef __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#endif

#include <GLFW/glfw3.h>

#include "demo.h"
#include "nanovg.h"
#include "nanovg_gl.h"

typedef enum { NANOVG_GL2,
               NANOVG_GL3,
               NANOVG_GLES2,
               NANOVG_GLES3 } NanoVG_GL_API;

const NanoVG_GL_Functions_VTable *NanoVG_GL_Functions[] = {
    &NanoVG_GL2_Functions_VTable,
    &NanoVG_GL3_Functions_VTable,
    &NanoVG_GLES2_Functions_VTable,
    &NanoVG_GLES3_Functions_VTable,
};

void errorcb(int error, const char *desc) { printf("GLFW error %d: %s\n", error, desc); }

int blowup = 0;
int screenshot = 0;
int premult = 0;

static void key(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    NVG_NOTUSED(scancode);
    NVG_NOTUSED(mods);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        blowup = !blowup;
    if (key == GLFW_KEY_S && action == GLFW_PRESS)
        screenshot = 1;
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        premult = !premult;
}

int main()
{
    GLFWwindow *window;
    DemoData data;
    NVGcontext *vg = NULL;
    const NanoVG_GL_Functions_VTable *nvgl = NanoVG_GL_Functions[NANOVG_GL2];
    // double prevt = 0;

    printf("Using NanoVG API: %s\n", nvgl->name);

    if (!glfwInit()) {
        printf("Failed to init GLFW.");
        return -1;
    }

    glfwSetErrorCallback(errorcb);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
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

    glfwSetKeyCallback(window, key);

    glfwMakeContextCurrent(window);

#ifdef DEMO_MSAA
    vg = nvgl->createContext(NVG_STENCIL_STROKES | NVG_DEBUG);
#else
    vg = nvgl->createContext(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
#endif
    if (vg == NULL) {
        printf("Could not init nanovg.\n");
        return -1;
    }

    if (loadDemoData(vg, &data) == -1)
        return -1;

    glfwSwapInterval(0);

    glfwSetTime(0);
    // prevt = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double mx, my, t;
        int winWidth, winHeight;
        int fbWidth, fbHeight;
        float pxRatio;

        t = glfwGetTime();
        // double dt = t - prevt;
        // prevt = t;

        glfwGetCursorPos(window, &mx, &my);
        glfwGetWindowSize(window, &winWidth, &winHeight);
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        // Calculate pixel ration for hi-dpi devices.
        pxRatio = (float)fbWidth / (float)winWidth;

        // Update and render
        glViewport(0, 0, fbWidth, fbHeight);
        if (premult)
            glClearColor(0, 0, 0, 0);
        else
            glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

        renderDemo(vg, mx, my, winWidth, winHeight, t, blowup, &data);

        nvgEndFrame(vg);

        if (screenshot) {
            screenshot = 0;
            saveScreenShot(fbWidth, fbHeight, premult, "dump.png");
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    freeDemoData(vg, &data);

    nvgl->deleteContext(vg);

    glfwTerminate();
    return 0;
}
