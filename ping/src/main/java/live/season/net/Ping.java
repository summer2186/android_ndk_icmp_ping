package live.season.net;

import android.system.ErrnoException;
import java.io.FileDescriptor;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Ping class
 */
public class Ping {
    public static final int PING_TIMEOUT_DEFAULT = 10000;
    public static final int PING_TIMEOUT_MIN = 1000;
    public static final int PING_TIMEOUT_MAX = 36000;

    /**
     * create socket for ping
     * @return return instance if success
     * @throws Exception if create socket failed, will throw ErrnoException
     */
    public static Ping create() throws Exception {
        FileDescriptor fd = createICMPSocket();
        return new Ping(fd);
    }

    /**
     * close socket
     *
     * it will call shutdown first, when calling ping, will return -1, but no exception throw
     *
     * @throws Exception if create close failed, will throw ErrnoException
     */
    public void close() throws Exception {
        if ( fd != null ) {
            synchronized (this) {
                if ( fd != null ) {
                    closeSocket(fd);
                    fd = null;
                }
            }
        }
    }

    /**
     *
     * send icmp package to host
     *
     * @param dest host's ip
     * @param timeout timeout in millseconds
     * @param seq icmp sequence, from 0
     * @param payload ping payload
     * @return return response bytes
     * @throws Exception
     */
    public int ping(String dest, int timeout, int seq, byte[] payload) throws Exception {
        if ( dest == null || dest.isEmpty() ) {
            throw new RuntimeException("dest is null or empty");
        }

        if ( timeout < PING_TIMEOUT_MIN || timeout > PING_TIMEOUT_MAX ) {
            throw new RuntimeException("timeout too small or large");
        }

        if ( seq < 0 || seq >= Short.MAX_VALUE ) {
            throw new RuntimeException("seq too small or large");
        }

        if ( payload != null && payload.length >= 65535 ) {
            throw new RuntimeException("payload more than 64KiB");
        }

        if ( fd == null ) {
            throw new NullPointerException("fd is null");
        }

        if (!isSending.compareAndSet(false, true)) {
            throw new RuntimeException("sending icmp package, waiting for response");
        }

        try {
            return ping(fd, dest, timeout, seq, payload);
        }finally {
            isSending.set(false);
        }
    }

    public boolean isPinging() {
        return isSending.get();
    }

    static {
        System.loadLibrary("ping");
    }

    private Ping(FileDescriptor fd) {
        this.fd = fd;
    }

    private FileDescriptor fd;
    private AtomicBoolean isSending = new AtomicBoolean(false);

    private static native FileDescriptor createICMPSocket() throws ErrnoException;
    private static native void closeSocket(FileDescriptor fd) throws ErrnoException;
    private static native int ping(FileDescriptor fd,  String dest, int timeout, int seq, byte[] payload);
}
