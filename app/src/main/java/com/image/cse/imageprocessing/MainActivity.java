package com.image.cse.imageprocessing;

import android.Manifest;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.support.v4.content.FileProvider;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.core.Size;
import org.opencv.imgproc.Imgproc;
import org.w3c.dom.Text;
import permissions.dispatcher.NeedsPermission;
import permissions.dispatcher.OnShowRationale;
import permissions.dispatcher.PermissionRequest;
import permissions.dispatcher.RuntimePermissions;

import java.io.*;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;


@RuntimePermissions
public class MainActivity extends Activity implements View.OnClickListener {

    private static final int REQUEST_TAKE_PHOTO = 1;
    private boolean isOffload = false;
    private boolean isLoop = false;

    ArrayList<Long> times = new ArrayList<>();
    ArrayList<Long> timesProcess = new ArrayList<>();

    Button btnImageOffload;
    Button btnImageOnload;
    Button btnImageOffloadWithLoad;
    Button btnImageOnloadWithLoad;
    ImageView ivPreview;
    TextView processTime;

    String mCurrentPhotoPath;

    File file;
    Uri imageUri;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        );
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        setContentView(R.layout.activity_main);

        if(!OpenCVLoader.initDebug()) {
            Log.d("TAG", "OpenCV not Loaded");
        }else{
            Log.d("TAG", "OpenCV Loaded");
        }

        initInstances();
    }

    private void initInstances() {
        btnImageOffload = (Button) findViewById(R.id.OffloadButton);
        btnImageOnload = (Button) findViewById(R.id.OnloadButton);
        btnImageOffloadWithLoad = (Button) findViewById(R.id.OffloadWithLoadButton);
        btnImageOnloadWithLoad = (Button) findViewById(R.id.OnloadWithLoadButton);
        ivPreview = (ImageView) findViewById(R.id.imageView);
        processTime = (TextView) findViewById(R.id.processTime);

        btnImageOffload.setOnClickListener(this);
        btnImageOnload.setOnClickListener(this);
        btnImageOffloadWithLoad.setOnClickListener(this);
        btnImageOnloadWithLoad.setOnClickListener(this);

    }

    /////////////////////
    // OnClickListener //
    /////////////////////

    @Override
    public void onClick(View view) {
        if (view == btnImageOnload) {
            MainActivityPermissionsDispatcher.startCameraWithCheck(this);
            isOffload = false;
            isLoop = false;
        } else if (view == btnImageOffload) {
            MainActivityPermissionsDispatcher.startCameraWithCheck(this);
            isOffload = true;
            isLoop = false;
        } else if (view == btnImageOnloadWithLoad) {
            MainActivityPermissionsDispatcher.startCameraWithCheck(this);
            isOffload = false;
            isLoop = true;
        } else if (view == btnImageOffloadWithLoad) {
            MainActivityPermissionsDispatcher.startCameraWithCheck(this);
            isOffload = true;
            isLoop = true;
        }
    }

    ////////////
    // Camera //
    ////////////

    @NeedsPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
    void startCamera() {
        try {
            dispatchTakePictureIntent();
        } catch (IOException e) {
        }
    }

    @OnShowRationale(Manifest.permission.WRITE_EXTERNAL_STORAGE)
    void showRationaleForCamera(final PermissionRequest request) {
        new AlertDialog.Builder(this)
                .setMessage("Access to External Storage is required")
                .setPositiveButton("Allow", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        request.proceed();
                    }
                })
                .setNegativeButton("Deny", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        request.cancel();
                    }
                })
                .show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        long startProcessTime;
        long endProcessTime;
        long endTime;

        int width;
        int heigth;

        long startTime = System.currentTimeMillis();

        if (requestCode == REQUEST_TAKE_PHOTO && resultCode == RESULT_OK) {
            int count = 0;
            do {
                // Show the thumbnail on ImageView
                imageUri = Uri.parse(mCurrentPhotoPath);
                file = new File(imageUri.getPath());
                Log.i("", imageUri.getPath());

                try {

                    InputStream ims = new FileInputStream(file);
                    Bitmap bitmap = BitmapFactory.decodeStream(ims);
                    ivPreview.setImageBitmap(bitmap);

                    final Mat inputImage=new Mat();//The input image to be sent
                    Utils.bitmapToMat(bitmap,inputImage); //change the bitmap to mat to pass the image as argument
                    //final Mat outputImage=new Mat();//The result image to be returned

                    int[] colors={0};
                    //Bitmap outputImageBitmap=Bitmap.createBitmap(colors,inputImage.cols(),inputImage.rows(),Bitmap.Config.RGB_565);// I think this creates a bitmap equals to inputImage height and width and RGB channels.
                    Mat outputImage = new Mat();
                    startProcessTime = System.currentTimeMillis();
                    if (isOffload) {
                        outputImage = new Mat(NativePart.sendImage(inputImage.getNativeObjAddr()));//call to native method
                    } else {
                        outputImage = new Mat(inputImage.size(), CvType.CV_8UC1);
                        Imgproc.cvtColor(inputImage, outputImage, Imgproc.COLOR_RGB2GRAY, 4);
                        Imgproc.Canny(outputImage, outputImage, 80, 100);
                        Imgproc.blur(outputImage, outputImage, new Size(50, 50));
                    }
                    endProcessTime = System.currentTimeMillis();
                    Log.i("", "Process Time: " + (endProcessTime - startProcessTime));
                    //Utils.matToBitmap(outputImage,outputImageBitmap);//the outputImage is changed into Bitmap in order to be displayed in the image view
                    Utils.matToBitmap(outputImage,bitmap);//the outputImage is changed into Bitmap in order to be displayed in the image view


                    Matrix matrix = new Matrix();
                    matrix.postRotate(90);
                    width = bitmap.getWidth();
                    heigth = bitmap.getHeight();
                    Bitmap scaledBitmap = Bitmap.createScaledBitmap(bitmap, bitmap.getWidth(), bitmap.getHeight(), true);
                    Bitmap rotatedBitmap = Bitmap.createBitmap(scaledBitmap, 0, 0, scaledBitmap.getWidth(), scaledBitmap.getHeight(), matrix, true);
                    //ivPreview.setImageBitmap(outputImageBitmap);
                    ivPreview.setImageBitmap(rotatedBitmap);
                } catch (FileNotFoundException e) {
                    return;
                }

                endTime = System.currentTimeMillis();
                Log.i("", "Total Time: " + (endTime-startTime));
                long diffProcess = (endProcessTime-startProcessTime);
                long diff = (endTime-startTime);
                times.add(diff);
                timesProcess.add(diffProcess);
                int avgTime = (int)calculateAverage(times);
                int avgTimeProcess = (int)calculateAverage(timesProcess);
                String timeProcess = "Times: " + times.size() + " Total:" + diff + "(" + avgTime + ") Process: " + diffProcess + "(" + avgTimeProcess + ") : " + width + "x" + heigth;
                processTime.setText(timeProcess);
                if(count == 10) {
                    break;
                }
                count++;
            } while (isLoop);

            file.delete();

            // ScanFile so it will be appeared on Gallery
            MediaScannerConnection.scanFile(MainActivity.this,
                    new String[]{imageUri.getPath()}, null,
                    new MediaScannerConnection.OnScanCompletedListener() {
                        public void onScanCompleted(String path, Uri uri) {
                        }
                    });
        }
    }

    private double calculateAverage(List<Long> marks) {
        Long sum = 0L;
        if(!marks.isEmpty()) {
            for (Long mark : marks) {
                sum += mark;
            }
            return sum.doubleValue() / marks.size();
        }
        return sum;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        MainActivityPermissionsDispatcher.onRequestPermissionsResult(this, requestCode, grantResults);
    }

    private File createImageFile() throws IOException {
        // Create an image file name
        String timeStamp = new SimpleDateFormat("yyyyMMdd_HHmmss").format(new Date());
        String imageFileName = "JPEG_" + timeStamp + "_";
        File storageDir = new File(Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DCIM), "Camera");
        File image = File.createTempFile(
                imageFileName,  /* prefix */
                ".jpg",         /* suffix */
                storageDir      /* directory */
        );

        // Save a file: path for use with ACTION_VIEW intents
        mCurrentPhotoPath = "file:" + image.getAbsolutePath();
        return image;
    }

    private void dispatchTakePictureIntent() throws IOException {
        Intent takePictureIntent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        // Ensure that there's a camera activity to handle the intent
        if (takePictureIntent.resolveActivity(getPackageManager()) != null) {
            // Create the File where the photo should go
            File photoFile = null;
            try {
                photoFile = createImageFile();
            } catch (IOException ex) {
                // Error occurred while creating the File
                return;
            }
            // Continue only if the File was successfully created
            if (photoFile != null) {
                Uri photoURI = FileProvider.getUriForFile(MainActivity.this,
                        BuildConfig.APPLICATION_ID + ".provider",
                        createImageFile());
                takePictureIntent.putExtra(MediaStore.EXTRA_OUTPUT, photoURI);
                startActivityForResult(takePictureIntent, REQUEST_TAKE_PHOTO);
            }
        }
    }
}
