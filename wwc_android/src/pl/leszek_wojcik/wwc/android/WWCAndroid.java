/**
 * Copyright 2010-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * <p>
 * http://aws.amazon.com/apache2.0
 * <p>
 * This file is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and
 * limitations under the License.
 */

package pl.leszek_wojcik.wwc.android;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import com.amazonaws.mobile.client.AWSMobileClient;
import com.amazonaws.mobile.client.Callback;
import com.amazonaws.mobile.client.UserStateDetails;
import com.amazonaws.mobileconnectors.iot.AWSIotKeystoreHelper;
import com.amazonaws.mobileconnectors.iot.AWSIotMqttClientStatusCallback;

import com.amazonaws.mobileconnectors.iot.AWSIotMqttManager;
import com.amazonaws.mobileconnectors.iot.AWSIotMqttNewMessageCallback;
import com.amazonaws.mobileconnectors.iot.AWSIotMqttQos;
import com.amazonaws.regions.Region;
import com.amazonaws.regions.Regions;
import com.amazonaws.services.iot.AWSIotClient;


import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.security.KeyStore;

public class WWCAndroid extends Activity {

    static final String LOG_TAG = "PUBSUB";
    private static final String CUSTOMER_SPECIFIC_ENDPOINT = "acpzbsqyyfpu4-ats.iot.eu-west-1.amazonaws.com";
    // Name of the AWS IoT policy to attach to a newly created certificate
    private static final String AWS_IOT_POLICY_NAME = "CHANGE_ME";

    // Region of AWS IoT
    private static final Regions MY_REGION = Regions.EU_WEST_1;
    // Filename of KeyStore file on the filesystem
    private static final String KEYSTORE_NAME = "keystore.bks";
    // Password for the private key in the KeyStore
    private static final String KEYSTORE_PASSWORD = "keystore";
    // Certificate and key aliases in the KeyStore
    private static final String CERTIFICATE_ID = "keystore";

    EditText txtSubcribe;
    EditText txtTopic;
    EditText txtMessage;

    TextView tvLastMessage;
    TextView tvClientId;
    TextView tvStatus;

    Button btnConnect;

    AWSIotClient mIotAndroidClient;
    AWSIotMqttManager mqttManager;
    String clientId;
    String keystorePath;
    String keystoreName;
    String keystorePassword;

    KeyStore clientKeyStore = null;
    String certificateId;

    public void connectClick(final View view) {
        Log.d(LOG_TAG, "clientId = " + clientId);

        try {
            mqttManager.connect(clientKeyStore, new AWSIotMqttClientStatusCallback() {
                @Override
                public void onStatusChanged(final AWSIotMqttClientStatus status,
                                            final Throwable throwable) {
                    Log.d(LOG_TAG, "Status = " + String.valueOf(status));

                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            tvStatus.setText(status.toString());
                            if (throwable != null) {
                                Log.e(LOG_TAG, "Connection error.", throwable);
                            }
                        }
                    });
                }
            });
        } catch (final Exception e) {
            Log.e(LOG_TAG, "Connection error.", e);
            tvStatus.setText("Error! " + e.getMessage());
        }
    }

    public void subscribeClick(final View view) {
        final String topic = txtSubcribe.getText().toString();

        Log.d(LOG_TAG, "topic = " + topic);

        try {
            mqttManager.subscribeToTopic(topic, AWSIotMqttQos.QOS0,
                    new AWSIotMqttNewMessageCallback() {
                        @Override
                        public void onMessageArrived(final String topic, final byte[] data) {
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    try {
                                        String message = new String(data, "UTF-8");
                                        Log.d(LOG_TAG, "Message arrived:");
                                        Log.d(LOG_TAG, "   Topic: " + topic);
                                        Log.d(LOG_TAG, " Message: " + message);

                                        tvLastMessage.setText(message);
                                    } catch (UnsupportedEncodingException e) {
                                        Log.e(LOG_TAG, "Message encoding error.", e);
                                    }
                                }
                            });
                        }
                    });
        } catch (Exception e) {
            Log.e(LOG_TAG, "Subscription error.", e);
        }
    }

    public void publishClick(final View view) {
        final String topic = txtTopic.getText().toString();
        final String msg = txtMessage.getText().toString();

        try {
            mqttManager.publishString(msg, topic, AWSIotMqttQos.QOS0);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Publish error.", e);
        }
    }


    public void disconnectClick(final View view) {
        try {
            mqttManager.disconnect();
        } catch (Exception e) {
            Log.e(LOG_TAG, "Disconnect error.", e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        txtSubcribe = findViewById(R.id.txtSubcribe);
        txtTopic = findViewById(R.id.txtTopic);
        txtMessage = findViewById(R.id.txtMessage);

        tvLastMessage = findViewById(R.id.tvLastMessage);
        tvClientId = findViewById(R.id.tvClientId);
        tvStatus = findViewById(R.id.tvStatus);

        btnConnect = findViewById(R.id.btnConnect);
        btnConnect.setEnabled(false);

        keystorePath = getFilesDir().getPath();

        Log.i(LOG_TAG, "keystore path:" + keystorePath);



        clientId = "android_controller";
        tvClientId.setText(clientId);

        Region region = Region.getRegion(MY_REGION);
        mqttManager = new AWSIotMqttManager(clientId, CUSTOMER_SPECIFIC_ENDPOINT);
        mqttManager.setKeepAlive(10);

        // IoT Client (for creation of certificate if needed)
        mIotAndroidClient = new AWSIotClient(AWSMobileClient.getInstance());
        mIotAndroidClient.setRegion(region);

        keystorePath = getFilesDir().getPath();
        Log.i(LOG_TAG, "keystore path:" + keystorePath);

        keystoreName = KEYSTORE_NAME;
        keystorePassword = KEYSTORE_PASSWORD;
        certificateId = CERTIFICATE_ID;

        AssetManager assetManager = getAssets();

        try
        {
            InputStream iStream = assetManager.open("keystore.bks");
            clientKeyStore = AWSIotKeystoreHelper.getIotKeystore(certificateId,
                            iStream, keystorePassword);

            /* initIoTClient is invoked from the callback passed during AWSMobileClient initialization.
               The callback is executed on a background thread so UI update must be moved to run on UI Thread. */
            runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            btnConnect.setEnabled(true);
                        }
                    });

        } catch (Exception e) {
            Log.e(LOG_TAG, "An error occurred retrieving cert/key from keystore.", e);
        }
    }
}
