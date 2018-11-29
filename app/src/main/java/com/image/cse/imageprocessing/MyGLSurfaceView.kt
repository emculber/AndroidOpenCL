package com.image.cse.imageprocessing

import org.opencv.android.CameraGLSurfaceView

import android.app.Activity
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.widget.TextView
import android.widget.Toast

class MyGLSurfaceView(context: Context, attrs: AttributeSet) : CameraGLSurfaceView(context, attrs),
    CameraGLSurfaceView.CameraTextureListener {
    protected var procMode = NativePart.PROCESSING_MODE_NO_PROCESSING
    protected var frameCounter: Int = 0
    protected var lastNanoTime: Long = 0
    internal var mFpsText: TextView? = null

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (e.action == MotionEvent.ACTION_DOWN)
            (context as Activity).openOptionsMenu()
        return true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        super.surfaceCreated(holder)
        //NativePart.initCL();
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        //NativePart.closeCL();
        super.surfaceDestroyed(holder)
    }

    fun setProcessingMode(newMode: Int) {
        if (newMode >= 0 && newMode < procModeName.size)
            procMode = newMode
        else
            Log.e(LOGTAG, "Ignoring invalid processing mode: $newMode")

        (context as Activity).runOnUiThread {
            Toast.makeText(
                context,
                "Selected mode: " + procModeName[procMode],
                Toast.LENGTH_LONG
            ).show()
        }
    }

    override fun onCameraViewStarted(width: Int, height: Int) {
        (context as Activity).runOnUiThread {
            Toast.makeText(context, "onCameraViewStarted", Toast.LENGTH_SHORT).show()
        }
//        NativePart.initCL()
        frameCounter = 0
        lastNanoTime = System.nanoTime()
    }

    override fun onCameraViewStopped() {
        (context as Activity).runOnUiThread {
            Toast.makeText(context, "onCameraViewStopped", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCameraTexture(texIn: Int, texOut: Int, width: Int, height: Int): Boolean {
        // FPS
        frameCounter++
        if (frameCounter >= 30) {
            val fps = (frameCounter * 1e9 / (System.nanoTime() - lastNanoTime)).toInt()
            Log.i(LOGTAG, "drawFrame() FPS: $fps")
            if (mFpsText != null) {
                val fpsUpdater = Runnable { mFpsText!!.text = "FPS: $fps" }
                Handler(Looper.getMainLooper()).post(fpsUpdater)
            } else {
                Log.d(LOGTAG, "mFpsText == null")
                mFpsText = (context as Activity).findViewById(R.id.fps_text_view)
            }
            frameCounter = 0
            lastNanoTime = System.nanoTime()
        }


        if (procMode == NativePart.PROCESSING_MODE_NO_PROCESSING)
            return false

        NativePart.processFrame(texIn, texOut, width, height, procMode)
        return true
    }

    companion object {

        internal val LOGTAG = "MyGLSurfaceView"
        internal val procModeName = arrayOf("No Processing", "CPU", "OpenCL Direct", "OpenCL via OpenCV")
    }
}