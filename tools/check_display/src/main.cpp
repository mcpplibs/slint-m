// glprobe -- three questions, no dependencies beyond libdl:
//   1. which of the libraries a GUI toolkit dlopen()s can this program (under mcpp's loader) load?
//   2. which EGL devices/vendors answer, and how many configurations does each give? (no X needed)
//   3. if DISPLAY is set: what EGL on X11 gives, for the configuration templates a toolkit asks for.
//   4. if DISPLAY is set: what GLX gives -- glutin tries EGL, then GLX, and "argument does not name a
//      valid config" is how it spells GLXBadFBConfig, so GLX is the other half of Slint's failure.
// Everything is resolved with dlopen/dlsym, so nothing here depends on how the ecosystem
// provides headers or libraries: it sees exactly what a toolkit's own dlopen() would see.
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <unistd.h>
#include <vector>

using EGLint = int;
using EGLBoolean = unsigned;
using EGLDisplay = void*;
using EGLConfig = void*;
using EGLDeviceEXT = void*;

namespace {
constexpr EGLint kNone = 0x3038, kSurfaceType = 0x3033, kWindowBit = 0x0004, kPbufferBit = 0x0001,
                 kRenderableType = 0x3040, kEs2Bit = 0x0004, kOpenGlBit = 0x0008,
                 kVendor = 0x3053, kVersion = 0x3054, kOpenGlApi = 0x30A2, kEsApi = 0x30A0;
constexpr unsigned kPlatformX11 = 0x31D5, kPlatformDevice = 0x313F;

struct Egl {
    void* lib = nullptr;
    void* (*getProc)(const char*) = nullptr;
    EGLint (*getError)() = nullptr;
    const char* (*queryString)(EGLDisplay, EGLint) = nullptr;
    EGLBoolean (*initialize)(EGLDisplay, EGLint*, EGLint*) = nullptr;
    EGLBoolean (*terminate)(EGLDisplay) = nullptr;
    EGLBoolean (*chooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*) = nullptr;
    EGLBoolean (*bindApi)(unsigned) = nullptr;
    EGLDisplay (*platformDisplay)(unsigned, void*, const EGLint*) = nullptr;
    EGLBoolean (*queryDevices)(EGLint, EGLDeviceEXT*, EGLint*) = nullptr;
};

bool g_nvidiaDevice = false;  // an EGL display whose vendor is NVIDIA initialised

template <class F> void sym(F& f, void* lib, const char* name) { f = reinterpret_cast<F>(dlsym(lib, name)); }

// How many configurations does `d` offer for the given API / surface kind.
int configs(const Egl& e, EGLDisplay d, unsigned api, EGLint renderable, EGLint surface) {
    e.bindApi(api);
    const EGLint attrs[] = {kSurfaceType, surface, kRenderableType, renderable, kNone};
    EGLConfig cfg[64];
    EGLint n = 0;
    if (!e.chooseConfig(d, attrs, cfg, 64, &n)) return -static_cast<int>(e.getError());
    return n;
}

// Count configurations for an API and a list of EGL attribute/value pairs.
int configsWith(const Egl& e, EGLDisplay d, unsigned api, std::initializer_list<EGLint> pairs) {
    e.bindApi(api);
    std::vector<EGLint> attrs(pairs);
    attrs.push_back(kNone);
    EGLConfig cfg[256];
    EGLint n = 0;
    if (!e.chooseConfig(d, attrs.data(), cfg, 256, &n)) return -static_cast<int>(e.getError());
    return n;
}

struct XErrorEvent { int type; void* display; unsigned long resourceid; unsigned long serial;
                     unsigned char error_code, request_code, minor_code; };
int g_xErrors = 0, g_lastXError = 0, g_lastXRequest = 0;
int xErrorHandler(void*, XErrorEvent* ev) { ++g_xErrors; g_lastXError = ev->error_code; g_lastXRequest = ev->request_code; return 0; }

void printEnv() {
    std::puts("== 0. environment this program runs in");
    for (const char* n : {"DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE", "LD_LIBRARY_PATH", "LD_PRELOAD",
                          "__EGL_VENDOR_LIBRARY_DIRS", "__EGL_VENDOR_LIBRARY_FILENAMES", "__GLX_VENDOR_LIBRARY_NAME",
                          "LIBGL_DRIVERS_PATH", "LIBGL_ALWAYS_SOFTWARE", "MESA_LOADER_DRIVER_OVERRIDE",
                          "GBM_BACKENDS_PATH", "VK_ICD_FILENAMES", "VK_DRIVER_FILES", "SLINT_BACKEND", "SLINT_HEADLESS"}) {
        const char* v = std::getenv(n);
        if (v && *v) std::printf("  %s=%s\n", n, v);
    }
}

// Initialise a display and report what it is and what it offers. Returns the window-capable
// ES2 configuration count (negative: an EGL error code), or -1 when the display did not initialise.
int report(const Egl& e, const char* label, EGLDisplay d) {
    if (!d) { std::printf("  %-26s no display (eglGetError=0x%x)\n", label, e.getError()); return -1; }
    EGLint major = 0, minor = 0;
    if (!e.initialize(d, &major, &minor)) {
        std::printf("  %-26s eglInitialize failed (eglGetError=0x%x)\n", label, e.getError());
        return -1;
    }
    const char* vendor = e.queryString(d, kVendor);
    if (vendor && std::strstr(vendor, "NVIDIA")) g_nvidiaDevice = true;
    const int es2 = configs(e, d, kEsApi, kEs2Bit, kWindowBit);
    const int gl = configs(e, d, kOpenGlApi, kOpenGlBit, kWindowBit);
    const int pb = configs(e, d, kEsApi, kEs2Bit, kPbufferBit);
    std::printf("  %-26s EGL %d.%d  vendor=%s  configs: ES2/window=%d  GL/window=%d  ES2/pbuffer=%d\n",
                label, major, minor, vendor ? vendor : "?", es2, gl, pb);
    e.terminate(d);
    return es2;
}
}  // namespace

int main() {
    printEnv();
    std::puts("\n== 1. libraries a toolkit dlopen()s, loaded by this program");
    bool nvidiaLibLoads = false;
    for (const char* n : {"libX11.so.6", "libXcursor.so.1", "libXi.so.6", "libxcb.so.1", "libxkbcommon.so.0",
                          "libwayland-client.so.0", "libEGL.so.1", "libGL.so.1",
                          "libEGL_mesa.so.0", "libEGL_nvidia.so.0", "libGLX_nvidia.so.0"}) {
        void* h = dlopen(n, RTLD_NOW);
        std::printf("  %-24s %s\n", n, h ? "loaded" : dlerror());
        if (h && std::strcmp(n, "libEGL_nvidia.so.0") == 0) nvidiaLibLoads = true;
    }
    const bool hostNvidia = access("/proc/driver/nvidia", F_OK) == 0;

    Egl e;
    e.lib = dlopen("libEGL.so.1", RTLD_NOW);
    if (!e.lib) { std::puts("\nlibEGL.so.1 does not load: nothing more to probe"); return 3; }
    sym(e.getProc, e.lib, "eglGetProcAddress");
    sym(e.getError, e.lib, "eglGetError");
    sym(e.queryString, e.lib, "eglQueryString");
    sym(e.initialize, e.lib, "eglInitialize");
    sym(e.terminate, e.lib, "eglTerminate");
    sym(e.chooseConfig, e.lib, "eglChooseConfig");
    sym(e.bindApi, e.lib, "eglBindAPI");
    e.platformDisplay = reinterpret_cast<decltype(e.platformDisplay)>(e.getProc("eglGetPlatformDisplayEXT"));
    e.queryDevices = reinterpret_cast<decltype(e.queryDevices)>(e.getProc("eglQueryDevicesEXT"));

    std::puts("\n== 2. EGL devices (no display server needed)");
    if (!e.queryDevices || !e.platformDisplay) {
        std::puts("  EGL_EXT_device_enumeration is not available");
    } else {
        EGLDeviceEXT dev[16];
        EGLint n = 0;
        if (!e.queryDevices(16, dev, &n)) n = 0;
        std::printf("  %d device(s)\n", n);
        for (int i = 0; i < n; ++i) {
            char label[32];
            std::snprintf(label, sizeof label, "device %d", i);
            report(e, label, e.platformDisplay(kPlatformDevice, dev[i], nullptr));
        }
    }

    auto summary = [&](const char* x11) {
        std::printf("\nSUMMARY: host NVIDIA driver: %s | libEGL_nvidia.so.0 loads: %s | NVIDIA EGL device initialised: %s | EGL on X11: %s\n",
                    hostNvidia ? "yes" : "no", nvidiaLibLoads ? "yes" : "no", g_nvidiaDevice ? "yes" : "no", x11);
        if (hostNvidia && !g_nvidiaDevice)
            std::puts("PROBLEM: the host has an NVIDIA driver, but this program gets no NVIDIA EGL device (the host-link package is absent or stale).");
        return hostNvidia && !g_nvidiaDevice ? 6 : 0;
    };

    std::puts("\n== 3. EGL on X11 (what Slint's glutin does)");
    const char* display = std::getenv("DISPLAY");
    if (!display || !*display) { std::puts("  DISPLAY is not set: skipped. Run this from a desktop terminal."); return summary("skipped (no DISPLAY)"); }
    void* x11 = dlopen("libX11.so.6", RTLD_NOW);
    using OpenDisplay = void* (*)(const char*);
    OpenDisplay xopen = x11 ? reinterpret_cast<OpenDisplay>(dlsym(x11, "XOpenDisplay")) : nullptr;
    void* xd = xopen ? xopen(nullptr) : nullptr;
    if (!xd) { std::printf("  XOpenDisplay(%s) failed (libX11 %s)\n", display, x11 ? "loaded" : "did not load"); summary("could not open the display"); return 4; }
    EGLDisplay xegl = e.platformDisplay ? e.platformDisplay(kPlatformX11, xd, nullptr) : nullptr;
    const int n = report(e, "X11", xegl);
    if (xegl && e.initialize(xegl, nullptr, nullptr)) {
        constexpr EGLint kRed = 0x3024, kGreen = 0x3023, kBlue = 0x3022, kAlpha = 0x3021, kStencil = 0x3026,
                         kCaveat = 0x3027, kDepth = 0x3025;
        std::puts("  configuration templates (window surface):");
        std::printf("    %-46s %d\n", "ES2",                                   configsWith(e, xegl, kEsApi, {kSurfaceType, kWindowBit, kRenderableType, kEs2Bit}));
        std::printf("    %-46s %d\n", "ES2 + RGBA8888",                        configsWith(e, xegl, kEsApi, {kSurfaceType, kWindowBit, kRenderableType, kEs2Bit, kRed, 8, kGreen, 8, kBlue, 8, kAlpha, 8}));
        std::printf("    %-46s %d\n", "ES2 + RGBA8888 + stencil 8",            configsWith(e, xegl, kEsApi, {kSurfaceType, kWindowBit, kRenderableType, kEs2Bit, kRed, 8, kGreen, 8, kBlue, 8, kAlpha, 8, kStencil, 8}));
        std::printf("    %-46s %d\n", "ES2 + RGBA8888 + stencil 8 + depth 24", configsWith(e, xegl, kEsApi, {kSurfaceType, kWindowBit, kRenderableType, kEs2Bit, kRed, 8, kGreen, 8, kBlue, 8, kAlpha, 8, kStencil, 8, kDepth, 24}));
        std::printf("    %-46s %d\n", "ES2 + caveat NONE (hardware only)",     configsWith(e, xegl, kEsApi, {kSurfaceType, kWindowBit, kRenderableType, kEs2Bit, kCaveat, kNone}));
        std::printf("    %-46s %d\n", "OpenGL + RGBA8888 + stencil 8",         configsWith(e, xegl, kOpenGlApi, {kSurfaceType, kWindowBit, kRenderableType, kOpenGlBit, kRed, 8, kGreen, 8, kBlue, 8, kAlpha, 8, kStencil, 8}));
    }
    std::printf("\nRESULT 3: EGL on X11 offers %d window configuration(s) -> %s\n", n, n > 0 ? "ok" : "NONE");

    std::puts("\n== 4. GLX on X11 (glutin's fallback; what an NVIDIA X server makes it ask)");
    int glxFb = -1;
    void* glx = dlopen("libGLX.so.0", RTLD_NOW);
    if (!glx) glx = dlopen("libGL.so.1", RTLD_NOW);
    if (!glx) {
        std::puts("  neither libGLX.so.0 nor libGL.so.1 loads");
    } else {
        using QueryExt = int (*)(void*, int*, int*);
        using GetStr = const char* (*)(void*, int);
        using ServerStr = const char* (*)(void*, int, int);
        using Choose = void** (*)(void*, int, const int*, int*);
        using SetHandler = int (*(*)(int (*)(void*, XErrorEvent*)))(void*, XErrorEvent*);
        using Sync = int (*)(void*, int);
        using Free = int (*)(void*);
        auto queryExt = reinterpret_cast<QueryExt>(dlsym(glx, "glXQueryExtension"));
        auto clientStr = reinterpret_cast<GetStr>(dlsym(glx, "glXGetClientString"));
        auto serverStr = reinterpret_cast<ServerStr>(dlsym(glx, "glXQueryServerString"));
        auto choose = reinterpret_cast<Choose>(dlsym(glx, "glXChooseFBConfig"));
        auto setHandler = reinterpret_cast<SetHandler>(dlsym(x11, "XSetErrorHandler"));
        auto xsync = reinterpret_cast<Sync>(dlsym(x11, "XSync"));
        auto xfree = reinterpret_cast<Free>(dlsym(x11, "XFree"));
        if (!queryExt || !choose || !setHandler || !xsync) {
            std::puts("  GLX entry points are missing");
        } else {
            setHandler(xErrorHandler);
            int errBase = 0, evBase = 0;
            if (!queryExt(xd, &errBase, &evBase)) {
                std::puts("  the X server has no GLX extension");
            } else {
                std::printf("  GLX client vendor: %s | server vendor: %s\n",
                            clientStr ? clientStr(xd, 1) : "?", serverStr ? serverStr(xd, 0, 1) : "?");
                const int attrs[] = {0x8010 /*DRAWABLE_TYPE*/, 0x1 /*WINDOW_BIT*/, 0x8011 /*RENDER_TYPE*/, 0x1 /*RGBA_BIT*/,
                                     0x8012 /*X_RENDERABLE*/, 1, 5 /*DOUBLEBUFFER*/, 1, 0x8 /*RED*/, 8, 0x9, 8, 0xA, 8, 0xB /*ALPHA*/, 8,
                                     0xD /*STENCIL*/, 8, 0};
                int count = 0;
                void** fb = choose(xd, 0, attrs, &count);
                xsync(xd, 0);
                glxFb = fb ? count : 0;
                if (fb && xfree) xfree(fb);
                std::printf("  glXChooseFBConfig: %d configuration(s)", glxFb);
                if (g_xErrors) std::printf(", X error %d (GLX error base %d => GLX error %d; 9 is GLXBadFBConfig), request %d",
                                           g_lastXError, errBase, g_lastXError - errBase, g_lastXRequest);
                std::puts("");
            }
        }
    }
    std::printf("\nRESULT 4: GLX on X11 offers %d framebuffer configuration(s) -> %s\n", glxFb,
                glxFb > 0 ? "ok" : "NONE or an X error: glutin would report \"argument does not name a valid config\"");
    const int rc = summary(n > 0 ? (glxFb > 0 ? "EGL ok, GLX ok" : "EGL ok, GLX NONE") : "EGL NONE");
    if (n <= 0) return 2;
    if (glxFb <= 0) return 7;
    return rc;
}
