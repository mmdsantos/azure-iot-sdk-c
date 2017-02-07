#ifndef IOTHUB_CLIENT_RETRY_CONTROL
#define IOTHUB_CLIENT_RETRY_CONTROL

#include "iothub_client_ll.h"

typedef enum RETRY_ACTION_TAG
{
	RETRY_ACTION_RETRY_NOW,
	RETRY_ACTION_RETRY_LATER,
	RETRY_ACTION_STOP_RETRYING
} RETRY_ACTION;

typedef RETRY_POLICY* RETRY_POLICY_HANDLE;
typedef RETRY_STATE* RETRY_STATE_HANDLE;

extern RETRY_POLICY_HANDLE retry_control_create_policy(IOTHUB_CLIENT_RETRY_POLICY policy_name, size_t max_retry_time_in_secs, size_t initial_wait_time_secs);
extern RETRY_POLICY_HANDLE retry_control_clone_policy(RETRY_POLICY_HANDLE policy_handle);
extern void retry_control_destroy_policy(RETRY_POLICY_HANDLE policy_handle);

extern RETRY_STATE_HANDLE retry_control_create_state_with_start_time(RETRY_POLICY_HANDLE policy_handle, time_t start_time);
extern RETRY_STATE_HANDLE retry_control_create_state(RETRY_POLICY_HANDLE policy_handle);
extern void retry_control_destroy_state(RETRY_STATE_HANDLE state_handle);

extern int retry_control_should_retry(RETRY_STATE_HANDLE retry_state_handle, RETRY_ACTION* retry_action);

extern int is_timeout_reached(time_t start_time, size_t timeout_in_secs, int *is_timed_out);

#endif IOTHUB_CLIENT_RETRY_CONTROL 