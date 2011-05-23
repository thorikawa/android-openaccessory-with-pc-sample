package com.polysfactory.usbtest;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

import android.app.Activity;
import android.content.Intent;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

import com.android.future.usb.UsbAccessory;
import com.android.future.usb.UsbManager;

public class Top extends Activity implements Runnable, SensorEventListener {

    UsbManager mUsbManager;

    UsbAccessory mUsbAccessory;

    boolean mIsForeground = true;

    SensorManager mSensorManager;

    // 端末の傾き角度
    int mRoll;

    // ボール追加ボタンが押されたかどうか
    boolean mAddBall = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);
        mUsbManager = UsbManager.getInstance(this);

        // 通信ボタンが押されたときの処理
        Button sendButton = (Button) findViewById(R.id.send);
        sendButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mUsbAccessory == null) {
                    UsbAccessory[] accessoryList = mUsbManager.getAccessoryList();
                    // UsbAccessoryは1つしかつながれていない前提で処理を行うので注意
                    mUsbAccessory = accessoryList[0];
                }
                Thread thread = new Thread(null, Top.this, "AccessoryThread");
                thread.start();
            }
        });

        // ボール追加ボタンが押されたときの処理
        Button ballButton = (Button) findViewById(R.id.ball);
        ballButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mAddBall = true;
            }
        });

        // USB Accessory接続時にIntent呼び出しされている場合はIntentからUsb Accessoryを取得する
        Intent intent = getIntent();
        if (UsbManager.ACTION_USB_ACCESSORY_ATTACHED.equals(intent.getAction())) {
            mUsbAccessory = UsbManager.getAccessory(intent);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        mIsForeground = true;

        List<Sensor> sensors = mSensorManager.getSensorList(Sensor.TYPE_ALL);

        for (Sensor sensor : sensors) {

            if (sensor.getType() == Sensor.TYPE_MAGNETIC_FIELD) {
                mSensorManager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_UI);
            }

            if (sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
                mSensorManager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_UI);
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();

        mIsForeground = false;

        // センサーリスナー停止
        mSensorManager.unregisterListener(this);
    }

    @Override
    public void run() {
        // USB Hostとの通信処理
        ParcelFileDescriptor pfd = mUsbManager.openAccessory(mUsbAccessory);
        FileDescriptor fd = pfd.getFileDescriptor();
        FileInputStream fis = new FileInputStream(fd);
        FileOutputStream fos = new FileOutputStream(fd);
        try {
            while (this.mIsForeground) {
                String s = String.format("%04d", mRoll);
                if (mAddBall) {
                    s = s + "0001";
                    mAddBall = false;
                } else {
                    s = s + "0000";
                }
                fos.write(s.getBytes());
                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
            pfd.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    private static final int MATRIX_SIZE = 16;

    /* 回転行列 */
    float[] inR = new float[MATRIX_SIZE];

    float[] outR = new float[MATRIX_SIZE];

    float[] I = new float[MATRIX_SIZE];

    float[] orientationValues = new float[3];

    float[] magneticValues = new float[3];

    float[] accelerometerValues = new float[3];

    @Override
    public void onSensorChanged(SensorEvent event) {
        // 地磁気センサーと加速度センサーで取得された値から、端末の傾きを計算する
        if (event.accuracy == SensorManager.SENSOR_STATUS_UNRELIABLE)
            return;

        switch (event.sensor.getType()) {
        case Sensor.TYPE_MAGNETIC_FIELD:
            magneticValues = event.values.clone();
            break;
        case Sensor.TYPE_ACCELEROMETER:
            accelerometerValues = event.values.clone();
            break;
        }

        if (magneticValues != null && accelerometerValues != null) {

            SensorManager.getRotationMatrix(inR, I, accelerometerValues, magneticValues);
            SensorManager.remapCoordinateSystem(inR, SensorManager.AXIS_X, SensorManager.AXIS_Z, outR);
            SensorManager.getOrientation(outR, orientationValues);

            Log.v("Poly", String.valueOf(radianToDegree(orientationValues[0])) + ", " + // Z軸方向,azmuth
                    String.valueOf(radianToDegree(orientationValues[1])) + ", " + // X軸方向,pitch
                    String.valueOf(radianToDegree(orientationValues[2]))); // Y軸方向,roll
            this.mRoll = radianToDegree(orientationValues[2]);
        }
    }

    private int radianToDegree(float rad) {
        return (int) Math.floor(Math.toDegrees(rad));
    }

    /**
     * roll命令を表すバイト列を返す
     * @param roll 傾き角度(-360〜360)
     * @return
     */
    private byte[] rollCommand(int roll) {
        byte[] command = {0, 0, 0, 10, 'r', 'o', 'l', 'l', 0, 0 };
        
        command[8] = (byte) (mRoll / 256);
        command[9] = (byte) (mRoll % 256);
        return command;
    }
}
