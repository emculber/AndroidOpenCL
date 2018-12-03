package com.image.cse.imageprocessing

import android.app.Activity
import android.content.Intent
import android.content.pm.ActivityInfo
import android.net.Uri
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.os.PersistableBundle
import android.provider.MediaStore
import android.support.v4.content.FileProvider
import android.util.Log
import android.view.KeyEvent
import android.view.View
import android.view.Window
import android.view.WindowManager
import android.widget.ImageButton
import kotlinx.android.synthetic.main.activity_main.*
import org.opencv.android.CameraBridgeViewBase
import org.opencv.android.JavaCameraView
import org.opencv.android.OpenCVLoader
import org.opencv.core.Core
import org.opencv.core.CvType
import org.opencv.core.Mat
import org.opencv.core.Scalar
import org.opencv.imgproc.Imgproc
import org.opencv.imgproc.Imgproc.Canny
import java.io.File
import java.text.SimpleDateFormat
import java.util.*

class MainActivity_cameraView : Activity(), CameraBridgeViewBase.CvCameraViewListener2 {

    companion object {
        init {
            if(!OpenCVLoader.initDebug()) {
                Log.d("TAG", "OpenCV not Loaded")
            }else{
                Log.d("TAG", "OpenCV Loaded")
            }
        }
    }

    private var camera = 0

    lateinit var imgHSV:Mat
    lateinit var imgThresholded:Mat
    lateinit var cameraView:JavaCameraView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)
        this.requestWindowFeature(Window.FEATURE_NO_TITLE)
        this.window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN)
        setContentView(R.layout.activity_main);

        cameraView = findViewById(R.id.cameraview)
        cameraView.setCameraIndex(camera) // 0 for rear, 1 for front
        cameraView.setCvCameraViewListener(this)
        cameraView.enableFpsMeter()
        cameraView.enableView()
    }

    override fun onPause() {
        super.onPause()
        cameraView.disableFpsMeter()
        cameraView.disableView()
    }

    override fun onCameraViewStarted(width: Int, height: Int) {
        imgHSV = Mat(height+height/2, width, CvType.CV_16UC4)
        imgThresholded = Mat(height+height/2, width, CvType.CV_16UC4)
    }

    override fun onCameraViewStopped() {
        TODO("not implemented") //To change body of created functions use File | Settings | File Templates.
    }

    override fun onCameraFrame(inputFrame: CameraBridgeViewBase.CvCameraViewFrame?): Mat {

        Imgproc.cvtColor(inputFrame!!.rgba(), imgHSV, Imgproc.COLOR_BGR2HSV)
        //Core.inRange(imgHSV, sc1, sc2, imgThresholded)
        Imgproc.Canny(imgHSV, imgThresholded, 10.0, 100.0, 3, true)


        return imgThresholded
    }
}
