/*  Copyright 2012 Google Inc. All Rights Reserved. */
/*  Author: vmr@google.com (Ville-Mikko Rautio) */

//#include "vp9hwd_decoder.h"
#include "vp9hwd_fifo.h"
#include "vp9hwd_container.h"
#include "vp9Hw_config.h"

/* Data structure to hold this picture buffer queue instance data. */
struct BQueue
{
  //pthread_mutex_t cs; /* Critical section to protect data. */
  s32 n_buffers;      /* Number of buffers contained in total. */
  s32 n_references[VP9DEC_MAX_PIC_BUFFERS]; /* Reference counts on buffers.
                                              Index is buffer#.  */
  s32 ref_status[VP9_REF_LIST_SIZE]; /* Reference status of the decoder. Each
                                       element contains index to buffer to an
                                       active reference. */
  FifoInst empty_fifo; /* Queue holding empty, unreferred buffer indices. */
};

s32 Vp9BufferQueueGetRef(BufferQueue queue, u32 index)
{
  struct BQueue* q = (struct BQueue*)queue;
  return q->ref_status[index];
}

static void IncreaseRefCount(struct BQueue* q, s32 i)
{
    q->n_references[i]++;
}

void Vp9BufferQueueAddRef(BufferQueue queue, s32 buffer)
{
  struct BQueue* q = (struct BQueue*)queue;
  IncreaseRefCount(q, buffer);
}

static void DecreaseRefCount(struct BQueue* q, s32 i)
{
    enum FifoRet ret;
    q->n_references[i]--;

    if(q->n_references[i] == 0)
    {
        /* Once picture buffer is no longer referred to, it can be put to
        the empty fifo. */
        ret = FifoPush(q->empty_fifo, (void*)(uintptr_t)i, FIFO_EXCEPTION_ENABLE);
        (void)ret;
    }
}

void Vp9BufferQueueUpdateRef(BufferQueue queue, u32 ref_flags, s32 buffer)
{
    u32 i = 0;
    //u32 index = 0;

    struct BQueue* q = (struct BQueue*)queue;

    for(i = 0; i < VP9_REF_LIST_SIZE; i++)
       {
        if((ref_flags & (1 << i)) && buffer != q->ref_status[i])
        {
            if(q->ref_status[i] != (s32)REFERENCE_NOT_SET)
            {
                DecreaseRefCount(q, q->ref_status[i]);
            }
            q->ref_status[i] = buffer;
            if(buffer != (s32)REFERENCE_NOT_SET)
                IncreaseRefCount(q, buffer);
        }
    }
}

void Vp9BufferQueueRemoveRef(BufferQueue queue, s32 buffer)
{
    struct BQueue* q = (struct BQueue*)queue;
    DecreaseRefCount(q, buffer);
}

void Vp9BufferQueueRelease(BufferQueue queue)
{
    struct BQueue* q = (struct BQueue*)queue;
    if(q->empty_fifo)
    {/* Empty the fifo before releasing. */
        s32 i, j;
        enum FifoRet ret;
        for(i = 0; i < q->n_buffers; i++)
        {
            ret = FifoPop(q->empty_fifo, (void**)&j, FIFO_EXCEPTION_ENABLE);
            (void)ret;
        }
        FifoRelease(q->empty_fifo);
    }
    free(q);
}

void Vp9BufferQueueResetReferences(BufferQueue queue)
{
    s32 i;
    struct BQueue* q = (struct BQueue*)queue;
    for(i = 0; i < (s32)(sizeof(q->ref_status)/sizeof(q->ref_status[0])); i++)
    {
        q->ref_status[i] = REFERENCE_NOT_SET;
    }
}

BufferQueue Vp9BufferQueueInitialize(s32 n_buffers)
{
    s32 i;
    enum FifoRet ret;
    struct BQueue* q = (struct BQueue*)calloc(1, sizeof(struct BQueue));
    logv("calloc BQueue, q = %p, size = %d",q,(int)sizeof(struct BQueue));
    if(q == NULL)
    {
        return NULL;
    }

   if(FifoInit(n_buffers, &q->empty_fifo) != FIFO_OK /*||
     pthread_mutex_init(&q->cs, NULL)*/)
   {
       Vp9BufferQueueRelease(q);
       return NULL;
   }
    /* Add picture buffers among empty picture buffers. */
    for (i = 0; i < n_buffers; i++)
    {
        ret = FifoPush(q->empty_fifo, (void*)(uintptr_t)i, FIFO_EXCEPTION_ENABLE);
        if(ret != FIFO_OK)
        {
            Vp9BufferQueueRelease(q);
            return NULL;
        }
        q->n_buffers++;
    }
    Vp9BufferQueueResetReferences(q);
    return q;
}

#if 0
u32 Vp9BufferQueueCountReferencedBuffers(BufferQueue queue, int num_buffers)
{
    u32 i,j;
    u32 is_referenced, ref_count = 0;
    struct BQueue* q = (struct BQueue*)queue;
    for(i = 0; i < num_buffers; i++)
    {
        is_referenced = 0;
        for(j = 0; j < VP9_REF_LIST_SIZE; j++)
        {
            if((u32)q->ref_status[j] == i)
            {
                is_referenced++;
            }
        }
        if(is_referenced)
        {
            ref_count++;
        }
    }
    return ref_count;
}
#endif

void Vp9BufferQueueAddBuffer(BufferQueue queue)
{
    enum FifoRet ret;
    struct BQueue* q = (struct BQueue*)queue;
    /* Add one picture buffer among empty picture buffers. */
    logd("******here2:empty push\n");
    ret = FifoPush(q->empty_fifo, (void*)(uintptr_t)q->n_buffers, FIFO_EXCEPTION_ENABLE);
    (void)ret;
    q->n_buffers++;
}

s32 Vp9BufferQueueGetBuffer(BufferQueue queue, u32 limit)
{
    s32 i;
    enum FifoRet ret;
    struct BQueue* q = (struct BQueue*)queue;

    ret = FifoPop(q->empty_fifo, (void**)&i, FIFO_EXCEPTION_ENABLE);
    if(ret == FIFO_EMPTY)
    {
        if((u32)q->n_buffers < limit)
        {/* return and allocate new buffer */
            loge("q->n_buffers=%d, limit=%d\n", q->n_buffers, limit);
            return -1;
        }
        else
        {/* wait for free one */
            ret = FifoPop(q->empty_fifo, (void**)&i, FIFO_EXCEPTION_DISABLE);
            logd("********here2: ret=%d, i=%d\n", ret, i);
        }
    }
    else
    {
        if(q->n_references[i] != 0)
        {
            loge("*******we need return and allocate new buffer\n");
            FifoPopRevert(q->empty_fifo);
            return -1;
        }
    }
    IncreaseRefCount(q, i);
    return i;
}

#if 0
void printf_ref_status(BufferQueue bq, int index)
{
   struct BQueue* q = NULL;
   int i = 0;
   q =  (struct BQueue*)bq;
   //index1 = q->ref_status[index];
   logd("********printf_reference number:%d\n");

    for(i=0; i< VP9_REF_LIST_SIZE; i++)
    {
        logd("q->ref_status[%d] = %d,",i,q->ref_status[i]);
    }
    logd("\n");

    for(i=0; i< VP9DEC_MAX_PIC_BUFFERS; i++)
    {
        logd("q->n_references[%d] is:%d",i,q->n_references[i]);
    }
}

#endif
