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
#include "xdl/include/xdl.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"
#include "KittyMemory/MemoryPatch.h"

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
           // LOGW(OBFUSCATE("can't parse %s"), app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
       // LOGI(OBFUSCATE("detect game: %s"), package_name);
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    } else {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////
#define INIT_F(x, y)  *(void **)(&x) = (void *)(il2cpp_base + y);
////////////////////////////////////////////////////////////////////////////////////
#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
    /*LOGW("api not found %s", #n);*/          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}
////////////////////////////////////////////////////////////////////////////////////
void  il2cpp_api_init(void *handle) {
    //LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
       // LOGE("Failed to initialize il2cpp api.");
        return;
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}
////////////////////////////////////////////////////////////////////////////////////
const char* nop = "1F2003D5";
const char* fal = "000080D2C0035FD6";
const char* tru = "200080D2C0035FD6";

struct GlobalPatches {
    MemoryPatch mh1;
}gPatches;
////////////////////////////////////////////////////////////////////////////////////
void Patches(){
    gPatches.mh1 = MemoryPatch::createWithHex(il2cpp_base + 0x1F7C014, fal);
}
////////////////////////////////////////////////////////////////////////////////////
int glHeight, glWidth;
bool setupimg;

HOOKAF(void, Input, void *thiz, void *ex_ab, void *ex_ac) {
    origInput(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
    return;
}

void SetupImgui() {
    
    auto Screen_SetResolution = (void (*)(int, int, bool)) (getAbsoluteAddress(il2cpp_base,il2cpp_base + 0x44A3F00));
    Screen_SetResolution(screenWidth, screenHeight, true);
    
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

    static bool norec;
    
    if(norec){
        gPatches.mh1.Modify();
    }
    else{
        gPatches.mh1.Restore();
    }
    
    // ImGui::ShowDemoWindow();
    ImGui::Begin("Discord : SwaggY7777");
    
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
   // LOGI(OBFUSCATE("Draw Done!"));
    
    void *handle = xdl_open("libil2cpp.so",0);
    if (handle) {
        il2cpp_api_init(handle);
    }else {
        //LOGI("libi2cpp not found %d", gettid());
    }
    
    Patches();
    
    // DobbyHook((void *) il2cpp_base + 0x44A3F00, (void *)SetResolutionn, (void **)&_SetResolutionn);
    
    return nullptr;
}
