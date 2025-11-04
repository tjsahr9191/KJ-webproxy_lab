#include "csapp.h"

/* 권장되는 최대 캐시 및 객체 크기 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* 제공된 User-Agent 헤더 상수 */
static const char *user_agent_hdr =
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
        "Firefox/10.0.3\r\n";

/* 함수 프로토타입 */
void doit(int fd);
void parse_uri(char *uri, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/*
 * main - 프록시의 메인 루틴. tiny.c의 main과 거의 동일합니다.
 */
int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        doit(connfd);

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
    rio_t client_rio, server_rio;
    int Does_send_host_header = 0;

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

    parse_uri(uri, host, port, path);

    /* 4. 서버로 보낼 HTTP 요청 재조립 (1단계: 요청 라인) */
    // 프록시는 HTTP/1.0 요청을 보냅니다.
    sprintf(request_buf, "GET %s HTTP/1.0\r\n", path);

    /* 5. 클라이언트의 나머지 헤더 읽기 및 서버 요청 재조립 (2단계: 헤더) */
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
        sprintf(request_buf, "%s%s", request_buf, buf);
    }

    /* 6. 필수 헤더 추가 (3단계: 고정 헤더) */
    // 클라이언트가 Host 헤더를 보내지 않았다면, URI에서 파싱한 호스트로 추가
    if (!Does_send_host_header) {
        sprintf(request_buf, "%sHost: %s\r\n", request_buf, host);
    }
    // 고정된 헤더 추가
    sprintf(request_buf, "%s%s", request_buf, user_agent_hdr);
    sprintf(request_buf, "%sConnection: close\r\n", request_buf);
    sprintf(request_buf, "%sProxy-Connection: close\r\n", request_buf);

    // 헤더 섹션의 끝을 알리는 빈 줄 추가
    sprintf(request_buf, "%s\r\n", request_buf);

    /* 7. 실제 웹 서버에 연결 */
    // printf("Connecting to %s:%s\n", host, port); // 디버깅용
    serverfd = Open_clientfd(host, port);

    /* 8. 재조립된 요청을 서버에 전송 */
    Rio_writen(serverfd, request_buf, strlen(request_buf));

    /* 9. 서버의 응답을 읽어 클라이언트에 그대로 전달 (중계) */
    Rio_readinitb(&server_rio, serverfd);
    size_t n;
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(fd, buf, n);
    }

    Close(serverfd);
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
