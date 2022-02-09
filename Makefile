# * ---------------------
# * UTF-8 한글확인 용
# * ---------------------
 
.SUFFIXES = .cpp .o
CXX=g++

# sha1.c, sha1.h 는 참조로 프로젝트에 추가만 한다, 실제는 openssl 것을 사용한다

TARGET = amicalld_r
SRCS =  http.cpp \
		http_routes.cpp \
		websocket.cpp \
		ws_routes.cpp \
		amicalldversion.cpp \
		processevents.cpp \
		amiaction.cpp \
		amiproc.cpp \
		amicalld.cpp
OBJS = $(SRCS:.cpp=.o)

CFLAGS_RELEASE = -O2 -fPIC
CFLAGS_DEBUG = -O0 -g -ggdb3 -DDEBUG -fno-default-inline
CFLAGS  = -Wall -Wextra -Wshadow -Wformat-security -Winit-self -fpermissive -Wno-strict-aliasing
CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function
CFLAGS += -DUSE_USEREVENT_CALLSTARTED -I../include
CFLAGS += -D_REENTRANT -D_PTHREADS

LFLAGS  = -lm -ldl -lpthread -lmemcached -lcrypto -L../lib 

ifeq (release, $(findstring release, $(MAKECMDGOALS))) #"make clean release"으로 clean 다음에 연이어 release가 나올 수가 있다.
	CFLAGS += $(CFLAGS_RELEASE)
	LFLAGS += -ltst -lAsyncThreadPool -lamiutil
else
	TARGET = amicalld_d
	CFLAGS += $(CFLAGS_DEBUG)
ifeq (debugtrace, $(findstring debugtrace, $(MAKECMDGOALS))) #"make clean debugtrace"으로 clean 다음에 연이어 debugtrace 나올 수가 있다.
	CFLAGS += -DDEBUGTRACE
	TARGET = amicalld_t
	LFLAGS += -ltst_t -lAsyncThreadPool_t -lamiutil_t
else
	LFLAGS += -ltst_d -lAsyncThreadPool_d -lamiutil_d
endif
endif

DATE = $(shell date +'char amicalldCompileDate[20] = "%Y-%m-%d %H:%M:%S";')

.PHONY: all clean debug release debugtrace check install trace version

all: version $(OBJS) $(TARGET)

$(TARGET):
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

debug: all
release: all
debugtrace: all

%.o: %.cpp
	$(CXX) -o $@ $< -c $(CFLAGS)

version:
	echo '$(DATE)' > amicalldversion.cpp
	echo `$(CXX) -v`

check:
	$(info os=$(OS))
	echo `g++ -v`

trace:
	$(info CFLAGS=$(CFLAGS))

install:
	mkdir -p ~/bin
	chmod 0755 calld
	cp -p calld ~/bin	# 복사하면 크론탭 멈출 수 있음
	cp $(TARGET).conf ~/bin
	cp $(TARGET) ~/bin
	mkdir -p ~/bin/conf
	cp -p conf/* ~/bin/conf

clean:
	rm -f $(TARGET) *.o


