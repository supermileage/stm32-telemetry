/*
 * imu_task.h
 *
 *  Created on: Oct 12, 2026
 *      Author: Faysal
 */

#ifndef IMU_TASK_H_
#define IMU_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

void IMU_task(void *parameters);
void IMU_INT1_from_ISR(void);
void IMU_start_task();

#ifdef __cplusplus
}
#endif

#endif /* IMU_TASK_H_ */
