#ifndef CACHE_H
#define CACHE_H

#include "csapp.h"

/* proxy.c에 정의된 매크로를 여기서도 사용 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* 캐시 객체를 위한 노드 (Doubly Linked List) */
typedef struct CacheNode {
    char *key;                // 캐시 키 (요청 URI)
    char *data;               // 웹 객체 데이터
    int size;                 // 데이터 크기
    struct CacheNode *prev;
    struct CacheNode *next;
} CacheNode;

/* 캐시 관리 함수 */
void cache_init();
int cache_find(char *key, int clientfd);
void cache_store(char *key, char *data, int size);

#endif /* CACHE_H */