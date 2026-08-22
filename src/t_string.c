/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "server.h"
#include "my/demo/demo.h"
#include <math.h> /* isnan(), isinf() */
#include "hiredis.h"
#include "async.h"

/* Forward declarations */
int getGenericCommand(client *c);

/*-----------------------------------------------------------------------------
 * String Commands
 *----------------------------------------------------------------------------*/

static int checkStringLength(client *c, long long size, long long append) {
    // ZZJ TODO mustObeyClient作用是什么？
    if (mustObeyClient(c))
        return C_OK;
    /* 'uint64_t' cast is there just to prevent undefined behavior on overflow */
    long long total = (uint64_t)size + append;
    // ZZJ PRV2 represending应该是representing
    /* Test configured max-bulk-len represending a limit of the biggest string object,
     * and also test for overflow. */
    if (total > server.proto_max_bulk_len || total < size || total < append) {
        addReplyError(c,"string exceeds maximum allowed size (proto-max-bulk-len)");
        return C_ERR;
    }
    return C_OK;
}

/* The setGenericCommand() function implements the SET operation with different
 * options and variants. This function is called in order to implement the
 * following commands: SET, SETEX, PSETEX, SETNX, GETSET.
 *
 * 'flags' changes the behavior of the command (NX, XX or GET, see below).
 *
 * 'expire' represents an expire to set in form of a Redis object as passed
 * by the user. It is interpreted according to the specified 'unit'.
 *
 * 'ok_reply' and 'abort_reply' is what the function will reply to the client
 * if the operation is performed, or when it is not because of NX or
 * XX flags.
 *
 * If ok_reply is NULL "+OK" is used.
 * If abort_reply is NULL, "$-1" is used. */

#define OBJ_NO_FLAGS 0
#define OBJ_SET_NX (1<<0)          /* Set if key not exists. */
#define OBJ_SET_XX (1<<1)          /* Set if key exists. */
#define OBJ_EX (1<<2)              /* Set if time in seconds is given */
#define OBJ_PX (1<<3)              /* Set if time in ms in given */
#define OBJ_KEEPTTL (1<<4)         /* Set and keep the ttl */
#define OBJ_SET_GET (1<<5)         /* Set if want to get key before set */
#define OBJ_EXAT (1<<6)            /* Set if timestamp in second is given */
#define OBJ_PXAT (1<<7)            /* Set if timestamp in ms is given */
#define OBJ_PERSIST (1<<8)         /* Set if we need to remove the ttl */

/* Forward declaration */
static int getExpireMillisecondsOrReply(client *c, robj *expire, int flags, int unit, long long *milliseconds);

// 这个就是开始执行SET命令了，往client.db里存数据了已经，并且根据过期参数设置key的过期、通知key变更事件，往reply写命令执行结果
void setGenericCommand(client *c, int flags, robj *key, robj *val, robj *expire, int unit, robj *ok_reply, robj *abort_reply) {
    long long milliseconds = 0; /* initialized to avoid any harmness warning */
    int found = 0;
    int setkey_flags = 0;

    if (expire && getExpireMillisecondsOrReply(c, expire, flags, unit, &milliseconds) != C_OK) {
        return;
    }

    if (flags & OBJ_SET_GET) {
        if (getGenericCommand(c) == C_ERR) return;
    }

    // ZZJ TODO lookupKeyWrite调用的lookupKey还没看
    found = (lookupKeyWrite(c->db,key) != NULL);

    if ((flags & OBJ_SET_NX && found) ||
        (flags & OBJ_SET_XX && !found))
    {
        // ZZJ TODO OBJ_SET_GET也会直接返回，但是不会调用addReply，为什么
        if (!(flags & OBJ_SET_GET)) {
            addReply(c, abort_reply ? abort_reply : shared.null[c->resp]);
        }
        return;
    }

    /* When expire is not NULL, we avoid deleting the TTL so it can be updated later instead of being deleted and then created again. */
    setkey_flags |= ((flags & OBJ_KEEPTTL) || expire) ? SETKEY_KEEPTTL : 0;
    setkey_flags |= found ? SETKEY_ALREADY_EXIST : SETKEY_DOESNT_EXIST;

    // ZZJ TODO setKey还没看
    setKey(c,c->db,key,val,setkey_flags);
    server.dirty++;
    // ZZJ TODO notifyKeyspaceEvent还没看
    notifyKeyspaceEvent(NOTIFY_STRING,"set",key,c->db->id);

    if (expire) {
        // ZZJ TODO setExpire还没看
        setExpire(c,c->db,key,milliseconds);
        /* Propagate as SET Key Value PXAT millisecond-timestamp if there is
         * EX/PX/EXAT flag. */
        if (!(flags & OBJ_PXAT)) {
            robj *milliseconds_obj = createStringObjectFromLongLong(milliseconds);
            // 所有过期设置都转换成PXAT
            // ZZJ TODO rewriteClientCommandVector还没看
            rewriteClientCommandVector(c, 5, shared.set, key, val, shared.pxat, milliseconds_obj);
            decrRefCount(milliseconds_obj);
        }
        notifyKeyspaceEvent(NOTIFY_GENERIC,"expire",key,c->db->id);
    }

    if (!(flags & OBJ_SET_GET)) {
        addReply(c, ok_reply ? ok_reply : shared.ok);
    }

    /* Propagate without the GET argument (Isn't needed if we had expire since in that case we completely re-written the command argv) */
    if ((flags & OBJ_SET_GET) && !expire) {
        int argc = 0;
        int j;
        robj **argv = zmalloc((c->argc-1)*sizeof(robj*));
        for (j=0; j < c->argc; j++) {
            char *a = c->argv[j]->ptr;
            // SET key val GET，这种命令是可以的，是redis 6.x引入的新特性
            /* Skip GET which may be repeated multiple times. */
            if (j >= 3 &&
                (a[0] == 'g' || a[0] == 'G') &&
                (a[1] == 'e' || a[1] == 'E') &&
                (a[2] == 't' || a[2] == 'T') && a[3] == '\0')
                continue;
            argv[argc++] = c->argv[j];
            incrRefCount(c->argv[j]);
        }
        // ZZJ TODO replaceClientCommandVector还没看
        replaceClientCommandVector(c, argc, argv);
    }
}

/*
 * Extract the `expire` argument of a given GET/SET command as an absolute timestamp in milliseconds.
 *
 * "client" is the client that sent the `expire` argument.
 * "expire" is the `expire` argument to be extracted.
 * "flags" represents the behavior of the command (e.g. PX or EX).
 * "unit" is the original unit of the given `expire` argument (e.g. UNIT_SECONDS).
 * "milliseconds" is output argument.
 *
 * If return C_OK, "milliseconds" output argument will be set to the resulting absolute timestamp.
 * If return C_ERR, an error reply has been added to the given client.
 */
// 已看，milliseconds会被赋值为时间戳绝对值
static int getExpireMillisecondsOrReply(client *c, robj *expire, int flags, int unit, long long *milliseconds) {
    int ret = getLongLongFromObjectOrReply(c, expire, milliseconds, NULL);
    if (ret != C_OK) {
        return ret;
    }

    if (*milliseconds <= 0 || (unit == UNIT_SECONDS && *milliseconds > LLONG_MAX / 1000)) {
        /* Negative value provided or multiplication is gonna overflow. */
        addReplyErrorExpireTime(c);
        return C_ERR;
    }

    if (unit == UNIT_SECONDS) *milliseconds *= 1000;

    if ((flags & OBJ_PX) || (flags & OBJ_EX)) {
        // 此时milliseconds还是相对时间，所以需要加上命令开始执行的时间戳，得到绝对时间
        *milliseconds += commandTimeSnapshot();
    }

    if (*milliseconds <= 0) {
        /* Overflow detected. */
        addReplyErrorExpireTime(c);
        return C_ERR;
    }

    return C_OK;
}

#define COMMAND_GET 0
#define COMMAND_SET 1
/*
 * The parseExtendedStringArgumentsOrReply() function performs the common validation for extended
 * string arguments used in SET and GET command.
 *
 * Get specific commands - PERSIST/DEL
 * Set specific commands - XX/NX/GET
 * Common commands - EX/EXAT/PX/PXAT/KEEPTTL
 *
 * Function takes pointers to client, flags, unit, pointer to pointer of expire obj if needed
 * to be determined and command_type which can be COMMAND_GET or COMMAND_SET.
 *
 * If there are any syntax violations C_ERR is returned else C_OK is returned.
 *
 * Input flags are updated upon parsing the arguments. Unit and expire are updated if there are any
 * EX/EXAT/PX/PXAT arguments. Unit is updated to millisecond if PX/PXAT is set.
 */
int parseExtendedStringArgumentsOrReply(client *c, int *flags, int *unit, robj **expire, int command_type) {

    // 解析extend参数，如果是get命令，扩展参数从argv[2]开始，如果是set命令，扩展参数从argv[3]开始
    // 从这个方法也能梳理出get set命令的所有扩展参数：看上面注释，上面注释写了GET还有个DEL命令，但是代码没看到处理 ZZJ PRV2
    int j = command_type == COMMAND_GET ? 2 : 3;
    for (; j < c->argc; j++) {
        char *opt = c->argv[j]->ptr;
        robj *next = (j == c->argc-1) ? NULL : c->argv[j+1];

        if ((opt[0] == 'n' || opt[0] == 'N') &&
            (opt[1] == 'x' || opt[1] == 'X') && opt[2] == '\0' &&
            !(*flags & OBJ_SET_XX) && (command_type == COMMAND_SET))
        {
            // flags参数是用来被赋值的，!(*flags & OBJ_SET_XX)加这个代码应该是为了避免有冲突的命令，比如同时有NX XX命令
            *flags |= OBJ_SET_NX;
        } else if ((opt[0] == 'x' || opt[0] == 'X') &&
                   (opt[1] == 'x' || opt[1] == 'X') && opt[2] == '\0' &&
                   !(*flags & OBJ_SET_NX) && (command_type == COMMAND_SET))
        {
            *flags |= OBJ_SET_XX;
        } else if ((opt[0] == 'g' || opt[0] == 'G') &&
                   (opt[1] == 'e' || opt[1] == 'E') &&
                   (opt[2] == 't' || opt[2] == 'T') && opt[3] == '\0' &&
                   (command_type == COMMAND_SET))
        {
            *flags |= OBJ_SET_GET;
        } else if (!strcasecmp(opt, "KEEPTTL") && !(*flags & OBJ_PERSIST) &&
            !(*flags & OBJ_EX) && !(*flags & OBJ_EXAT) &&
            !(*flags & OBJ_PX) && !(*flags & OBJ_PXAT) && (command_type == COMMAND_SET))
        {
            // 因为KEEPTTL会保留之前的过期时间，所以上面判断了flags不能有过期时间参数
            *flags |= OBJ_KEEPTTL;
        } else if (!strcasecmp(opt,"PERSIST") && (command_type == COMMAND_GET) &&
               !(*flags & OBJ_EX) && !(*flags & OBJ_EXAT) &&
               !(*flags & OBJ_PX) && !(*flags & OBJ_PXAT) &&
               !(*flags & OBJ_KEEPTTL))
        {
            // PERSIST是移除过期时间的，所以flags也不能有过期时间参数
            *flags |= OBJ_PERSIST;
        } else if ((opt[0] == 'e' || opt[0] == 'E') &&
                   (opt[1] == 'x' || opt[1] == 'X') && opt[2] == '\0' &&
                   !(*flags & OBJ_KEEPTTL) && !(*flags & OBJ_PERSIST) &&
                   !(*flags & OBJ_EXAT) && !(*flags & OBJ_PX) &&
                   !(*flags & OBJ_PXAT) && next)
        {
            *flags |= OBJ_EX;
            *expire = next;
            j++;
        } else if ((opt[0] == 'p' || opt[0] == 'P') &&
                   (opt[1] == 'x' || opt[1] == 'X') && opt[2] == '\0' &&
                   !(*flags & OBJ_KEEPTTL) && !(*flags & OBJ_PERSIST) &&
                   !(*flags & OBJ_EX) && !(*flags & OBJ_EXAT) &&
                   !(*flags & OBJ_PXAT) && next)
        {
            *flags |= OBJ_PX;
            *unit = UNIT_MILLISECONDS;
            *expire = next;
            j++;
        } else if ((opt[0] == 'e' || opt[0] == 'E') &&
                   (opt[1] == 'x' || opt[1] == 'X') &&
                   (opt[2] == 'a' || opt[2] == 'A') &&
                   (opt[3] == 't' || opt[3] == 'T') && opt[4] == '\0' &&
                   !(*flags & OBJ_KEEPTTL) && !(*flags & OBJ_PERSIST) &&
                   !(*flags & OBJ_EX) && !(*flags & OBJ_PX) &&
                   !(*flags & OBJ_PXAT) && next)
        {
            *flags |= OBJ_EXAT;
            *expire = next;
            j++;
        } else if ((opt[0] == 'p' || opt[0] == 'P') &&
                   (opt[1] == 'x' || opt[1] == 'X') &&
                   (opt[2] == 'a' || opt[2] == 'A') &&
                   (opt[3] == 't' || opt[3] == 'T') && opt[4] == '\0' &&
                   !(*flags & OBJ_KEEPTTL) && !(*flags & OBJ_PERSIST) &&
                   !(*flags & OBJ_EX) && !(*flags & OBJ_EXAT) &&
                   !(*flags & OBJ_PX) && next)
        {
            *flags |= OBJ_PXAT;
            *unit = UNIT_MILLISECONDS;
            *expire = next;
            j++;
        } else {
            // ZZJ TODO 这个还没看
            addReplyErrorObject(c,shared.syntaxerr);
            return C_ERR;
        }
    }
    return C_OK;
}

/* SET key value [NX] [XX] [KEEPTTL] [GET] [EX <seconds>] [PX <milliseconds>]
 *     [EXAT <seconds-timestamp>][PXAT <milliseconds-timestamp>] */
// 已看，先调用parseExtendedStringArgumentsOrReply解析参数，再调用setGenericCommand真正执行set命令
void setCommand(client *c) {
    robj *expire = NULL;
    int unit = UNIT_SECONDS;
    int flags = OBJ_NO_FLAGS;

    if (parseExtendedStringArgumentsOrReply(c,&flags,&unit,&expire,COMMAND_SET) != C_OK) {
        return;
    }

    // argv[2]是set参数的value，value确实需要encoding下
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    setGenericCommand(c,flags,c->argv[1],c->argv[2],expire,unit,NULL,NULL);
    serverLog(LL_NOTICE, "setCommand server.master_repl_offset is:%lld", server.master_repl_offset);
}

// SETNX命令，
void setnxCommand(client *c) {
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    setGenericCommand(c,OBJ_SET_NX,c->argv[1],c->argv[2],NULL,0,shared.cone,shared.czero);
}

// SETEX命令
void setexCommand(client *c) {
    c->argv[3] = tryObjectEncoding(c->argv[3]);
    setGenericCommand(c,OBJ_EX,c->argv[1],c->argv[3],c->argv[2],UNIT_SECONDS,NULL,NULL);
}

// PSETEX命令
void psetexCommand(client *c) {
    c->argv[3] = tryObjectEncoding(c->argv[3]);
    setGenericCommand(c,OBJ_PX,c->argv[1],c->argv[3],c->argv[2],UNIT_MILLISECONDS,NULL,NULL);
}

// 已看（级联方法还没看），真正执行GET命令的方法
int getGenericCommand(client *c) {
    robj *o;

    // ZZJ TODO lookupKeyReadOrReply调用的lookupKey还没看
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.null[c->resp])) == NULL)
        return C_OK;

    if (checkType(c,o,OBJ_STRING)) {
        return C_ERR;
    }

    // ZZJ TODO 这个还没看
    addReplyBulk(c,o);
    return C_OK;
}

void getCommand(client *c) {
    printf("getCommand\n");
    serverLog(LL_WARNING, "getCommand executed");
    getGenericCommand(c);
}

redisContext *myrc;

void myCmd(client *c) {
    printf("mycmd called, server.stat_total_reads_processed: %lld\n", server.stat_total_reads_processed);
    robj *param = c->argv[1];
    robj *decoded = getDecodedObject(param);
    printf("param: [%s], len: %zu\n", (char *)decoded->ptr, sdslen(decoded->ptr));
    if (strcasecmp((char *)decoded->ptr, "connTest") == 0) {
        // 这里要加\n，不然有输出缓冲区，导致打印不出来
        printf("connTest called\n");
        myConnTest();
    } else if (strncasecmp((char *)decoded->ptr, "hiredis", 7) == 0) {
        // hiredis使用示例，参考sentinelReconnectInstance(sentinel.c)方法
        redisOptions myOptions = {0};
        myOptions.push_cb = NULL;
        myOptions.options |= REDIS_OPT_NO_PUSH_AUTOFREE;
        if (strcasestr((char *)decoded->ptr, "nonblock")) {
            myOptions.options |= REDIS_OPT_NONBLOCK;
        }
        struct timeval tv = { .tv_sec = 1, .tv_usec = 500000 };
        myOptions.connect_timeout = &tv;
        myOptions.type = REDIS_CONN_TCP;
        myOptions.endpoint.tcp.ip = "127.0.0.1";
        char portStr[5] = {0};
        memcpy(portStr, (char *)decoded->ptr + 7, 4);
        myOptions.endpoint.tcp.port = atoi(portStr);
        redisContext *rc = redisConnectWithOptions(&myOptions);
        printf("rc.fd:%d, rc.err:%d, rc.errstr:%s, rc.tcp.host:%s, "
               "rc.tcp.source_addr:%s, rc.tcp.port:%d\n",
               rc->fd, rc->err, rc->errstr, rc->tcp.host, rc->tcp.source_addr, rc->tcp.port);
        /* 这里打印的是rc->flags=2，也就是REDIS_CONNECTED，是因为设置的是REDIS_OPT_NONBLOCK
         * 看_redisContextConnectTcp(net.c)代码，如果连接不通，会走到else if (errno == EINPROGRESS)分支，这里判断如果不是blocking
         * 会直接走到成功赋值c->flags |= REDIS_CONNECTED;
         * 如果设置了block模式，rc->flags=1，1代表REDIS_BLOCK(hiredis.h中定义的)
         * */
        printf("rc->flags:%d\n", rc->flags);
        printf("  flags detail:");
        if (rc->flags & REDIS_BLOCK) printf(" BLOCK(0x%x)", REDIS_BLOCK);
        if (rc->flags & REDIS_CONNECTED) printf(" CONNECTED(0x%x)", REDIS_CONNECTED);
        if (rc->flags & REDIS_DISCONNECTING) printf(" DISCONNECTING(0x%x)", REDIS_DISCONNECTING);
        if (rc->flags & REDIS_FREEING) printf(" FREEING(0x%x)", REDIS_FREEING);
        if (rc->flags & REDIS_IN_CALLBACK) printf(" IN_CALLBACK(0x%x)", REDIS_IN_CALLBACK);
        if (rc->flags & REDIS_SUBSCRIBED) printf(" SUBSCRIBED(0x%x)", REDIS_SUBSCRIBED);
        if (rc->flags & REDIS_MONITORING) printf(" MONITORING(0x%x)", REDIS_MONITORING);
        if (rc->flags & REDIS_REUSEADDR) printf(" REUSEADDR(0x%x)", REDIS_REUSEADDR);
        if (rc->flags & REDIS_SUPPORTS_PUSH) printf(" SUPPORTS_PUSH(0x%x)", REDIS_SUPPORTS_PUSH);
        if (rc->flags & REDIS_NO_AUTO_FREE) printf(" NO_AUTO_FREE(0x%x)", REDIS_NO_AUTO_FREE);
        if (rc->flags & REDIS_NO_AUTO_FREE_REPLIES) printf(" NO_AUTO_FREE_REPLIES(0x%x)", REDIS_NO_AUTO_FREE_REPLIES);
        if (rc->flags & REDIS_PREFER_IPV4) printf(" PREFER_IPV4(0x%x)", REDIS_PREFER_IPV4);
        if (rc->flags & REDIS_PREFER_IPV6) printf(" PREFER_IPV6(0x%x)", REDIS_PREFER_IPV6);
        printf("\n");
        myrc = rc;
    } else if (strncasecmp((char *)decoded->ptr, "1hiredis", 8) == 0) {
        // 调用redisBufferWrite(hiredis.c)写入ping命令，并阻塞读取返回值打印
        // 这里如果自己连自己测试，会卡住，因为下面的read是阻塞读，会导致mycmd命令阻塞，这样发送的ping命令就没机会执行
        // 启动一个slave，连接slave测试，测试通过
        if (myrc == NULL || myrc->err) {
            printf("whiredis: myrc is not connected, err: %s\n",
                   myrc ? myrc->errstr : "myrc is NULL");
        } else {
            int ret = redisAppendCommand(myrc, "PING");
            if (ret == REDIS_ERR) {
                printf("whiredis: redisAppendCommand failed\n");
            } else {
                int wdone = 0;
                // 关闭slave, write不会返回异常，下面读取数据会打印whiredis: redisGetReply failed, err: Server closed the connection
                // 重启slave会报同样的错误，因为之前的tcp连接已经没有了
                // 如果kill -9 slave，现象也是一样的，根据AI分析，如果kill -9但是操作系统还活着，read会返回-1
                // 关闭slave，tcp会发送FIN，recv返回0，net.c就会检测到
                ret = redisBufferWrite(myrc, &wdone);
                if (ret == REDIS_ERR) {
                    printf("whiredis: redisBufferWrite failed, err: %s\n", myrc->errstr);
                } else {
                    printf("whiredis: redisBufferWrite success: %d\n", ret);
                    void *reply = NULL;
                    sleep(1);
                    /*如果设置成nonblock，即使上面sleep一会，这里返回的reply是NULL，为什么?
                     * 答：因为redisGetReply中会判断，如果是nonblock模式，只会调用redisNextInBandReplyFromReader，这个方法只是从
                     * redisReader.buf中读取数据，不会调用recv接受socket数据
                     * sentinel中是因为把socket放到了eventLoop中，有数据可写会调用redisAeReadEvent，这个的调用流程语雀有梳理，
                     * 会调用recv把数据放到redisReader.buf中
                     * */
                    if ((myrc->flags & REDIS_BLOCK) == 0) {
                        // 根据上面的分析，如果是非阻塞模式，需要调用read方法先把数据读到buf中
                        redisBufferRead(myrc);
                    }
                    ret = redisGetReply(myrc, &reply);
                    if (ret == REDIS_ERR) {
                        printf("whiredis: redisGetReply failed, err: %s\n", myrc->errstr);
                    } else if (reply == NULL) {
                        printf("whiredis: reply is NULL\n");
                    } else {
                        redisReply *r = (redisReply *)reply;
                        printf("whiredis PING reply type: %d, str: %s\n", r->type, r->str);
                        freeReplyObject(reply);
                    }
                }
            }
        }
    } else if (strncasecmp((char *)decoded->ptr, "2hiredis", 8) == 0) {
        // 测试不写数据，只读
        if (myrc == NULL || myrc->err) {
            printf("whiredis: myrc is not connected, err: %s\n",
                   myrc ? myrc->errstr : "myrc is NULL");
        } else {
            int ret;
            void *reply = NULL;
            sleep(1);
            if ((myrc->flags & REDIS_BLOCK) == 0) {
                // 根据上面的分析，如果是非阻塞模式，需要调用read方法先把数据读到buf中
                redisBufferRead(myrc);
            }
            /* 如果是block模式，如果没有数据，这里会阻塞住
             * 如果是非阻塞模式，不会阻塞住，reply是NULL，注意非阻塞模式，上面的redisBufferRead也不会阻塞
             * 如果是阻塞模式，建立连接时就会调用redisSetBlocking(net.c)，否则_redisContextConnectTcp方法就会默认设置非阻塞模式
             * */
            ret = redisGetReply(myrc, &reply);
            if (ret == REDIS_ERR) {
                printf("whiredis: redisGetReply failed, err: %s\n", myrc->errstr);
            } else if (reply == NULL) {
                printf("whiredis: reply is NULL\n");
            } else {
                redisReply *r = (redisReply *)reply;
                printf("whiredis PING reply type: %d, str: %s\n", r->type, r->str);
                freeReplyObject(reply);
            }
        }
    } else if (strncasecmp((char *)decoded->ptr, "asyncCtx", 8) == 0) {
        redisAsyncContext* ac = redisAsyncConnectBind("127.0.0.1", 6380, NULL);
        redisAsyncCommand(ac, NULL, NULL, "PING");
        // TODO 还没写完
    }
    decrRefCount(decoded);  // 注意释放引用
    robj *o = createStringObject("mycmd reply", 11);
    addReplyBulk(c, o);
    decrRefCount(o);

}

/*
 * GETEX <key> [PERSIST][EX seconds][PX milliseconds][EXAT seconds-timestamp][PXAT milliseconds-timestamp]
 *
 * The getexCommand() function implements extended options and variants of the GET command. Unlike GET
 * command this command is not read-only.
 *
 * The default behavior when no options are specified is same as GET and does not alter any TTL.
 *
 * Only one of the below options can be used at a given time.
 *
 * 1. PERSIST removes any TTL associated with the key.
 * 2. EX Set expiry TTL in seconds.
 * 3. PX Set expiry TTL in milliseconds.
 * 4. EXAT Same like EX instead of specifying the number of seconds representing the TTL
 *      (time to live), it takes an absolute Unix timestamp
 * 5. PXAT Same like PX instead of specifying the number of milliseconds representing the TTL
 *      (time to live), it takes an absolute Unix timestamp
 *
 * Command would either return the bulk string, error or nil.
 */
void getexCommand(client *c) {
    robj *expire = NULL;
    int unit = UNIT_SECONDS;
    int flags = OBJ_NO_FLAGS;

    if (parseExtendedStringArgumentsOrReply(c,&flags,&unit,&expire,COMMAND_GET) != C_OK) {
        return;
    }

    robj *o;

    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.null[c->resp])) == NULL)
        return;

    if (checkType(c,o,OBJ_STRING)) {
        return;
    }

    /* Validate the expiration time value first */
    long long milliseconds = 0;
    if (expire && getExpireMillisecondsOrReply(c, expire, flags, unit, &milliseconds) != C_OK) {
        return;
    }

    /* We need to do this before we expire the key or delete it */
    addReplyBulk(c,o);

    /* This command is never propagated as is. It is either propagated as PEXPIRE[AT],DEL,UNLINK or PERSIST.
     * This why it doesn't need special handling in feedAppendOnlyFile to convert relative expire time to absolute one. */
    // ZZJ TODO checkAlreadyExpired还没看
    if (((flags & OBJ_PXAT) || (flags & OBJ_EXAT)) && checkAlreadyExpired(milliseconds)) {
        /* When PXAT/EXAT absolute timestamp is specified, there can be a chance that timestamp
         * has already elapsed so delete the key in that case. */
        // ZZJ TODO 这个还没看
        int deleted = dbGenericDelete(c->db, c->argv[1], server.lazyfree_lazy_expire, DB_FLAG_KEY_EXPIRED);
        serverAssert(deleted);
        // 如果是key被删除了，直接将命令重写为UNLINK(shared.unlink)或DEL(shared.del)
        robj *aux = server.lazyfree_lazy_expire ? shared.unlink : shared.del;
        // 重写client命令，读到这里理解下来，这个一般用来将原始命令转换成另一种形式，方便同步给集群其他节点
        rewriteClientCommandVector(c,2,aux,c->argv[1]);
        // ZZJ TODO signalModifiedKey这个还没看
        signalModifiedKey(c, c->db, c->argv[1]);
        notifyKeyspaceEvent(NOTIFY_GENERIC, "del", c->argv[1], c->db->id);
        server.dirty++;
    } else if (expire) {
        setExpire(c,c->db,c->argv[1],milliseconds);
        /* Propagate as PXEXPIREAT millisecond-timestamp if there is
         * EX/PX/EXAT/PXAT flag and the key has not expired. */
        robj *milliseconds_obj = createStringObjectFromLongLong(milliseconds);
        // 把命令重写为 PEXPIREAT key millisseconds
        rewriteClientCommandVector(c,3,shared.pexpireat,c->argv[1],milliseconds_obj);
        decrRefCount(milliseconds_obj);
        signalModifiedKey(c, c->db, c->argv[1]);
        notifyKeyspaceEvent(NOTIFY_GENERIC,"expire",c->argv[1],c->db->id);
        server.dirty++;
    } else if (flags & OBJ_PERSIST) {
        // ZZJ TODO removeExpire还没看
        if (removeExpire(c->db, c->argv[1])) {
            signalModifiedKey(c, c->db, c->argv[1]);
            // 命令重写为PERSIST key
            rewriteClientCommandVector(c, 2, shared.persist, c->argv[1]);
            notifyKeyspaceEvent(NOTIFY_GENERIC,"persist",c->argv[1],c->db->id);
            server.dirty++;
        }
    }
}

// GETDEL命令
void getdelCommand(client *c) {
    if (getGenericCommand(c) == C_ERR) return;
    // ZZJ TODO dbSyncDelete还没看
    if (dbSyncDelete(c->db, c->argv[1])) {
        /* Propagate as DEL command */
        rewriteClientCommandVector(c,2,shared.del,c->argv[1]);
        signalModifiedKey(c, c->db, c->argv[1]);
        notifyKeyspaceEvent(NOTIFY_GENERIC, "del", c->argv[1], c->db->id);
        server.dirty++;
    }
}

// GETSET命令
void getsetCommand(client *c) {
    if (getGenericCommand(c) == C_ERR) return;
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    setKey(c,c->db,c->argv[1],c->argv[2],0);
    notifyKeyspaceEvent(NOTIFY_STRING,"set",c->argv[1],c->db->id);
    server.dirty++;

    /* Propagate as SET command */
    // ZZJ TODO 这个还没看，注意和rewriteClientCommandVector区分
    // 看这个方法的意思应该是把命令中的第0个元素替换成SET，也就是把GETSET命令替换为SET，其他不变。
    rewriteClientCommandArgument(c,0,shared.set);
}

// SETRANGE命令 setrange key offset value
void setrangeCommand(client *c) {
    robj *o;
    long offset;
    sds value = c->argv[3]->ptr;

    if (getLongFromObjectOrReply(c,c->argv[2],&offset,NULL) != C_OK)
        return;

    if (offset < 0) {
        addReplyError(c,"offset is out of range");
        return;
    }

    // ZZJ TODO lookupKeyWrite还没看
    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        /* Return 0 when setting nothing on a non-existing string */
        if (sdslen(value) == 0) {
            addReply(c,shared.czero);
            return;
        }

        /* Return when the resulting string exceeds allowed size */
        if (checkStringLength(c,offset,sdslen(value)) != C_OK)
            return;

        o = createObject(OBJ_STRING,sdsnewlen(NULL, offset+sdslen(value)));
        // ZZJ TODO dbAdd还没看
        dbAdd(c->db,c->argv[1],o);
    } else {
        size_t olen;

        /* Key exists, check type */
        if (checkType(c,o,OBJ_STRING))
            return;

        /* Return existing string length when setting nothing */
        olen = stringObjectLen(o);
        if (sdslen(value) == 0) {
            // ZZJ TODO addReplyLongLong还没看
            addReplyLongLong(c,olen);
            return;
        }

        /* Return when the resulting string exceeds allowed size */
        if (checkStringLength(c,offset,sdslen(value)) != C_OK)
            return;

        /* Create a copy when the object is shared or encoded. */
        // ZZJ TODO dbUnshareStringValue还没看
        o = dbUnshareStringValue(c->db,c->argv[1],o);
    }

    // 如果之前key存在，到这里，o是之前存在的值，还没修改
    if (sdslen(value) > 0) {
        o->ptr = sdsgrowzero(o->ptr,offset+sdslen(value));
        // ZZJ TODO 如果在中文，这个offset对吗，中文一个占用不止一个字节
        memcpy((char*)o->ptr+offset,value,sdslen(value));
        signalModifiedKey(c,c->db,c->argv[1]);
        notifyKeyspaceEvent(NOTIFY_STRING,
            "setrange",c->argv[1],c->db->id);
        server.dirty++;
    }
    addReplyLongLong(c,sdslen(o->ptr));
}

// GETRANGE命令
void getrangeCommand(client *c) {
    robj *o;
    long long start, end;
    char *str, llbuf[32];
    size_t strlen;

    if (getLongLongFromObjectOrReply(c,c->argv[2],&start,NULL) != C_OK)
        return;
    if (getLongLongFromObjectOrReply(c,c->argv[3],&end,NULL) != C_OK)
        return;
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.emptybulk)) == NULL ||
        checkType(c,o,OBJ_STRING)) return;

    if (o->encoding == OBJ_ENCODING_INT) {
        str = llbuf;
        strlen = ll2string(llbuf,sizeof(llbuf),(long)o->ptr);
    } else {
        str = o->ptr;
        strlen = sdslen(str);
    }

    /* Convert negative indexes */
    if (start < 0 && end < 0 && start > end) {
        addReply(c,shared.emptybulk);
        return;
    }
    if (start < 0) start = strlen+start;
    if (end < 0) end = strlen+end;
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if ((unsigned long long)end >= strlen) end = strlen-1;

    /* Precondition: end >= 0 && end < strlen, so the only condition where
     * nothing can be returned is: start > end. */
    if (start > end || strlen == 0) {
        addReply(c,shared.emptybulk);
    } else {
        // ZZJ TODO 这个还没看
        addReplyBulkCBuffer(c,(char*)str+start,end-start+1);
    }
}

void mgetCommand(client *c) {
    int j;

    // ZZJ TODO 这个还没看
    addReplyArrayLen(c,c->argc-1);
    for (j = 1; j < c->argc; j++) {
        robj *o = lookupKeyRead(c->db,c->argv[j]);
        if (o == NULL) {
            addReplyNull(c);
        } else {
            if (o->type != OBJ_STRING) {
                addReplyNull(c);
            } else {
                addReplyBulk(c,o);
            }
        }
    }
}

void msetGenericCommand(client *c, int nx) {
    int j;

    if ((c->argc % 2) == 0) {
        // ZZJ TODO 这个还没看
        addReplyErrorArity(c);
        return;
    }

    /* Handle the NX flag. The MSETNX semantic is to return zero and don't
     * set anything if at least one key already exists. */
    if (nx) {
        for (j = 1; j < c->argc; j += 2) {
            if (lookupKeyWrite(c->db,c->argv[j]) != NULL) {
                addReply(c, shared.czero);
                return;
            }
        }
    }

    int setkey_flags = nx ? SETKEY_DOESNT_EXIST : 0;
    for (j = 1; j < c->argc; j += 2) {
        // 如果encode了，会返回新的obj，tryObjectEncoding里会处理旧obj的内存释放
        c->argv[j+1] = tryObjectEncoding(c->argv[j+1]);
        setKey(c, c->db, c->argv[j], c->argv[j + 1], setkey_flags);
        notifyKeyspaceEvent(NOTIFY_STRING,"set",c->argv[j],c->db->id);
        /* In MSETNX, It could be that we're overriding the same key, we can't be sure it doesn't exist. */
        if (nx)
            setkey_flags = SETKEY_ADD_OR_UPDATE;
    }
    server.dirty += (c->argc-1)/2;
    // ZZJ TODO 这里有nx的是否，返回shared.cone（返回1），有什么说法吗？
    addReply(c, nx ? shared.cone : shared.ok);
}

void msetCommand(client *c) {
    msetGenericCommand(c,0);
}

void msetnxCommand(client *c) {
    msetGenericCommand(c,1);
}

void incrDecrCommand(client *c, long long incr) {
    long long value, oldvalue;
    robj *o, *new;

    o = lookupKeyWrite(c->db,c->argv[1]);
    if (checkType(c,o,OBJ_STRING)) return;
    if (getLongLongFromObjectOrReply(c,o,&value,NULL) != C_OK) return;

    oldvalue = value;
    if ((incr < 0 && oldvalue < 0 && incr < (LLONG_MIN-oldvalue)) ||
        (incr > 0 && oldvalue > 0 && incr > (LLONG_MAX-oldvalue))) {
        addReplyError(c,"increment or decrement would overflow");
        return;
    }
    value += incr;

    if (o && o->refcount == 1 && o->encoding == OBJ_ENCODING_INT &&
        (value < 0 || value >= OBJ_SHARED_INTEGERS) &&
        value >= LONG_MIN && value <= LONG_MAX)
    {
        // 如果value是int编码的，并且不在缓存范围内，则直接替换obj.ptr的值，不需要新建obj
        // 如果value在缓存范围内，下面的createStringObjectFromLongLongForValue方法中会判断是否在缓存内，在缓存内会复用缓存对象。
        new = o;
        o->ptr = (void*)((long)value);
    } else {
        new = createStringObjectFromLongLongForValue(value);
        if (o) {
            dbReplaceValue(c->db,c->argv[1],new);
        } else {
            dbAdd(c->db,c->argv[1],new);
        }
    }
    signalModifiedKey(c,c->db,c->argv[1]);
    notifyKeyspaceEvent(NOTIFY_STRING,"incrby",c->argv[1],c->db->id);
    server.dirty++;
    addReplyLongLong(c, value);
}

void incrCommand(client *c) {
    incrDecrCommand(c,1);
}

void decrCommand(client *c) {
    incrDecrCommand(c,-1);
}

void incrbyCommand(client *c) {
    long long incr;

    if (getLongLongFromObjectOrReply(c, c->argv[2], &incr, NULL) != C_OK) return;
    incrDecrCommand(c,incr);
}

void decrbyCommand(client *c) {
    long long incr;

    if (getLongLongFromObjectOrReply(c, c->argv[2], &incr, NULL) != C_OK) return;
    /* Overflow check: negating LLONG_MIN will cause an overflow */
    if (incr == LLONG_MIN) {
        addReplyError(c, "decrement would overflow");
        return;
    }
    incrDecrCommand(c,-incr);
}

void incrbyfloatCommand(client *c) {
    long double incr, value;
    robj *o, *new;

    o = lookupKeyWrite(c->db,c->argv[1]);
    if (checkType(c,o,OBJ_STRING)) return;
    if (getLongDoubleFromObjectOrReply(c,o,&value,NULL) != C_OK ||
        getLongDoubleFromObjectOrReply(c,c->argv[2],&incr,NULL) != C_OK)
        return;

    value += incr;
    if (isnan(value) || isinf(value)) {
        addReplyError(c,"increment would produce NaN or Infinity");
        return;
    }
    new = createStringObjectFromLongDouble(value,1);
    if (o)
        dbReplaceValue(c->db,c->argv[1],new);
    else
        dbAdd(c->db,c->argv[1],new);
    signalModifiedKey(c,c->db,c->argv[1]);
    notifyKeyspaceEvent(NOTIFY_STRING,"incrbyfloat",c->argv[1],c->db->id);
    server.dirty++;
    addReplyBulk(c,new);

    /* Always replicate INCRBYFLOAT as a SET command with the final value
     * in order to make sure that differences in float precision or formatting
     * will not create differences in replicas or after an AOF restart. */
    rewriteClientCommandArgument(c,0,shared.set);
    rewriteClientCommandArgument(c,2,new);
    rewriteClientCommandArgument(c,3,shared.keepttl);
}

void appendCommand(client *c) {
    size_t totlen;
    robj *o, *append;

    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        /* Create the key */
        c->argv[2] = tryObjectEncoding(c->argv[2]);
        dbAdd(c->db,c->argv[1],c->argv[2]);
        // ZZJ TODO 这里为什么要incrRefCount，是因为加入dbAdd了吗？
        incrRefCount(c->argv[2]);
        totlen = stringObjectLen(c->argv[2]);
    } else {
        /* Key exists, check type */
        if (checkType(c,o,OBJ_STRING))
            return;

        /* "append" is an argument, so always an sds */
        append = c->argv[2];
        if (checkStringLength(c,stringObjectLen(o),sdslen(append->ptr)) != C_OK)
            return;

        /* Append the value */
        // ZZJ TODO 这个还没看
        o = dbUnshareStringValue(c->db,c->argv[1],o);
        o->ptr = sdscatlen(o->ptr,append->ptr,sdslen(append->ptr));
        totlen = sdslen(o->ptr);
    }
    signalModifiedKey(c,c->db,c->argv[1]);
    notifyKeyspaceEvent(NOTIFY_STRING,"append",c->argv[1],c->db->id);
    server.dirty++;
    addReplyLongLong(c,totlen);
}

void strlenCommand(client *c) {
    robj *o;
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.czero)) == NULL ||
        checkType(c,o,OBJ_STRING)) return;
    addReplyLongLong(c,stringObjectLen(o));
}

/* LCS key1 key2 [LEN] [IDX] [MINMATCHLEN <len>] [WITHMATCHLEN] */
// ZZJ TODO 没细看
void lcsCommand(client *c) {
    uint32_t i, j;
    long long minmatchlen = 0;
    sds a = NULL, b = NULL;
    int getlen = 0, getidx = 0, withmatchlen = 0;
    robj *obja = NULL, *objb = NULL;

    obja = lookupKeyRead(c->db,c->argv[1]);
    objb = lookupKeyRead(c->db,c->argv[2]);
    if ((obja && obja->type != OBJ_STRING) ||
        (objb && objb->type != OBJ_STRING))
    {
        addReplyError(c,
            "The specified keys must contain string values");
        /* Don't cleanup the objects, we need to do that
         * only after calling getDecodedObject(). */
        obja = NULL;
        objb = NULL;
        goto cleanup;
    }
    obja = obja ? getDecodedObject(obja) : createStringObject("",0);
    objb = objb ? getDecodedObject(objb) : createStringObject("",0);
    a = obja->ptr;
    b = objb->ptr;

    for (j = 3; j < (uint32_t)c->argc; j++) {
        char *opt = c->argv[j]->ptr;
        int moreargs = (c->argc-1) - j;

        if (!strcasecmp(opt,"IDX")) {
            getidx = 1;
        } else if (!strcasecmp(opt,"LEN")) {
            getlen = 1;
        } else if (!strcasecmp(opt,"WITHMATCHLEN")) {
            withmatchlen = 1;
        } else if (!strcasecmp(opt,"MINMATCHLEN") && moreargs) {
            if (getLongLongFromObjectOrReply(c,c->argv[j+1],&minmatchlen,NULL)
                != C_OK) goto cleanup;
            if (minmatchlen < 0) minmatchlen = 0;
            j++;
        } else {
            addReplyErrorObject(c,shared.syntaxerr);
            goto cleanup;
        }
    }

    /* Complain if the user passed ambiguous parameters. */
    if (getlen && getidx) {
        addReplyError(c,
            "If you want both the length and indexes, please just use IDX.");
        goto cleanup;
    }

    /* Detect string truncation or later overflows. */
    if (sdslen(a) >= UINT32_MAX-1 || sdslen(b) >= UINT32_MAX-1) {
        addReplyError(c, "String too long for LCS");
        goto cleanup;
    }

    /* Compute the LCS using the vanilla dynamic programming technique of
     * building a table of LCS(x,y) substrings. */
    uint32_t alen = sdslen(a);
    uint32_t blen = sdslen(b);

    /* Setup an uint32_t array to store at LCS[i,j] the length of the
     * LCS A0..i-1, B0..j-1. Note that we have a linear array here, so
     * we index it as LCS[j+(blen+1)*i] */
    #define LCS(A,B) lcs[(B)+((A)*(blen+1))]

    /* Try to allocate the LCS table, and abort on overflow or insufficient memory. */
    unsigned long long lcssize = (unsigned long long)(alen+1)*(blen+1); /* Can't overflow due to the size limits above. */
    unsigned long long lcsalloc = lcssize * sizeof(uint32_t);
    uint32_t *lcs = NULL;
    if (lcsalloc < SIZE_MAX && lcsalloc / lcssize == sizeof(uint32_t)) {
        if (lcsalloc > (size_t)server.proto_max_bulk_len) {
            addReplyError(c, "Insufficient memory, transient memory for LCS exceeds proto-max-bulk-len");
            goto cleanup;
        }
        lcs = ztrymalloc(lcsalloc);
    }
    if (!lcs) {
        addReplyError(c, "Insufficient memory, failed allocating transient memory for LCS");
        goto cleanup;
    }

    /* Start building the LCS table. */
    for (uint32_t i = 0; i <= alen; i++) {
        for (uint32_t j = 0; j <= blen; j++) {
            if (i == 0 || j == 0) {
                /* If one substring has length of zero, the
                 * LCS length is zero. */
                LCS(i,j) = 0;
            } else if (a[i-1] == b[j-1]) {
                /* The len LCS (and the LCS itself) of two
                 * sequences with the same final character, is the
                 * LCS of the two sequences without the last char
                 * plus that last char. */
                LCS(i,j) = LCS(i-1,j-1)+1;
            } else {
                /* If the last character is different, take the longest
                 * between the LCS of the first string and the second
                 * minus the last char, and the reverse. */
                uint32_t lcs1 = LCS(i-1,j);
                uint32_t lcs2 = LCS(i,j-1);
                LCS(i,j) = lcs1 > lcs2 ? lcs1 : lcs2;
            }
        }
    }

    /* Store the actual LCS string in "result" if needed. We create
     * it backward, but the length is already known, we store it into idx. */
    uint32_t idx = LCS(alen,blen);
    sds result = NULL;        /* Resulting LCS string. */
    void *arraylenptr = NULL; /* Deferred length of the array for IDX. */
    uint32_t arange_start = alen, /* alen signals that values are not set. */
             arange_end = 0,
             brange_start = 0,
             brange_end = 0;

    /* Do we need to compute the actual LCS string? Allocate it in that case. */
    int computelcs = getidx || !getlen;
    if (computelcs) result = sdsnewlen(SDS_NOINIT,idx);

    /* Start with a deferred array if we have to emit the ranges. */
    uint32_t arraylen = 0;  /* Number of ranges emitted in the array. */
    if (getidx) {
        addReplyMapLen(c,2);
        addReplyBulkCString(c,"matches");
        arraylenptr = addReplyDeferredLen(c);
    }

    i = alen, j = blen;
    while (computelcs && i > 0 && j > 0) {
        int emit_range = 0;
        if (a[i-1] == b[j-1]) {
            /* If there is a match, store the character and reduce
             * the indexes to look for a new match. */
            result[idx-1] = a[i-1];

            /* Track the current range. */
            if (arange_start == alen) {
                arange_start = i-1;
                arange_end = i-1;
                brange_start = j-1;
                brange_end = j-1;
            } else {
                /* Let's see if we can extend the range backward since
                 * it is contiguous. */
                if (arange_start == i && brange_start == j) {
                    arange_start--;
                    brange_start--;
                } else {
                    emit_range = 1;
                }
            }
            /* Emit the range if we matched with the first byte of
             * one of the two strings. We'll exit the loop ASAP. */
            if (arange_start == 0 || brange_start == 0) emit_range = 1;
            idx--; i--; j--;
        } else {
            /* Otherwise reduce i and j depending on the largest
             * LCS between, to understand what direction we need to go. */
            uint32_t lcs1 = LCS(i-1,j);
            uint32_t lcs2 = LCS(i,j-1);
            if (lcs1 > lcs2)
                i--;
            else
                j--;
            if (arange_start != alen) emit_range = 1;
        }

        /* Emit the current range if needed. */
        uint32_t match_len = arange_end - arange_start + 1;
        if (emit_range) {
            if (minmatchlen == 0 || match_len >= minmatchlen) {
                if (arraylenptr) {
                    addReplyArrayLen(c,2+withmatchlen);
                    addReplyArrayLen(c,2);
                    addReplyLongLong(c,arange_start);
                    addReplyLongLong(c,arange_end);
                    addReplyArrayLen(c,2);
                    addReplyLongLong(c,brange_start);
                    addReplyLongLong(c,brange_end);
                    if (withmatchlen) addReplyLongLong(c,match_len);
                    arraylen++;
                }
            }
            arange_start = alen; /* Restart at the next match. */
        }
    }

    /* Signal modified key, increment dirty, ... */

    /* Reply depending on the given options. */
    if (arraylenptr) {
        addReplyBulkCString(c,"len");
        addReplyLongLong(c,LCS(alen,blen));
        setDeferredArrayLen(c,arraylenptr,arraylen);
    } else if (getlen) {
        addReplyLongLong(c,LCS(alen,blen));
    } else {
        addReplyBulkSds(c,result);
        result = NULL;
    }

    /* Cleanup. */
    sdsfree(result);
    zfree(lcs);

cleanup:
    if (obja) decrRefCount(obja);
    if (objb) decrRefCount(objb);
    return;
}

