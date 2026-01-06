#include "who_task_state.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace who {
namespace task {
WhoTaskState::WhoTaskState(int interval) : WhoTask("TaskState"), m_interval(interval)
{
    m_task_state = {"Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"};
}

void WhoTaskState::task()
{
    const TickType_t interval = pdMS_TO_TICKS(1000 * m_interval);
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        set_and_clear_bits(BLOCKING, RUNNING);
        vTaskDelayUntil(&last_wake_time, interval);
        EventBits_t event_bits = xEventGroupGetBits(m_event_group);
        if (event_bits & STOP) {
            set_and_clear_bits(TERMINATE, BLOCKING | STOP);
            break;
        } else if (event_bits & PAUSE) {
            xEventGroupClearBits(m_event_group, PAUSE);
            EventBits_t pause_event_bits =
                xEventGroupWaitBits(m_event_group, RESUME | STOP, pdTRUE, pdFALSE, portMAX_DELAY);
            if (pause_event_bits & STOP) {
                set_and_clear_bits(TERMINATE, BLOCKING);
                break;
            }
        }
        set_and_clear_bits(RUNNING, BLOCKING);
        print_task_status();
    }
    vTaskDelete(NULL);
}

bool WhoTaskState::stop()
{
    if (xEventGroupGetBits(m_event_group) & TERMINATE) {
        return false;
    }
    xEventGroupSetBits(m_event_group, STOP);
    xTaskAbortDelay(m_task_handle);
    xEventGroupWaitBits(m_event_group, TERMINATE, pdFALSE, pdFALSE, portMAX_DELAY);
    return true;
}

void WhoTaskState::print_task_status()
{
    configRUN_TIME_COUNTER_TYPE total_run_time;
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status_array =
        (TaskStatus_t *)heap_caps_malloc(num_tasks * sizeof(TaskStatus_t), MALLOC_CAP_SPIRAM);
    uxTaskGetSystemState(task_status_array, num_tasks, &total_run_time);

    printf("\nTask Name       |  coreid  |   State   | Priority |  Stack HWM  |   Run Time  | Run Time Per |\n");
    printf("----------------------------------------------------------------------------------------------\n");

    for (UBaseType_t i = 0; i < num_tasks; i++) {
        // Get core ID - in ESP-IDF 5.x, the field name may have changed
        BaseType_t core_id = -1;
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
        // Try to get core ID from task handle using affinity
        TaskHandle_t task_handle = task_status_array[i].xHandle;
        if (task_handle != NULL) {
            BaseType_t affinity = xTaskGetAffinity(task_handle);
            // Convert affinity bitmask to core number (0 or 1 for dual-core)
            if (affinity & 0x01) {
                core_id = 0;
            } else if (affinity & 0x02) {
                core_id = 1;
            } else {
                core_id = -1; // Pinned to both cores or invalid
            }
        }
#else
        core_id = -1;
#endif
#if CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U32
        printf("%-15s | %-8x | %-9s | %-8u | %-11lu | %-11lu | %-12lu |\n",
               task_status_array[i].pcTaskName,
               (unsigned int)core_id,
               m_task_state[task_status_array[i].eCurrentState].c_str(),
               task_status_array[i].uxCurrentPriority,
               task_status_array[i].usStackHighWaterMark,
               (unsigned long)task_status_array[i].ulRunTimeCounter,
               (unsigned long)(task_status_array[i].ulRunTimeCounter * 100 / total_run_time));
#else
        printf("%-15s | %-8x | %-9s | %-8u | %-11lu | %-11llu | %-12llu |\n",
               task_status_array[i].pcTaskName,
               (unsigned int)core_id,
               m_task_state[task_status_array[i].eCurrentState].c_str(),
               task_status_array[i].uxCurrentPriority,
               task_status_array[i].usStackHighWaterMark,
               (unsigned long long)task_status_array[i].ulRunTimeCounter,
               (unsigned long long)(task_status_array[i].ulRunTimeCounter * 100 / total_run_time));
#endif
    }
    printf("\n");
    heap_caps_free(task_status_array);
}
} // namespace task
} // namespace who
