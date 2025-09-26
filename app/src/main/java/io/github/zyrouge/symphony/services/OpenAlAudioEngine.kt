package io.github.zyrouge.symphony.services

import android.content.Context
import java.io.File
import kotlin.math.atan2
import kotlin.math.sqrt

/**
 * AudioEngine is the primary interface for managing 3D audio playback using OpenAL.
 *
 * This object provides Kotlin-friendly methods for initializing the audio engine,
 * loading and playing sounds, controlling playback, and managing 3D audio positioning.
 * It communicates with the native C++ audio engine through JNI.
 */
object tOpenAlAudioEngine {

    init {
        System.loadLibrary("openal_musicplayer")
    }

    /**
     * Callback interface for receiving audio playback events.
     */
    interface AudioCallback {
        /**
         * Called when a sound finishes playing naturally.
         *
         * @param soundId The unique identifier of the sound that finished playing
         */
        fun onSoundFinished(soundId: String)
    }

    private var callback: AudioCallback? = null


    /**
     * Initializes the OpenAL audio engine with the specified HRTF name.
     *
     * @param selectedHrtf The Head-Related Transfer Function name to use.
     * @return `true` if initialization was successful, `false` otherwise.
     *
     * @throws IllegalStateException if the audio engine is already initialized
     */
    external fun initOpenAL(selectedHrtf: String): Boolean

    /**
     * Cleans up and shuts down the OpenAL audio engine, releasing all resources.
     *
     * This should be called when the audio engine is no longer needed, typically
     * when the application is closing or audio functionality is being disabled.
     */
    external fun cleanupOpenAL()

    /**
     * Creates a sound instance from the specified audio file.
     *
     * @param filePath The absolute path to the audio file to load.
     * @return A unique sound identifier that can be used to reference this sound in other operations,
     *         or `null` if the sound could not be loaded.
     *
     * @throws IllegalArgumentException if the file path is invalid or the file doesn't exist
     * @throws IllegalStateException if the audio engine is not initialized
     */
    external fun createSound(filePath: String): String?

    /**
     * Starts playback of the specified sound.
     *
     * @param soundId The unique identifier of the sound to play.
     *
     * @throws IllegalArgumentException if the soundId is not valid
     * @throws IllegalStateException if the audio engine is not initialized
     */
    external fun playSound(soundId: String)

    /**
     * Stops playback of the specified sound and releases its resources.
     *
     * @param soundId The unique identifier of the sound to stop.
     */
    external fun stopSound(soundId: String)

    /**
     * Stops playback of all currently active sounds and releases all resources.
     *
     * This is useful for quickly cleaning up all audio playback, such as when
     * the application is paused or when switching contexts.
     */
    external fun stopAllSounds()

    /**
     * Pauses playback of the specified sound.
     *
     * @param soundId The unique identifier of the sound to pause.
     *
     * @throws IllegalArgumentException if the soundId is not valid
     */
    external fun pauseSound(soundId: String)

    /**
     * Resumes playback of a previously paused sound.
     *
     * @param soundId The unique identifier of the sound to resume.
     *
     * @throws IllegalArgumentException if the soundId is not valid
     * @throws IllegalStateException if the sound is not paused
     */
    external fun resumeSound(soundId: String)

    /**
     * Sets the 3D position of a sound in spherical coordinates.
     *
     * @param soundId The unique identifier of the sound to position.
     * @param angle The horizontal angle in degrees (0-360), where 0 is directly in front.
     * @param radius The distance from the listener in arbitrary units.
     * @param height The vertical position relative to the listener.
     *
     * @throws IllegalArgumentException if the soundId is not valid
     */
    external fun setSoundPosition(soundId: String, angle: Float, radius: Float, height: Float)

    /**
     * Seeks to a specific position in the sound playback.
     *
     * @param soundId The unique identifier of the sound to seek.
     * @param seconds The playback position in seconds to seek to.
     *
     * @throws IllegalArgumentException if the soundId is not valid or position is out of bounds
     */
    external fun setPlaybackTime(soundId: String, seconds: Float)

    /**
     * Gets the current playback time of the specified sound.
     *
     * @param soundId The unique identifier of the sound.
     * @return The current playback time in seconds, or -1 if the sound is not found or an error occurred.
     */
    external fun getPlaybackTime(soundId: String): Float

    /**
     * Gets the total duration of the specified sound.
     *
     * @param soundId The unique identifier of the sound.
     * @return The total duration of the sound in seconds, or 0 if the sound is not found.
     */
    external fun getSoundDuration(soundId: String): Float

    /**
     * Sets the stereo separation angle for stereo sounds.
     *
     * @param angle The separation angle in degrees. Larger values create wider stereo separation.
     */
    external fun setStereoAngle(angle: Float)

    /**
     * Sets the callback for receiving audio playback events.
     *
     * @param callback The [AudioCallback] implementation to receive events, or `null` to remove the current callback.
     */
    fun setCallback(callback: AudioCallback?) {
        this.callback = callback
        setCallbackNative(callback)
    }

    private external fun setCallbackNative(callback: Any?)


    /**
     * Initializes the audio engine with default settings.
     *
     * @param context The application context used for file operations.
     * @param selectedHrtf The HRTF name to use.
     * @return `true` if initialization was successful, `false` otherwise.
     */
    fun init(context: Context, selectedHrtf: String): Boolean {
        return initOpenAL(selectedHrtf)
    }

    /**
     * Creates a sound instance from an audio file in the assets folder.
     *
     * @param context The application context.
     * @param assetPath The path to the audio file in the assets folder (e.g., "sounds/music.wav").
     * @return The sound identifier, or `null` if the sound could not be loaded.
     */
    fun createSoundFromAssets(context: Context, assetPath: String): String? {
        return try {
            // Copy asset to temporary file
            val tempFile = File.createTempFile("audio_", null, context.cacheDir)
            tempFile.deleteOnExit()

            context.assets.open(assetPath).use { input ->
                tempFile.outputStream().use { output ->
                    input.copyTo(output)
                }
            }

            createSound(tempFile.absolutePath)
        } catch (_: Exception) {
            null
        }
    }

    /**
     * Creates a sound instance from a raw resource.
     *
     * @param context The application context.
     * @param resId The resource ID of the audio file.
     * @return The sound identifier, or `null` if the sound could not be loaded.
     */
    fun createSoundFromResource(context: Context, resId: Int): String? {
        return try {
            // Copy resource to temporary file
            val tempFile = File.createTempFile("audio_", null, context.cacheDir)
            tempFile.deleteOnExit()

            context.resources.openRawResource(resId).use { input ->
                tempFile.outputStream().use { output ->
                    input.copyTo(output)
                }
            }

            createSound(tempFile.absolutePath)
        } catch (e: Exception) {
            null
        }
    }

    /**
     * Sets the 3D position of a sound using Cartesian coordinates.
     *
     * @param soundId The unique identifier of the sound to position.
     * @param x The X coordinate (left/right).
     * @param y The Y coordinate (up/down).
     * @param z The Z coordinate (front/back).
     */
    fun setSoundPositionCartesian(soundId: String, x: Float, y: Float, z: Float) {
        // Convert Cartesian to spherical coordinates
        val radius = sqrt((x*x + y*y + z*z).toDouble()).toFloat()
        val angle = Math.toDegrees(atan2(x.toDouble(), z.toDouble())).toFloat()
        val height = y

        setSoundPosition(soundId, angle, radius, height)
    }

    /**
     * TODO
     * Smoothly changes the position of a sound over time.
     *
     * @param soundId The unique identifier of the sound to animate.
     * @param targetAngle The target horizontal angle in degrees.
     * @param targetRadius The target distance from the listener.
     * @param targetHeight The target vertical position.
     * @param durationMs The duration of the animation in milliseconds.
     */
    fun animateSoundPosition(
        soundId: String,
        targetAngle: Float,
        targetRadius: Float,
        targetHeight: Float,
        durationMs: Long
    ) {
        // This would typically be implemented with a coroutine or ValueAnimator
        // For now, this is a placeholder for the animation API
        setSoundPosition(soundId, targetAngle, targetRadius, targetHeight)
    }

    /**
     * Called by native code when a sound finishes playing.
     *
     * @param soundId The unique identifier of the sound that finished.
     *
     * @hide Internal use only
     */
    @Suppress("unused")
    private fun onSoundFinished(soundId: String) {
        callback?.onSoundFinished(soundId)
    }
}