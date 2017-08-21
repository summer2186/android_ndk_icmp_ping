package live.season.libping;

import android.content.Context;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.util.Log;

import org.junit.Test;
import org.junit.runner.RunWith;

import live.season.net.Ping;

import static junit.framework.Assert.assertEquals;
import static org.junit.Assert.*;

/**
 * Instrumentation test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class ExampleInstrumentedTest {
    @Test
    public void useAppContext() throws Exception {
        // Context of the app under test.
        Context appContext = InstrumentationRegistry.getTargetContext();

        assertEquals("live.season.libping", appContext.getPackageName());
    }

    @Test
    public void pingTest() throws Exception {
        Ping ping = Ping.create();
        for(int i=1; i<1000; ++i) {
            long now = System.currentTimeMillis();
            try{
                ping.ping("192.168.1.1", 5000, i, new byte[20*1024]);
                Log.i("PingTest", i + " ping response: " + (System.currentTimeMillis() - now));
                Thread.sleep(1000);
            }catch (Exception e) {
                Log.i("PingTest", "error:"  + e.getMessage());
            }
        }
        ping.close();
    }
}
