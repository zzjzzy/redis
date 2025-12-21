# Redis源码学习路线指南

## 进度
main方法看到这里了initServerConfig();，目前看到这里暂停了，先去看dict.c了。

## TODO 
### zmalloc
zmalloc没细看，有空再研究

## Q&A
- redis hash rehash过程中，如果有并发问题，怎么办
- dict中元素不使用了，如何释放的内存

## 知识点
### 计算大于x的最下2次幂数
看_dictNextExp(dict.c)

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

### 通过定义宏而不是函数优化性能
dict.h中类似这种定义，#define dictHashKey(d, key) ((d)->type->hashFunction(key))，为什么要定义成宏，而不是声明一个函数
宏是内联展开的，没有函数调用开销。

### struct dictEntry使用union表示val
有点：节省内存占用、如果统一使用指针，会增加内存操作的耗时

### redis把dictEntry分成entryIsKey、entryIsNormal、entryIsNoValue
减少内存占用，可以问ai【redis为什么要把dictEntry分成entryIsKey、entryIsNormal、entryIsNoValue】

## PR!
### siphash.c from -> form
```c
/*
   2. Hard-code rounds in the hope the compiler can optimize it more
      in this raw from. Anyway we always want the standard 2-4 variant.
*/
```

## 可以写文章的功能
1. redis如何执行一条命令（可以结合微信收藏的一篇文章学习）

## 项目概述
Redis是一个开源的高性能键值数据库，采用C语言编写。本指南将帮助你系统地学习Redis源码。

## 学习路径

### 第一阶段：基础数据结构（1-2周）
**目标：理解Redis的核心数据结构实现**

1. **sds.c/h** - 简单动态字符串
   - Redis字符串的基础实现
   - 学习内存管理和字符串操作

2. **adlist.c/h** - 双向链表
   - 基础链表数据结构

3. **dict.c/h** - 哈希表
   - Redis的核心数据结构，用于存储键值对

4. **intset.c/h** - 整数集合
   - 小整数集合的优化存储

### 第二阶段：核心对象系统（1周）
**目标：理解Redis的对象模型**

5. **object.c/h** - Redis对象系统
   - `robj`结构的实现
   - 引用计数和内存管理

6. **server.h** - 主要数据结构定义
   - `struct redisServer`全局结构
   - `struct client`客户端结构

### 第三阶段：网络和事件处理（1周）
**目标：理解Redis的网络架构**

7. **ae.c/h** - 事件循环
   - Redis的事件驱动架构

8. **networking.c** - 网络通信
   - 客户端连接管理
   - 请求处理和响应

### 第四阶段：数据持久化（1周）
**目标：理解Redis的持久化机制**

9. **rdb.c/h** - RDB快照
   - 内存快照持久化

10. **aof.c/h** - AOF日志
    - 命令日志持久化

### 第五阶段：命令执行（1周）
**目标：理解命令处理流程**

11. **server.c** - 主服务器逻辑
    - `main()`函数和服务器启动
    - 命令调度和执行

12. **db.c** - 数据库操作
    - 键值对的基本操作

### 第六阶段：数据类型实现（2-3周）
**目标：深入理解各种数据类型的实现**

13. **t_string.c** - 字符串类型
14. **t_list.c** - 列表类型  
15. **t_hash.c** - 哈希类型
16. **t_set.c** - 集合类型
17. **t_zset.c** - 有序集合类型
18. **t_stream.c** - 流类型

### 第七阶段：高级特性（2-3周）
**目标：理解Redis的高级功能**

19. **replication.c** - 主从复制
20. **cluster.c** - 集群功能
21. **script.c/lua** - Lua脚本支持
22. **module.c** - 模块系统

## 实践建议

### 1. 边读边调试
```bash
# 编译Redis
make

# 启动Redis服务器
cd src && ./redis-server

# 使用redis-cli测试
./redis-cli
```

### 2. 重点关注的文件
- **server.h** - 所有核心数据结构的定义
- **server.c** - 主逻辑入口
- **networking.c** - 网络通信核心
- **db.c** - 数据库操作接口

### 3. 学习方法
- 从简单的数据结构开始（sds, adlist）
- 结合使用场景理解代码
- 使用调试器跟踪函数调用
- 阅读代码注释（Redis代码注释很详细）

### 4. 推荐阅读顺序
按照阶段顺序进行，每个阶段完成后可以尝试实现一些简单的功能来巩固理解。

## 学习资源

- 官方文档：https://redis.io/documentation
- GitHub仓库：https://github.com/redis/redis
- Redis设计与实现（书籍推荐）

## 进度跟踪

使用下面的表格来跟踪你的学习进度：

| 阶段 | 开始日期 | 完成日期 | 备注 |
|------|----------|----------|------|
| 基础数据结构 | | | |
| 核心对象系统 | | | |
| 网络和事件处理 | | | |
| 数据持久化 | | | |
| 命令执行 | | | |
| 数据类型实现 | | | |
| 高级特性 | | | |

---
*最后更新：2025-12-13*