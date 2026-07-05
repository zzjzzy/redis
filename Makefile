# Top level makefile, the real shit is at src/Makefile

default: all

.DEFAULT:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

mydebug:
	make clean
	make CFLAGS="-g -O0" MALLOC=jemalloc

myhiredis:
	cd deps && make hiredis && cd ..
#   如果按照下面命令报错【'jemalloc/jemalloc.h' file not found】，就单独把jemalloc make一次试试
#    make hiredis lua fpconv hdr_histogram jemalloc linenoise

# mac模拟网络丢包
# 验证功能是否开启 sudo pfctl -E
# 验证规则是否加上sudo pfctl -a com.apple/lo_drop -sr
# 关闭功能sudo pfctl -d
mocknetblock:
	echo "block drop on lo0 proto tcp from 127.0.0.1 port 6379 to any" | sudo pfctl -a com.apple/lo_drop_reply -f -
	# 关闭回包后，再kill掉master，模拟master下线，由于不会收到fin包，不会立即触发读时间，现象和不kill是一样的
	#lsof -t -iTCP:6379 -sTCP:LISTEN | xargs kill
	# echo "block drop on lo0 proto tcp from any to 127.0.0.1 port 6379" | sudo pfctl -a com.apple/lo_drop -f -

mocknetblockclear:
	sudo pfctl -a com.apple/lo_drop_reply -F all
	#sudo pfctl -a com.apple/lo_drop -F all

cli:
	./src/redis-cli

cli1:
	./src/redis-cli -p 6380

cli2:
	./src/redis-cli -p 6381

sent:
	./src/redis-cli -p 26379

sent1:
	./src/redis-cli -p 26380

sent2:
	./src/redis-cli -p 26381

.PHONY: install
