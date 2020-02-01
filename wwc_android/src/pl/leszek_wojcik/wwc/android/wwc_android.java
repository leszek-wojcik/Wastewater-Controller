package pl.leszek_wojcik.wwc.android;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Switch;
import android.widget.TextView;

import org.json.JSONObject;

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

public class wwc_android extends Activity {

    static final String LOG_TAG = "PUBSUB";

    private static final String KEYSTORE_NAME = "keystore.bks";
    private static final String KEYSTORE_PASSWORD = "keystore";
    private static final String CERTIFICATE_ID = "keystore";

    TextView tvMessages;
    TextView tvClientId;
    TextView tvStatus;
    TextView tvWWCid;
    TextView tvDutyCycle;
    TextView tvNormalPeriod;
    TextView tvGMTOffset;
    Switch aSwitchAreation;
    Switch aSwitchCirculation;
    Button aRequestStatusBtn;
    Button aSubmitBtn;

    AWSIotClient mIotAndroidClient;
    AWSIotMqttManager mqttManager;

    String statusTopic;
    String controlTopic;
    String endpoint;
    String clientId;
    String deviceId;
    KeyStore clientKeyStore = null;


     public void submitClick(final View view) {
            JSONObject msg = new JSONObject();
            try {

                if (tvDutyCycle.length() != 0)
                    msg.put("normalDutyCycle", new Float(tvDutyCycle.getText().toString()));
                else
                    msg.put("normalDutyCycle", new Float(tvDutyCycle.getHint().toString()));

                if (tvGMTOffset.length() != 0)
                    msg.put("circulationStartGMTOffset", new Integer(tvGMTOffset.getText().toString()));
                else
                    msg.put("circulationStartGMTOffset", new Integer(tvGMTOffset.getHint().toString()));

                if (tvNormalPeriod.length() !=0)
                    msg.put("normalPeriod", new Integer(tvNormalPeriod.getText().toString()));
                else
                    msg.put("normalPeriod", new Integer(tvNormalPeriod.getHint().toString()));

                mqttManager.publishString(msg.toString(), controlTopic, AWSIotMqttQos.QOS0);
                Log.i(LOG_TAG, msg.toString());

            } catch (Exception e) {
                Log.e(LOG_TAG, "Publish error.", e);
            }
     }

    public void requestStatusClick(final View view) {
        JSONObject msg = new JSONObject();
        try {
            msg.put("requestStatus", true);
            mqttManager.publishString(msg.toString(), controlTopic, AWSIotMqttQos.QOS0);
            Log.i(LOG_TAG, msg.toString());

        } catch (Exception e) {
            Log.e(LOG_TAG, "Publish error.", e);
        }
    }



     public void areationClick(final View view) {

        JSONObject msg = new JSONObject();
        try {
            if (aSwitchAreation.isChecked()) {
                msg.put("Areation", true);
            }else {
                msg.put("Areation", false);
            }

            mqttManager.publishString(msg.toString(), controlTopic, AWSIotMqttQos.QOS0);
            Log.i(LOG_TAG, msg.toString());

        } catch (Exception e) {
            Log.e(LOG_TAG, "Publish error.", e);
        }
    }

    public void circulationClick(final View view) {
        JSONObject msg = new JSONObject();

        try {
            if (aSwitchCirculation.isChecked()) {
                msg.put("Circulation", true);
            }else {
                msg.put("Circulation", false);
            }

            mqttManager.publishString(msg.toString(), controlTopic, AWSIotMqttQos.QOS0);
            Log.i(LOG_TAG, msg.toString());

        } catch (Exception e) {
            Log.e(LOG_TAG, "Publish error.", e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvMessages = findViewById(R.id.tvMessages);
        tvClientId = findViewById(R.id.tvClientId);
        tvWWCid = findViewById(R.id.tvWWCid);
        tvStatus = findViewById(R.id.tvStatus);
        aSwitchAreation = findViewById(R.id.btnAreation);
        aSwitchCirculation = findViewById(R.id.btnCirculation);
        aRequestStatusBtn = findViewById(R.id.reqStatusBtn);

        tvDutyCycle = findViewById(R.id.normalDutyCycleTxt);
        tvNormalPeriod = findViewById(R.id.normalPeriodTxt);
        tvGMTOffset = findViewById(R.id.circulationGMTOffsetTxt);
        aSubmitBtn = findViewById(R.id.submitBtn);

        aSwitchAreation.setEnabled(false);
        aSwitchCirculation.setEnabled(false);
        aRequestStatusBtn.setEnabled(false);
        tvDutyCycle.setEnabled(false);
        tvNormalPeriod.setEnabled(false);
        tvGMTOffset.setEnabled(false);
        aSubmitBtn.setEnabled(false);

        endpoint = getResources().getString(R.string.endpoint);
        clientId = getResources().getString(R.string.wwc_app_id);
        deviceId = getResources().getString(R.string.wwc_device_id);
        statusTopic = "wwc/" + getResources().getString(R.string.wwc_device_id) + "/status";
        controlTopic = "wwc/" + getResources().getString(R.string.wwc_device_id) + "/control";

        tvClientId.setText(clientId);
        tvWWCid.setText(deviceId);

        mqttManager = new AWSIotMqttManager(clientId, endpoint);
        mqttManager.setKeepAlive(10);

        mIotAndroidClient = new AWSIotClient(AWSMobileClient.getInstance());

        AssetManager assetManager = getAssets();

        AWSIotMqttClientStatusCallback statusCallback = new AWSIotMqttClientStatusCallback() {
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

                        Log.d(LOG_TAG, "subscribing "+ statusTopic);

                        if (status == AWSIotMqttClientStatus.Connected) {

                            aSwitchAreation.setEnabled(true);
                            aSwitchCirculation.setEnabled(true);
                            aRequestStatusBtn.setEnabled(true);
                            tvDutyCycle.setEnabled(true);
                            tvNormalPeriod.setEnabled(true);
                            tvGMTOffset.setEnabled(true);
                            aSubmitBtn.setEnabled(true);

                            try {
                                mqttManager.subscribeToTopic(statusTopic, AWSIotMqttQos.QOS0,
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

                                                            tvMessages.append(message);
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
                    }
                });
            }
        };

        try
        {
            InputStream iStream = assetManager.open(KEYSTORE_NAME);
            clientKeyStore = AWSIotKeystoreHelper.getIotKeystore(CERTIFICATE_ID,
                            iStream, KEYSTORE_PASSWORD);
        } catch (Exception e) {
            Log.e(LOG_TAG, "An error occurred retrieving cert/key from keystore.", e);
        }
        Log.d(LOG_TAG, "clientId = " + clientId);

        try {
            mqttManager.connect(clientKeyStore, statusCallback );
        } catch (final Exception e) {
            Log.e(LOG_TAG, "Connection error.", e);
            tvStatus.setText("Error! " + e.getMessage());
        }


    }
}
