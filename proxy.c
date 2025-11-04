#include "csapp.h"
#include "cache.h"
#include "sbuf.h"

/* 권장되는 최대 캐시 및 객체 크기 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS  6  // 워커 스레드 수
#define SBUFSIZE 16  // 공유 버퍼(큐) 크기

sbuf_t sbuf; // 공유 버퍼 전역 변수

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
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);
    cache_init();

    /* [수정] 스레드 풀 초기화 */
    sbuf_init(&sbuf, SBUFSIZE);
    listenfd = Open_listenfd(argv[1]);

    /* [수정] NTHREADS 개의 워커 스레드를 미리 생성 */
    for (int i = 0; i < NTHREADS; i++) {
        Pthread_create(&tid, NULL, thread_function, NULL);
    }

    /* [수정] main 스레드는 이제 '생산자' 역할만 수행 */
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        /* connfd를 공유 버퍼에 삽입 */
        sbuf_insert(&sbuf, connfd);
    }
}

/*
 * 스레드의 메인 루틴 (시작 함수)
 */
void *thread_function(void *vargp) {

    // 1. 스레드를 분리(detach)하여 자원을 자동 해제
    Pthread_detach(pthread_self());

    // 2. 워커 스레드는 무한 루프를 돌며 작업을 기다림
    while (1) {
        /* 공유 버퍼에서 connfd를 꺼냄 (없으면 대기) */
        int connfd = sbuf_remove(&sbuf);

        /* 핵심 로직 수행 */
        doit(connfd);

        /* 연결 종료 */
        Close(connfd);
    }
}

/*
 * doit - 단일 HTTP 트랜잭션을 처리합니다.
 */
void doit(int fd) {
    int serverfd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    char request_buf[MAXLINE]; // 서버로 보낼 요청을 저장할 버퍼

    /* [수정] request_buf의 끝을 가리킬 포인터 선언 */
    char *p = request_buf;

    rio_t client_rio, server_rio;
    int Does_send_host_header = 0; // Host 헤더 전송 여부 플래그

    /* 1. 클라이언트로부터 요청 라인과 헤더 읽기 */
    Rio_readinitb(&client_rio, fd);
    if (Rio_readlineb(&client_rio, buf, MAXLINE) == 0) {
        return; // 빈 요청은 무시
    }

    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

    char cache_key[MAXLINE];
    strcpy(cache_key, uri);

    /*
     * [캐싱] 2. 캐시에서 객체 찾기
     */
    if (cache_find(cache_key, fd)) {
        printf("Cache hit for %s\n", cache_key);
        return;
    }
    printf("Cache miss for %s\n", cache_key);


    /*
     * 3. 캐시 미스(Miss): 서버에 요청 (1부 로직)
     */
    parse_uri(uri, host, port, path);

    /* [수정] 3a. 포인터(p)를 이용해 request_buf에 쓰기 */
    p += sprintf(p, "GET %s HTTP/1.0\r\n", path);

    /* [수정] 3b. 포인터를 이동시키며 헤더 이어 붙이기 */
    while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break;

        if (strstr(buf, "User-Agent:"))
            continue;
        if (strstr(buf, "Connection:"))
            continue;
        if (strstr(buf, "Proxy-Connection:"))
            continue;

        if (strstr(buf, "Host:")) {
            Does_send_host_header = 1;
        }
        /* p가 가리키는 곳(버퍼의 끝)에 안전하게 이어 씀 */
        p += sprintf(p, "%s", buf);
    }

    /* [수정] 3c. 포인터를 이용해 필수 헤더 이어 붙이기 */
    if (!Does_send_host_header) {
        p += sprintf(p, "Host: %s\r\n", host);
    }
    /* user_agent_hdr은 \r\n을 이미 포함하고 있습니다. */
    p += sprintf(p, "%s", user_agent_hdr);
    p += sprintf(p, "Connection: close\r\n");
    p += sprintf(p, "Proxy-Connection: close\r\n");
    p += sprintf(p, "\r\n"); // 헤더 끝

    /* 4. 실제 웹 서버에 연결 및 요청 전송 */
    serverfd = Open_clientfd(host, port);

    /* [수정] strlen 대신 포인터 연산으로 정확한 크기 전송 (더 안전함) */
    Rio_writen(serverfd, request_buf, (p - request_buf));

    /*
     * 5. 서버 응답 중계 및 캐시 저장 (이 부분은 문제가 없었음)
     */
    Rio_readinitb(&server_rio, serverfd);
    size_t n;

    char *cache_buf = Malloc(MAX_OBJECT_SIZE);
    int total_bytes_read = 0;
    int can_cache = 1;

    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(fd, buf, n);
        if (can_cache) {
            if (total_bytes_read + n <= MAX_OBJECT_SIZE) {
                memcpy(cache_buf + total_bytes_read, buf, n);
                total_bytes_read += n;
            } else {
                can_cache = 0;
            }
        }
    }
    Close(serverfd);

    if (can_cache && total_bytes_read > 0) {
        cache_store(cache_key, cache_buf, total_bytes_read);
    }
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
