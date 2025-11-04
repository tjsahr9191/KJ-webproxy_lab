#include "csapp.h"
#include "cache.h"

/* 권장되는 최대 캐시 및 객체 크기 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* 제공된 User-Agent 헤더 상수 */
static const char *user_agent_hdr =
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
        "Firefox/10.0.3\r\n";
/* BASIC */
void doit(int fd);
void parse_uri(char *uri, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* Concurrency */
void *thread_function(void *vargp);

/*
 * main - 프록시의 메인 루틴. (동시성 적용)
 */
int main(int argc, char **argv) {
    int listenfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid; // 스레드 ID 변수 추가

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN); // SIGPIPE 무시
    cache_init();

    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);

        // 1. connfd를 위한 메모리를 힙에 할당 (스레드 경쟁 상태 방지)
        int *connfd_ptr = Malloc(sizeof(int));

        // 2. 연결 수락
        *connfd_ptr = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        // 3. 새 스레드를 생성하고 connfd_ptr을 인자로 전달
        Pthread_create(&tid, NULL, thread_function, connfd_ptr);
    }
}

/*
 * 스레드의 메인 루틴 (시작 함수)
 */
void *thread_function(void *vargp) {
    // 1. 메인 스레드가 전달한 connfd_ptr(void*)를 int*로 변환
    int connfd = *((int *)vargp);

    // 2. 스레드를 분리(detach)하여 자원을 자동 해제하도록 설정
    Pthread_detach(pthread_self());

    // 3. connfd 값을 꺼냈으므로, 힙에 할당된 포인터 메모리 해제
    Free(vargp);

    // 4. 프록시의 핵심 로직 수행 (1부의 doit 함수 재사용)
    doit(connfd);

    // 5. 작업이 끝났으므로 클라이언트 소켓을 닫음 (매우 중요)
    Close(connfd);

    return NULL;
}

/*
 * doit - 단일 HTTP 트랜잭션을 처리합니다.
 */
void doit(int fd) {
    int serverfd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    char request_buf[MAXLINE]; // 서버로 보낼 요청을 저장할 버퍼
    rio_t client_rio, server_rio;
    int Does_send_host_header = 0; // Host 헤더 전송 여부 플래그

    /* 1. 클라이언트로부터 요청 라인과 헤더 읽기 */
    Rio_readinitb(&client_rio, fd);
    if (Rio_readlineb(&client_rio, buf, MAXLINE) == 0) {
        return; // 빈 요청은 무시
    }

    // 요청 라인 파싱 (예: "GET http://www.cmu.edu/hub/index.html HTTP/1.1")
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

    // [캐싱] 캐시 키로 전체 URI를 사용합니다.
    char cache_key[MAXLINE];
    strcpy(cache_key, uri);

    /*
     * [캐싱] 2. 캐시에서 객체 찾기
     */
    if (cache_find(cache_key, fd)) {
        printf("Cache hit for %s\n", cache_key);
        return; // cache_find가 클라이언트에게 전송 완료
    }
    printf("Cache miss for %s\n", cache_key);


    /* * 3. 캐시 미스(Miss): 서버에 요청 (1부 로직)
     */
    parse_uri(uri, host, port, path);

    // 3a. 서버로 보낼 요청 라인 재조립
    sprintf(request_buf, "GET %s HTTP/1.0\r\n", path);

    // 3b. 클라이언트의 나머지 헤더 읽기 및 재조립
    while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break;

        // 프록시가 직접 설정할 헤더는 건너뛰기
        if (strstr(buf, "User-Agent:"))
            continue;
        if (strstr(buf, "Connection:"))
            continue;
        if (strstr(buf, "Proxy-Connection:"))
            continue;

        // Host 헤더가 있는지 확인하고, 나머지 헤더는 그대로 추가
        if (strstr(buf, "Host:")) {
            Does_send_host_header = 1;
        }
        sprintf(request_buf, "%s%s", request_buf, buf);
    }

    // 3c. 필수 헤더 추가
    if (!Does_send_host_header) {
        sprintf(request_buf, "%sHost: %s\r\n", request_buf, host);
    }
    sprintf(request_buf, "%s%s", request_buf, user_agent_hdr);
    sprintf(request_buf, "%sConnection: close\r\n", request_buf);
    sprintf(request_buf, "%sProxy-Connection: close\r\n", request_buf);
    sprintf(request_buf, "%s\r\n", request_buf); // 헤더 끝

    /* 4. 실제 웹 서버에 연결 및 요청 전송 */
    serverfd = Open_clientfd(host, port);
    Rio_writen(serverfd, request_buf, strlen(request_buf));

    /*
     * 5. 서버 응답 중계 및 캐시 저장
     */
    Rio_readinitb(&server_rio, serverfd);
    size_t n;

    // 캐싱을 위한 임시 버퍼 할당
    char *cache_buf = Malloc(MAX_OBJECT_SIZE);
    int total_bytes_read = 0;
    int can_cache = 1; // 객체가 MAX_OBJECT_SIZE 이하인지 추적

    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        // 5a. 클라이언트에게 즉시 전송
        Rio_writen(fd, buf, n);

        // 5b. 캐시 버퍼에 데이터 누적
        if (can_cache) {
            if (total_bytes_read + n <= MAX_OBJECT_SIZE) {
                // 바이너리 데이터이므로 memcpy 사용
                memcpy(cache_buf + total_bytes_read, buf, n);
                total_bytes_read += n;
            } else {
                // 객체가 너무 커서 캐시 불가.
                can_cache = 0;
            }
        }
    }

    Close(serverfd);

    // 5c. 캐시 가능하면 캐시에 저장
    if (can_cache && total_bytes_read > 0) {
        cache_store(cache_key, cache_buf, total_bytes_read);
    }

    // 임시 버퍼 해제
    Free(cache_buf);
}

/*
 * parse_uri - HTTP 프록시 URI를 파싱합니다.
 * (예: "http://www.cmu.edu:8080/hub/index.html")
 */
void parse_uri(char *uri, char *host, char *port, char *path) {
    char *ptr;

    /* "http://" 부분 건너뛰기 */
    if (!(ptr = strstr(uri, "http://"))) {
        // 이 실습에서는 "http://"가 항상 있다고 가정합니다.
        return;
    }
    ptr += 7; // "http://" 다음부터 시작 (예: "www.cmu.edu:8080/...")

    /* 경로(path) 찾기 */
    char *path_ptr = strchr(ptr, '/');
    if (path_ptr) {
        strcpy(path, path_ptr); // (예: "/hub/index.html")
        *path_ptr = '\0'; // 호스트/포트 부분과 경로 분리
    } else {
        strcpy(path, "/"); // 경로가 없으면 기본값 "/"
    }

    /* 포트(port) 찾기 */
    char *port_ptr = strchr(ptr, ':');
    if (port_ptr) {
        *port_ptr = '\0'; // 호스트와 포트 분리
        strcpy(port, port_ptr + 1); // (예: "8080")
    } else {
        strcpy(port, "80"); // 포트가 없으면 기본값 "80"
    }

    /* 남은 부분이 호스트(host) */
    strcpy(host, ptr); // (예: "www.cmu.edu")
}

/*
 * clienterror - tiny.c에서 가져온 오류 메시지 전송 함수
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int) strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
