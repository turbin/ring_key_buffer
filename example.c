#include "c_circular_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

volatile int _gnData=1;


#define __DEBUG_OUTPUT(...) ({printf("[fn=%s,ln=%d]", __FUNCTION__,__LINE__);printf(__VA_ARGS__);printf("\n");})
#define sleepmilli(x) usleep((x) * 1000)

volatile int _running =0;

void __generator(void* queue)
{
    int _data = _gnData;
    while (_running)
    {

        __DEBUG_OUTPUT("push data=%d", _data);
        CircularQueue_nAppendData(queue, _data);
        _data = (_data + 1) % 255;
        __DEBUG_OUTPUT("push data exit");
    }
    
}

void __dump_datas(int* start, int sizes)
{
    printf("dump datas, num(%d):\n", sizes);

    while(sizes--){
        printf("%d, ", *start++);
    }
    printf("\ndump datas end\n");
}

void __handler(void* queue)
{
    // int _data =0;
    int _datas[10]={0};

    while (_running)
    {
        __DEBUG_OUTPUT("polling data ...");

        if(CircularQueue_bIsEmpty(queue)){
            sleepmilli(30);
            __DEBUG_OUTPUT("is empty ...");
            continue;
        }

        memset(_datas,  0,  sizeof(_datas));
        int _num = CircularQueue_nFetchDatas(queue, sizeof(_datas)/sizeof(int),&_datas);

        __DEBUG_OUTPUT("_num = %d", _num);
        __dump_datas(&_datas[0], _num);
    }
}


void __signal_handler()
{
    
}

int main(int argc, char** argv)
{
    _running = 1;

    void* _queue= CircularQueue_vCreate(10);

    pthread_t _th_genrator;
    pthread_t _th_handler;

    pthread_create(&_th_genrator, NULL, __generator, _queue);
    pthread_create(&_th_handler,  NULL, __handler,  _queue);

    pthread_join(&_th_genrator, NULL);
    pthread_join(&_th_handler, NULL);
    return 0;
}
