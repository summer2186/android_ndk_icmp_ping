package live.season.libping;

import android.app.Activity;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.widget.EditText;

import live.season.net.Ping;


public class MainActivity extends Activity {
    private static final String LOG_TAG = "PingTest";

    private EditText pingOutputEditText;
    private EditText pingDestEditText;

    private static final int MSG_PING_RESPONSE = 1;
    private static final int MSG_PING_FINISH = 2;

    private static final int PING_STATE_READY = 0;
    private static final int PING_STATE_SENDING = 1;
    private static final int PING_STATE_CANCELING = 2;

    private int pingState = PING_STATE_READY;
    private PingAsyncTask pingAsyncTask;

    private Handler hander = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            if ( msg.what == MSG_PING_RESPONSE  ) {
                pingOutputEditText.append((String)msg.obj);
                pingOutputEditText.append("\n");
            } else if ( msg.what == MSG_PING_FINISH ) {
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        pingDestEditText = (EditText)findViewById(R.id.inputDest);
        pingOutputEditText = (EditText)findViewById(R.id.pingOutput);

        findViewById(R.id.buttonPing).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if ( pingState == PING_STATE_SENDING) {
                    if ( pingAsyncTask != null ) {
                        Log.d(LOG_TAG, "cancel ping...");
                        pingAsyncTask.closePing();
                        pingState = PING_STATE_CANCELING;
                        pingAsyncTask = null;
                    }
                } else if ( pingState == PING_STATE_READY  ){
                    pingOutputEditText.setText("");
                    pingAsyncTask = new PingAsyncTask(pingDestEditText.getText().toString());
                    pingAsyncTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
                }
            }
        });
    }

    private class PingAsyncTask extends AsyncTask<String, Integer, Void> {
        private Ping ping;
        private int pingTimes = 5;
        private int payloadBytes = 56;
        private String dest;

        public PingAsyncTask(String dest) {
            this.dest = dest;
        }

        protected void onPreExecute() {
            try {
                ping = Ping.create();
                pingState = PING_STATE_SENDING;
                Log.d(LOG_TAG, "onPreExecute...");
            }catch (Exception e) {
                pingOutputEditText.setText(e.toString());
            }
        }

        protected Void doInBackground(String ... params) {
            byte[] payload = new byte[payloadBytes];
            for (int i=0; i<pingTimes; ++i) {
                try {

                    if ( ping == null )
                        return null;

                    long now = System.currentTimeMillis();
                    int rv = ping.ping(dest, Ping.PING_TIMEOUT_DEFAULT, i, payload);

                    Message msg = Message.obtain();
                    msg.what = MSG_PING_RESPONSE;
                    msg.obj = String.format("seq: %d ping, response: bytes: %d, time: %d ms", i, rv, System.currentTimeMillis() - now);
                    hander.sendMessage(msg);

                    if ( ping == null )
                        return null;

                    Thread.sleep(1000);
                }catch (Exception e) {
                    Message msg = Message.obtain();
                    msg.what = MSG_PING_RESPONSE;
                    msg.obj = String.format("seq: %d timeout, msg: %s", i, e.getMessage());
                    hander.sendMessage(msg);
                }
            }
            return null;
        }

        public void closePing() {
            if ( ping != null ) {
                try {
                    Log.d(LOG_TAG, "close ping...");
                    ping.close();
                    Log.d(LOG_TAG, "close socket end...");
                } catch (Exception e) {
                    e.printStackTrace();
                }
                ping = null;
            }
        }

        protected void onPostExecute(Void p) {
            Log.d(LOG_TAG, "onPostExecute...");
            pingAsyncTask = null;
            closePing();
            pingState = PING_STATE_READY;
            pingOutputEditText.append("ping finish...");
        }
    }
}
