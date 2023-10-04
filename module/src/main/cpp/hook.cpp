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

inline std::map < std::string, void*> _methods;
inline std::map < std::string, size_t > _fields;

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
void *Il2CppGetImageByName(const char *image);
void *Il2CppGetClassType(const char *image, const char *namespaze, const char *clazz);
void *Il2CppGetMethodOffset(const char *image, const char *namespaze, const char *clazz, const char *name, int argsCount = 0);
void *Il2CppGetMethodOffset(const char *image, const char *namespaze, const char *clazz, const char *name, char **args, int argsCount);
////////////////////////////////////////////////////////////////////////////////////
void *Il2CppGetImageByName(const char *image) {
    size_t size;
    const Il2CppAssembly** assemblies = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &size);
    for (int i = 0; i < size; ++i) {
        const Il2CppImage* img = (void *)il2cpp_assembly_get_image(assemblies[i]);
        const char *img_name = il2cpp_image_get_name(img);
        if (strcmp(img_name, image) == 0) {
            return img;
        }
    }
    return 0;
}

void *Il2CppGetClassType(const char *image, const char *namespaze, const char *clazz) {
    static std::map<std::string, void *> cache;
    std::string s = image;
    s += namespaze;
    s += clazz;
    if (cache.count(s) > 0) return cache[s];
    void *img = Il2CppGetImageByName(image);
    if (!img) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find image %s!", image);
        return 0;
    }
    void *klass = il2cpp_class_from_name(img, namespaze, clazz);
    if (!klass) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find class %s!", clazz);
        return 0;
    }
    cache[s] = klass;
    return klass;
}

void *Il2CppGetMethodOffset(const char *image, const char *namespaze, const char *clazz, const char *name, int argsCount) {
    void *img = Il2CppGetImageByName(image);
    if (!img) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find image %s!", image);
        return 0;
    }
    void *klass = Il2CppGetClassType(image, namespaze, clazz);
    if (!klass) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find method %s!", name);
        return 0;
    }
    void **method = (void **)il2cpp_class_get_method_from_name(klass, name, argsCount);
    if (!method) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find method %s in class %s!", name, clazz);
        return 0;
    }
    LOG(ANDROID_LOG_DEBUG, g_LogTag, "%s - [%s] %s::%s: %p", image, namespaze, clazz, name, *method);
    return *method;
}

void *Il2CppGetMethodOffset(const char *image, const char *namespaze, const char *clazz, const char *name, char **args, int argsCount) {
    void *img = Il2CppGetImageByName(image);
    if (!img) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find image %s!", image);
        return 0;
    }
    void *klass = Il2CppGetClassType(image, namespaze, clazz);
    if (!klass) {
        LOG(ANDROID_LOG_ERROR, g_LogTag, "Can't find class %s for method %s!", clazz, name);
        return 0;
    }
    void *iter = 0;
    int score = 0;
    void **method = (void **)il2cpp_class_get_methods(klass, &iter);

    while (method) {
        const char *fname = il2cpp_method_get_name(method);
        if (strcmp(fname, name) == 0) {
            for (int i = 0; i < argsCount; i++) {
                void *arg = il2cpp_method_get_param(method, i);
                if (arg) {
                    const char *tname = il2cpp_type_get_name(arg);
                    if (strcmp(tname, args[i]) == 0) {
                        score++;
                    } else {
                        LOG(ANDROID_LOG_INFO, g_LogTag, "Argument at index %d didn't matched requested argument!\n\tRequested: %s\n\tActual: %s\nnSkipping function...", i, args[i], tname);
                        score = 0;
                        goto skip;
                    }
                }
            }
        }
    skip:
        if (score == argsCount) {
            LOG(ANDROID_LOG_DEBUG, g_LogTag, "%s - [%s] %s::%s: %p", image, namespaze, clazz, name, *method);
            return *method;
        }
        method = (void **)il2cpp_class_get_methods(klass, &iter);
    }
    LOG(ANDROID_LOG_ERROR, g_LogTag, "Cannot find function %s in class %s!", name, clazz);
    return 0;
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
    
    _methods["Screen::SetResolution"] = Il2CppGetMethodOffset("UnityEngine.CoreModule.dll", "UnityEngine", "Screen", "SetResolution", 3);
    DobbyHook((void *)&_methods["Screen::SetResolution"], (void *)SetResolutionn, (void **)&_SetResolutionn);
    
    return nullptr;
}
