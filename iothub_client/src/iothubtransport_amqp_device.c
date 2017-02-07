// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include "azure_c_shared_utility/strings.h"
#include "iothubtransport_amqp_cbs_auth.h"
#include "iothubtransport_amqp_messenger.h"
#include "iothubtransport_amqp_device.h"

#define RESULT_OK                                  0
#define INDEFINITE_TIME                            ((time_t)-1)
#define DEFAULT_AUTH_STATE_CHANGED_TIMEOUT_SECS    60
#define DEFAULT_MSGR_STATE_CHANGED_TIMEOUT_SECS    60

typedef struct DEVICE_INSTANCE_TAG
{
	DEVICE_CONFIG* config;
	DEVICE_STATE state;

	SESSION_HANDLE session_handle;
	CBS_HANDLE cbs_handle;

	AUTHENTICATION_HANDLE authentication_handle;
	AUTHENTICATION_STATE auth_state;
	AUTHENTICATION_ERROR_CODE auth_error_code;
	OPTIONHANDLER_HANDLE saved_auth_options;
	time_t auth_state_last_changed_time;
	size_t auth_state_change_timeout_secs;

	MESSENGER_HANDLE messenger_handle;
	MESSENGER_STATE msgr_state;
	time_t msgr_state_last_changed_time;
	size_t msgr_state_change_timeout_secs;
} DEVICE_INSTANCE;

typedef struct SEND_EVENT_TASK_TAG
{
	ON_DEVICE_D2C_EVENT_SEND_COMPLETE on_event_send_complete_callback;
	void* on_event_send_complete_context;
} SEND_EVENT_TASK;

typedef struct RECEIVE_C2D_MESSAGE_CONTEXT_TAG
{
	ON_DEVICE_C2D_MESSAGE_RECEIVED callback;
	void* context;
} RECEIVE_C2D_MESSAGE_CONTEXT;


// Internal state control

static void update_state(DEVICE_INSTANCE* instance, DEVICE_STATE new_state)
{
	if (new_state != instance->state)
	{
		DEVICE_STATE previous_state = instance->state;
		instance->state = new_state;

		if (instance->config->on_state_changed_callback != NULL)
		{
			instance->config->on_state_changed_callback(instance->config->on_state_changed_context, previous_state, new_state);
		}
	}
}

static int is_timeout_reached(time_t start_time, size_t timeout_in_secs, int *is_timed_out)
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


// Callback Handlers

static D2C_EVENT_SEND_RESULT get_d2c_event_send_result_from(MESSENGER_EVENT_SEND_COMPLETE_RESULT result)
{
	D2C_EVENT_SEND_RESULT d2c_esr;

	switch (result)
	{
		case MESSENGER_EVENT_SEND_COMPLETE_RESULT_OK:
			d2c_esr = D2C_EVENT_SEND_COMPLETE_RESULT_OK;
			break;
		case MESSENGER_EVENT_SEND_COMPLETE_RESULT_ERROR_CANNOT_PARSE:
			d2c_esr = D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_CANNOT_PARSE;
			break;
		case MESSENGER_EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING:
			d2c_esr = D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING;
			break;
		case MESSENGER_EVENT_SEND_COMPLETE_RESULT_ERROR_TIMEOUT:
			d2c_esr = D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_TIMEOUT;
			break;
		case MESSENGER_EVENT_SEND_COMPLETE_RESULT_MESSENGER_DESTROYED:
			d2c_esr = D2C_EVENT_SEND_COMPLETE_RESULT_DEVICE_DESTROYED;
			break;
		default:
			// This is not expected. All states should be mapped.
			d2c_esr = D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_UNKNOWN;
			break;
	};

	return d2c_esr;
}

static void on_event_send_complete_messenger_callback(IOTHUB_MESSAGE_LIST* iothub_message, MESSENGER_EVENT_SEND_COMPLETE_RESULT ev_send_comp_result, void* context)
{
	SEND_EVENT_TASK* task = (SEND_EVENT_TASK*)context;
	D2C_EVENT_SEND_RESULT device_send_result = get_d2c_event_send_result_from(ev_send_comp_result);

	task->on_event_send_complete_callback(iothub_message, device_send_result, task->on_event_send_complete_context);
}

static void on_authentication_error_callback(void* context, AUTHENTICATION_ERROR_CODE error_code)
{
	DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)context;
	instance->auth_error_code = error_code;
}

static void on_authentication_state_changed_callback(void* context, AUTHENTICATION_STATE previous_state, AUTHENTICATION_STATE new_state)
{
	(void)previous_state;
	DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)context;
	instance->auth_state = new_state;
}

static void on_messenger_state_changed_callback(void* context, MESSENGER_STATE previous_state, MESSENGER_STATE new_state)
{
	(void)previous_state;
	DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)context;
	instance->msgr_state = new_state;
}

static MESSENGER_DISPOSITION_RESULT on_messenger_new_message_received_callback(IOTHUB_MESSAGE_HANDLE iothub_message_handle, void* context)
{
	MESSENGER_DISPOSITION_RESULT msgr_disposition_result;

	if (iothub_message_handle == NULL)
	{
		LogError("Failed receiving incoming C2D message (message handle is NULL)");
		msgr_disposition_result = MESSENGER_DISPOSITION_RESULT_ABANDONED;
	}
	else if (context == NULL)
	{
		LogError("Failed receiving incoming C2D message (context is NULL)");
		msgr_disposition_result = MESSENGER_DISPOSITION_RESULT_ABANDONED;
	}
	else
	{
		RECEIVE_C2D_MESSAGE_CONTEXT* device_context = (RECEIVE_C2D_MESSAGE_CONTEXT*)context;

		switch (device_context->callback(iothub_message_handle, device_context->context))
		{
		case DEVICE_MESSAGE_DISPOSITION_RESULT_ACCEPTED:
			msgr_disposition_result = MESSENGER_DISPOSITION_RESULT_ACCEPTED;
			break;
		case DEVICE_MESSAGE_DISPOSITION_RESULT_REJECTED:
			msgr_disposition_result = MESSENGER_DISPOSITION_RESULT_REJECTED;
			break;
		case DEVICE_MESSAGE_DISPOSITION_RESULT_ABANDONED:
		default:
			msgr_disposition_result = MESSENGER_DISPOSITION_RESULT_ABANDONED;
			break;
		}
	}

	return msgr_disposition_result;
}


// Configuration Helpers

static void destroy_device_config(DEVICE_CONFIG* config)
{
	if (config != NULL)
	{
		free(config->device_id);
		free(config->iothub_host_fqdn);
		free(config->device_primary_key);
		free(config->device_secondary_key);
		free(config->device_sas_token);
		free(config);
	}
}

static DEVICE_CONFIG* clone_device_config(DEVICE_CONFIG *config)
{
	DEVICE_CONFIG* new_config;

	if ((new_config = (DEVICE_CONFIG*)malloc(sizeof(DEVICE_CONFIG))) == NULL)
	{
		LogError("Failed copying the DEVICE_CONFIG (malloc failed)");
	}
	else
	{
		int result;

		memset(new_config, 0, sizeof(DEVICE_CONFIG));

		if (mallocAndStrcpy_s(&new_config->device_id, config->device_id) != RESULT_OK)
		{
			LogError("Failed copying the DEVICE_CONFIG (failed copying device_id)");
			result = __LINE__;
		}
		else if (mallocAndStrcpy_s(&new_config->iothub_host_fqdn, config->iothub_host_fqdn) != RESULT_OK)
		{
			LogError("Failed copying the DEVICE_CONFIG (failed copying iothub_host_fqdn)");
			result = __LINE__;
		}
		else if (config->device_sas_token != NULL &&
			mallocAndStrcpy_s(&new_config->device_sas_token, config->device_sas_token) != RESULT_OK)
		{
			LogError("Failed copying the DEVICE_CONFIG (failed copying device_sas_token)");
			result = __LINE__;
		}
		else if (config->device_primary_key != NULL &&
			mallocAndStrcpy_s(&new_config->device_primary_key, config->device_primary_key) != RESULT_OK)
		{
			LogError("Failed copying the DEVICE_CONFIG (failed copying device_primary_key)");
			result = __LINE__;
		}
		else if (config->device_secondary_key != NULL &&
			mallocAndStrcpy_s(&new_config->device_secondary_key, config->device_secondary_key) != RESULT_OK)
		{
			LogError("Failed copying the DEVICE_CONFIG (failed copying device_secondary_key)");
			result = __LINE__;
		}
		else
		{
			new_config->authentication_mode = config->authentication_mode;
			new_config->on_state_changed_callback = config->on_state_changed_callback;
			new_config->on_state_changed_context = config->on_state_changed_context;

			result = RESULT_OK;
		}

		if (result != RESULT_OK)
		{
			destroy_device_config(new_config);
			new_config = NULL;
		}
	}

	return new_config;
}

static void set_authentication_config(DEVICE_INSTANCE* device_instance, AUTHENTICATION_CONFIG* auth_config)
{
	DEVICE_CONFIG *device_config = device_instance->config;

	auth_config->device_id = device_config->device_id;
	auth_config->iothub_host_fqdn = device_config->iothub_host_fqdn;
	auth_config->on_error_callback = on_authentication_error_callback;
	auth_config->on_error_callback_context = device_instance;
	auth_config->on_state_changed_callback = on_authentication_state_changed_callback;
	auth_config->on_state_changed_callback_context = device_instance;
	auth_config->device_primary_key = device_config->device_primary_key;
	auth_config->device_secondary_key = device_config->device_secondary_key;
	auth_config->device_sas_token = device_config->device_sas_token;
}


// Create and Destroy Helpers

static void internal_destroy_device(DEVICE_INSTANCE* instance)
{
	if (instance != NULL)
	{
		if (instance->messenger_handle != NULL)
		{
			messenger_destroy(instance->messenger_handle);
		}

		if (instance->authentication_handle != NULL)
		{
			authentication_destroy(instance->authentication_handle);
		}

		destroy_device_config(instance->config);
		free(instance);
	}
}

static int create_authentication_instance(DEVICE_INSTANCE *instance)
{
	int result;
	AUTHENTICATION_CONFIG auth_config;

	set_authentication_config(instance, &auth_config);

	if ((instance->authentication_handle = authentication_create(&auth_config)) == NULL)
	{
		LogError("Failed creating the AUTHENTICATION_HANDLE (authentication_create failed)");
		result = __LINE__;
	}
	else
	{
		if (instance->saved_auth_options != NULL &&
			OptionHandler_FeedOptions(instance->saved_auth_options, instance->authentication_handle) != OPTIONHANDLER_OK)
		{
			authentication_destroy(instance->authentication_handle);
			result = __LINE__;
		}
		else
		{
			instance->saved_auth_options = NULL;
			result = RESULT_OK;
		}
	}

	return result;
}

static int create_messenger_instance(DEVICE_INSTANCE* instance)
{
	int result;

	MESSENGER_CONFIG messenger_config;
	messenger_config.device_id = instance->config->device_id;
	messenger_config.iothub_host_fqdn = instance->config->iothub_host_fqdn;
	messenger_config.on_state_changed_callback = on_messenger_state_changed_callback;
	messenger_config.on_state_changed_context = instance;

	if ((instance->messenger_handle = messenger_create(&messenger_config)) == NULL)
	{
		LogError("Failed creating the MESSENGER_HANDLE (messenger_create failed)");
		result = __LINE__;
	}
	else
	{
		result = RESULT_OK;
	}

	return result;
}


// Public APIs:

DEVICE_HANDLE device_create(DEVICE_CONFIG *config)
{
	DEVICE_INSTANCE *instance;

	if (config == NULL)
	{
		LogError("Failed creating the device instance (config is NULL)");
		instance = NULL;
	}
	else if (config->device_id == NULL)
	{
		LogError("Failed creating the device instance (config->device_id is NULL)");
		instance = NULL;
	}
	else if (config->iothub_host_fqdn == NULL)
	{
		LogError("Failed creating the device instance (config->iothub_host_fqdn is NULL)");
		instance = NULL;
	}
	else if (config->on_state_changed_callback == NULL)
	{
		LogError("Failed creating the device instance (config->on_state_changed_callback is NULL)");
		instance = NULL;
	}
	else if ((instance = (DEVICE_INSTANCE*)malloc(sizeof(DEVICE_INSTANCE))) == NULL)
	{
		LogError("Failed creating the device instance (malloc failed)");
	}
	else
	{
		int result;

		memset(instance, 0, sizeof(DEVICE_INSTANCE));

		if ((instance->config = clone_device_config(config)) == NULL)
		{
			LogError("Failed creating the device instance for device '%s' (failed copying the configuration)", config->device_id);
			result = __LINE__;
		}

		else if (instance->config->authentication_mode == DEVICE_AUTH_MODE_CBS &&
			     create_authentication_instance(instance) != RESULT_OK)
		{
			LogError("Failed creating the device instance for device '%s' (failed creating the authentication instance)", instance->config->device_id);
			result = __LINE__;
		}
		else if (create_messenger_instance(instance) != RESULT_OK)
		{
			LogError("Failed creating the device instance for device '%s' (failed creating the messenger instance)", instance->config->device_id);
			result = __LINE__;
		}
		else
		{
			instance->auth_state = AUTHENTICATION_STATE_STOPPED;
			instance->msgr_state = MESSENGER_STATE_STOPPED;
			instance->state = DEVICE_STATE_STOPPED;
			instance->auth_state_last_changed_time = INDEFINITE_TIME;
			instance->auth_state_change_timeout_secs = DEFAULT_AUTH_STATE_CHANGED_TIMEOUT_SECS;
			instance->msgr_state_last_changed_time = INDEFINITE_TIME;
			instance->msgr_state_change_timeout_secs = DEFAULT_MSGR_STATE_CHANGED_TIMEOUT_SECS;

			result = RESULT_OK;
		}

		if (result != RESULT_OK)
		{
			internal_destroy_device(instance);
			instance = NULL;
		}
	}

	return (DEVICE_HANDLE)instance;
}

int device_start_async(DEVICE_HANDLE handle, SESSION_HANDLE session_handle, CBS_HANDLE cbs_handle)
{
	int result;

	if (handle == NULL)
	{
		LogError("Failed starting device (handle is NULL)");
		result = __LINE__;
	}
	else
	{
		DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

		if (instance->state != DEVICE_STATE_STOPPED)
		{
			LogError("Failed starting device (device is not stopped)");
			result = __LINE__;
		}
		else if (session_handle == NULL)
		{
			LogError("Failed starting device (session_handle is NULL)");
			result = __LINE__;
		}
		else if (instance->config->authentication_mode == DEVICE_AUTH_MODE_CBS && cbs_handle == NULL)
		{
			LogError("Failed starting device (device using CBS authentication, but cbs_handle is NULL)");
			result = __LINE__;
		}
		else
		{
			instance->session_handle = session_handle;
			instance->cbs_handle = cbs_handle;

			update_state(instance, DEVICE_STATE_STARTING);
			result = RESULT_OK;
		}
	}

	return result;
}

// @brief
//     stops a device instance (stops messenger and authentication) synchronously.
// @returns
//     0 if the function succeeds, non-zero otherwise.
int device_stop(DEVICE_HANDLE handle)
{
	int result;

	if (handle == NULL)
	{
		LogError("Failed stopping device (handle is NULL)");
		result = __LINE__;
	}
	else
	{
		DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

		if (instance->state == DEVICE_STATE_STOPPED || instance->state == DEVICE_STATE_STOPPING)
		{
			LogError("Failed stopping device '%s' (device is already stopped or stopping)", instance->config->device_id);
			result = __LINE__;
		}
		else
		{
			update_state(instance, DEVICE_STATE_STOPPING);

			if (instance->msgr_state != MESSENGER_STATE_STOPPED && 
				instance->msgr_state != MESSENGER_STATE_STOPPING &&
				messenger_stop(instance->messenger_handle) != RESULT_OK)
			{
				LogError("Failed stopping device '%s' (messenger_stop failed)", instance->config->device_id);
				result = __LINE__;
				update_state(instance, DEVICE_STATE_ERROR_MSG);
			}
			else if (instance->config->authentication_mode == DEVICE_AUTH_MODE_CBS &&
				instance->auth_state != AUTHENTICATION_STATE_STOPPED &&
				authentication_stop(instance->authentication_handle) != RESULT_OK)
			{
				LogError("Failed stopping device '%s' (authentication_stop failed)", instance->config->device_id);
				result = __LINE__;
				update_state(instance, DEVICE_STATE_ERROR_AUTH);
			}
			else
			{
				result = RESULT_OK;
			}
		}
	}

	return result;
}

void device_do_work(DEVICE_HANDLE handle)
{
	if (handle == NULL)
	{
		LogError("Failed to perform device_do_work (handle is NULL)");
	}
	else
	{
		// Cranking the state monster:
		DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

		if (instance->state == DEVICE_STATE_STARTING)
		{
			if (instance->config->authentication_mode == DEVICE_AUTH_MODE_CBS)
			{
				if (instance->auth_state == AUTHENTICATION_STATE_STOPPED)
				{
					if (authentication_start(instance->authentication_handle, instance->cbs_handle) != RESULT_OK)
					{
						LogError("Device '%s' failed to be authenticated (authentication_start failed)", instance->config->device_id);

						update_state(instance, DEVICE_STATE_ERROR_AUTH);
					}
					else if ((instance->auth_state_last_changed_time = get_time(NULL)) == INDEFINITE_TIME)
					{
						LogError("Device '%s' failed to set time for tracking authentication start timeout (get_time failed)");

						if (authentication_stop(instance->authentication_handle) != RESULT_OK)
						{
							LogError("Device '%s' failed stopping the authentication instance (authentication_stop failed)");
						}

						update_state(instance, DEVICE_STATE_ERROR_AUTH);
					}
				}
				else if (instance->auth_state == AUTHENTICATION_STATE_STARTING)
				{
					int is_timed_out;
					if (is_timeout_reached(instance->auth_state_last_changed_time, instance->auth_state_change_timeout_secs, &is_timed_out) != RESULT_OK)
					{
						LogError("Device '%s' failed verifying the timeout for authentication start (is_timeout_reached failed)", instance->config->device_id);
						update_state(instance, DEVICE_STATE_ERROR_AUTH);
					}
					else if (is_timed_out == 1)
					{
						LogError("Device '%s' authentication did not complete starting within expected timeout (%d)", instance->config->device_id, instance->auth_state_change_timeout_secs);

						update_state(instance, DEVICE_STATE_ERROR_AUTH_TIMEOUT);
					}
				}
				else if (instance->auth_state == AUTHENTICATION_STATE_ERROR)
				{
					if (instance->auth_error_code == AUTHENTICATION_ERROR_AUTH_FAILED)
					{
						update_state(instance, DEVICE_STATE_ERROR_AUTH);
					}
					else // DEVICE_STATE_ERROR_TIMEOUT
					{
						update_state(instance, DEVICE_STATE_ERROR_AUTH_TIMEOUT);
					}
				}
				// There is no AUTHENTICATION_STATE_STOPPING
			}

			if (instance->config->authentication_mode == DEVICE_AUTH_MODE_X509 || instance->auth_state == AUTHENTICATION_STATE_STARTED)
			{
				if (instance->msgr_state == MESSENGER_STATE_STOPPED)
				{
					if (messenger_start(instance->messenger_handle, instance->session_handle) != RESULT_OK)
					{
						LogError("Device '%s' messenger failed to be started (messenger_start failed)", instance->config->device_id);

						update_state(instance, DEVICE_STATE_ERROR_MSG);
					}
					else if ((instance->msgr_state_last_changed_time = get_time(NULL)) == INDEFINITE_TIME)
					{
						LogError("Device '%s' failed to set time for tracking messenger start timeout (get_time failed)");

						if (messenger_stop(instance->messenger_handle) != RESULT_OK)
						{
							LogError("Device '%s' failed stopping the messenger instance (messenger_stop failed)");
						}

						update_state(instance, DEVICE_STATE_ERROR_MSG);
					}
				}
				else if (instance->msgr_state == MESSENGER_STATE_STARTING)
				{
					int is_timed_out;
					if (is_timeout_reached(instance->msgr_state_last_changed_time, instance->msgr_state_change_timeout_secs, &is_timed_out) != RESULT_OK)
					{
						LogError("Device '%s' failed verifying the timeout for messenger start (is_timeout_reached failed)", instance->config->device_id);

						update_state(instance, DEVICE_STATE_ERROR_MSG);
					}
					else if (is_timed_out == 1)
					{
						LogError("Device '%s' messenger did not complete starting within expected timeout (%d)", instance->config->device_id, instance->msgr_state_change_timeout_secs);

						update_state(instance, DEVICE_STATE_ERROR_MSG);
					}
				}
				else if (instance->msgr_state == MESSENGER_STATE_ERROR)
				{
					LogError("Device '%s' messenger failed to be started (messenger got into error state)", instance->config->device_id);
					
					update_state(instance, DEVICE_STATE_ERROR_MSG);
				}
				else if (instance->msgr_state == MESSENGER_STATE_STARTED)
				{
					update_state(instance, DEVICE_STATE_STARTED);
				}
			}
		}
		else if (instance->state == DEVICE_STATE_STARTED)
		{
			if (instance->config->authentication_mode == DEVICE_AUTH_MODE_CBS &&
				instance->auth_state != AUTHENTICATION_STATE_STARTED)
			{
				LogError("Device '%s' is started but authentication reported unexpected state %d", instance->auth_state);

				if (instance->auth_state != AUTHENTICATION_STATE_ERROR)
				{
					if (instance->auth_error_code == AUTHENTICATION_ERROR_AUTH_FAILED)
					{
						update_state(instance, DEVICE_STATE_ERROR_AUTH);
					}
					else // AUTHENTICATION_ERROR_AUTH_TIMEOUT
					{
						update_state(instance, DEVICE_STATE_ERROR_AUTH_TIMEOUT);
					}
				}
				else
				{
					update_state(instance, DEVICE_STATE_ERROR_AUTH);
				}
			}
			else
			{
				if (instance->msgr_state != MESSENGER_STATE_STARTED)
				{
					LogError("Device '%s' is started but messenger reported unexpected state %d", instance->msgr_state);
					update_state(instance, DEVICE_STATE_ERROR_MSG);
				}
			}
		}

		// Invoking the do_works():
		if (instance->config->authentication_mode == DEVICE_AUTH_MODE_CBS)
		{
			if (instance->auth_state != AUTHENTICATION_STATE_STOPPED && instance->auth_state != AUTHENTICATION_STATE_ERROR)
			{
				authentication_do_work(instance->authentication_handle);
			}
		}

		if (instance->msgr_state != MESSENGER_STATE_STOPPED && instance->msgr_state != MESSENGER_STATE_ERROR)
		{
			messenger_do_work(instance->messenger_handle);
		}
	}
}

void device_destroy(DEVICE_HANDLE handle)
{
	if (handle != NULL)
	{
		DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

		if (instance->state == DEVICE_STATE_STARTED || instance->state == DEVICE_STATE_STARTING)
		{
			device_stop((DEVICE_HANDLE)instance);
			device_do_work((DEVICE_HANDLE)instance); // This creates a situation for a race hazzard since device_do_work is usually being invoked on a different thread.
		}

		internal_destroy_device((DEVICE_INSTANCE*)handle);
	}
}

int device_send_event_async(DEVICE_HANDLE handle, IOTHUB_MESSAGE_LIST* iothub_message_list, ON_DEVICE_D2C_EVENT_SEND_COMPLETE on_device_d2c_event_send_complete_callback, void* context)
{
	int result;

	if (handle == NULL)
	{
		LogError("Failed sending event (handle is NULL)");
		result = __LINE__;
	}
	else if (iothub_message_list == NULL)
	{
		LogError("Failed sending event (iothub_message_list is NULL)");
		result = __LINE__;
	}
	else if (on_device_d2c_event_send_complete_callback == NULL)
	{
		LogError("Failed sending event (on_event_send_complete_callback is NULL)");
		result = __LINE__;
	}
	else
	{
		SEND_EVENT_TASK* task;

		if ((task = (SEND_EVENT_TASK*)malloc(sizeof(SEND_EVENT_TASK))) == NULL)
		{
			LogError("Failed sending event (failed creating task to send event)");
			result = __LINE__;
		}
		else
		{
			DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

			memset(task, 0, sizeof(SEND_EVENT_TASK));
			task->on_event_send_complete_callback = on_device_d2c_event_send_complete_callback;
			task->on_event_send_complete_context = context;
			
			if (messenger_send_async(instance->messenger_handle, iothub_message_list, on_event_send_complete_messenger_callback, (void*)task) != RESULT_OK)
			{
				LogError("Failed sending event (messenger_send_async failed)");
				free(task);
				result = __LINE__;
			}
			else
			{
				result = RESULT_OK;
			}
		}
	}

	return result;
}

int device_get_send_status(DEVICE_HANDLE handle, DEVICE_SEND_STATUS *send_status)
{
	int result;

	if (handle == NULL)
	{
		LogError("Failed getting the device messenger send status (handle is NULL)");
		result = __LINE__;
	}
	else if (send_status == NULL)
	{
		LogError("Failed getting the device messenger send status (send_status is NULL)");
		result = __LINE__;
	}
	else
	{
		DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;
		MESSENGER_SEND_STATUS messenger_send_status;
		
		if (messenger_get_send_status(instance->messenger_handle, &messenger_send_status) != RESULT_OK)
		{
			LogError("Failed getting the device messenger send status (messenger_get_send_status failed)");
			result = __LINE__;
		}
		else
		{
			if (messenger_send_status == MESSENGER_SEND_STATUS_IDLE)
			{
				*send_status = DEVICE_SEND_STATUS_IDLE;
			}
			else // i.e., messenger_send_status == MESSENGER_SEND_STATUS_BUSY
			{
				*send_status = DEVICE_SEND_STATUS_BUSY;
			}

			result = RESULT_OK;
		}
	}

	return result;
}

int device_subscribe_message(DEVICE_HANDLE handle, ON_DEVICE_C2D_MESSAGE_RECEIVED on_message_received_callback, void* context)
{
	int result;

	if (handle == NULL)
	{
		LogError("Failed subscribing to C2D messages (handle is NULL)");
		result = __LINE__;
	}
	else if (on_message_received_callback == NULL)
	{
		LogError("Failed subscribing to C2D messages (on_message_received_callback is NULL)");
		result = __LINE__;
	}
	else
	{
		RECEIVE_C2D_MESSAGE_CONTEXT* device_context;

		if ((device_context = (RECEIVE_C2D_MESSAGE_CONTEXT*)malloc(sizeof(RECEIVE_C2D_MESSAGE_CONTEXT))) == NULL)
		{
			LogError("Failed subscribing to C2D messages (malloc failed to create internal context)");
			result = __LINE__;
		}
		else
		{
			DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

			memset(device_context, 0, sizeof(device_context));
			device_context->callback = on_message_received_callback;
			device_context->context = context;

			if (messenger_subscribe_for_messages(instance->messenger_handle, on_messenger_new_message_received_callback, device_context) != RESULT_OK)
			{
				LogError("Failed subscribing to C2D messages (messenger_subscribe_for_messages failed)");
				free(device_context);
				result = __LINE__;
			}
			else
			{
				result = RESULT_OK;
			}
		}
	}

	return result;
}

int device_unsubscribe_message(DEVICE_HANDLE handle)
{
	int result;

	if (handle == NULL)
	{
		LogError("Failed unsubscribing to C2D messages (handle is NULL)");
		result = __LINE__;
	}
	else
	{
		DEVICE_INSTANCE* instance = (DEVICE_INSTANCE*)handle;

		if (messenger_unsubscribe_for_messages(instance->messenger_handle) != RESULT_OK)
		{
			LogError("Failed unsubscribing to C2D messages (messenger_unsubscribe_for_messages failed)");
			result = __LINE__;
		}
		else
		{
			result = RESULT_OK;
		}
	}

	return result;
}

int device_set_retry_policy(DEVICE_HANDLE handle, IOTHUB_CLIENT_RETRY_POLICY policy, size_t retry_timeout_limit_in_seconds)
{
	(void)retry_timeout_limit_in_seconds;
	(void)policy;
	int result;

	if (handle == NULL)
	{
		LogError("Failed setting retry policy (handle is NULL)");
		result = __LINE__;
	}
	else
	{
		LogError("Failed setting retry policy (functionality not supported)");
		result = __LINE__;
	}

	return result;
}

int device_set_option(DEVICE_HANDLE handle, const char* name, void* value)
{
	(void)value;
	int result;

	if (handle == NULL)
	{
		LogError("Failed setting option (handle is NULL)");
		result = __LINE__;
	}
	else if (name == NULL)
	{
		LogError("Failed setting option (name is NULL)");
		result = __LINE__;
	}
	// TODO: implement this.
	else
	{
		result = RESULT_OK;
	}

	return result;
}
