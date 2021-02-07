/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.example.hellojnicallback;

import androidx.annotation.Keep;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {
    String TAG = "jnicallback";
    int hour = 0;
    int minute = 0;
    int second = 0;
    TextView tickView;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        tickView = (TextView) findViewById(R.id.tickView);
    }
    @Override
    public void onResume() {
        super.onResume();
        hour = minute = second = 0;
        ((TextView)findViewById(R.id.hellojniMsg)).setText(stringFromJNI());
        startTicks("11211");
//        createThread("create thread para");
    }

    @Override
    public void onPause () {
        super.onPause();
        StopTicks();
    }


    @Keep
    private Long method_para_long_return_long(Long para) {
        return 1L;
    }

    @Keep
    private Object method_para_long_return_obj(Long para) {
        Log.e(TAG,"java "+"");
        Log.e(TAG,"java :"+"method_para_long_return_obj");
        return null;
    }
    @Keep
    private Object method_para_void_return_obj() {
        Log.e(TAG,"java "+"");
        Log.e(TAG,"java :"+"method_para_void_return_obj");
        return null;
    }    @Keep
    private void method_para_long_return_void(long para) {
        Log.e(TAG,"java "+"");
        Log.e(TAG,"java :"+"method_para_long_return_void");
    }

    @Keep
    private void method_para_void_return_void() {
        Log.e(TAG,"java "+"");
        Log.e(TAG,"java :"+"method_para_void_return_void");

    }


    @Keep
    private void method_para_string_return_void(String para) {
        Log.e(TAG,"java :"+"method_para_string_return_void");
    }


    @Keep
    private String method_para_void_return_String() {
        Log.e(TAG,"java :"+"method_para_void_return_String");

        return "String 2";
    }

    @Keep
    private int method_para_int_return_int(int para ) {

      return 1;
    }

    @Keep
    private String method_para_string_return_string(String para ) {
        Log.e(TAG,"java :"+"method_para_string_return_string");

        return "method_para_string_return_string append para:"+para;
    }


    /*
     * A function calling from JNI to update current timer
     */
    @Keep
    private void updateTimer() {
        Log.e(TAG,"java updateTimer"+System.currentTimeMillis());
        ++second;
        if(second >= 60) {
            ++minute;
            second -= 60;
            if(minute >= 60) {
                ++hour;
                minute -= 60;
            }
        }
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String ticks = "" + MainActivity.this.hour + ":" +
                        MainActivity.this.minute + ":" +
                        MainActivity.this.second;
                MainActivity.this.tickView.setText(ticks);
            }
        });
    }
    static {
        System.loadLibrary("hello-jnicallback");
    }
    public native  String stringFromJNI();
    public native void startTicks(String para1);
    public native void createThread(String para1);
    public native void StopTicks();
}
