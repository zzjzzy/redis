make CFLAGS="-g -O0" MALLOC=jemalloc
Makefile中添加了命令，可以不用执行上面的命令，执行make mydebug即可
然后运行右上角的redis-server-my，记得去掉before配置。
redis-server-my已经保存为项目配置项，切换设备也能用。
然后执行./src/redis-cli连接进行测试
