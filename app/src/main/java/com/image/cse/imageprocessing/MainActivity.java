package com.image.cse.imageprocessing;

import android.Manifest;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
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
import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.Mat;
import permissions.dispatcher.NeedsPermission;
import permissions.dispatcher.OnShowRationale;
import permissions.dispatcher.PermissionRequest;
import permissions.dispatcher.RuntimePermissions;

import java.io.*;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.text.SimpleDateFormat;
import java.util.Date;


@RuntimePermissions
public class MainActivity extends Activity implements View.OnClickListener {

    private static final int REQUEST_TAKE_PHOTO = 1;

    Button btnTakePhoto;
    ImageView ivPreview;

    String mCurrentPhotoPath;

    Socket socket;
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
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        setContentView(R.layout.activity_main);

        if(!OpenCVLoader.initDebug()) {
            Log.d("TAG", "OpenCV not Loaded");
        }else{
            Log.d("TAG", "OpenCV Loaded");
        }

        initInstances();
    }

    private void initInstances() {
        btnTakePhoto = (Button) findViewById(R.id.CameraButton);
        ivPreview = (ImageView) findViewById(R.id.imageView);

        btnTakePhoto.setOnClickListener(this);

    }

    /////////////////////
    // OnClickListener //
    /////////////////////

    @Override
    public void onClick(View view) {
        if (view == btnTakePhoto) {
            MainActivityPermissionsDispatcher.startCameraWithCheck(this);
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




        if (requestCode == REQUEST_TAKE_PHOTO && resultCode == RESULT_OK) {
            // Show the thumbnail on ImageView
            imageUri = Uri.parse(mCurrentPhotoPath);
            file = new File(imageUri.getPath());
            Log.i("", imageUri.getPath());
            //new ConnectSocket().execute();

            try {

                InputStream ims = new FileInputStream(file);
                Bitmap bitmap = BitmapFactory.decodeStream(ims);
                ivPreview.setImageBitmap(bitmap);

                final Mat inputImage=new Mat();//The input image to be sent
                Utils.bitmapToMat(bitmap,inputImage); //change the bitmap to mat to pass the image as argument
                //final Mat outputImage=new Mat();//The result image to be returned

                int[] colors={0};
                //Bitmap outputImageBitmap=Bitmap.createBitmap(colors,inputImage.cols(),inputImage.rows(),Bitmap.Config.RGB_565);// I think this creates a bitmap equals to inputImage height and width and RGB channels.
                final Mat outputImage = new Mat(NativePart.sendImage(inputImage.getNativeObjAddr()));//call to native method
                //Utils.matToBitmap(outputImage,outputImageBitmap);//the outputImage is changed into Bitmap in order to be displayed in the image view
                Utils.matToBitmap(outputImage,bitmap);//the outputImage is changed into Bitmap in order to be displayed in the image view

                //ivPreview.setImageBitmap(outputImageBitmap);
                ivPreview.setImageBitmap(bitmap);
            } catch (FileNotFoundException e) {
                return;
            }

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

    class ConnectSocket extends AsyncTask<String, Void, Void> {
        public Void doInBackground(String... urls) {
            try {
                Log.v("DEV", "SocketClass, connectSocket");
                SocketAddress socketAddress = new InetSocketAddress("home.luwsnet.com", 5050);
                ObjectOutputStream oos = null;
                ObjectInputStream ois = null;
                java.util.Date date = null;

                try {
                    socket = new Socket();
                    socket.setTcpNoDelay(true);
                    socket.setSoTimeout(5000);
                    socket.connect(socketAddress, 5000);

                    if (socket.isConnected()) {
                        Log.v("DEV", "SocketClass successfully connected!");

                        oos = new ObjectOutputStream(socket.getOutputStream());

                        oos.flush();
                        oos.writeObject(file);

                        oos.flush();
                        oos.reset();

                        ois = new ObjectInputStream(socket.getInputStream());
                        int sz = (Integer) ois.readObject();
                        System.out.println("Receving " + (sz / 1024) + " Bytes From Sever");

                        byte b[] = new byte[sz];
                        int bytesRead = ois.read(b, 0, b.length);
                        for (int i = 0; i < sz; i++) {
                            System.out.print(b[i]);
                        }
                        FileOutputStream fos = new FileOutputStream(new File(imageUri.getPath()));
                        fos.write(b, 0, b.length);
                        System.out.println("From Server : " + ois.readObject());
                        oos.close();
                        ois.close();
                    }

                } catch (Exception e) {
                    Log.e("TCP", e.toString());
                }
            } catch (Exception e) {
                Log.e("DEV", e.toString());
            }
            return null;
        }
    }
}
