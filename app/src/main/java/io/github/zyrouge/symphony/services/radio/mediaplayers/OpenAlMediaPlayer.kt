package io.github.zyrouge.symphony.services.radio.mediaplayers

import android.content.Context
import android.media.MediaPlayer
import android.net.Uri

class OpenAlMediaPlayer() : MediaPlayerI {
    override val isPlaying get() = true
    override val currentTrackTimeMs get() = 1
    override val trackDurationMs get() = 1

    override fun setDataSource(context: Context, uri: Uri) {
        TODO("Not yet implemented")
    }

    override fun prepareAsync() {
        TODO("Not yet implemented")
    }

    override fun start() {
        TODO("Not yet implemented")
    }

    override fun pause() {
        TODO("Not yet implemented")
    }

    override fun stop() {
        TODO("Not yet implemented")
    }

    override fun release() {
        TODO("Not yet implemented")
    }

    override fun seekTo(timeMs: Int) {
        TODO("Not yet implemented")
    }

    override fun setVolume(leftVolume: Float, rightVolume: Float) {
        TODO("Not yet implemented")
    }

    override fun setPlaybackSpeed(speed: Float) {
        TODO("Not yet implemented")
    }

    override fun setPitch(pitch: Float) {
        TODO("Not yet implemented")
    }
}