package hf.iot.getowminfo;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.os.Bundle;
import android.os.Handler;
import android.os.ParcelUuid;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Objects;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    //GUI references
    Button humidBtn;
    Button tempBtn;
    TextView msgText;
    //msgType
    final static char MSG_TEMP     = 0;
    final static char MSG_HUMIDITY = 1;

    //initialization
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //set GUI references
        tempBtn = findViewById(R.id.tempBtn);
        humidBtn = findViewById(R.id.humidityBtn);
        msgText = findViewById(R.id.msgText);

        //Set event listeners for the 2 buttons
        tempBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Reset textview & disable buttons
                msgText.setText("");
                tempBtn.setEnabled(false);
                humidBtn.setEnabled(false);
                requestViaBle(MSG_TEMP);
            }
        });

        humidBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Reset textview & disable buttons
                msgText.setText("");
                tempBtn.setEnabled(false);
                humidBtn.setEnabled(false);
                requestViaBle(MSG_HUMIDITY);
            }
        });

        //Initialize stuff for BLE-communication
        initBleCommunication();
    }


    //references for BLE communication
    BluetoothLeAdvertiser advertiser;
    AdvertiseSettings settingsAdvertise;
    ScanSettings settingsScan;
    ParcelUuid pUuid;

    BluetoothLeScanner scanner;
    Handler timeoutHandler;
    ArrayList<ScanFilter> filters;

    void initBleCommunication() {

        //Set advertiser part
        BluetoothAdapter btAdapter = BluetoothAdapter.getDefaultAdapter();
        btAdapter.setName("tel");
        advertiser = btAdapter.getBluetoothLeAdvertiser();
        //advertiser = BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();

        settingsAdvertise = new AdvertiseSettings.Builder()
                .setAdvertiseMode( AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY )
                .setTxPowerLevel( AdvertiseSettings.ADVERTISE_TX_POWER_HIGH )
                .setConnectable( false )
                .build();

        pUuid = new ParcelUuid( UUID.fromString(getString(R.string.ble_uuid) ) );


        //Set scanner part
        scanner = BluetoothAdapter.getDefaultAdapter().getBluetoothLeScanner();

        timeoutHandler = new Handler();

        filters = new ArrayList<>();
        //ScanFilter filter = new ScanFilter.Builder()
        //        .setServiceUuid( new ParcelUuid(UUID.fromString( getString(R.string.ble_uuid) ) ) )
        //        .build();
        ScanFilter filter = new ScanFilter.Builder()
                .setDeviceName("wea")
                .build();
        //"00001809-0000-1000-8000-00805F9B34FB"
        filters.add( filter );

        settingsScan = new ScanSettings.Builder()
                .setScanMode( ScanSettings.SCAN_MODE_LOW_LATENCY )
                .build();

    }


    void requestViaBle(final char msgType) {

        //HELP: https://code.tutsplus.com/tutorials/how-to-advertise-android-as-a-bluetooth-le-peripheral--cms-25426

        // * Send request for getting data back from BLE device

        //Data to send
        byte[] payload = new byte[] {(byte) msgType};               //.setIncludeDeviceName( false )
        final AdvertiseData data = new AdvertiseData.Builder()
                .setIncludeDeviceName( false )
                .setIncludeTxPowerLevel( false )
                .addServiceData( pUuid , payload )
                .build();

        //callback for advertising
        final AdvertiseCallback advertiseCallback = new AdvertiseCallback() {
            @Override
            public void onStartSuccess(AdvertiseSettings settingsInEffect) {
                super.onStartSuccess(settingsInEffect);
                Log.d("BLE", "Advertising onStartSuccess");
            }

            @Override
            public void onStartFailure(int errorCode) {
                Log.e( "BLE", "Advertising onStartFailure: " + errorCode );
                super.onStartFailure(errorCode);
            }
        };

        //Start advertising
        advertiser.startAdvertising(settingsAdvertise, data, advertiseCallback);

        // * Waiting answer from BLE device

        //callback for scanning
        final ScanCallback scanCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                super.onScanResult(callbackType, result);
                Log.d("BLE", "Scanning onScanResult");
                if( result == null
                        || result.getDevice() == null
                        || TextUtils.isEmpty(result.getDevice().getName()) ){
                    Log.d("BLE", "Scanning onScanResult: null");
                    return;
                }

                //Get result string
                String resultString;

                try {
                    resultString = getStringWithMagic(Objects.requireNonNull(result.getScanRecord()).toString());
                    Log.d("BLE", "resultString: " + resultString);
                } catch (NullPointerException e) {
                    Log.d("BLE", "Scanning onScanResult: NullPointerException");
                    return;
                }

                String prefix = "";
                String postfix = "";
                switch (msgType) {
                    case MSG_TEMP:
                        prefix = "Temperature: ";
                        postfix = " C";
                        break;
                    case MSG_HUMIDITY:
                        prefix = "Humidity: ";
                        postfix = " %";
                        break;
                }
                resultString = prefix + resultString + postfix;
                Log.d("BLE", "Scanning onScanResult: " + resultString);

                //Set textview & enable buttons
                msgText.setText(resultString);
                tempBtn.setEnabled(true);
                humidBtn.setEnabled(true);

                //Stop advertising & scanning
                advertiser.stopAdvertising(advertiseCallback);

                scanner.stopScan(this);

                //Stop timeout runnable call
                timeoutHandler.removeCallbacksAndMessages(null);

            }

            @Override
            public void onScanFailed(int errorCode) {
                Log.e( "BLE", "Scanning onScanFailed: " + errorCode );
                super.onScanFailed(errorCode);
            }
        };

        //Start scanning
        scanner.startScan(filters, settingsScan, scanCallback);

        //Stop every services after delay & reset GUI elements
        long timeout = 20000;
        timeoutHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                Log.d("BLE", "Timeout...");
                //Stop advertising & scanning
                advertiser.stopAdvertising(advertiseCallback);

                scanner.stopScan(scanCallback);

                //Set textview text & enable buttons
                if (msgText.getText().toString().equals(""))
                    msgText.setText(R.string.no_response);
                tempBtn.setEnabled(true);
                humidBtn.setEnabled(true);
            }
        }, timeout);

    }

    String getStringWithMagic(String result) {
        try {
            int startPos = result.indexOf("mServiceData={") + "mServiceData={".length();
            result = result.substring(startPos);
            int endPos = result.indexOf("]},");
            result = result.substring(0, endPos);
            int firsPartEnd = result.indexOf("-");
            String firstPart = result.substring(0, firsPartEnd);    //00007361
            firstPart = firstPart.substring(4);                     //7361
            int lastPartStart = result.indexOf("[") + 1;
            String lastPart = result.substring(lastPartStart);  //100, 102, 0
            String[] lastChars = lastPart.split(", ");

            ArrayList<String> hexChars = new ArrayList<>(4);

            hexChars.add(firstPart.substring(2));                       //61 hex
            hexChars.add(firstPart.substring(0, 2)); //73 hex
            for (String lastChar : lastChars) {
                if (lastChar.equals("0")) break;
                hexChars.add(lastChar);
            }

            StringBuilder out = new StringBuilder();
            out.append(Character.toString((char) Integer.parseInt(hexChars.get(0), 16)));
            out.append(Character.toString((char) Integer.parseInt(hexChars.get(1), 16)));
            for (int i = 2; i < hexChars.size(); i++) {
                String code = hexChars.get(i);
                out.append(Character.toString((char) Integer.parseInt(code)));
            }
            return out.toString();
        } catch (Exception e) {
            Log.d("STRING CONVERT", "¯\\_(ツ)_/¯");
            return "";
        }
    }

}
