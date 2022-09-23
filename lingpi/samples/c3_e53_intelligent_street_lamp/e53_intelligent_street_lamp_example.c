/*
 * Copyright (c) 2022 FuZhou Lockzhiner Electronic Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ohos_init.h"
#include "los_task.h"
#include "e53_intelligent_street_lamp.h"

/* 任务的堆栈大小 */
#define TASK_STACK_SIZE     20480
/* 任务的优先级 */
#define TASK_PRIO           24

/* 循环等待时间 */
#define WAIT_MSEC           2000

/* 光强度报警值 */
#define LUMN_ALARM          20.0

/***************************************************************
* 函数名称: e53_isl_thread
* 说    明: E53智慧路灯线程
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void e53_isl_thread(void *args)
{
    float lum = 0;

    e53_isl_init();

    while (1) {
        lum = e53_isl_read_data();

        printf("luminance value is %.2f\n", lum);

        if (lum < LUMN_ALARM) {
            isl_light_set_status(ON);
            printf("light on\n");
        } else {
            isl_light_set_status(OFF);
            printf("light off\n");
        }

        LOS_Msleep(WAIT_MSEC);
    }
}

/***************************************************************
* 函数名称: e53_isl_example
* 说    明: 智慧路灯例程
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void e53_isl_example(void)
{
    unsigned int ret = LOS_OK;
    unsigned int thread_id;
    TSK_INIT_PARAM_S task = {0};

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)e53_isl_thread;
    task.uwStackSize = TASK_STACK_SIZE;
    task.pcName = "e53_isl_thread";
    task.usTaskPrio = TASK_PRIO;
    ret = LOS_TaskCreate(&thread_id, &task);
    if (ret != LOS_OK) {
        printf("Falied to create e53_isl_thread ret:0x%x\n", ret);
        return;
    }
}

APP_FEATURE_INIT(e53_isl_example);
