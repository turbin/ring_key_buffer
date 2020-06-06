#include "c_circular_queue.h"
#include <pthread.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>

#define __DEBUG_OUTPUT(...) ({/*printf("[fn=%s,ln=%d]", __FUNCTION__,__LINE__);printf(__VA_ARGS__);printf("\n");*/})

#define _CQ_SUCC    (0)
#define _CQ_FAIL    (-1)
#define _CQ_TIMEOUT (-2)


typedef struct  _thread_safe_queue_t
{
    int   nHead;
    int   nTail;

    int   nMaxSize;
    int*  pdata;

volatile int _running;

    pthread_mutex_t *mutex;
    pthread_condattr_t *cattr;
	pthread_cond_t *cond_get;
}ThreadSafeQueue;

static inline void __lock(pthread_mutex_t* mutex)
{
    pthread_mutex_lock(mutex);
}

static inline void __unlock(pthread_mutex_t* mutex)
{
    pthread_mutex_unlock(mutex);
}

static inline void __wakeup(pthread_cond_t *cond)
{
    pthread_cond_signal(cond);
}

static inline int __waitfortimeout(pthread_cond_t *cond, pthread_mutex_t* mutex, int nMillisecond)
{
    struct timespec outtime;

    int nRet;

    if (nMillisecond >= 0) // for zero ,quit immediately
    {
        clock_gettime(CLOCK_MONOTONIC, &outtime);

        outtime.tv_sec += nMillisecond/1000;
        uint64_t  us = outtime.tv_nsec/1000 + 1000 * (nMillisecond % 1000);
        outtime.tv_sec += us / 1000000;

        us = us % 1000000;
        outtime.tv_nsec = us * 1000;

        nRet = pthread_cond_timedwait(cond, mutex, &outtime);
    }
    else
    {
        nRet = pthread_cond_wait(cond, mutex);
    }

    if (nRet == 0)
    {
        return _CQ_SUCC;
    }
    else if (ETIMEDOUT == nRet)
    {
        __DEBUG_OUTPUT("time out");
        return _CQ_TIMEOUT;
    }

    return _CQ_FAIL;
}

static inline int __q_size(const ThreadSafeQueue* q)
{
    return q->nMaxSize;
}

static inline int __q_length(ThreadSafeQueue* q) {return (q->nTail - q->nHead + __q_size(q)) % __q_size(q);}
static inline int __q_is_full(ThreadSafeQueue* q) {return  ((q->nTail + 1) % __q_size(q) == q->nHead);}
static inline int __q_is_empty(ThreadSafeQueue* q) {return (q->nHead == q->nTail);}


static ThreadSafeQueue* __to_queue(void* handle){return (ThreadSafeQueue*)(handle);}

static inline void __release_queue(ThreadSafeQueue** pq)
{
    ThreadSafeQueue* _q = (*pq);

    if(_q == NULL) return;

    if(_q->pdata) {free(_q->pdata); _q->pdata = NULL; _q->nMaxSize=0;}
    if(_q->mutex) {pthread_mutex_destroy(_q->mutex); free(_q->mutex);}

    if(_q->cattr) {pthread_condattr_destroy(_q->cattr); free(_q->cattr);}
    if(_q->cond_get) {pthread_cond_destroy(_q->cond_get); free(_q->cond_get);}

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

    q->cattr = (pthread_condattr_t *)malloc(sizeof(pthread_condattr_t));
    if(q->cattr == NULL) { __release_queue(&q); return NULL;}
    pthread_condattr_init(q->cattr);
    pthread_condattr_setclock(q->cattr, CLOCK_MONOTONIC);

    q->cond_get = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    if(q->cond_get == NULL) { __release_queue(&q); return NULL;}
    pthread_cond_init(q->cond_get, q->cattr);

    q->_running = 0;
    return (void*)(q);
}

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
    if(q == NULL) return -1;
    __lock(q->mutex);

    if(__q_is_full(q))
    {
        q->nHead = (q->nHead + 1) % __q_size(q);
    }

    q->pdata[q->nTail] = _int32Data;
    q->nTail = (q->nTail + 1) % __q_size(q);

    __unlock(q->mutex);
    __wakeup(q->cond_get);

    return 0;
}

int   CircularQueue_nFetchDatas(void* handleOfQueue, int nExpectedSize, int* pnDataOutput, int _nTimeoutMs)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    int nRet = 0;

    if(q == NULL) return -1;
    __lock(q->mutex);

    if(__q_is_empty(q))
    {
        __DEBUG_OUTPUT("start : wait  _nTimeoutMs %d", _nTimeoutMs);
        if ((nRet = __waitfortimeout(q->cond_get, q->mutex, _nTimeoutMs)) != _CQ_SUCC)
        {
            memset(pnDataOutput, 0, nExpectedSize);
             __unlock(q->mutex);
            __DEBUG_OUTPUT("__wait for timeout err [%d]", nRet);

            return nRet == _CQ_TIMEOUT ? 0 : -1;
        }
        __DEBUG_OUTPUT("wake up ");
    }

    int nSize = __q_length(q) > nExpectedSize? nExpectedSize : __q_length(q);

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

void   CircularQueue_clearAll(void* handleOfQueue)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);

    if(q == NULL) return;
    __lock(q->mutex);
    q->nTail = 0;
    q->nHead = 0;
    __unlock(q->mutex);
}

void  CircularQueue_setRunningFlag(void* handleOfQueue, int flag)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);

    if(q == NULL) return;
    __lock(q->mutex);
    q->_running = flag ? 1 : 0;
    __unlock(q->mutex);
}

int   CircularQueue_getRunningFlag(void* handleOfQueue)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);

    if(q == NULL) return 0;

    int flag = 0;
     __lock(q->mutex);
    flag = q->_running;
    __unlock(q->mutex);

    return flag;
}

#if 0
int   CircularDumpQueue(void* handleOfQueue)
{
    ThreadSafeQueue* q = __to_queue(handleOfQueue);
    __lock(q->mutex);

    int p = q->nHead;
    while (q->nTail != p) {
       printf("[%d]  ", q->pdata[p]);
       p = (p + 1) % (q->nMaxSize);
    }

    printf("\n");

    __unlock(q->mutex);
    return 0;
}
#endif

