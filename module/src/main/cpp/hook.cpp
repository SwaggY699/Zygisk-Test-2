#include "hook.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <dobby.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"

#include <map>
#include "Tools/Obfuscate.h"
#include "Logger.h"

inline std::map < std::string, void*> _methods;
inline std::map < std::string, size_t > _fields;

uintptr_t il2cppMap;

#include "Tools/Tools.h"
#include "AU/Il2Cpp.h"


#define GamePackageName "com.ngame.allstar.eu"

int isGame(JNIEnv *env, jstring appDataDir) {
    if (!appDataDir)
        return 0;
    const char *app_data_dir = env->GetStringUTFChars(appDataDir, nullptr);
    int user = 0;
    static char package_name[256];
    if (sscanf(app_data_dir, "/data/%*[^/]/%d/%s", &user, package_name) != 2) {
        if (sscanf(app_data_dir, "/data/%*[^/]/%s", package_name) != 1) {
            package_name[0] = '\0';
            LOGW(OBFUSCATE("can't parse %s"), app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
        LOGI(OBFUSCATE("detect game: %s"), package_name);
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    } else {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 0;
    }
}

int glHeight, glWidth;
bool setupimg;

bool SetCustomResolution = true;
void (*_SetResolutionn)(...);
void SetResolutionn(int width, int height, bool fullscreen){
if(SetCustomResolution){
  width = glWidth;
 height = glHeight;
}
_SetResolutionn(width, height, fullscreen);
}

HOOKAF(void, Input, void *thiz, void *ex_ab, void *ex_ac) {
    origInput(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
    return;
}

void SetupImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float) glWidth, (float) glHeight);
    ImGui_ImplOpenGL3_Init("#version 100");
    ImGui::StyleColorsLight();

    ImGui::GetStyle().ScaleAllSizes(3.0f);
}

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setupimg) {
        SetupImgui();
        setupimg = true;
    }

    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();



    ImGui::ShowDemoWindow();

    ImGui::EndFrame();
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return old_eglSwapBuffers(dpy, surface);
}

void *hack_thread(void *arg) {
    sleep(5);
    auto eglhandle = dlopen("libunity.so", RTLD_LAZY);
    auto eglSwapBuffers = dlsym(eglhandle, "eglSwapBuffers");
    DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
    void *sym_input = DobbySymbolResolver(("/system/lib/libinput.so"), ("_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE"));
    if (NULL != sym_input) {
    DobbyHook(sym_input, (void*)myInput, (void**)&origInput);
    }
    LOGI(OBFUSCATE("Draw Done!"));
    
    //void *il2cppHandle = dlopen("libil2cpp.so", RTLD_LAZY);
    uintptr_t il2cppHandle = reinterpret_cast<uintptr_t>(dlopen("libil2cpp.so", RTLD_LAZY));
    const char *il2cpp_error = dlerror();
    if (il2cpp_error || !il2cppHandle)
    {
        LOGE(OBFUSCATE("Cannot load dl 'il2cpp': %s"), il2cpp_error);
        return NULL;
    }
    
    while (!il2cppMap) {
		il2cppMap = Tools::GetBaseAddress(il2cppHandle);
		sleep(5);
	}
	
    Il2CppAttach();
    sleep(1);
    
    _methods["Screen::SetResolution"] = Il2CppGetMethodOffset("UnityEngine.CoreModule.dll", "UnityEngine", "Screen", "SetResolution", 3);
    DobbyHook((void *)&_methods["Screen::SetResolution"], (void *)SetResolutionn, (void **)&_SetResolutionn);
    
    return nullptr;
}
