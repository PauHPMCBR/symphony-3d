package io.github.zyrouge.symphony.services.radio.mediaplayers

import android.content.Context
import android.media.MediaPlayer
import android.net.Uri
import io.github.zyrouge.symphony.services.radio.RadioPlayer

class AndroidMediaPlayer() : MediaPlayerI {
    val mediaPlayer = MediaPlayer()

    override val isPlaying get() = mediaPlayer.isPlaying
    override val currentTrackTimeMs get() = mediaPlayer.currentPosition
    override val trackDurationMs get() = mediaPlayer.duration

    override fun setDataSource(context: Context, uri: Uri) {
        mediaPlayer.setDataSource(context, uri)
    }

    override fun prepareAsync() {
        mediaPlayer.prepareAsync()
    }

    override fun start() {
        mediaPlayer.start()
    }

    override fun pause() {
        mediaPlayer.pause()
    }

    override fun stop() {
        mediaPlayer.stop()
    }

    override fun release() {
        mediaPlayer.release()
    }

    override fun seekTo(timeMs: Int) {
        mediaPlayer.seekTo(timeMs)
    }

    override fun setVolume(leftVolume: Float, rightVolume: Float) {
        mediaPlayer.setVolume(leftVolume, rightVolume)
    }

    override fun setPlaybackSpeed(speed: Float) {
        mediaPlayer.playbackParams = mediaPlayer.playbackParams.setSpeed(speed)
    }

    override fun setPitch(pitch: Float) {
        mediaPlayer.playbackParams = mediaPlayer.playbackParams.setPitch(pitch)
    }
}
