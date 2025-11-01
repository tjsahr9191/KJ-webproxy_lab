# Makefile for Proxy Lab
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

# 'all'은 'echoserver'와 'echoclient' 타겟에 의존합니다.
all: echoserver echoclient proxy

# 에코서버 테스트------------------------------------------------------

# echoclient 빌드 규칙: 'echo_client' 실행 파일을 생성합니다.
echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echo_client echoclient.c csapp.o $(LDFLAGS)

# echoserver 빌드 규칙: 'echo_server' 실행 파일을 생성합니다.
# [수정됨] echoserver.c와 함께 echo.c를 컴파일 및 링크하도록 추가했습니다.
echoserver: echoserver.c echo.c csapp.o
	$(CC) $(CFLAGS) -o echo_server echoserver.c echo.c csapp.o $(LDFLAGS)

# 에코서버 테스트------------------------------------------------------

# csapp.o 오브젝트 파일 빌드 규칙
csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

# proxy.o 오브젝트 파일 빌드 규칙
proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

# proxy 실행 파일 빌드 규칙
proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

# clean 규칙
# [수정됨] 빌드로 생성되는 'echo_client', 'echo_server', 'proxy'를 삭제하도록 수정했습니다.
clean:
	rm -f *~ *.o echo_client echo_server proxy core *.tar *.zip *.gzip *.bzip *.gz