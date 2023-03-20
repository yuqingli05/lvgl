/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-18     Meco Man     the first version
 * 2022-05-10     Meco Man     improve rt-thread initialization process
 */

#ifdef __RTTHREAD__

#include <lvgl.h>
#include <rtthread.h>
#include <rtdevice.h>

#define DBG_TAG "LVGL"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifndef PKG_LVGL_THREAD_STACK_SIZE
#define PKG_LVGL_THREAD_STACK_SIZE 4096
#endif /* PKG_LVGL_THREAD_STACK_SIZE */

#ifndef PKG_LVGL_THREAD_PRIO
#define PKG_LVGL_THREAD_PRIO (RT_THREAD_PRIORITY_MAX * 2 / 3)
#endif /* PKG_LVGL_THREAD_PRIO */

extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);
extern void lv_user_gui_init(void);

static struct rt_thread lvgl_thread;

#ifdef rt_align
rt_align(RT_ALIGN_SIZE) static rt_uint8_t lvgl_thread_stack[PKG_LVGL_THREAD_STACK_SIZE];
#else
ALIGN(RT_ALIGN_SIZE) static rt_uint8_t lvgl_thread_stack[PKG_LVGL_THREAD_STACK_SIZE];
#endif
static rt_mutex_t lvm = RT_NULL;
static struct rt_completion lvcompletion;
static int IsEnLvgl = 0;

#if LV_USE_LOG
static void lv_rt_log(const char *buf)
{
    LOG_I(buf);
}
#endif /* LV_USE_LOG */

void lv_mutex_take(void)
{
    if (IsEnLvgl && lvm)
        rt_mutex_take(lvm, RT_WAITING_FOREVER);
}
void lv_mutex_release(void)
{
    if (IsEnLvgl && lvm)
        rt_mutex_release(lvm);
}
void lv_flush(void)
{
    if (!IsEnLvgl)
        return;
    rt_completion_done(&lvcompletion);
}

static void lvgl_thread_entry(void *parameter)
{
#if LV_USE_LOG
    lv_log_register_print_cb(lv_rt_log);
#endif /* LV_USE_LOG */
    rt_mutex_take(lvm, RT_WAITING_FOREVER);
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_user_gui_init();
    rt_mutex_release(lvm);

    /* handle the tasks of LVGL */
    while (1)
    {
        uint32_t tick;
        rt_mutex_take(lvm, RT_WAITING_FOREVER);
        tick = lv_task_handler();
        rt_mutex_release(lvm);
        if (tick < LV_DISP_DEF_REFR_PERIOD)
            tick = LV_DISP_DEF_REFR_PERIOD;
        if (tick > INT32_MAX)
            tick = RT_WAITING_FOREVER;
        LOG_D("tick = %d\n", tick);
        rt_completion_wait(&lvcompletion, tick);
    }
}

static int lvgl_thread_init(void)
{
    rt_err_t err;

    if (rt_device_find("lcd") == RT_NULL)
    {
        IsEnLvgl = 0;
        return 0;
    }

    rt_completion_init(&lvcompletion);

    lvm = rt_mutex_create("lv_mutex", RT_IPC_FLAG_FIFO);
    RT_ASSERT(lvm != NULL);

    err = rt_thread_init(&lvgl_thread, "LVGL", lvgl_thread_entry, RT_NULL,
                         &lvgl_thread_stack[0], sizeof(lvgl_thread_stack), PKG_LVGL_THREAD_PRIO, 0);
    if (err != RT_EOK)
    {
        LOG_E("Failed to create LVGL thread");
        return -1;
    }
    rt_thread_startup(&lvgl_thread);

    IsEnLvgl = 1;
    return 0;
}
INIT_ENV_EXPORT(lvgl_thread_init);

#endif /*__RTTHREAD__*/
