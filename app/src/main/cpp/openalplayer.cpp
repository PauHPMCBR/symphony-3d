#include <string>
#include <cmath>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <memory>
#include <random>

#include <jni.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <android/log.h>

#define LOG_TAG "AudioEngine"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#include "utils.h"
#include "soundLoader.h"
#include "openalInitializer.h"

// Constants
constexpr int SAMPLE_RATE = 44100;
constexpr float INITIAL_STEREO_ANGLE = M_PI / 6.0f;
constexpr ALfloat LISTENER_ORIENTATION[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

// Type aliases
using SoundId = std::string;
using AlSourcePair = std::pair<ALuint, ALuint>;
using AlBufferPair = std::pair<ALuint, ALuint>;

// Forward declarations
class AudioEngine;

class SoundInstance;

// Sound instance class
class SoundInstance {
private:
    std::string m_filePath;
    SoundId m_soundId;
    AlSourcePair m_sources;
    AlBufferPair m_buffers;
    float m_duration;
    std::atomic<bool> m_isPlaying;

public:
    SoundInstance(std::string filePath, SoundId soundId)
            : m_filePath(std::move(filePath)), m_soundId(std::move(soundId)),
              m_sources({AL_NONE, AL_NONE}), m_buffers({AL_NONE, AL_NONE}),
              m_duration(0.0f), m_isPlaying(false) {}

    ~SoundInstance() {
        stop();
    }

    bool load() {
        m_buffers = LoadSound(m_filePath.c_str());
        if (!m_buffers.first) {
            LOGE("Failed to load sound buffer for: %s", m_filePath.c_str());
            return false;
        }

        // Create first source
        alGenSources(1, &m_sources.first);
        setupSource(m_sources.first, m_buffers.first);

        // Create second source for stereo if we have a second buffer
        if (m_buffers.second != AL_NONE) {
            alGenSources(1, &m_sources.second);
            setupSource(m_sources.second, m_buffers.second);
        } else {
            m_sources.second = AL_NONE;
        }

        m_duration = getDurationSeconds(m_buffers.first);
        LOGD("Sound loaded successfully: %s (duration: %.2fs, stereo: %s)",
             m_soundId.c_str(), m_duration,
             (m_buffers.second != AL_NONE) ? "yes" : "no");
        return true;
    }

    void play(const std::function<void()> &onFinished) {
        if (m_isPlaying) return;

        m_isPlaying = true;
        alSourcePlay(m_sources.first);
        if (hasStereo()) {
            alSourcePlay(m_sources.second);
        }

        // Start monitoring thread
        std::thread monitorThread([this, onFinished]() {
            ALint state;
            do {
                sleep(1);
                alGetSourcei(m_sources.first, AL_SOURCE_STATE, &state);
            } while (alGetError() == AL_NO_ERROR && state == AL_PLAYING && m_isPlaying);

            if (m_isPlaying) {
                onFinished();
            }
        });
        monitorThread.detach();

        LOGD("Started playing sound: %s", m_soundId.c_str());
    }

    void stop() {
        m_isPlaying = false;

        // Stop and delete sources
        if (m_sources.first != AL_NONE) {
            alSourceStop(m_sources.first);
            alDeleteSources(1, &m_sources.first);
            m_sources.first = AL_NONE;
        }

        if (m_sources.second != AL_NONE) {
            alSourceStop(m_sources.second);
            alDeleteSources(1, &m_sources.second);
            m_sources.second = AL_NONE;
        }

        // Delete buffers
        if (m_buffers.first != AL_NONE) {
            alDeleteBuffers(1, &m_buffers.first);
            m_buffers.first = AL_NONE;
        }

        if (m_buffers.second != AL_NONE) {
            alDeleteBuffers(1, &m_buffers.second);
            m_buffers.second = AL_NONE;
        }

        LOGD("Sound stopped: %s", m_soundId.c_str());
    }

    void pause() {
        if (!m_isPlaying) return;

        alSourcePause(m_sources.first);
        if (hasStereo()) {
            alSourcePause(m_sources.second);
        }
        LOGD("Sound paused: %s", m_soundId.c_str());
    }

    void resume() {
        if (!m_isPlaying) return;

        alSourcePlay(m_sources.first);
        if (hasStereo()) {
            alSourcePlay(m_sources.second);
        }
        LOGD("Sound resumed: %s", m_soundId.c_str());
    }

    void updatePosition(float angle, float radius, float height, float stereoAngle) const {
        if (!hasStereo()) {
            // Mono sound - single source
            setPosition(m_sources.first, angle, radius, height);
        } else {
            // Stereo sound - two sources with stereo separation
            setPosition(m_sources.first, angle - stereoAngle / 2, radius, height);
            setPosition(m_sources.second, angle + stereoAngle / 2, radius, height);
        }
    }

    void setPlaybackTime(float seconds) const {
        alSourcef(m_sources.first, AL_SEC_OFFSET, seconds);
        if (hasStereo()) {
            alSourcef(m_sources.second, AL_SEC_OFFSET, seconds);
        }
    }

    float getPlaybackTime() const {
        ALfloat seconds = 0.0f;
        alGetSourcef(m_sources.first, AL_SEC_OFFSET, &seconds);
        return (alGetError() == AL_NO_ERROR) ? seconds : -1.0f;
    }

    float getDuration() const { return m_duration; }

    const SoundId &getId() const { return m_soundId; }

    bool isPlaying() const { return m_isPlaying; }

    bool hasStereo() const { return m_buffers.second != AL_NONE; }

private:
    static void setupSource(ALuint source, ALuint buffer) {
        alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(source, AL_POSITION, 0.0f, 0.0f, -1.0f);
        alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));

        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOGE("Error setting up source: %s", alGetString(error));
        }
    }
};

// Main audio engine class
class AudioEngine {
private:
    ALCdevice *m_device;
    ALCcontext *m_context;
    JavaVM *m_javaVM;
    jobject m_globalCallback;
    std::atomic<bool> m_stopFlag;

    std::map<SoundId, std::unique_ptr<SoundInstance>> m_activeSounds;
    std::mutex m_soundsMutex;
    float m_stereoAngle;

    // UUID generation
    static std::string generateSoundID(std::string filePath) {
        static std::atomic<int> counter{0};

        size_t lastSlash = filePath.find_last_of("/\\");
        std::string fileName = (lastSlash != std::string::npos)
                               ? filePath.substr(lastSlash + 1)
                               : filePath;

        int idNumber = ++counter;
        return std::to_string(idNumber) + "_" + fileName;
    }

public:
    AudioEngine() : m_device(nullptr), m_context(nullptr), m_javaVM(nullptr),
                    m_globalCallback(nullptr), m_stopFlag(false),
                    m_stereoAngle(INITIAL_STEREO_ANGLE) {}

    ~AudioEngine() {
        cleanup();
    }

    // Initialization
    bool initialize(JNIEnv *env, const std::string &selectedHrtf) {
        LOGI("Initializing OpenAL with HRTF: %s", selectedHrtf.c_str());

        m_device = alcOpenDevice(nullptr);
        if (!m_device) {
            LOGE("Failed to open OpenAL device");
            return false;
        }

        loadHRTF(m_device, selectedHrtf.c_str());

        m_context = alcCreateContext(m_device, nullptr);
        if (!m_context) {
            LOGE("Failed to create OpenAL context");
            alcCloseDevice(m_device);
            m_device = nullptr;
            return false;
        }

        if (!alcMakeContextCurrent(m_context)) {
            LOGE("Failed to make context current");
            cleanup();
            return false;
        }

        // Set up listener
        alListener3f(AL_POSITION, 0.0f, 0.0f, 1.0f);
        alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        alListenerfv(AL_ORIENTATION, LISTENER_ORIENTATION);

        LOGI("OpenAL initialized successfully");
        return true;
    }

    void cleanup() {
        stopAllSounds();

        if (m_context) {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(m_context);
            m_context = nullptr;
        }

        if (m_device) {
            alcCloseDevice(m_device);
            m_device = nullptr;
        }

        if (m_globalCallback) {
            JNIEnv *env;
            m_javaVM->AttachCurrentThread(&env, nullptr);
            env->DeleteGlobalRef(m_globalCallback);
            m_globalCallback = nullptr;
            m_javaVM->DetachCurrentThread();
        }

        LOGI("OpenAL cleanup completed");
    }

    // Sound management
    SoundId createSound(const std::string &filePath) {
        SoundId soundId = generateSoundID(filePath);
        LOGD("Creating sound instance %s for file: %s", soundId.c_str(), filePath.c_str());

        auto sound = std::make_unique<SoundInstance>(filePath, soundId);
        if (!sound->load()) {
            LOGE("Failed to load sound for file: %s", filePath.c_str());
            return "";
        }

        std::lock_guard<std::mutex> lock(m_soundsMutex);
        m_activeSounds[soundId] = std::move(sound);

        return soundId;
    }

    void playSound(const SoundId &soundId) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        if (it != m_activeSounds.end()) {
            it->second->play([this, soundId]() { onSoundFinished(soundId); });
        } else {
            LOGW("Sound not found for ID: %s", soundId.c_str());
        }
    }

    void stopSound(const SoundId &soundId) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        if (it != m_activeSounds.end()) {
            it->second->stop();
            m_activeSounds.erase(it);
        }
    }

    void stopAllSounds() {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        for (auto &[soundId, sound]: m_activeSounds) {
            sound->stop();
        }
        m_activeSounds.clear();
        LOGI("All sounds stopped");
    }

    void pauseSound(const SoundId &soundId) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        if (it != m_activeSounds.end()) {
            it->second->pause();
        }
    }

    void resumeSound(const SoundId &soundId) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        if (it != m_activeSounds.end()) {
            it->second->resume();
        }
    }

    // Sound control
    void setSoundPosition(const SoundId &soundId, float angle, float radius, float height) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        if (it != m_activeSounds.end()) {
            alcSuspendContext(m_context);
            it->second->updatePosition(angle, radius, height, m_stereoAngle);
            alcProcessContext(m_context);

            ALenum error = alGetError();
            if (error != AL_NO_ERROR) {
                LOGW("Error updating position for sound %s: %s", soundId.c_str(),
                     alGetString(error));
            }
        }
    }

    void setPlaybackTime(const SoundId &soundId, float seconds) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        if (it != m_activeSounds.end()) {
            it->second->setPlaybackTime(seconds);
        }
    }

    float getPlaybackTime(const SoundId &soundId) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        return (it != m_activeSounds.end()) ? it->second->getPlaybackTime() : -1.0f;
    }

    float getSoundDuration(const SoundId &soundId) {
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        auto it = m_activeSounds.find(soundId);
        return (it != m_activeSounds.end()) ? it->second->getDuration() : 0.0f;
    }

    // Configuration
    void setStereoAngle(float angle) {
        m_stereoAngle = angle;
        LOGD("Stereo angle set to: %f radians", angle);
    }

    void setCallback(JNIEnv *env, jobject callback) {
        if (m_globalCallback) {
            env->DeleteGlobalRef(m_globalCallback);
        }
        m_globalCallback = env->NewGlobalRef(callback);
    }

    void setJavaVM(JavaVM *vm) {
        m_javaVM = vm;
    }

private:
    void onSoundFinished(const SoundId &soundId) {
        if (!m_javaVM || !m_globalCallback) return;

        JNIEnv *env;
        m_javaVM->AttachCurrentThread(&env, nullptr);

        jclass callbackClass = env->GetObjectClass(m_globalCallback);
        jmethodID onSoundFinishedMethod = env->GetMethodID(
                callbackClass, "onSoundFinished", "(Ljava/lang/String;)V");

        if (onSoundFinishedMethod) {
            jstring jSoundId = env->NewStringUTF(soundId.c_str());
            env->CallVoidMethod(m_globalCallback, onSoundFinishedMethod, jSoundId);
            env->DeleteLocalRef(jSoundId);
        }

        m_javaVM->DetachCurrentThread();

        // Clean up the sound instance
        std::lock_guard<std::mutex> lock(m_soundsMutex);
        m_activeSounds.erase(soundId);

        LOGD("Sound finished and cleaned up: %s", soundId.c_str());
    }
};

// Global audio engine instance
std::unique_ptr<AudioEngine> g_audioEngine;

// JNI Functions
extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    g_audioEngine = std::make_unique<AudioEngine>();
    g_audioEngine->setJavaVM(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_setCallbackNative(JNIEnv *env, jobject thiz,
                                                                             jobject callback) {
    if (g_audioEngine) {
        g_audioEngine->setCallback(env, callback);
    }
}

JNIEXPORT jboolean JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_initOpenAL(JNIEnv *env, jobject thiz,
                                                                      jstring jselectedHrtf) {
    const char *selectedHrtf = env->GetStringUTFChars(jselectedHrtf, nullptr);
    bool result = g_audioEngine->initialize(env, selectedHrtf);
    env->ReleaseStringUTFChars(jselectedHrtf, selectedHrtf);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_cleanupOpenAL(JNIEnv *env, jobject thiz) {
    if (g_audioEngine) {
        g_audioEngine->cleanup();
        g_audioEngine.reset();
    }
}

JNIEXPORT jstring JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_createSound(JNIEnv *env, jobject thiz,
                                                                       jstring jFilePath) {
    const char *filePath = env->GetStringUTFChars(jFilePath, nullptr);
    SoundId soundId = g_audioEngine->createSound(filePath);
    env->ReleaseStringUTFChars(jFilePath, filePath);

    return (soundId.empty()) ? nullptr : env->NewStringUTF(soundId.c_str());
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_playSound(JNIEnv *env, jobject thiz,
                                                                     jstring jSoundId) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    if (g_audioEngine && soundId) {
        g_audioEngine->playSound(soundId);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_stopSound(JNIEnv *env, jobject thiz,
                                                                     jstring jSoundId) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    if (g_audioEngine && soundId) {
        g_audioEngine->stopSound(soundId);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_stopAllSounds(JNIEnv *env, jobject thiz) {
    if (g_audioEngine) {
        g_audioEngine->stopAllSounds();
    }
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_pauseSound(JNIEnv *env, jobject thiz,
                                                                      jstring jSoundId) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    if (g_audioEngine && soundId) {
        g_audioEngine->pauseSound(soundId);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_resumeSound(JNIEnv *env, jobject thiz,
                                                                       jstring jSoundId) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    if (g_audioEngine && soundId) {
        g_audioEngine->resumeSound(soundId);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_setSoundPosition(JNIEnv *env, jobject thiz,
                                                                            jstring jSoundId,
                                                                            jfloat angle,
                                                                            jfloat radius,
                                                                            jfloat height) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    if (g_audioEngine && soundId) {
        g_audioEngine->setSoundPosition(soundId, angle, radius, height);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_setPlaybackTime(JNIEnv *env, jobject thiz,
                                                                           jstring jSoundId,
                                                                           jfloat seconds) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    if (g_audioEngine && soundId) {
        g_audioEngine->setPlaybackTime(soundId, seconds);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
}

JNIEXPORT jfloat JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_getPlaybackTime(JNIEnv *env, jobject thiz,
                                                                           jstring jSoundId) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    float position = -1.0f;
    if (g_audioEngine && soundId) {
        position = g_audioEngine->getPlaybackTime(soundId);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
    return position;
}

JNIEXPORT jfloat JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_getSoundDuration(JNIEnv *env, jobject thiz,
                                                                            jstring jSoundId) {
    const char *soundId = env->GetStringUTFChars(jSoundId, nullptr);
    float duration = 0.0f;
    if (g_audioEngine && soundId) {
        duration = g_audioEngine->getSoundDuration(soundId);
    }
    env->ReleaseStringUTFChars(jSoundId, soundId);
    return duration;
}

JNIEXPORT void JNICALL
Java_io_github_zyrouge_symphony_services_OpenAlAudioEngine_setStereoAngle(JNIEnv *env, jobject thiz,
                                                                          jfloat angle) {
    if (g_audioEngine) {
        g_audioEngine->setStereoAngle(angle);
    }
}

} // extern "C"