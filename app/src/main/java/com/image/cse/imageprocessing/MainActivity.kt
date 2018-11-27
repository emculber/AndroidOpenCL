package com.image.cse.imageprocessing

import android.app.Activity
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.*
import android.widget.TextView

internal class MainActivity : Activity() {
    private var mView: MyGLSurfaceView? = null
    private var mProcMode: TextView? = null

    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        )
        window.setFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        )
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE

        //mView = new MyGLSurfaceView(this, null);
        //setContentView(mView);
        setContentView(R.layout.activity_main)
        mView = findViewById<View>(R.id.my_gl_surface_view) as MyGLSurfaceView
        mView!!.cameraTextureListener = mView
        val tv = findViewById<View>(R.id.fps_text_view) as TextView
        mProcMode = findViewById<View>(R.id.proc_mode_text_view) as TextView
        runOnUiThread { mProcMode!!.text = "Processing mode: No processing" }

        mView!!.setProcessingMode(NativePart.PROCESSING_MODE_NO_PROCESSING)
    }

    override fun onPause() {
        mView!!.onPause()
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        mView!!.onResume()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        val inflater = menuInflater
        inflater.inflate(R.menu.menu, menu)
        return super.onCreateOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.no_proc -> {
                runOnUiThread { mProcMode!!.text = "Processing mode: No Processing" }
                mView!!.setProcessingMode(NativePart.PROCESSING_MODE_NO_PROCESSING)
                return true
            }
            R.id.cpu -> {
                runOnUiThread { mProcMode!!.text = "Processing mode: CPU" }
                mView!!.setProcessingMode(NativePart.PROCESSING_MODE_CPU)
                return true
            }
            R.id.ocl_direct -> {
                runOnUiThread { mProcMode!!.text = "Processing mode: OpenCL direct" }
                mView!!.setProcessingMode(NativePart.PROCESSING_MODE_OCL_DIRECT)
                return true
            }
            R.id.ocl_ocv -> {
                runOnUiThread { mProcMode!!.text = "Processing mode: OpenCL via OpenCV (TAPI)" }
                mView!!.setProcessingMode(NativePart.PROCESSING_MODE_OCL_OCV)
                return true
            }
            else -> return false
        }
    }
}
