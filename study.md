！！！！！这个弃用，改到语雀

# Redis源码学习路线指南

常用代码
```c
// server.h
struct redisObject {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS; /* LRU time (relative to global lru_clock) or
                            * LFU data (least significant 8 bits frequency
                            * and most significant 16 bits access time). */
    int refcount;
    void *ptr;
};
```

20260501
最近在看slaveof执行流程，这个在语雀上记笔记呢，直接在语雀上看进度

## 进度 ！！！！quicklistNext的优化，在github上提个discussion
main方法看到这里了initServerConfig();，目前看到这里暂停了，先看基础数据结构

===== map =====
dict.c 看完了

===== string =====
sds.c 看完了

===== set or list =====
intset.c（set小对象用） 看完了
skiplist(zset的底层实现(大对象)) 不用看（在t_zset中实现的，直接看t_zset）
adlist.c（被listpack替代） 看完了（scanGenericCommand用的adlist装的scan结果，server.h中也有很多内部结构使用adlist，比如list *clients;）
listpack（替代ziplist） 看完了
ziplist 可以先不看，后面看其他部分的时候如果看到了，再看
quicklist(adlist+listpack的混合,list的唯一实现) 看完了

===== t系列 =====
t_hash(1175行) 已看完
t_list(1388行) 已看完
t_set(1680行)  已看完
t_string(1009行) 已看完
t_zset(4461行) 已看完（不容易啊！）
t_stream(4051行)
看完t_系列后去看db.c和notify.c和networking.c

===== 其他 =====
2026-03-29记录：下面的已经阶段性看完了一些，剩下的先不看了，后面再看源码基于某个功能去看，然后在看的过程中了解
object.c 【Memory introspection】以上都看完了，其他部分等用到了再看。
server.c dict(从【Hash table type implementation】到【int allPersistenceDisabled(void)】) 已看完
server.h struct client 已看完，后面有用到啥字段再看就行
db.c(2560行) 已看完（db.c引用了很多其他xxx.c，有很多还没看，估计后面看的越来越多了，可能还会回头再重新看一遍db.c）
evict.c(770行) 已看完，粗略看了下，很多细节没有研究，知道每个方法大概在干什么就够了，后面如果有必要再详细研究
networking.c(4589行) 已粗略看完，看到processInlineBuffer，processInlineBuffer及之后粗略过了一遍，不细看了，后面哪里用到了再看（看完这个再会看一个比如t_string.c，看下addReply怎么用的）
atomicvar.h(158行) 已看完，这个没有实现，只是重新define了下c原生的功能
expire.c(754行) 已粗略看完，就是对expire命令的支持、主动expire等能力，没怎么细看，后面用到有需要再看。
connection.c(208行) 已看完
socket.c(473行)  已看完，梳理了一些方法调用流程，想看调用流程，可以搜索这个文件的“流程”关键字
unix.c(209行) 已看完，这个比较少，用到啥看啥就行
anet.c(730行) 已看完，是一些socket, listen, connect等网络调用的封装
ae.c(512行) 已看完，是一些底层的epoll等事件循环操作
syncio.c(145行) 已看完
notify.c(145行) 已看完
pubsub.c(754行) 待看  这个后续可以结合pub,sub命令怎么执行的来看
timeout.c(202行) 已看完
blocked.c(767行) 这个没看完，先不看了
monotonic.c(180行)
latency.c(739行)
rax.c(基数树，STREAM 的核心)(1927行)
zmalloc.c(852行)
cluster.c(7825行)
multi.c(500行) 待看
rdb.c(3722行)
logreqres.c(315行) 已看完，reqresAppendResponse看的比较粗略
tracking.c(660行) 待看
module.c(13904行)
slowlog.c(206行)

===== 废弃 =====
zipmap.c 不用学

总结
- String → SDS / long
- List → Quicklist → Listpack 节点
- Hash → Listpack / Dict → SDS
- Set → Intset / Dict → SDS
- Sorted Set → Listpack / (Skiplist + Dict) → SDS
- Stream → Radix Tree → Listpack


## TODO 
### util.c stringmatchlen_impl这个可能是简单的正则匹配实现，有空研究下
### 看下reply = opt_withscore ? shared.nullarray[c->resp] : shared.null[c->resp];这种的作用
### 有空再总结下各个基础数据结构的实现：结构、数据怎么编码的、怎么存储的
### sds的这个特性【an SDS string is always an odd pointer 】再深入研究下，用在dict中有什么优势？
### 看下代码里的ZZJ TODO
### t_string.c的lcsCommand没细看，需要研究下
### util.c中很多方法没看，只知道是做什么的
### quicklist.c quicklistGetIteratorAtIdx
这个方法看明白了，但是感觉各种索引计算还是有点乱，有时间再梳理下

### quicklist.c quicklistNext
看明白了，但是有点复杂，有空再梳理下

### zmalloc
zmalloc没细看，有空再研究

### dictScanDefrag
dict.c dictScanDefrag中的桶遍历算法没明白，有时间再继续研究

### 这个方法有时间看下，感觉刷算法会遇到
string2ll
ll2string
lpStringToInt64

### *lpGetWithSize unsigned转signed那儿没有细看

### lpRandomPairs assert
listpack.c lpRandomPairs 会有assert(total_size);，如果total_size真为空，调用者怎么处理的？

### listpack.c 中lpNextRandom应该是用到了什么随机算法
看下labuladong的算法里有没有提到

### test看一下
有时间把redis的test看一下，看别人是怎么写测试的

## Q&A
- redis hash rehash过程中，如果有并发问题，怎么办

- dict中元素不使用了，如何释放的内存

- dict rehash pause和resume起什么作用
dictResetIterator会dictResumeRehashing

- redis dict中的两个哈希桶是怎么用的，哈希期间是从桶0迁移到桶1，那hash期间增删改是在哪个桶操作？
```c
    // 如果在rehash中，insert到桶1
    /* If rehashing is ongoing, we insert in table 1, otherwise in table 0.
     * Assert that the provided bucket is the right table. */
    int htidx = dictIsRehashing(d) ? 1 : 0;
```

- redis为什么快？
宏调用减少函数调用开销，宏调用还能避免参数解析，比如server.h中【#define serverLog(level, ...) do {...】
分支预测

- RDB文件是什么样的？

## 笔记
### _anetTcpServer处理地址绑定
```c
    // 这段代码就和redis配置文件对上了，指定服务端绑定的地址
    if (bindaddr && !strcmp("*", bindaddr))
        bindaddr = NULL;
    if (af == AF_INET6 && bindaddr && !strcmp("::*", bindaddr))
        bindaddr = NULL;
```
### generate-command-code.py 这个文件可以用于生成新的redis命令，可以试下
### 源码读着很顺，一个文件从上往下读就可以，不用跳来跳去，基本是你读到一个方法定义，下面很快就会用到这个方法。
### redis中用到的数据结构，这个方法应该能说明问题
```c
char *strEncoding(int encoding) {
    switch(encoding) {
    case OBJ_ENCODING_RAW: return "raw";
    case OBJ_ENCODING_INT: return "int";
    case OBJ_ENCODING_HT: return "hashtable";
    case OBJ_ENCODING_QUICKLIST: return "quicklist";
    case OBJ_ENCODING_LISTPACK: return "listpack";
    case OBJ_ENCODING_INTSET: return "intset";
    case OBJ_ENCODING_SKIPLIST: return "skiplist";
    case OBJ_ENCODING_EMBSTR: return "embstr";
    case OBJ_ENCODING_STREAM: return "stream";
    default: return "unknown";
    }
}
```

### 计算大于x的最下2次幂数
看_dictNextExp(dict.c)

### listpack不能存>=UINT32_MAX的
assert(lpbytes < UINT32_MAX); /* larger values can't be stored */

## 可以练手的功能
1. 自己实现一个redis命令

## redis中的性能优化
### 小字段放后面，优化内存(dict.h)
```c
/* Keep small vars at end for optimal (minimal) struct padding */
int16_t pauserehash;
```

### serverLog优化，不用打印的日志可以避免函数提前调用(server.h)
```c
/* Use macro for checking log level to avoid evaluating arguments in cases log
 * should be ignored due to low level. */
#define serverLog(level, ...) do {\
        if (((level)&0xff) < server.verbosity) break;\
        _serverLog(level, __VA_ARGS__);\
    } while(0)
```

### 分支预测
quicklist.c用到挺多，搜索likely和unlikely

### 通过定义宏而不是函数优化性能
dict.h中类似这种定义，#define dictHashKey(d, key) ((d)->type->hashFunction(key))，为什么要定义成宏，而不是声明一个函数
宏是内联展开的，没有函数调用开销。

### struct dictEntry使用union表示val
有点：节省内存占用、如果统一使用指针，会增加内存操作的耗时

### redis把dictEntry分成entryIsKey、entryIsNormal、entryIsNoValue
减少内存占用，可以问ai【redis为什么要把dictEntry分成entryIsKey、entryIsNormal、entryIsNoValue】

### listpack的内存优化就很好
- 数字型字符串转成数字存储，比如123这个字符串，如果用字符串存储，占3个字节，如果转成int8，只占用1个字节

## PR!
### 后续把PR直接写在代码对应的地方了，通过版本号标识是第几次PR 示例：ZZJ PRV2 XXX
### server.c
- 多了个for: Dict for for case-insensitive search using null terminated C strings.

### [listpack.c]fetch the elements form the listpack into a output array respecting the original order.
form应该是from，a应该是an
pickindex++; 前面多了个空格
这里可以加个注释，好理解些 while (pickindex < count && lpindex == picks[pickindex].index) {

### quicklist.h
- 这个字段定义没有写到quicklistNode开头的注释中：unsigned int dont_compress : 1; /* prevent compression of entry that will be used later */
- 多了个of：Bookmarks are padded with realloc at the end of of the quicklist struct.
- were应该是where：They should only be used for very big lists if thousands of nodes were the

### quicklist.c quicklistNext
quicklist.c quicklistNext感觉写的有点复杂，看能不能简化下步骤。也不是太好理解，大概梳理了下，算是明白了，但是不思路还是不清晰。

### object.c 注释
/* If the maxmemory policy permits, we can still return shared integers */
感觉这句注释不太对，可以发起个讨论讨论下

## PR!(old 以下PR已提交，待通过)
### siphash.c from -> form
```c
/*
   2. Hard-code rounds in the hope the compiler can optimize it more
      in this raw from. Anyway we always want the standard 2-4 variant.
*/
```

### 多了个The
* The when the would_regrow argument is set to 1, it prevents the use of
* SDS_TYPE_5, which is desired when the sds is likely to be changed again.

### 多了个into
/* Helper method to store a string into(这个into应该是打多了) from val or lval into dest */

## 可以写文章的功能
1. redis如何执行一条命令（可以结合微信收藏的一篇文章学习）
2. 当执行set key=val时，这条数据在内存中是怎样存储的？
