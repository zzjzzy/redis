# Redis源码学习路线指南

## 进度 ！！！！quicklistNext的优化，在github上提个discussion
main方法看到这里了initServerConfig();，目前看到这里暂停了，先看基础数据结构

===== map =====
dict.c 看完了

===== string =====
sds.c 看完了

===== set or list =====
intset.c（set小对象用） 看完了
skiplist(zset的底层实现(大对象)) 不用看（在t_zset中实现的，直接看t_zset）
adlist.c（被listpack替代） 看完了
listpack（替代ziplist） 看完了
ziplist 可以先不看，后面看其他部分的时候如果看到了，再看
quicklist(adlist+listpack的混合,list的唯一实现) 看完了

===== t系列 =====
t_hash
t_list
t_set
t_string
t_zset

===== 其他 =====
rax(STREAM 的核心)

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
### 看下代码里的ZZJ TODO
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

- RDB文件是什么样的？

## 知识点
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
