#include "sbuf.h"

/* 공유 버퍼 초기화 */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;                       /* 버퍼 크기 */
    sp->front = sp->rear = 0;        /* 큐는 비어있음 */
    Sem_init(&sp->mutex, 0, 1);      /* 뮤텍스 (초기값 1) */
    Sem_init(&sp->slots, 0, n);      /* 비어있는 슬롯 (초기값 n) */
    Sem_init(&sp->items, 0, 0);      /* 사용 가능한 아이템 (초기값 0) */
}

/* 버퍼 정리 */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

/* 버퍼에 아이템(connfd) 삽입 */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* 비어있는 슬롯을 기다림 */
    P(&sp->mutex);                          /* 버퍼 락 */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* 아이템 삽입 */
    V(&sp->mutex);                          /* 버퍼 언락 */
    V(&sp->items);                          /* 아이템이 사용 가능함을 알림 */
}

/* 버퍼에서 아이템(connfd) 꺼내기 */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                          /* 아이템을 기다림 */
    P(&sp->mutex);                          /* 버퍼 락 */
    item = sp->buf[(++sp->front)%(sp->n)];  /* 아이템 꺼내기 */
    V(&sp->mutex);                          /* 버퍼 언락 */
    V(&sp->slots);                          /* 비어있는 슬롯이 생겼음을 알림 */
    return item;
}