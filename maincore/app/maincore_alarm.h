/* maincore_alarm.h — 主核状态机接口
 *
 * 流程：读副核状态 → 显示 → 震动/等按键 → CMD_SLEEP → 等断电。
 * 主循环调 AlarmProcess() 推状态机，其余三个是查询口。
 */

#ifndef MAINCORE_ALARM_H
#define MAINCORE_ALARM_H

#include "global_types.h"
#include "protocol.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 主核状态机
 *============================================================================*/

typedef enum {
    MC_STATE_INIT      = 0,  /**< 摆状态机，硬件在 main 里已初始化 */
    MC_STATE_READY     = 1,  /**< 就绪 */
    MC_STATE_QUERY     = 2,  /**< 读副核状态（可原地重试） */
    MC_STATE_NORMAL    = 3,  /**< 正常界面 */
    MC_STATE_ALARM     = 4,  /**< 告警界面 + 震动 */
    MC_STATE_CONFIRMED = 5,  /**< 用户已按键确认 */
    MC_STATE_SLEEPING  = 6,  /**< 发 CMD_SLEEP */
    MC_STATE_OFF       = 7,  /**< 等副核断电 */
} MainCoreState_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化状态机（不含硬件）
 */
void MainCore_AlarmInit(void);

/**
 * @brief   推一次状态机
 * @note    由 main 的 while(1) 调用，每轮检查超时
 * @return  true 继续跑; false 该退出了（断电路径）
 */
bool MainCore_AlarmProcess(void);

/**
 * @brief   查当前状态（调试用）
 */
MainCoreState_t MainCore_GetState(void);

/**
 * @brief   按键事件输入
 * @param   pressed true=按下
 * @note    在 EXTI1 中断里调用，只置标志
 */
void MainCore_OnKeyPress(bool pressed);

#ifdef __cplusplus
}
#endif

#endif /* MAINCORE_ALARM_H */
