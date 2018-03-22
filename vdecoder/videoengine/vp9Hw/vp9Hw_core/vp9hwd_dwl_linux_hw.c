
/*  Copyright 2013 Google Inc. All Rights Reserved. */

//#include "basetype.h"
#include "vp9hwd_dwl_linux.h"
#include "vp9hwd_dwl.h"
#include "vp9hwd_hantrodec.h"
#include "vp9hwd_memalloc.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

u32 instances = 0;

/* the decoder device driver nod */
const char *dec_dev = DEC_MODULE_PATH;

/* the memalloc device driver nod */
const char *mem_dev = MEMALLOC_MODULE_PATH;

/* counters for core usage statistics */
u32 core_usage_counts[MAX_ASIC_CORES] = {0};

/* a mutex protecting the wrapper init */
static pthread_mutex_t x170_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int n_dwl_instance_count = 0;

extern u32 dwl_shadow_regs[MAX_ASIC_CORES][256];

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : void * param - not in use, application passes NULL
------------------------------------------------------------------------------*/
const void *DWLInit(struct DWLInitParam *param)
{
    struct HX170DWL *dec_dwl;
    //unsigned long multicore_base[MAX_ASIC_CORES];
    //unsigned int i;

    DWL_DEBUG("INITIALIZE\n");

    dec_dwl = (struct HX170DWL *)calloc(1, sizeof(struct HX170DWL));

    if(dec_dwl == NULL)
    {
        DWL_DEBUG("failed to alloc struct HX170DWL struct\n");
        return NULL;
    }

    dec_dwl->client_type = param->client_type;
    //pthread_mutex_lock(&x170_init_mutex);

    #ifdef INTERNAL_TEST
    InternalTestInit()
    #endif

    dec_dwl->fd = -1;
    dec_dwl->fd_mem = -1;
    dec_dwl->fd_memalloc = -1;

#if 0
    /* Linear memories not needed in pp */
    if(dec_dwl->client_type != DWL_CLIENT_TYPE_PP)
    {
        /* open memalloc for linear memory allocation */
        dec_dwl->fd_memalloc = open(mem_dev, O_RDWR | O_SYNC);
        if(dec_dwl->fd_memalloc == -1)
        {
            DWL_DEBUG("failed to open: %s\n", mem_dev);
            goto err;
        }
    }
    dec_dwl->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if(dec_dwl->fd_mem == -1)
    {
        DWL_DEBUG("failed to open: %s\n", "/dev/mem");
        goto err;
    }

    if(n_dwl_instance_count++==1)
    {
        /* open the device */
        dec_dwl->fd = open(dec_dev, O_RDWR);
        if(dec_dwl->fd == -1)
        {
            DWL_DEBUG("failed to open '%s'\n", dec_dev);
            goto err;
        }

        switch(dec_dwl->client_type)
        {
            case DWL_CLIENT_TYPE_HEVC_DEC:
            case DWL_CLIENT_TYPE_VP9_DEC:
            case DWL_CLIENT_TYPE_PP:
            {
                break;
            }
            default:
            {
                DWL_DEBUG("Unknown client type no. %d\n", dec_dwl->client_type);
                goto err;
            }
        }

        if(ioctl(dec_dwl->fd, HANTRODEC_IOC_MC_CORES, &dec_dwl->num_cores) == -1)
        {
            DWL_DEBUG("ioctl HANTRODEC_IOC_MC_CORES failed\n");
            goto err;
        }

        //assert(dec_dwl->num_cores <= MAX_ASIC_CORES);
        if(ioctl(dec_dwl->fd, HANTRODEC_IOC_MC_OFFSETS, multicore_base) == -1)
        {
            DWL_DEBUG("ioctl HANTRODEC_IOC_MC_OFFSETS failed\n");
            goto err;
        }
        if(ioctl(dec_dwl->fd, HANTRODEC_IOCGHWIOSIZE, &dec_dwl->reg_size) == -1)
        {
            DWL_DEBUG("ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
            goto err;
        }
    }
#endif
    DWL_DEBUG("SUCCESS\n");
    //pthread_mutex_unlock(&x170_init_mutex);
    return dec_dwl;
err:
    DWL_DEBUG("FAILED\n");
    //pthread_mutex_unlock(&x170_init_mutex);
    DWLRelease(dec_dwl);
    return NULL;
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
s32 DWLRelease(const void *instance)
{
    struct HX170DWL *dec_dwl = (struct HX170DWL *)instance;
    //unsigned int i = 0;

    DWL_DEBUG("RELEASE\n");
    //assert(dec_dwl != NULL);
    if(dec_dwl == NULL)
    {
        return DWL_OK;
    }
#if 0
    //pthread_mutex_lock(&x170_init_mutex);
    n_dwl_instance_count--;

    if(dec_dwl->fd_mem != -1)
    {
        close(dec_dwl->fd_mem);
        if(dec_dwl->fd != -1)
        {
            close(dec_dwl->fd);
        }

        /* linear memory allocator */
        if(dec_dwl->fd_memalloc != -1)
        {
            close(dec_dwl->fd_memalloc);
        }

        /* print core usage stats */
        if(dec_dwl->client_type != DWL_CLIENT_TYPE_PP)
        {
            u32 total_usage = 0;
            u32 cores = dec_dwl->num_cores;
            for(i = 0; i < cores; i++)
            {
                total_usage += core_usage_counts[i];
            }
            /* avoid zero division */
            total_usage = total_usage ? total_usage : 1;
        }

#ifdef INTERNAL_TEST
  InternalTestFinalize();
#endif
    ActivityTraceRelease(&dec_dwl->activity);
#endif
    free(dec_dwl);
    //pthread_mutex_unlock(&x170_init_mutex);
    DWL_DEBUG("SUCCESS\n");
    return (DWL_OK);
}

#if 0
/* HW locking */

/*------------------------------------------------------------------------------
    Function name   : DWLReserveHwPipe
    Description     :
    Return type     : i32
    Argument        : const void *instance
    Argument        : i32 *core_id - ID of the reserved HW core
------------------------------------------------------------------------------*/
s32 DWLReserveHwPipe(const void *instance, s32 *core_id) {
  //s32 ret;
  struct HX170DWL *dec_dwl = (struct HX170DWL *)instance;

  //assert(dec_dwl != NULL);
  //assert(dec_dwl->client_type != DWL_CLIENT_TYPE_PP);

  DWL_DEBUG("Start\n");

  /* reserve decoder */
  #if 0
  *core_id =
      ioctl(dec_dwl->fd, HANTRODEC_IOCH_DEC_RESERVE, dec_dwl->client_type);

  if (*core_id != 0) {
    return DWL_ERROR;
  }
  #endif

  /* reserve PP */
  #if 0
  ret = ioctl(dec_dwl->fd, HANTRODEC_IOCQ_PP_RESERVE);

  /* for pipeline we expect same core for both dec and PP */
  if (ret != *core_id) {
    /* release the decoder */
    ioctl(dec_dwl->fd, HANTRODEC_IOCT_DEC_RELEASE, core_id);
    return DWL_ERROR;
  }
  #endif

  core_usage_counts[*core_id]++;

  dec_dwl->b_ppreserved = 1;

  DWL_DEBUG("Reserved DEC+PP core %d\n", *core_id);

  return DWL_OK;
}
#endif

#if 0
/*------------------------------------------------------------------------------
    Function name   : DWLReserveHw
    Description     :
    Return type     : i32
    Argument        : const void *instance
    Argument        : i32 *core_id - ID of the reserved HW core
------------------------------------------------------------------------------*/
s32 DWLReserveHw(const void *instance, s32 *core_id) {
  struct HX170DWL *dec_dwl = (struct HX170DWL *)instance;
  int is_pp;

  //assert(dec_dwl != NULL);

  is_pp = dec_dwl->client_type == DWL_CLIENT_TYPE_PP ? 1 : 0;

  DWL_DEBUG(" %s\n", is_pp ? "PP" : "DEC");

#if 0
  if (is_pp) {
    *core_id = ioctl(dec_dwl->fd, HANTRODEC_IOCQ_PP_RESERVE);

    /* PP is single core so we expect a zero return value */
    if (*core_id != 0) {
      return DWL_ERROR;
    }
  } else {
    *core_id =
        ioctl(dec_dwl->fd, HANTRODEC_IOCH_DEC_RESERVE, dec_dwl->client_type);
  }

  /* negative value signals an error */
  if (*core_id < 0) {
    DWL_DEBUG("ioctl HANTRODEC_IOCS_%s_reserve failed, %d\n",
              is_pp ? "PP" : "DEC", *core_id);
    return DWL_ERROR;
  }
  #endif

  core_usage_counts[*core_id]++;

  DWL_DEBUG("Reserved %s core %d\n", is_pp ? "PP" : "DEC", *core_id);

  return DWL_OK;
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
void DWLReleaseHw(const void *instance, s32 core_id) {
  struct HX170DWL *dec_dwl = (struct HX170DWL *)instance;
  int is_pp;

  //assert((u32)core_id < dec_dwl->num_cores);
  //assert(dec_dwl != NULL);

  is_pp = dec_dwl->client_type == DWL_CLIENT_TYPE_PP ? 1 : 0;

  if ((u32)core_id >= dec_dwl->num_cores) {
    //assert(0);
    return;
  }

  DWL_DEBUG(" %s core %d\n", is_pp ? "PP" : "DEC", core_id);
}
#if 0
void DWLSetIRQCallback(const void *instance, s32 core_id,
                       DWLIRQCallbackFn *callback_fn, void *arg) {
  struct HX170DWL *dec_dwl = (struct HX170DWL *)instance;

  dec_dwl->sync_params->callback[core_id] = callback_fn;
  dec_dwl->sync_params->callback_arg[core_id] = arg;
}
#endif

