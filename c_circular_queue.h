#ifndef C_CIRCULAR_QUEUE_H
#define C_CIRCULAR_QUEUE_H

void* CircularQueue_vCreate(int _max_size_of_queue);
void  CircularQueue_vRelease(void* handleOfQueue);

int   CircularQueue_bIsEmpty(void* handleOfQueue);
int   CircularQueue_nAppendData(void* handleOfQueue, int _int32Data);
int   CircularQueue_nFetchDatas(void* handleOfQueue, int nExpectedSize, int* pnDataOutput);
#endif
