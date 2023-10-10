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

bool SetCustomResolution = true;
void (*_SetResolutionn)(...);
void SetResolutionn(int width, int height, bool fullscreen){
if(SetCustomResolution){
  width = glWidth;
 height = glHeight;
}
_SetResolutionn(width, height, fullscreen);
}
///////////
bool HandleInputEvent(JNIEnv *env, int motionEvent, int x, int y, int p);

typedef enum { TOUCH_ACTION_DOWN = 0, TOUCH_ACTION_UP, TOUCH_ACTION_MOVE } TOUCH_ACTION;

typedef struct {
    TOUCH_ACTION action;
    float x;
    float y;
    int pointers;
    float y_velocity;
    float x_velocity;
}TOUCH_EVENT;
TOUCH_EVENT g_LastTouchEvent;

bool  HandleInputEvent(JNIEnv *env, int motionEvent, int x, int y, int p) {
    float velocity_y = (float)((float)y - g_LastTouchEvent.y) / 100.f;
    g_LastTouchEvent = {.action = (TOUCH_ACTION) motionEvent, .x = static_cast<float>(x), .y = static_cast<float>(y), .pointers = p, .y_velocity = velocity_y};
    ImGuiIO &io = ImGui::GetIO();
    io.MousePos.x = g_LastTouchEvent.x;
    io.MousePos.y = g_LastTouchEvent.y;
    if(motionEvent == 2){
        if (g_LastTouchEvent.pointers > 1) {
            io.MouseWheel = g_LastTouchEvent.y_velocity;
            io.MouseDown[0] = false;
        }
        else {
            io.MouseWheel = 0;
        }
    }
    if(motionEvent == 0){
        io.MouseDown[0] = true;
    }
    if(motionEvent == 1){
        io.MouseDown[0] = false;
    }
    return true;
}

bool (*old_nativeInjectEvent )(JNIEnv*, jobject ,jobject event);
bool hook_nativeInjectEvent(JNIEnv* env, jobject instance,jobject event){
    jclass MotionEvent = env->FindClass(("android/view/MotionEvent"));
    if(!MotionEvent){
        LOGI("Can't find MotionEvent!");
    }

    if(!env->IsInstanceOf(event, MotionEvent)){
        return old_nativeInjectEvent(env, instance, event);
    }
    //LOGD("Processing Touch Event!");
    jmethodID id_getAct = env->GetMethodID(MotionEvent, ("getActionMasked"), ("()I"));
    jmethodID id_getX = env->GetMethodID(MotionEvent, ("getX"), ("()F"));
    jmethodID id_getY = env->GetMethodID(MotionEvent, ("getY"), ("()F"));
    jmethodID id_getPs = env->GetMethodID(MotionEvent, ("getPointerCount"), ("()I"));
    HandleInputEvent(env, env->CallIntMethod(event, id_getAct),env->CallFloatMethod(event, id_getX), env->CallFloatMethod(event, id_getY), env->CallIntMethod(event, id_getPs));
    if (!ImGui::GetIO().MouseDownOwnedUnlessPopupClose[0]){
        return old_nativeInjectEvent(env, instance, event);
    }
    return false;
}

jint (*old_RegisterNatives )(JNIEnv*, jclass, JNINativeMethod*,jint);
jint hook_RegisterNatives(JNIEnv* env, jclass destinationClass, JNINativeMethod* methods,
                          jint totalMethodCount){

    int currentNativeMethodNumeration;
    for (currentNativeMethodNumeration = 0; currentNativeMethodNumeration < totalMethodCount; ++currentNativeMethodNumeration )
    {
        if (!strcmp(methods[currentNativeMethodNumeration].name, ("nativeInjectEvent")) ){
            DobbyHook(methods[currentNativeMethodNumeration].fnPtr, (void*)hook_nativeInjectEvent, (void **)&old_nativeInjectEvent);
        }
    }
    //SearchActivity(env);
    return old_RegisterNatives(env, destinationClass, methods, totalMethodCount);
}
///////////
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
    ImGui::Checkbox("MapHack", &norec);
    
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
    
    DobbyHook((void *) KittyMemory::getAbsoluteAddress("libil2cpp.so", il2cpp_base + 0x44A3F00), (void *) SetResolutionn, (void **) &_SetResolutionn);

    return nullptr;
}
