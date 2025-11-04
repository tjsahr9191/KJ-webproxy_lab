#include "cache.h"

/* 캐시 리스트의 시작(가장 최근 사용)과 끝(가장 오래된)을 가리킴 */
static CacheNode *cache_head;
static CacheNode *cache_tail;
static int total_cache_size;

/* Readers-Writers Lock */
static pthread_rwlock_t cache_lock;

/* 내부 헬퍼 함수: 꼬리에서 노드 제거 (wrlock 안에서 호출되어야 함) */
static void evict_lru_node() {
    if (cache_tail == NULL) return; // 캐시가 비어있음

    CacheNode *node_to_evict = cache_tail;

    // 꼬리 노드 업데이트
    if (cache_tail->prev) {
        cache_tail = cache_tail->prev;
        cache_tail->next = NULL;
    } else { // 노드가 하나뿐이었던 경우
        cache_head = NULL;
        cache_tail = NULL;
    }
    
    // 리소스 해제
    total_cache_size -= node_to_evict->size;
    Free(node_to_evict->key);
    Free(node_to_evict->data);
    Free(node_to_evict);
}

/* 내부 헬퍼 함수: 노드를 리스트 맨 앞으로 이동 (wrlock 안에서 호출됨) */
/* (참고: 이 랩의 "근사" 요구사항만 맞추려면 이 함수는 선택사항임) */
/*
static void move_to_front(CacheNode *node) {
    if (node == cache_head) return; // 이미 맨 앞

    // 1. 리스트에서 노드 분리
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (node == cache_tail) cache_tail = node->prev;

    // 2. 맨 앞에 노드 삽입
    node->next = cache_head;
    node->prev = NULL;
    if (cache_head) cache_head->prev = node;
    cache_head = node;
    if (cache_tail == NULL) cache_tail = node;
}
*/

/*
 * cache_init - 캐시와 락을 초기화 (main에서 한 번 호출)
 */
void cache_init() {
    cache_head = NULL;
    cache_tail = NULL;
    total_cache_size = 0;
    pthread_rwlock_init(&cache_lock, NULL);
}

/*
 * cache_find - 'key'(URI)에 해당하는 객체를 찾아 clientfd로 전송
 * 성공 시 1, 실패(miss) 시 0 리턴
 */
/*
 * cache_find - 'key'(URI)에 해당하는 객체를 찾아 clientfd로 전송
 * 성공 시 1, 실패(miss) 시 0 리턴
 */
int cache_find(char *key, int clientfd) {

    char *data_copy; // [수정] 데이터를 복사할 로컬 포인터
    int data_size;
    int found = 0; // [수정] 찾았는지 여부를 기록할 플래그

    pthread_rwlock_rdlock(&cache_lock); // [읽기 락] 획득

    CacheNode *current = cache_head;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            // [캐시 히트!]

            // 1. [수정] 데이터를 즉시 전송하는 대신, Malloc과 memcpy로 복사
            data_size = current->size;
            data_copy = Malloc(data_size);
            memcpy(data_copy, current->data, data_size);
            found = 1;

            // (LRU 근사를 위해 여기서 move_to_front 같은 쓰기 작업을
            //  수행할 수도 있지만, 이 랩의 요구사항에는 필수가 아님)

            break; // 찾았으므로 루프 탈출
        }
        current = current->next;
    }

    pthread_rwlock_unlock(&cache_lock); // [읽기 락] 해제

    // [수정] 락을 푼 상태에서 복사본(data_copy)을 전송
    if (found) {
        Rio_writen(clientfd, data_copy, data_size);
        Free(data_copy); // 복사본 사용 후 반드시 해제
        return 1; // 1 (찾았음)
    }

    return 0; // 0 (못 찾음)
}

/*
 * cache_store - 'key'와 'data'를 캐시에 저장
 */
void cache_store(char *key, char *data, int size) {
    if (size > MAX_OBJECT_SIZE) {
        return; // 너무 큰 객체는 캐시하지 않음
    }

    pthread_rwlock_wrlock(&cache_lock); // [쓰기 락] 획득

    // 1. 공간 확보 (퇴출)
    while (total_cache_size + size > MAX_CACHE_SIZE) {
        evict_lru_node();
    }

    // 2. 새 캐시 노드 생성
    CacheNode *new_node = Malloc(sizeof(CacheNode));
    new_node->key = Malloc(strlen(key) + 1);
    new_node->data = Malloc(size);
    new_node->size = size;

    strcpy(new_node->key, key);
    memcpy(new_node->data, data, size); // 바이너리 데이터이므로 memcpy

    // 3. 리스트의 맨 앞에 노드 삽입 (가장 최근 사용)
    new_node->prev = NULL;
    new_node->next = cache_head;

    if (cache_head) {
        cache_head->prev = new_node;
    }
    cache_head = new_node;

    if (cache_tail == NULL) { // 리스트가 비어있었다면
        cache_tail = new_node;
    }
    
    total_cache_size += size;

    pthread_rwlock_unlock(&cache_lock); // [쓰기 락] 해제
}