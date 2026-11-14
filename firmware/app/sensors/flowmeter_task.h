/*
 * flowmeter_task.h
 *
 *  Created on: Nov 7, 2026
 *      Author: Faysal
 */

#ifndef FLOWMETER_TASK_H_
#define FLOWMETER_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

void FlowMeter_task(void *parameters);
void FlowMeter_start_task();

typedef struct{
	float current_a;
	float voltage_bus_v;
} flowmeter_data_t;

#ifdef __cplusplus
}
#endif

#endif /* FLOWMETER_TASK_H_ */
