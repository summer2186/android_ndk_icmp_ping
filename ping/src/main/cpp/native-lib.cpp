#include <jni.h>
#include <string>

#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/vfs.h> // Bionic doesn't have <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/icmp.h>

static struct CachedFields {
    jclass fileDescriptorClass;
    jmethodID fileDescriptorCtor;
    jfieldID descriptorField;

    jclass errnoExceptionClass;
} gCachedFields;


std::string JStringToStdString(JNIEnv* env, jstring jstr) {
    const char* local_char = env->GetStringUTFChars(jstr, JNI_FALSE);
    jsize size = env->GetStringUTFLength(jstr);
    std::string rv("");
    if ( local_char != NULL && size > 0) {
        rv.assign(local_char, static_cast<size_t>(size));
    }

    env->ReleaseStringUTFChars(jstr, local_char);
    return rv;
}


jint ThrowRuntimeException(JNIEnv* env, const char* msg) {
    static jclass cls = NULL;
    if ( cls == NULL ) {
        jclass localCls = env->FindClass("java/lang/RuntimeException");
        if ( localCls != NULL ) {
            cls = (jclass)env->NewGlobalRef((jobject)localCls);
            env->DeleteLocalRef((jobject)localCls);
        }
    }

    if ( cls == NULL )
        return -1;

    return env->ThrowNew(cls, msg);
}

static void throwException(JNIEnv* env, jclass exceptionClass, jmethodID ctor3, jmethodID ctor2,
                           const char* functionName, int error) {
    jthrowable cause = NULL;
    if (env->ExceptionCheck()) {
        cause = env->ExceptionOccurred();
        env->ExceptionClear();
    }

    jstring detailMessage = env->NewStringUTF(functionName);
    if (detailMessage == NULL) {
        // Not really much we can do here. We're probably dead in the water,
        // but let's try to stumble on...
        env->ExceptionClear();
    }

    jobject exception;
    if (cause != NULL) {
        exception = env->NewObject(exceptionClass, ctor3, detailMessage, error, cause);
    } else {
        exception = env->NewObject(exceptionClass, ctor2, detailMessage, error);
    }
    env->Throw(reinterpret_cast<jthrowable>(exception));
    env->DeleteLocalRef(detailMessage);
}

static void throwErrnoException(JNIEnv* env, const char* functionName) {
    int error = errno;
    static jmethodID ctor3 = env->GetMethodID(gCachedFields.errnoExceptionClass,
                                              "<init>", "(Ljava/lang/String;ILjava/lang/Throwable;)V");
    static jmethodID ctor2 = env->GetMethodID(gCachedFields.errnoExceptionClass,
                                              "<init>", "(Ljava/lang/String;I)V");
    throwException(env, gCachedFields.errnoExceptionClass, ctor3, ctor2, functionName, error);
}

jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        abort();
    }

    gCachedFields.errnoExceptionClass =
            reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass("libcore/io/ErrnoException")));
    if (gCachedFields.errnoExceptionClass == NULL) {
        abort();
    }

    gCachedFields.fileDescriptorClass =
            reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass("java/io/FileDescriptor")));
    if (gCachedFields.fileDescriptorClass == NULL) {
        abort();
    }

    gCachedFields.fileDescriptorCtor =
            env->GetMethodID(gCachedFields.fileDescriptorClass, "<init>", "()V");
    if (gCachedFields.fileDescriptorCtor == NULL) {
        abort();
    }

    gCachedFields.descriptorField =
            env->GetFieldID(gCachedFields.fileDescriptorClass, "descriptor", "I");
    if (gCachedFields.descriptorField == NULL) {
        abort();
    }

    return JNI_VERSION_1_6;
}

int jniGetFDFromFileDescriptor(JNIEnv* env, jobject fileDescriptor) {
    return env->GetIntField(fileDescriptor, gCachedFields.descriptorField);
}

void jniSetFileDescriptorOfFD(JNIEnv* env, jobject fileDescriptor, int value) {
    env->SetIntField(fileDescriptor, gCachedFields.descriptorField, value);
}

jobject jniCreateFileDescriptor(JNIEnv* env, int fd) {
    jobject fileDescriptor = env->NewObject(gCachedFields.fileDescriptorClass, gCachedFields.fileDescriptorCtor);
    jniSetFileDescriptorOfFD(env, fileDescriptor, fd);
    return fileDescriptor;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_live_season_net_Ping_createICMPSocket(JNIEnv* env, jobject thiz) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);

    if(sock < 0) {
        throwErrnoException(env, "socket");
        return NULL;
    } else {
        return jniCreateFileDescriptor(env, sock);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_live_season_net_Ping_closeSocket(JNIEnv* env, jobject thiz, jobject fileDescriptor) {
    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
    shutdown(fd, SHUT_RDWR);
    int rv = close(fd);
    if ( rv == -1 ) {
        throwErrnoException(env, "close");
    }
}

int SetSockTimeout(int sock, int timeout) {

    if ( timeout <= 0 )
        timeout = 5000;

    int sec = timeout / 1000;
    int usec = (timeout - (sec * 1000)) * 1000;
    if ( usec < 0 )
        usec = 0;
    struct timeval tv_timeout;
    tv_timeout.tv_sec  = sec;
    tv_timeout.tv_usec = usec;

    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout));
}

extern "C"
JNIEXPORT jint JNICALL
Java_live_season_net_Ping_ping(JNIEnv* env, jobject thiz, jobject fileDescriptor,
                               jstring dest, jint timeout, jint seq, jbyteArray jbytePayload) {
    if (fileDescriptor == NULL) {
        jclass nullPointerException = env->FindClass("java/lang/NullPointerException");
        env->ThrowNew(nullPointerException, "FileDescriptor == null");
        return -1;
    }

    std::string str_dest = JStringToStdString(env, dest);
    if (str_dest.empty()) {
        ThrowRuntimeException(env, "dest is empty");
        return -1;
    }

    int sock = jniGetFDFromFileDescriptor(env, fileDescriptor);

    sockaddr_in addr_self;
    memset(&addr_self, 0, sizeof(addr_self));
    int len = sizeof(addr_self);

    int rv = getsockname(sock, (sockaddr *) &addr_self, &len);
    if (rv != 0) {
        throwErrnoException(env, "getsockname");
        return -1;
    }

    rv = SetSockTimeout(sock, timeout);
    if (rv != 0) {
        throwErrnoException(env, "SetSockTimeout");
        return false;
    }

    jsize payload_size = env->GetArrayLength(jbytePayload);
    jbyte *payload = env->GetByteArrayElements(jbytePayload, JNI_FALSE);

    icmphdr icmp_hdr;

    size_t pack_size = sizeof(icmp_hdr) + payload_size;
    char *pack_data = new char[pack_size];

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(str_dest.c_str());
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.code = 0;
    icmp_hdr.checksum = 0;
    icmp_hdr.un.echo.id = (unsigned short)(getpid() & 0xffff);
    icmp_hdr.un.echo.sequence = (unsigned short)seq;
    memcpy(pack_data, &icmp_hdr, sizeof(icmp_hdr));
    if (payload_size > 0) {
        memcpy(pack_data + sizeof(icmp_hdr), &payload, (size_t)payload_size);
    }

    if (payload != NULL) {
        env->ReleaseByteArrayElements(jbytePayload, payload, 0);
    }

    if (sendto(sock, pack_data, pack_size, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        delete[] pack_data;
        throwErrnoException(env, "sendto");
        return -1;
    }

    delete [] pack_data;

    char recv_data[50 * 1024];
    struct sockaddr_in src;
    socklen_t slen = sizeof(src);
    int rc = recvfrom(sock, recv_data, sizeof(recv_data), 0, (struct sockaddr *) &src, &slen);
    if (rc < 0) {
        throwErrnoException(env, "recvfrom");
        return -1;
    }

    int resp_payload_size = -1;

    if (rc == pack_size) {
        if ( src.sin_addr.s_addr == addr.sin_addr.s_addr ) {
            icmphdr* resp_hdr = (icmphdr*)recv_data;
            if ( resp_hdr->type == ICMP_ECHOREPLY &&
                    resp_hdr->un.echo.sequence == icmp_hdr.un.echo.sequence) {
                    resp_payload_size = rc - sizeof(icmp_hdr);
                }
            }
        }


    return resp_payload_size;
}
