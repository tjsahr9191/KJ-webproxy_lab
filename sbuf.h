#ifndef __SBUF_H__
#define __SBUF_H__

#include "csapp.h"

/* 공유 버퍼 구조체 */
typedef struct {
    int *buf;       /* 버퍼 배열 */
    int n;          /* 최대 슬롯 수 */
    int front;      /* buf[(front+1)%n]가 첫 번째 아이템 */
    int rear;       /* buf[rear%n]가 마지막 아이템 */
    sem_t mutex;    /* 버퍼 접근을 보호하기 위한 뮤텍스 */
    sem_t slots;    /* 비어있는 슬롯의 수 */
    sem_t items;    /* 사용 가능한 아이템(connfd)의 수 */
} sbuf_t;

/* 함수 프로토타입 */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

#endif /* __SBUF_H__ */