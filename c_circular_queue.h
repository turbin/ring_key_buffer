#ifndef C_CIRCULAR_QUEUE_H
#define C_CIRCULAR_QUEUE_H

void* CircularQueue_vCreate(int _max_size_of_queue);
void  CircularQueue_vRelease(void* handleOfQueue);

int   CircularQueue_bIsEmpty(void* handleOfQueue);
int   CircularQueue_nAppendData(void* handleOfQueue, int _int32Data);
int   CircularQueue_nFetchDatas(void* handleOfQueue, int nExpectedSize, int* pnDataOutput, int _nTimeoutMs);
void  CircularQueue_clearAll(void* handleOfQueue);
void  CircularQueue_setRunningFlag(void* handleOfQueue, int flag);
int   CircularQueue_getRunningFlag(void* handleOfQueue);


#endif
