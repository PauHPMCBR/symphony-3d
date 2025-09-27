package io.github.zyrouge.symphony.services.radio.mediaplayers

import android.content.Context
import android.media.MediaPlayer
import android.net.Uri

interface MediaPlayerI {
    val isPlaying: Boolean
    val currentTrackTimeMs: Int
    val trackDurationMs: Int

    fun setDataSource(context: Context, uri: Uri)
    fun prepareAsync()

    fun start()
    fun pause()
    fun stop()
    fun release()

    fun seekTo(timeMs: Int)
    fun setVolume(leftVolume: Float, rightVolume: Float)
    fun setPlaybackSpeed(speed: Float)
    fun setPitch(pitch: Float)
}
