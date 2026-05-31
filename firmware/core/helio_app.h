#pragma once

/*
 * helio_app.h — Application entry: setup/loop, FreeRTOS metering task, overproduction regulation.
 * See: /en/project-overview/ § High-level runtime model.
 */

#include <sys/time.h>

void helio_setup(void);
void helio_loop(void);
void helio_metering_task(void *pvParameters);
void helio_init_action_gpios(void);
void helio_apply_surplus_regulation(void);
void helio_daily_energy_tick(void);
void time_sync_notification(struct timeval *tv);
void helio_poll_temperature(void);
void helio_update_status_leds(void);
