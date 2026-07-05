//
// Created by root on 5/2/26.
//

#include "demo.h"
#include "../../server.h"
#include "../../connection.h"


/* -------- CT_My 的各字段方法实现（留空，待完善）-------- */

static const char *myGetType(connection *conn) {
    (void) conn;
    return "myconn";
}

static connection *myConnCreate(void) {
    return NULL;
}

static connection *myConnCreateAccepted(int fd, void *priv) {
    (void) fd;
    (void) priv;
    return NULL;
}

static void myShutdown(connection *conn) {
    (void) conn;
}

static void myClose(connection *conn) {
    (void) conn;
}

static int myConnect(connection *conn, const char *addr, int port, const char *src_addr,
                     ConnectionCallbackFunc connect_handler) {
    (void) conn;
    (void) addr;
    (void) port;
    (void) src_addr;
    (void) connect_handler;
    return C_ERR;
}

static int myBlockingConnect(connection *conn, const char *addr, int port, long long timeout) {
    (void) conn;
    (void) addr;
    (void) port;
    (void) timeout;
    return C_ERR;
}

static int myAccept(connection *conn, ConnectionCallbackFunc accept_handler) {
    (void) conn;
    (void) accept_handler;
    return C_ERR;
}

static int myWrite(connection *conn, const void *data, size_t data_len) {
    (void) conn;
    (void) data;
    (void) data_len;
    return 0;
}

static int myWritev(connection *conn, const struct iovec *iov, int iovcnt) {
    (void) conn;
    (void) iov;
    (void) iovcnt;
    return 0;
}

static int myRead(connection *conn, void *buf, size_t buf_len) {
    (void) conn;
    (void) buf;
    (void) buf_len;
    return 0;
}

static int mySetWriteHandler(connection *conn, ConnectionCallbackFunc func, int barrier) {
    (void) conn;
    (void) func;
    (void) barrier;
    return C_OK;
}

static int mySetReadHandler(connection *conn, ConnectionCallbackFunc func) {
    (void) conn;
    (void) func;
    return C_OK;
}

static const char *myGetLastError(connection *conn) {
    (void) conn;
    return NULL;
}

static ssize_t mySyncWrite(connection *conn, char *ptr, ssize_t size, long long timeout) {
    (void) conn;
    (void) ptr;
    (void) size;
    (void) timeout;
    return 0;
}

static ssize_t mySyncRead(connection *conn, char *ptr, ssize_t size, long long timeout) {
    (void) conn;
    (void) ptr;
    (void) size;
    (void) timeout;
    return 0;
}

static ssize_t mySyncReadLine(connection *conn, char *ptr, ssize_t size, long long timeout) {
    (void) conn;
    (void) ptr;
    (void) size;
    (void) timeout;
    return 0;
}

static void myAeHandler(struct aeEventLoop *el, int fd, void *clientData, int mask) {
    (void) el;
    (void) fd;
    (void) clientData;
    (void) mask;
}

static int myAddr(connection *conn, char *ip, size_t ip_len, int *port, int remote) {
    (void) conn;
    (void) ip;
    (void) ip_len;
    (void) port;
    (void) remote;
    return C_ERR;
}

static int myIsLocal(connection *conn) {
    (void) conn;
    return 0;
}

static int myListen(connListener *listener) {
    (void) listener;
    return C_ERR;
}

static void myAcceptHandler(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    (void) eventLoop;
    (void) fd;
    (void) clientData;
    (void) mask;
    printf("myAcceptHandler\n");
}


/* -------- CT_My 定义 -------- */

/**
 * 自定义的 ConnectionType，用于测试 connection.h 相关 api
 * 这个先留着，暂时用不到，可以先用CT_Socket学习，主要是想测试connConnect和connSetReadHandler传入的两个handler，这个用CT_Socket就可以测试
*/
static ConnectionType CT_My = {
    /* connection type */
    .get_type = myGetType,

    /* connection type initialize & finalize & configure */
    .init = NULL,
    .cleanup = NULL,
    .configure = NULL,

    /* ae & accept & listen & error & address handler */
    .ae_handler = myAeHandler,
    .accept_handler = myAcceptHandler,
    .addr = myAddr,
    .is_local = myIsLocal,
    .listen = myListen,

    /* create/shutdown/close connection */
    .conn_create = myConnCreate,
    .conn_create_accepted = myConnCreateAccepted,
    .shutdown = myShutdown,
    .close = myClose,

    /* connect & accept */
    .connect = myConnect,
    .blocking_connect = myBlockingConnect,
    .accept = myAccept,

    /* IO */
    .write = myWrite,
    .writev = myWritev,
    .read = myRead,
    .set_write_handler = mySetWriteHandler,
    .set_read_handler = mySetReadHandler,
    .get_last_error = myGetLastError,
    .sync_write = mySyncWrite,
    .sync_read = mySyncRead,
    .sync_readline = mySyncReadLine,

    /* pending data */
    .has_pending_data = NULL,
    .process_pending_data = NULL,
};

static void myConnectionHandler(connection *conn) {
    /*
     * 这里打印的连接状态是ConnectionState.CONN_STATE_CONNECTED
     * 刚创建时，connection.state是没有赋值的，所以默认是0，什么时候state变成的CONN_STATE_CONNECTED的？
     * 调用connSocketConnect(socket.c)建立连接是，会赋值conn->state = CONN_STATE_CONNECTING;
     * 然后调用connSocketEventHandler(socket.c)时(也就是触发了epoll_wait后)，判断如果conn->state == CONN_STATE_CONNECTING，并且检查没有异常，
     * 就会赋值conn->state = CONN_STATE_CONNECTED;
     * 设置完CONN_STATE_CONNECTED，就会调用conn->conn_handler(conn);
     * */
    ConnectionState state = connGetState(conn);
    printf("myConnectionHandler conn.state: %d\n", state);
    void *pd = connGetPrivateData(conn);
    printf("myConnectionHandler pd: %p\n", pd);
}

static void myConnReadHandler(connection *conn) {
    void *pd = connGetPrivateData(conn);
    UNUSED(pd);
    // 这个日志先不打印了，因为可能会有readBuf是0的事件，频繁调用myConnReadHandler
//    printf("myConnReadHandler pd: %p\n", pd);
    // 可以调用connRead读取数据
    char buf[1024];
    int ret = connRead(conn, buf, 1024);
    if (ret > 0) {
        printf("myConnReadHandler ret: %d\n", ret);
        printf("myConnReadHandler buf: %s\n", buf);
    }
}


/**
 * 学习 redis 中 connection.h 相关 api
 * 这个是测试用 tcp 连接服务端的 api
 * 需要先启动 mymain.c 中的服务端
 * */
void myConnTest(void) {
    // 避免编译报错 not used
    printf("CT_My: %p\n", (void *)&CT_My);
    // 第一步：创建连接对象
    connection *conn = connCreate(connectionTypeTcp());
    // 第二步：建立连接
    connConnect(conn, "127.0.0.1", 8080, NULL, myConnectionHandler);
    // 第三步：设置读回调
    connSetReadHandler(conn, myConnReadHandler);
    // 第四步：设置私有数据，目前没有需求，先留空
    connSetPrivateData(conn, NULL);
    // 第五步：发送数据，测试myConnReadHandler
    connWrite(conn, "hello", 5);
}
