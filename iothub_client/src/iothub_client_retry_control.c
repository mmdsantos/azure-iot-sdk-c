#include "iothub_client_retry_control.h"

#define RESULT_OK           0
#define INDEFINITE_TIME     ((time_t)-1)

typedef struct YOU_KNOW_WHATS_COMING_RETRY_POLICY_TAG
{
	IOTHUB_CLIENT_RETRY_POLICY policy_name;
	size_t max_retry_time_secs;
	size_t initial_wait_time_secs;
} RETRY_POLICY;

typedef struct THIS_UNIT_WILL_SOON_BE_REWORKED_RETRY_POLICY_TAG
{
	RETRY_POLICY *policy;
	time_t start_time;
	time_t last_retry_time;
	size_t current_wait_time_secs
} RETRY_STATE;

static int evaluate_fixed_interval(RETRY_STATE *retry_state, RETRY_ACTION *retry_action)
{
	int result;

	time_t current_time;

	if ((current_time = get_time(NULL)) == INDEFINITE_TIME)
	{
		LogError("Cannot evaluate if should retry (get_time failed)");
		result = __LINE__;
	}
	else
	{
		if (retry_state->policy->max_retry_time_secs > 0 && 
			get_difftime(current_time, retry_state->start_time) >= retry_state->policy->max_retry_time_secs)
		{
			*retry_action = RETRY_ACTION_STOP_RETRYING;
		}
		else
		{
			if (get_difftime(current_time, retry_state->last_retry_time) >= retry_state->current_wait_time_secs)
			{
				*retry_action = RETRY_ACTION_RETRY_NOW;
				retry_state->last_retry_time = get_time(NULL);
			}
			else
			{
				*retry_action = RETRY_ACTION_RETRY_LATER;
			}
		}

		result = RESULT_OK;
	}

	return result;
}


// Public API

int is_timeout_reached(time_t start_time, size_t timeout_in_secs, int *is_timed_out)
{
	int result;

	if (start_time == INDEFINITE_TIME)
	{
		LogError("Failed to verify timeout (start_time is INDEFINITE)");
		result = __LINE__;
	}
	else
	{
		time_t current_time;

		if ((current_time = get_time(NULL)) == INDEFINITE_TIME)
		{
			LogError("Failed to verify timeout (get_time failed)");
			result = __LINE__;
		}
		else
		{
			if (get_difftime(current_time, start_time) >= timeout_in_secs)
			{
				*is_timed_out = 1;
			}
			else
			{
				*is_timed_out = 0;
			}

			result = RESULT_OK;
		}
	}

	return result;
}

RETRY_POLICY_HANDLE retry_control_create_policy(IOTHUB_CLIENT_RETRY_POLICY policy_name, size_t max_retry_time_in_secs, size_t initial_wait_time_secs)
{
	RETRY_POLICY* policy;

	if (policy_name != IOTHUB_CLIENT_RETRY_EXPONENTIAL_BACKOFF_WITH_JITTER)
	{
		LogError("Failed creating the retry policy (policy_name %d is not supported yet)", policy_name);
		policy = NULL;
	}
	else if ((policy = (RETRY_POLICY*)malloc(sizeof(RETRY_POLICY))) == NULL)
	{
		LogError("Failed creating the retry policy (malloc failed)");
	}
	else
	{
		memset(policy, 0, sizeof(RETRY_POLICY));
		policy->policy_name = policy_name;
		policy->max_retry_time_secs = max_retry_time_in_secs;
		policy->initial_wait_time_secs = initial_wait_time_secs;
	}

	return (RETRY_POLICY_HANDLE)policy;
}

RETRY_POLICY_HANDLE retry_control_clone_policy(RETRY_POLICY_HANDLE policy_handle)
{
	RETRY_POLICY* new_instance;

	if (policy_handle == NULL)
	{
		LogError("Failed cloning retry policy (policy_handle is NULL)");
		new_instance = NULL;
	}
	else
	{
		RETRY_POLICY* instance = (RETRY_POLICY*)policy_handle;

		if ((new_instance = retry_control_create_policy(instance->policy_name, instance->max_retry_time_secs, instance->initial_wait_time_secs)) == NULL)
		{
			LogError("Failed cloning retry policy (retry_control_create_policy failed)");
		}
	}

	return (RETRY_POLICY_HANDLE)new_instance;
}

RETRY_STATE_HANDLE retry_control_create_state_with_start_time(RETRY_POLICY_HANDLE policy_handle, time_t start_time)
{
	RETRY_STATE* state;

	if (policy_handle == NULL)
	{
		LogError("Failed creating the retry control state with start time (policy_handle is NULL)");
		state = NULL;
	}
	else if (start_time == INDEFINITE_TIME)
	{
		LogError("Failed creating the retry control state with start time (start_time is INDEFINITE)");
		state = NULL;
	}
	else if ((state = (RETRY_STATE*)malloc(sizeof(RETRY_STATE))) == NULL)
	{
		LogError("Failed creating the retry control state with start time (malloc failed)");
	}
	else
	{
		memset(state, 0, sizeof(RETRY_STATE));
		state->policy = (RETRY_POLICY*)policy_handle;
		state->start_time = start_time;
		state->last_retry_time = start_time;
		state->current_wait_time_secs = state->policy->initial_wait_time_secs;
	}

	return (RETRY_STATE_HANDLE)state;
}

RETRY_STATE_HANDLE retry_control_create_state(RETRY_POLICY_HANDLE policy_handle)
{
	RETRY_STATE* state;

	if (policy_handle == NULL)
	{
		LogError("Failed creating the retry control state (policy_handle is NULL)");
		state = NULL;
	}
	else
	{
		time_t current_time = get_time(NULL); // No worries, it will be tested within the next API below.

		if ((state = retry_control_create_state_with_start_time(policy_handle, current_time)) == NULL)
		{
			LogError("Failed creating the retry control state (retry_control_create_state_with_start_time failed)");
		}
	}

	return (RETRY_STATE_HANDLE)state;
}

void retry_control_destroy_policy(RETRY_POLICY_HANDLE policy_handle)
{
	if (policy_handle != NULL)
	{
		free(policy_handle);
	}
}

void retry_control_destroy_state(RETRY_STATE_HANDLE state_handle)
{
	if (state_handle != NULL)
	{
		RETRY_STATE* state = (RETRY_STATE*)state_handle;

		retry_control_destroy_policy(state->policy);
		free(state_handle);
	}
}

int retry_control_should_retry(RETRY_STATE_HANDLE retry_state_handle, RETRY_ACTION *retry_action)
{
	int result;

	if (retry_state_handle == NULL)
	{
		LogError("Failed to evaluate if retry should be attempted (retry_state_handle is NULL)");
		result = __LINE__;
	}
	else if (retry_action == NULL)
	{
		LogError("Failed to evaluate if retry should be attempted (retry_action is NULL)");
		result = __LINE__;
	}
	else
	{
		RETRY_STATE* retry_state = (RETRY_STATE*)retry_state_handle;

		if (retry_state->policy->policy_name == IOTHUB_CLIENT_RETRY_INTERVAL)
		{
			if ((result = evaluate_fixed_interval(retry_state, retry_action)) != RESULT_OK)
			{
				LogError("Failed to evaluate if retry should be attempted (failed to evaluate fixed interval retry action)");
				result = __LINE__;
			}
		}
		else
		{
			LogError("Cannot evaluate if should retry (do not support policy %s)", retry_state->policy->policy_name);
			result = __LINE__;
		}
	}

	return result;
}