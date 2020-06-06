#include "c_circular_queue.h"
#include <pthread.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define __DEBUG_OUTPUT(...) ({printf("[fn=%s,ln=%d]", __FUNCTION__,__LINE__);printf(__VA_ARGS__);printf("\n");})

typedef struct  _thread_safe_queue_t
{
    int   nHead;
    int   nTail;

    int   nMaxSize;
    int*  pdata;

volatile int _running;

    pthread_mutex_t *mutex;
	pthread_cond_t *cond_get;
	pthread_cond_t *cond_put;
}ThreadSafeQueue;

static inline void __lock(pthread_mutex_t* mutex)
{
    pthread_mutex_lock(mutex);
}

static inline void __unlock(pthread_mutex_t* mutex)
{
    pthread_mutex_unlock(mutex);
}

static inline int __q_size(const ThreadSafeQueue* q)
{
    return q->nMaxSize;
}

static inline int __q_capacity(const ThreadSafeQueue* q)
{
    return __q_size(q) -1;
}

static inline int __q_free_bytes(ThreadSafeQueue* q) 
{
//   return (q->nHead <= q->nTail)? (__q_capacity(q)- (q->nTail - q->nHead)) : (q->nHead - q->nTail -1);

    return (q->nTail > q->nHead)?(q->nTail - q->nHead) : (__q_capacity(q)- (q->nHead - q->nTail));
}
static inline int __q_is_full(ThreadSafeQueue* q) {return  (__q_free_bytes(q) == 0);}
static inline int __q_is_empty(ThreadSafeQueue* q) {return (__q_free_bytes(q) == __q_capacity(q));}


static ThreadSafeQueue* __to_queue(void* handle){return (ThreadSafeQueue*)(handle);}

static inline void __release_queue(ThreadSafeQueue** pq)
{
    ThreadSafeQueue* _q = (*pq);

    if(_q == NULL) return;

    if(_q->pdata) {free(_q->pdata); _q->pdata = NULL; _q->nMaxSize=0;}
    if(_q->mutex) {pthread_mutex_destroy(_q->mutex); free(_q->mutex);}

    if(_q->cond_get) {pthread_cond_destroy(_q->cond_get); free(_q->cond_get);}
    if(_q->cond_put) {pthread_cond_destroy(_q->cond_put); free(_q->cond_put);}

    _q->_running = 0;

    free(_q);
    _q = NULL;
}

void* CircularQueue_vCreate(int _capacity)
{
    ThreadSafeQueue* q = (ThreadSafeQueue* )malloc(sizeof(ThreadSafeQueue));
    if(q == NULL) return NULL;

    memset(q, 0, sizeof(ThreadSafeQueue));

    q->nMaxSize = _capacity+1;
    q->nTail     = 0;
    q->nHead     = 0;
    q->pdata     = (int*)malloc(sizeof(int)* (q->nMaxSize));

    q->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    if(q->mutex == NULL) { __release_queue(&q); return NULL;}
    pthread_mutex_init(q->mutex, NULL);

    q->cond_put = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    
    if(q->cond_put == NULL) { __release_queue(&q); return NULL;}
    pthread_cond_init(q->cond_put, NULL);


    q->cond_get = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    
    if(q->cond_get == NULL) { __release_queue(&q); return NULL;}
    pthread_cond_init(q->cond_get, NULL);

    q->_running = 1;
    return (void*)(q);
}

#if 0
void  CRB_vPushInt32Data(void* handleOfQueue, int _int32Data)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    __lock(q->mutex);

    while(q->_running && __isFull(q)) {
        __DEBUG_OUTPUT("is full.");
        pthread_cond_wait(q->cond_put, q->mutex);
        __DEBUG_OUTPUT("is full wake up");
    }

    if(!q->_running){ __unlock(q->mutex); return;}
    
    __DEBUG_OUTPUT("push data =%d", _int32Data);
    q->pdata[q->nHead] = _int32Data;
    __advance_pointer(q);

    __DEBUG_OUTPUT("wake up cond_get");
    pthread_cond_signal(q->cond_get);

    __unlock(q->mutex);

    __DEBUG_OUTPUT("exit from push");
}


int   CRB_nPollInt32Data(void* handleOfQueue,int* _int32Data, int _nTimeoutMs)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    __lock(q->mutex);

    while(q->_running && __isEmpty(q)) {
        pthread_cond_wait(q->cond_get, q->mutex);
    }

    if(!q->_running){ __unlock(q->mutex); return -1;}
    
    *_int32Data = (int*)q->pdata[q->nTail];
    __retreat_pionter(q);
    __unlock(q->mutex);

    pthread_cond_signal(q->cond_put);

    return 0;
}
#endif

void  CircularQueue_vRelease(void* handleOfQueue)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    if(q == NULL) return;

    if(q->_running) q->_running = 0;
    __release_queue(&q);
}

int   CircularQueue_bIsEmpty(void* handleOfQueue)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    __lock(q->mutex);
    int __is_empty = __q_is_empty(q);
    __unlock(q->mutex);

    return __is_empty;
}


int  CircularQueue_nAppendData(void* handleOfQueue, int _int32Data)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    __lock(q->mutex);

    if(__q_is_full(q))
    {
        q->nHead = (q->nHead + 1) % q->nMaxSize;
    }

    __DEBUG_OUTPUT("push data =%d", _int32Data);
    q->pdata[q->nTail] = _int32Data;
    q->nTail = (q->nTail + 1) % __q_size(q);

    __unlock(q->mutex);
    return 0;
}


int   CircularQueue_nFetchDatas(void* handleOfQueue, int nExpectedSize, int* pnDataOutput)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    __lock(q->mutex);

    if(__q_is_empty(q)) {__unlock(q->mutex); return 0;}

    __DEBUG_OUTPUT("remaind bytes =%d", __q_size(q) - __q_free_bytes(q));
    __DEBUG_OUTPUT("free bytes =%d", __q_free_bytes(q));


    int nSize = __q_free_bytes(q) > nExpectedSize? nExpectedSize: __q_free_bytes(q);

    nExpectedSize = nSize;

    while(nSize--)
    {
        *pnDataOutput = q->pdata[q->nHead];
         q->nHead = (q->nHead +1) % __q_size(q);
         pnDataOutput++;
    }
    __unlock(q->mutex);
    return nExpectedSize;
}




