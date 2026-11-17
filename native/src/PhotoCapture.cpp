#include "PhotoCapture.h"

#include "Log.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/api-level.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>

namespace savr::photocapture {
namespace {

using Clock = std::chrono::steady_clock;
constexpr auto QueueTimeout = std::chrono::seconds(2);
constexpr int MaxPendingWrites = 2;

struct Readback {
    int sequence{-1};
    GLuint texture{};
    int width{}, height{};
    bool flipHorizontal{}, flipVertical{};
    Clock::time_point queuedAt{};
    EGLContext context{EGL_NO_CONTEXT};
    GLuint buffer{};
    GLsync fence{};

    std::size_t Bytes() const {
        return static_cast<std::size_t>(width) * height * 4;
    }
};

struct ScopedPackState {
    static constexpr GLenum Fields[] = {
        GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS
    };
    GLint framebuffer{}, buffer{}, values[4]{};

    ScopedPackState() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
        for (int i = 0; i < 4; ++i) {
            glGetIntegerv(Fields[i], &values[i]);
            glPixelStorei(Fields[i], i == 0 ? 1 : 0);
        }
    }

    ~ScopedPackState() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(buffer));
        for (int i = 0; i < 4; ++i)
            glPixelStorei(Fields[i], values[i]);
    }
};

std::mutex gMutex;
Readback gReadback;
std::atomic<bool> gTextureBusy{false};
std::atomic<int> gPendingWrites{0};
std::atomic<Clock::duration::rep> gFailureUntil{};
JavaVM* gVm{};
jclass gApplication{};
jmethodID gSavePhoto{};
std::atomic<bool> gStorageReady{false};

void ClearReadback() {
    gReadback = {};
    gTextureBusy.store(false, std::memory_order_release);
}

void SavePhoto(Readback info, std::unique_ptr<unsigned char[]> pixels) {
    JNIEnv* env{};
    bool saved = false;
    if (gVm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
        jobject buffer = env->NewDirectByteBuffer(pixels.get(), info.Bytes());
        if (buffer) {
            saved = env->CallStaticBooleanMethod(
                gApplication, gSavePhoto, buffer, info.width, info.height,
                static_cast<jboolean>(info.flipHorizontal),
                static_cast<jboolean>(info.flipVertical)) == JNI_TRUE;
            env->DeleteLocalRef(buffer);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            saved = false;
        }
        gVm->DetachCurrentThread();
    }
    if (!saved) ReportFailure("MediaStore write");
    gPendingWrites.fetch_sub(1, std::memory_order_release);
}

bool StartReadback(Readback& info) {
    const ScopedPackState pack;
    GLuint framebuffer{};
    glGenFramebuffers(1, &framebuffer);
    if (!framebuffer) return false;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, info.texture, 0);
    const bool complete =
        glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (complete) {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glGenBuffers(1, &info.buffer);
        if (info.buffer) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, info.buffer);
            glBufferData(GL_PIXEL_PACK_BUFFER, info.Bytes(), nullptr, GL_STREAM_READ);
            GLint size{};
            glGetBufferParameteriv(GL_PIXEL_PACK_BUFFER, GL_BUFFER_SIZE, &size);
            if (size == static_cast<GLint>(info.Bytes())) {
                glReadPixels(0, 0, info.width, info.height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                if (glGetError() == GL_NO_ERROR)
                    info.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            }
        }
    }
    glDeleteFramebuffers(1, &framebuffer);
    if (info.fence) {
        glFlush();
        return true;
    }
    if (info.buffer) glDeleteBuffers(1, &info.buffer);
    return false;
}

void DispatchSave(const Readback& info) {
    if (gPendingWrites.fetch_add(1, std::memory_order_acq_rel) >= MaxPendingWrites) {
        gPendingWrites.fetch_sub(1, std::memory_order_release);
        glDeleteBuffers(1, &info.buffer);
        ReportFailure("photo write queue full");
        return;
    }
    auto pixels = std::unique_ptr<unsigned char[]>(new (std::nothrow) unsigned char[info.Bytes()]);
    GLint savedBuffer{};
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &savedBuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, info.buffer);
    const void* mapped = pixels ? glMapBufferRange(
        GL_PIXEL_PACK_BUFFER, 0, info.Bytes(), GL_MAP_READ_BIT) : nullptr;
    if (mapped) std::memcpy(pixels.get(), mapped, info.Bytes());
    const bool copied = mapped && glUnmapBuffer(GL_PIXEL_PACK_BUFFER) == GL_TRUE;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(savedBuffer));
    glDeleteBuffers(1, &info.buffer);
    if (copied) {
        try {
            std::thread(SavePhoto, info, std::move(pixels)).detach();
            return;
        } catch (const std::system_error&) {
            ReportFailure("photo worker unavailable");
        }
    } else {
        ReportFailure("photo readback copy");
    }
    gPendingWrites.fetch_sub(1, std::memory_order_release);
}

} // namespace

void InitializeStorage(JNIEnv* env, jclass application) {
    if (android_get_device_api_level() < 29 || env->GetJavaVM(&gVm) != JNI_OK) return;
    gSavePhoto = env->GetStaticMethodID(application, "savePhoto",
        "(Ljava/nio/ByteBuffer;IIZZ)Z");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!gSavePhoto) return;
    gApplication = static_cast<jclass>(env->NewGlobalRef(application));
    gStorageReady.store(gApplication != nullptr, std::memory_order_release);
}

bool QueueReadback(int sequence, unsigned int texture, int width, int height,
                   bool flipHorizontal, bool flipVertical) {
    if (!gStorageReady.load(std::memory_order_acquire)) {
        ReportFailure("MediaStore unavailable");
        return false;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    if (gReadback.sequence >= 0 || !texture) {
        ReportFailure("photo texture unavailable");
        return false;
    }
    gReadback = {sequence, texture, width, height, flipHorizontal, flipVertical, Clock::now()};
    gTextureBusy.store(true, std::memory_order_release);
    return true;
}

bool TextureBusy() {
    if (!gTextureBusy.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> lock(gMutex);
    if (gReadback.sequence >= 0 && !gReadback.fence &&
        Clock::now() - gReadback.queuedAt > QueueTimeout) {
        ClearReadback();
        ReportFailure("stereo readback timed out");
    }
    return gReadback.sequence >= 0;
}

void ReadbackAfterStereoFence(int sequence) {
    if (!gTextureBusy.load(std::memory_order_acquire)) return;
    std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
    if (!lock || gReadback.sequence < 0) return;
    const EGLContext context = eglGetCurrentContext();
    if (context == EGL_NO_CONTEXT) return;
    if (!gReadback.fence) {
        if (sequence < gReadback.sequence) return;
        gReadback.context = context;
        if (!StartReadback(gReadback)) {
            ClearReadback();
            ReportFailure("GPU readback unavailable");
        }
        return;
    }
    if (gReadback.context != context) {
        ClearReadback();
        ReportFailure("photo graphics context changed");
        return;
    }
    const GLenum result = glClientWaitSync(gReadback.fence, 0, 0);
    if (result == GL_TIMEOUT_EXPIRED) return;
    const Readback finished = gReadback;
    glDeleteSync(finished.fence);
    if (result == GL_WAIT_FAILED) glDeleteBuffers(1, &finished.buffer);
    ClearReadback();
    lock.unlock();
    if (result == GL_WAIT_FAILED)
        ReportFailure("photo GPU fence failed");
    else
        DispatchSave(finished);
}

void ReportFailure(const char* reason) {
    const auto end = Clock::now() + std::chrono::seconds(4);
    gFailureUntil.store(end.time_since_epoch().count(), std::memory_order_relaxed);
    LOGW("[photo] %s", reason);
}

bool FailureVisible() {
    return Clock::now().time_since_epoch().count() <
        gFailureUntil.load(std::memory_order_relaxed);
}

} // namespace savr::photocapture
