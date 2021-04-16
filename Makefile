BASE=../../../..


LOCAL_CFLAGS=-I/usr/include -fpic -Werror=declaration-after-statement
LOCAL_LDFLAGS=-L/usr/lib64 -lpthread 

LOCAL_OBJS=thpool.o

include $(BASE)/build/modmake.rules