// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include "azure_c_shared_utility/agenttime.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/doublylinkedlist.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/urlencode.h"
#include "azure_c_shared_utility/tlsio.h"

#include "azure_uamqp_c/cbs.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"

#include "iothub_client_ll.h"
#include "iothub_client_options.h"
#include "iothub_client_private.h"
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
#include "iothubtransportamqp_methods.h"
#endif
#include "iothubtransport_amqp_common.h"
#include "iothubtransport_amqp_connection.h"
#include "iothubtransport_amqp_device.h"
#include "iothub_client_version.h"

#define RESULT_OK                                 0
#define INDEFINITE_TIME                           ((time_t)(-1))
#define DEFAULT_SAS_TOKEN_LIFETIME_MS             3600000
#define DEFAULT_CBS_REQUEST_TIMEOUT_MS            30000
#define MAX_NUMBER_OF_DEVICE_FAILURES             5
#define DEFAULT_DEVICE_STATE_CHANGE_TIMEOUT_SECS  300


// ---------- Data Definitions ---------- //

typedef enum AMQP_TRANSPORT_AUTHENTICATION_MODE_TAG
{
	AMQP_TRANSPORT_AUTHENTICATION_MODE_NOT_SET,
	AMQP_TRANSPORT_AUTHENTICATION_MODE_CBS,
	AMQP_TRANSPORT_AUTHENTICATION_MODE_X509
} AMQP_TRANSPORT_AUTHENTICATION_MODE;

typedef struct AMQP_TRANSPORT_INSTANCE_TAG
{
    STRING_HANDLE iothub_host_fqdn;                                     // FQDN of the IoT Hub.
    XIO_HANDLE tls_io;                                                  // TSL I/O transport.
    AMQP_GET_IO_TRANSPORT underlying_io_transport_provider;             // Pointer to the function that creates the TLS I/O (internal use only).
	AMQP_CONNECTION_HANDLE amqp_connection;                             // Base amqp connection with service.
	AMQP_CONNECTION_STATE amqp_connection_state;                        // Current state of the amqp_connection.
	AMQP_TRANSPORT_AUTHENTICATION_MODE preferred_authentication_mode;   // Used to avoid registered devices using different authentication modes.
	SINGLYLINKEDLIST_HANDLE registered_devices;                         // List of devices currently registered in this transport.
    bool is_trace_on;                                                   // Turns logging on and off.
    OPTIONHANDLER_HANDLE xioOptions;                                    /*here are the options from the xio layer if any is saved*/
	bool is_connection_retry_required;                                  // Flag that controls whether the connection should be restablished or not.
} AMQP_TRANSPORT_INSTANCE;

typedef struct AMQP_TRANSPORT_DEVICE_INSTANCE_TAG
{
    STRING_HANDLE device_id;                                             // Identity of the device.
	DEVICE_HANDLE device_handle;                                        // Logic unit that performs authentication, messaging, etc.
    IOTHUB_CLIENT_LL_HANDLE iothub_client_handle;                       // Saved reference to the IoTHub LL Client.
    AMQP_TRANSPORT_INSTANCE* transport_state;                           // Saved reference to the transport the device is registered on.
	PDLIST_ENTRY waiting_to_send;                                         // List of events waiting to be sent to the iot hub (i.e., haven't been processed by the transport yet).
	DEVICE_STATE device_state;                                          // Current state of the device_handle instance.
	size_t number_of_previous_failures;                                 // Number of times the device has failed in sequence; this value is reset to 0 if device succeeds to authenticate, send and/or recv messages.
	time_t time_of_last_state_change;                                   // Time the device_handle last changed state; used to track timeouts of device_start_async and device_stop.
	size_t max_state_change_timeout_secs;                               // Maximum number of seconds allowed for device_handle to complete start and stop state changes.
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
    // the methods portion
    IOTHUBTRANSPORT_AMQP_METHODS_HANDLE methods_handle;
    // is subscription for methods needed?
    int subscribe_methods_needed : 1;
    // is the transport subscribed for methods?
    int subscribed_for_methods : 1;
#endif
} AMQP_TRANSPORT_DEVICE_INSTANCE;


// ---------- General Helpers ---------- //

// @brief
//     Evaluates if the ammount of time since start_time is greater or lesser than timeout_in_secs.
// @param is_timed_out
//     Set to 1 if a timeout has been reached, 0 otherwise. Not set if any failure occurs.
// @returns
//     0 if no failures occur, non-zero otherwise.
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

static STRING_HANDLE get_target_iothub_fqdn(const IOTHUBTRANSPORT_CONFIG* config)
{
	STRING_HANDLE fqdn;

	if (config->upperConfig->protocolGatewayHostName == NULL)
	{
		if ((fqdn = STRING_new()) == NULL)
		{
			LogError("Failed to copy iotHubName and iotHubSuffix (STRING_new failed)");
		}
		else if (STRING_sprintf(fqdn, "%s.%s", config->upperConfig->iotHubName, config->upperConfig->iotHubSuffix) != RESULT_OK)
		{
			LogError("Failed to copy iotHubName and iotHubSuffix (STRING_sprintf failed)");
			STRING_delete(fqdn);
			fqdn = NULL;
		}
	}
	else if ((fqdn = STRING_construct(config->upperConfig->protocolGatewayHostName)) == NULL)
	{
		LogError("Failed to copy protocolGatewayHostName (STRING_construct failed)");
	}

	return fqdn;
}


// ---------- Register/Unregister Helpers ---------- //

static void destroy_registered_device_instance(AMQP_TRANSPORT_DEVICE_INSTANCE *trdev_inst)
{
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
	if (trdev_inst->methods_handle != NULL)
	{
		iothubtransportamqp_methods_destroy(trdev_inst->methods_handle);
	}
#endif
	if (trdev_inst->device_handle != NULL)
	{
		device_destroy(trdev_inst->device_handle);
	}

	if (trdev_inst->device_id != NULL)
	{
		STRING_delete(trdev_inst->device_id);
	}

	free(trdev_inst);
}

// @brief
//     Saves the new state, if it is different than the previous one.
static void on_device_state_changed_callback(void* context, DEVICE_STATE previous_state, DEVICE_STATE new_state)
{
	if (context != NULL && new_state != previous_state)
	{
		AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device = (AMQP_TRANSPORT_DEVICE_INSTANCE*)context;
		registered_device->device_state = new_state;
		registered_device->time_of_last_state_change = get_time(NULL);
	}
}

// @brief    Auxiliary function to be used to find a device in the registered_devices list.
// @returns  true if the device ids match, false otherwise.
static bool find_device_by_id(LIST_ITEM_HANDLE list_item, const void* match_context)
{
	bool result;

	if (match_context == NULL)
	{
		result = false;
	}
	else
	{
		AMQP_TRANSPORT_DEVICE_INSTANCE* device_instance = (AMQP_TRANSPORT_DEVICE_INSTANCE*)singlylinkedlist_item_get_value(list_item);

		if (device_instance == NULL || 
			device_instance->device_id == NULL || 
			STRING_c_str(device_instance->device_id) != match_context)
		{
			result = false;
		}
		else
		{
			result = true;
		}
	}

	return result;
}

// @brief       Verifies if a device is already registered within the transport that owns the list of registered devices.
// @returns     1 if the device is already in the list, 0 otherwise.
static int is_device_registered(SINGLYLINKEDLIST_HANDLE registered_devices, const char* device_id, LIST_ITEM_HANDLE *list_item)
{
	return ((*list_item = singlylinkedlist_find(registered_devices, find_device_by_id, device_id)) != NULL ? 1 : 0);
}

static DEVICE_MESSAGE_DISPOSITION_RESULT on_message_received(IOTHUB_MESSAGE_HANDLE iothub_message, void* context)
{
	AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device = (AMQP_TRANSPORT_DEVICE_INSTANCE*)context;

	DEVICE_MESSAGE_DISPOSITION_RESULT dev_disp_result;
    IOTHUBMESSAGE_DISPOSITION_RESULT iothc_disp_result;

    iothc_disp_result = IoTHubClient_LL_MessageCallback(registered_device->iothub_client_handle, iothub_message);

    if (iothc_disp_result == IOTHUBMESSAGE_ACCEPTED)
    {
		dev_disp_result = DEVICE_MESSAGE_DISPOSITION_RESULT_ACCEPTED;
	}
    else if (iothc_disp_result == IOTHUBMESSAGE_ABANDONED)
    {
		dev_disp_result = DEVICE_MESSAGE_DISPOSITION_RESULT_ABANDONED;
	}
    else // i.e, if (iothc_disp_result == IOTHUBMESSAGE_REJECTED)
    {
		dev_disp_result = DEVICE_MESSAGE_DISPOSITION_RESULT_REJECTED;
	}

    return dev_disp_result;
}


#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
static void on_methods_error(void* context)
{
    /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_030: [ `on_methods_error` shall do nothing. ]*/
    (void)context;
}

static int on_method_request_received(void* context, const char* method_name, const unsigned char* request, size_t request_size, IOTHUBTRANSPORT_AMQP_METHOD_HANDLE method_handle)
{
    int result;
    AMQP_TRANSPORT_DEVICE_INSTANCE* device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)context;

    /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_017: [ `on_methods_request_received` shall call the `IoTHubClient_LL_DeviceMethodComplete` passing the method name, request buffer and size and the newly created BUFFER handle. ]*/
    /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_022: [ The status code shall be the return value of the call to `IoTHubClient_LL_DeviceMethodComplete`. ]*/
    if (IoTHubClient_LL_DeviceMethodComplete(device_state->iothub_client_handle, method_name, request, request_size, (void*)method_handle) != 0)
    {
        LogError("Failure: IoTHubClient_LL_DeviceMethodComplete");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

static int subscribe_methods(AMQP_TRANSPORT_DEVICE_INSTANCE* deviceState)
{
    int result;

    if (deviceState->subscribe_methods_needed == 0)
    {
        result = 0;
    }
    else
    {
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_024: [ If the device authentication status is AUTHENTICATION_STATUS_OK and `IoTHubTransport_AMQP_Common_Subscribe_DeviceMethod` was called to register for methods, `IoTHubTransport_AMQP_Common_DoWork` shall call `iothubtransportamqp_methods_subscribe`. ]*/
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_027: [ The current session handle shall be passed to `iothubtransportamqp_methods_subscribe`. ]*/
        if (iothubtransportamqp_methods_subscribe(deviceState->methods_handle, deviceState->transport_state->session, on_methods_error, deviceState, on_method_request_received, deviceState) != 0)
        {
            LogError("Cannot subscribe for methods");
            result = __LINE__;
        }
        else
        {
            deviceState->subscribed_for_methods = 1;
            result = 0;
        }
    }

    return result;
}
#endif

// @brief    Destroys the XIO_HANDLE obtained with underlying_io_transport_provider(), saving its options beforehand.
static void destroy_underlying_io_transport(AMQP_TRANSPORT_INSTANCE* transport_state)
{
	if (transport_state->tls_io != NULL)
	{
		xio_destroy(transport_state->tls_io);
		transport_state->tls_io = NULL;
	}
}

// @brief    Invokes underlying_io_transport_provider() and retrieves a new XIO_HANDLE to use for I/O (TLS, or websockets, or w/e is supported).
// @param    xio_handle: if successfull, set with the new XIO_HANDLE acquired; not changed otherwise.
// @returns  0 if successfull, non-zero otherwise.
static int get_new_underlying_io_transport(AMQP_TRANSPORT_INSTANCE* transport_instance, XIO_HANDLE *xio_handle)
{
	int result;

	if ((*xio_handle = transport_instance->underlying_io_transport_provider(STRING_c_str(transport_instance->iothub_host_fqdn))) == NULL)
	{
		LogError("Failed to obtain a TLS I/O transport layer (underlying_io_transport_provider() failed)");
		result = __LINE__;
	}
	else
	{
		if (transport_instance->xioOptions != NULL)
		{
			if (OptionHandler_FeedOptions(transport_instance->xioOptions, *xio_handle) != 0)
			{
				LogError("Failed feeding existing options to new TLS instance."); /*pessimistically hope TLS will fail, be recreated and options re-given*/
			}
		}

		result = RESULT_OK;
	}

	return result;
}

static void on_amqp_connection_state_changed(const void* context, AMQP_CONNECTION_STATE old_state, AMQP_CONNECTION_STATE new_state)
{
	if (context != NULL && new_state != old_state)
	{
		AMQP_TRANSPORT_INSTANCE* transport_instance = (AMQP_TRANSPORT_INSTANCE*)context;

		transport_instance->amqp_connection_state = new_state;

		if (new_state == AMQP_CONNECTION_STATE_ERROR)
		{
			LogError("Transport received an ERROR from the amqp_connection (state changed %d->%d); it will be flagged for connection retry.", old_state, new_state);

			transport_instance->is_connection_retry_required = true;
		}
	}
}

static int establish_amqp_connection(AMQP_TRANSPORT_INSTANCE* transport_instance)
{
    int result;

	if (transport_instance->tls_io == NULL &&
		get_new_underlying_io_transport(transport_instance, &transport_instance->tls_io) != RESULT_OK)
	{
		LogError("Failed establishing connection (failed to obtain a TLS I/O transport layer).");
		result = __LINE__;
	}
	else
	{
		AMQP_CONNECTION_CONFIG amqp_connection_config;
		amqp_connection_config.iothub_host_fqdn = STRING_c_str(transport_instance->iothub_host_fqdn);
		amqp_connection_config.underlying_io_transport = transport_instance->tls_io;
		amqp_connection_config.is_trace_on = true; // transport_instance->is_trace_on;
		amqp_connection_config.on_state_changed_callback = on_amqp_connection_state_changed;
		amqp_connection_config.on_state_changed_context = transport_instance;

		if (transport_instance->preferred_authentication_mode == AMQP_TRANSPORT_AUTHENTICATION_MODE_CBS)
		{
			amqp_connection_config.create_sasl_io = true;
			amqp_connection_config.create_cbs_connection = true;
		}
		else if (transport_instance->preferred_authentication_mode == AMQP_TRANSPORT_AUTHENTICATION_MODE_X509)
		{
			amqp_connection_config.create_sasl_io = false;
			amqp_connection_config.create_cbs_connection = false;
		}
		// If new AMQP_TRANSPORT_AUTHENTICATION_MODE values are added, they need to be covered here.
		// This function is not supposed to be called if AMQP_TRANSPORT_AUTHENTICATION_MODE_NOT_SET.

		if ((transport_instance->amqp_connection = amqp_connection_create(&amqp_connection_config)) == NULL)
		{
			LogError("Failed establishing connection (failed to create the amqp_connection instance).");
			result = __LINE__;
		}
		else
		{
			transport_instance->amqp_connection_state = AMQP_CONNECTION_STATE_CLOSED;
			result = RESULT_OK;
		}
	}

    return result;
}

static void prepare_device_for_connection_retry(AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device)
{
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
    iothubtransportamqp_methods_unsubscribe(device_state->methods_handle);
    device_state->subscribed_for_methods = 0;
#endif

	device_stop(registered_device->device_handle);
}

static void prepare_for_connection_retry(AMQP_TRANSPORT_INSTANCE* transport_instance)
{
	LIST_ITEM_HANDLE list_item = singlylinkedlist_get_head_item(transport_instance->registered_devices);

	while (list_item != NULL)
	{
		AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device = (AMQP_TRANSPORT_DEVICE_INSTANCE*)singlylinkedlist_item_get_value(list_item);

		if (registered_device != NULL)
		{
			prepare_device_for_connection_retry(registered_device);
		}

		list_item = singlylinkedlist_get_next_item(list_item);
	}

    amqp_connection_destroy(transport_instance->amqp_connection);
    transport_instance->amqp_connection_state = AMQP_CONNECTION_STATE_CLOSED;
}

// @brief    Verifies if the crendentials used by the device match the requirements and authentication mode currently supported by the transport.
// @returns  1 if credentials are good, 0 otherwise.
static int is_device_credential_acceptable(const IOTHUB_DEVICE_CONFIG* device_config, AMQP_TRANSPORT_AUTHENTICATION_MODE preferred_authentication_mode)
{
    int result;

	// Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_03_003: [IoTHubTransport_AMQP_Common_Register shall return NULL if both deviceKey and deviceSasToken are not NULL.]
	if ((device_config->deviceSasToken != NULL) && (device_config->deviceKey != NULL))
	{
		LogError("Credential of device '%s' is not acceptable (must provide EITHER deviceSasToken OR deviceKey)", device_config->deviceId);
		result = 0;
	}
    else if (preferred_authentication_mode == AMQP_TRANSPORT_AUTHENTICATION_MODE_NOT_SET)
    {
        result = 1;
    }
    else if (preferred_authentication_mode == AMQP_TRANSPORT_AUTHENTICATION_MODE_X509 && (device_config->deviceKey != NULL || device_config->deviceSasToken != NULL))
    {
		LogError("Credential of device '%s' is not acceptable (transport is using X509 certificate authentication, but device config contains deviceKey or sasToken)", device_config->deviceId);
        result = 0;
    }
    else if (preferred_authentication_mode != AMQP_TRANSPORT_AUTHENTICATION_MODE_X509 && (device_config->deviceKey == NULL && device_config->deviceSasToken == NULL))
    {
		LogError("Credential of device '%s' is not acceptable (transport is using CBS authentication, but device config does not contain deviceKey nor sasToken)", device_config->deviceId);
        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
}



//---------- DoWork Helpers ----------//

static IOTHUB_MESSAGE_LIST* get_next_event_to_send(AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device)
{
	IOTHUB_MESSAGE_LIST* message;

	if (!DList_IsListEmpty(registered_device->waiting_to_send))
	{
		PDLIST_ENTRY list_entry = registered_device->waiting_to_send->Flink;
		message = containingRecord(list_entry, IOTHUB_MESSAGE_LIST, entry);
		(void)DList_RemoveEntryList(list_entry);
	}
	else
	{
		message = NULL;
	}

	return message;
}

// @brief    "Parses" the D2C_EVENT_SEND_RESULT (from iothubtransport_amqp_device module) into a IOTHUB_CLIENT_CONFIRMATION_RESULT.
static IOTHUB_CLIENT_CONFIRMATION_RESULT get_iothub_client_confirmation_result_from(D2C_EVENT_SEND_RESULT result)
{
	IOTHUB_CLIENT_CONFIRMATION_RESULT iothub_send_result;

	switch (result)
	{
		case D2C_EVENT_SEND_COMPLETE_RESULT_OK:
			iothub_send_result = IOTHUB_CLIENT_CONFIRMATION_OK;
			break;
		case D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_CANNOT_PARSE:
		case D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING:
			iothub_send_result = IOTHUB_CLIENT_CONFIRMATION_ERROR;
			break;
		case D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_TIMEOUT:
			iothub_send_result = IOTHUB_CLIENT_CONFIRMATION_MESSAGE_TIMEOUT;
			break;
		case D2C_EVENT_SEND_COMPLETE_RESULT_DEVICE_DESTROYED:
			iothub_send_result = IOTHUB_CLIENT_CONFIRMATION_BECAUSE_DESTROY;
			break;
		case D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_UNKNOWN:
		default:
			iothub_send_result = IOTHUB_CLIENT_CONFIRMATION_ERROR;
			break;
	}

	return iothub_send_result;
}

// @brief
//     Callback function for device_send_event_async.
static void on_event_send_complete(IOTHUB_MESSAGE_LIST* message, D2C_EVENT_SEND_RESULT result, void* context)
{
	(void)context;
	IOTHUB_CLIENT_CONFIRMATION_RESULT iothub_send_result = get_iothub_client_confirmation_result_from(result);

	if (message->callback != NULL)
	{
		message->callback(iothub_send_result, message->context);
	}

	IoTHubMessage_Destroy(message->messageHandle);
	free(message);
}

// @brief
//     Gets events from wait to send list and sends to service in the order they were added.
// @returns
//     0 if all events could be sent to the next layer successfully, non-zero otherwise.
static int send_pending_events(AMQP_TRANSPORT_DEVICE_INSTANCE* device_state)
{
	int result;
	IOTHUB_MESSAGE_LIST* message;

	result = RESULT_OK;

	while ((message = get_next_event_to_send(device_state)) != NULL)
	{
		if (device_send_event_async(device_state->device_handle, message, on_event_send_complete, device_state) != RESULT_OK)
		{
			LogError("Device '%s' failed to send message (device_send_event_async failed)", STRING_c_str(device_state->device_id));
			result = __LINE__;
		}

		if (result != RESULT_OK)
		{
			on_event_send_complete(message, D2C_EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING, device_state);
			
			break;
		}
	}

	return result;
}

// @brief
//     Auxiliary function for the public DoWork API, performing DoWork activities (authenticate, messaging) for a specific device.
// @requires
//     The transport to have a valid instance of AMQP_CONNECTION (from which to obtain SESSION_HANDLE and CBS_HANDLE)
// @returns
//     0 if no errors occur, non-zero otherwise.
static int IoTHubTransport_AMQP_Common_Device_DoWork(AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device)
{
	int result;

	if (registered_device->device_state != DEVICE_STATE_STARTED)
	{
		if (registered_device->device_state == DEVICE_STATE_STOPPED)
		{
			SESSION_HANDLE session_handle;
			CBS_HANDLE cbs_handle = NULL;

			if (amqp_connection_get_session_handle(registered_device->transport_state->amqp_connection, &session_handle) != RESULT_OK)
			{
				LogError("Failed performing DoWork for device '%s' (failed to get the amqp_connection session_handle)", STRING_c_str(registered_device->device_id));
				result = __LINE__;
			}
			else if (registered_device->transport_state->preferred_authentication_mode == AMQP_TRANSPORT_AUTHENTICATION_MODE_CBS &&
				amqp_connection_get_cbs_handle(registered_device->transport_state->amqp_connection, &cbs_handle) != RESULT_OK)
			{
				LogError("Failed performing DoWork for device '%s' (failed to get the amqp_connection cbs_handle)", STRING_c_str(registered_device->device_id));
				result = __LINE__;
			}
			else if (device_start_async(registered_device->device_handle, session_handle, cbs_handle) != RESULT_OK)
			{
				LogError("Failed performing DoWork for device '%s' (failed to start device)", STRING_c_str(registered_device->device_id));
				result = __LINE__;
			}
			else
			{
				result = RESULT_OK;
			}
		}
		else if (registered_device->device_state == DEVICE_STATE_STARTING ||
                 registered_device->device_state == DEVICE_STATE_STOPPING)
		{
			int is_timed_out;
			if (is_timeout_reached(registered_device->time_of_last_state_change, registered_device->max_state_change_timeout_secs, &is_timed_out) != RESULT_OK)
			{
				LogError("Failed performing DoWork for device '%s' (failed tracking timeout of device %d state)", STRING_c_str(registered_device->device_id), registered_device->device_state);
				result = __LINE__;
			}
			else if (is_timed_out == 1)
			{
				LogError("Failed performing DoWork for device '%s' (device failed to start or stop within expected timeout)", STRING_c_str(registered_device->device_id));
				result = __LINE__;
			}
			else
			{
				result = RESULT_OK;
			}
		}
		else // i.e., DEVICE_STATE_ERROR_AUTH || DEVICE_STATE_ERROR_AUTH_TIMEOUT || DEVICE_STATE_ERROR_MSG
		{
			LogError("Failed performing DoWork for device '%s' (device reported state %d; number of previous failures: %d)", 
				STRING_c_str(registered_device->device_id), registered_device->device_state, registered_device->number_of_previous_failures);

			registered_device->number_of_previous_failures++;

			if (registered_device->number_of_previous_failures >= MAX_NUMBER_OF_DEVICE_FAILURES)
			{
				result = __LINE__;
			}
			else if (device_stop(registered_device->device_handle) != RESULT_OK)
			{
				LogError("Failed to stop reset device '%s' (device_stop failed)", STRING_c_str(registered_device->device_id));
				result = __LINE__;
			}
			else
			{
				result = RESULT_OK;
			}
		}
	}
	else
	{
		// Event send timeouts are handled by the lower layer (device module).

		registered_device->number_of_previous_failures = 0;

		if (send_pending_events(registered_device) != RESULT_OK)
		{
			LogError("Failed performing DoWork for device '%s' (failed sending pending events)", STRING_c_str(registered_device->device_id));
			result = __LINE__;
		}
		else
		{
			result = RESULT_OK;
		}
	}

	// No harm in invoking this as API will simply exit if the state is not "started".
	device_do_work(registered_device->device_handle); 

	return result;
}


//---------- API functions ----------//

TRANSPORT_LL_HANDLE IoTHubTransport_AMQP_Common_Create(const IOTHUBTRANSPORT_CONFIG* config, AMQP_GET_IO_TRANSPORT get_io_transport)
{
    AMQP_TRANSPORT_INSTANCE* instance;

	if (config == NULL || config->upperConfig == NULL)
    {
        LogError("IoTHub AMQP client transport null configuration parameter.");
		instance = NULL;
    }
	else if (config->upperConfig->protocol == NULL)
    {
        LogError("Invalid configuration (NULL protocol detected)");
		instance = NULL;
	}
    else if (config->upperConfig->iotHubName == NULL)
    {
        LogError("Invalid configuration (NULL iotHubName detected)");
		instance = NULL;
	}
    else if (config->upperConfig->iotHubSuffix == NULL)
    {
        LogError("Invalid configuration (NULL iotHubSuffix detected)");
		instance = NULL;
	}
	else if (get_io_transport == NULL)
    {
        LogError("Invalid configuration (get_io_transport is NULL)");
		instance = NULL;
	}
    else
    {
		if ((instance = (AMQP_TRANSPORT_INSTANCE*)malloc(sizeof(AMQP_TRANSPORT_INSTANCE))) == NULL)
        {
            LogError("Could not allocate AMQP transport state (malloc failed)");
        }
        else
        {
            bool cleanup_required;
			memset(instance, 0, sizeof(AMQP_TRANSPORT_INSTANCE));

			if ((instance->iothub_host_fqdn = get_target_iothub_fqdn(config)) == NULL)
			{
				LogError("Failed to obtain the iothub target fqdn.");
				cleanup_required = true;
			}
            else if ((instance->registered_devices = singlylinkedlist_create()) == NULL)
            {
                LogError("Failed to initialize the internal list of registered devices (singlylinkedlist_create failed)");
				cleanup_required = true;
            }
			else
			{
				instance->underlying_io_transport_provider = get_io_transport;
				instance->preferred_authentication_mode = AMQP_TRANSPORT_AUTHENTICATION_MODE_NOT_SET;
				cleanup_required = false;
			}

            if (cleanup_required)
            {
				if (instance->iothub_host_fqdn != NULL)
				{
					STRING_delete(instance->iothub_host_fqdn);
				}

				if (instance->registered_devices != NULL)
				{
					singlylinkedlist_destroy(instance->registered_devices);
				}

                free(instance);
				instance = NULL;
            }
        }
    }

	// Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_011: [If IoTHubTransport_AMQP_Common_Create succeeds it shall return a pointer to `instance`.]
    return instance;
}

IOTHUB_PROCESS_ITEM_RESULT IoTHubTransport_AMQP_Common_ProcessItem(TRANSPORT_LL_HANDLE handle, IOTHUB_IDENTITY_TYPE item_type, IOTHUB_IDENTITY_INFO* iothub_item)
{
    (void)handle;
    (void)item_type;
    (void)iothub_item;
    LogError("Currently Not Supported.");
    return IOTHUB_PROCESS_ERROR;
}

void IoTHubTransport_AMQP_Common_DoWork(TRANSPORT_LL_HANDLE handle, IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle)
{
    (void)iotHubClientHandle; // unused as of now.

    if (handle == NULL)
    {
        LogError("IoTHubClient DoWork failed: transport handle parameter is NULL.");
    } 
    else
    {
        bool trigger_connection_retry = false;
        AMQP_TRANSPORT_INSTANCE* transport_state = (AMQP_TRANSPORT_INSTANCE*)handle;
		LIST_ITEM_HANDLE list_item;

		// We need to check if there are devices, otherwise the amqp_connection won't be able to be created since
		// there is not a preferred authentication mode set yet on the transport.
		if ((list_item = singlylinkedlist_get_head_item(transport_state->registered_devices)) != NULL)
		{
			if (transport_state->is_connection_retry_required)
			{
				LogError("An error occured on AMQP connection. The connection will be restablished.");
				
				prepare_for_connection_retry(transport_state);
			}
			else if (transport_state->amqp_connection == NULL && establish_amqp_connection(transport_state) != RESULT_OK)
			{
				LogError("AMQP transport failed to establish connection with service.");
				trigger_connection_retry = true;
			}
			else if (transport_state->amqp_connection_state == AMQP_CONNECTION_STATE_OPENED)
			{
				while (list_item != NULL)
				{
					AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device;

					if ((registered_device = (AMQP_TRANSPORT_DEVICE_INSTANCE*)singlylinkedlist_item_get_value(list_item)) == NULL)
					{
						LogError("Transport had an unexpected failure during DoWork (failed to fetch a registered_devices list item value)");
					}
					else if (IoTHubTransport_AMQP_Common_Device_DoWork(registered_device) != RESULT_OK)
					{
						if (registered_device->number_of_previous_failures >= MAX_NUMBER_OF_DEVICE_FAILURES)
						{
							LogError("Device '%s' reported a critical failure; connection retry will be triggered.");

							transport_state->is_connection_retry_required = true;
						}
					}

					list_item = singlylinkedlist_get_next_item(list_item);
				}
			}
		}

        if (transport_state->amqp_connection != NULL)
        {
			amqp_connection_do_work(transport_state->amqp_connection);
        }
    }
}

int IoTHubTransport_AMQP_Common_Subscribe(IOTHUB_DEVICE_HANDLE handle)
{
    int result;

    if (handle == NULL)
    {
        LogError("Invalid handle to IoTHubClient AMQP transport device handle.");
        result = __LINE__;
    }
    else
    {
        AMQP_TRANSPORT_DEVICE_INSTANCE* amqp_device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)handle;

		if (device_subscribe_message(amqp_device_state->device_handle, on_message_received, amqp_device_state) != RESULT_OK)
		{
			LogError("Failed subscribing to cloud-to-device messages (device_subscribe_message failed)");
			result = __LINE__;
		}
		else
		{
			result = RESULT_OK;
		}
    }

    return result;
}

void IoTHubTransport_AMQP_Common_Unsubscribe(IOTHUB_DEVICE_HANDLE handle)
{
    if (handle == NULL)
    {
        LogError("Invalid handle to IoTHubClient AMQP transport device handle.");
    }
    else
    {
        AMQP_TRANSPORT_DEVICE_INSTANCE* amqp_device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)handle;
        
		if (device_unsubscribe_message(amqp_device_state->device_handle) != RESULT_OK)
		{
			LogError("Failed unsubscribing to cloud-to-device messages (device_unsubscribe_message failed)");
		}
    }
}

int IoTHubTransport_AMQP_Common_Subscribe_DeviceTwin(IOTHUB_DEVICE_HANDLE handle)
{
    (void)handle;
    /*Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_02_009: [ IoTHubTransport_AMQP_Common_Subscribe_DeviceTwin shall return a non-zero value. ]*/
    int result = __LINE__;
    LogError("IoTHubTransport_AMQP_Common_Subscribe_DeviceTwin Not supported");
    return result;
}

void IoTHubTransport_AMQP_Common_Unsubscribe_DeviceTwin(IOTHUB_DEVICE_HANDLE handle)
{
    (void)handle;
    /*Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_02_010: [ IoTHubTransport_AMQP_Common_Unsubscribe_DeviceTwin shall return. ]*/
    LogError("IoTHubTransport_AMQP_Common_Unsubscribe_DeviceTwin Not supported");
}

int IoTHubTransport_AMQP_Common_Subscribe_DeviceMethod(IOTHUB_DEVICE_HANDLE handle)
{
    int result;

    if (handle == NULL)
    {
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_004: [ If `handle` is NULL, `IoTHubTransport_AMQP_Common_Subscribe_DeviceMethod` shall fail and return a non-zero value. ] */
        LogError("NULL handle");
        result = __LINE__;
    }
    else
    {
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
        AMQP_TRANSPORT_DEVICE_INSTANCE* device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)handle;
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_026: [ `IoTHubTransport_AMQP_Common_Subscribe_DeviceMethod` shall remember that a subscribe is to be performed in the next call to DoWork and on success it shall return 0. ]*/
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_005: [ If the transport is already subscribed to receive C2D method requests, `IoTHubTransport_AMQP_Common_Subscribe_DeviceMethod` shall perform no additional action and return 0. ]*/
        device_state->subscribe_methods_needed = 1;
        result = 0;
#else
        LogError("Not implemented");
        result = __LINE__;
#endif
    }

    return result;
}

void IoTHubTransport_AMQP_Common_Unsubscribe_DeviceMethod(IOTHUB_DEVICE_HANDLE handle)
{
    if (handle == NULL)
    {
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_006: [ If `handle` is NULL, `IoTHubTransport_AMQP_Common_Unsubscribe_DeviceMethod` shall do nothing. ]*/
        LogError("NULL handle");
    }
    else
    {
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
        AMQP_TRANSPORT_DEVICE_INSTANCE* device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)handle;

        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_008: [ If the transport is not subscribed to receive C2D method requests then `IoTHubTransport_AMQP_Common_Unsubscribe_DeviceMethod` shall do nothing. ]*/
        if (device_state->subscribe_methods_needed != 0)
        {
            /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_007: [ `IoTHubTransport_AMQP_Common_Unsubscribe_DeviceMethod` shall unsubscribe from receiving C2D method requests by calling `iothubtransportamqp_methods_unsubscribe`. ]*/
            device_state->subscribe_methods_needed = 0;
            iothubtransportamqp_methods_unsubscribe(device_state->methods_handle);
        }
#else
        LogError("Not implemented");
#endif
    }
}

int IoTHubTransport_AMQP_Common_DeviceMethod_Response(IOTHUB_DEVICE_HANDLE handle, METHOD_HANDLE methodId, const unsigned char* response, size_t response_size, int status_response)
{
    (void)response;
    (void)response_size;
    (void)status_response;
    (void)methodId;
    int result;
    AMQP_TRANSPORT_DEVICE_INSTANCE* device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)handle;
    if (device_state != NULL)
    {
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
        IOTHUBTRANSPORT_AMQP_METHOD_HANDLE saved_handle = (IOTHUBTRANSPORT_AMQP_METHOD_HANDLE)methodId;
        /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_019: [ `IoTHubTransport_AMQP_Common_DeviceMethod_Response` shall call `iothubtransportamqp_methods_respond` passing to it the `method_handle` argument, the response bytes, response size and the status code. ]*/
        if (iothubtransportamqp_methods_respond(saved_handle, response, response_size, status_response) != 0)
        {
            /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_029: [ If `iothubtransportamqp_methods_respond` fails, `on_methods_request_received` shall return a non-zero value. ]*/
            LogError("iothubtransportamqp_methods_respond failed");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
#else
        result = 0;
        LogError("Not implemented");
#endif
    }
    else
    {
        result = __LINE__;
    }
    return result;
}

IOTHUB_CLIENT_RESULT IoTHubTransport_AMQP_Common_GetSendStatus(IOTHUB_DEVICE_HANDLE handle, IOTHUB_CLIENT_STATUS *iotHubClientStatus)
{
    IOTHUB_CLIENT_RESULT result;

    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_041: [IoTHubTransport_AMQP_Common_GetSendStatus shall return IOTHUB_CLIENT_INVALID_ARG if called with NULL parameter.]
    if (handle == NULL)
    {
        result = IOTHUB_CLIENT_INVALID_ARG;
        LogError("Invalid handle to IoTHubClient AMQP transport instance.");
    }
    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_041: [IoTHubTransport_AMQP_Common_GetSendStatus shall return IOTHUB_CLIENT_INVALID_ARG if called with NULL parameter.]
    else if (iotHubClientStatus == NULL)
    {
        result = IOTHUB_CLIENT_INVALID_ARG;
        LogError("Invalid pointer to output parameter IOTHUB_CLIENT_STATUS.");
    }
    else
    {
        AMQP_TRANSPORT_DEVICE_INSTANCE* amqp_device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)handle;

		DEVICE_SEND_STATUS device_send_status;
		if (device_get_send_status(amqp_device_state->device_handle, &device_send_status) != RESULT_OK)
		{
			LogError("Failed retrieving the device send status (device_get_send_status failed)");
			result = IOTHUB_CLIENT_ERROR;
		}
		else
		{
			if (device_send_status == DEVICE_SEND_STATUS_BUSY)
			{
				*iotHubClientStatus = IOTHUB_CLIENT_SEND_STATUS_BUSY;
			}
			else // DEVICE_SEND_STATUS_IDLE
			{
				*iotHubClientStatus = IOTHUB_CLIENT_SEND_STATUS_IDLE;
			}

			result = IOTHUB_CLIENT_OK;
		}

    }

    return result;
}

IOTHUB_CLIENT_RESULT IoTHubTransport_AMQP_Common_SetOption(TRANSPORT_LL_HANDLE handle, const char* option, const void* value)
{
    IOTHUB_CLIENT_RESULT result;

    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_044: [If handle parameter is NULL then IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_INVALID_ARG.]
    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_045: [If parameter optionName is NULL then IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_INVALID_ARG.] 
    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_046: [If parameter value is NULL then IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_INVALID_ARG.]
    if (
        (handle == NULL) ||
        (option == NULL) ||
        (value == NULL)
        )
    {
        result = IOTHUB_CLIENT_INVALID_ARG;
        LogError("Invalid parameter (NULL) passed to AMQP transport SetOption()");
    }
    else
    {
   //     AMQP_TRANSPORT_INSTANCE* transport_state = (AMQP_TRANSPORT_INSTANCE*)handle;

   //     // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_048: [IotHubTransportAMQP_SetOption shall save and apply the value if the option name is "sas_token_lifetime", returning IOTHUB_CLIENT_OK] 
   //     if (strcmp(OPTION_SAS_TOKEN_LIFETIME, option) == 0)
   //     {
			//// TODO: fill it up
   //         result = IOTHUB_CLIENT_OK;
   //     }
   //     // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_049: [IotHubTransportAMQP_SetOption shall save and apply the value if the option name is "sas_token_refresh_time", returning IOTHUB_CLIENT_OK] 
   //     else if (strcmp(OPTION_SAS_TOKEN_REFRESH_TIME, option) == 0)
   //     {
			//// TODO: fill it up
			//result = IOTHUB_CLIENT_OK;
   //     }
   //     // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_148: [IotHubTransportAMQP_SetOption shall save and apply the value if the option name is "cbs_request_timeout", returning IOTHUB_CLIENT_OK] 
   //     else if (strcmp(OPTION_CBS_REQUEST_TIMEOUT, option) == 0)
   //     {
			//// TODO: fill it up
			//result = IOTHUB_CLIENT_OK;
   //     }
   //     else if (strcmp(OPTION_LOG_TRACE, option) == 0)
   //     {
   //         // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_198: [If `optionName` is `logtrace`, IoTHubTransport_AMQP_Common_SetOption shall save the value on the transport instance.]
   //         transport_state->is_trace_on = *((bool*)value);

			//// TODO: fill it up

			//result = IOTHUB_CLIENT_OK;

   //         //if (transport_state->connection != NULL)
   //         //{
   //         //    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_202: [If `optionName` is `logtrace`, IoTHubTransport_AMQP_Common_SetOption shall apply it using connection_set_trace() to current connection instance if it exists and return IOTHUB_CLIENT_OK.]
   //         //    connection_set_trace(transport_state->connection, transport_state->is_trace_on);
   //         //}

   //         //// Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_203: [If `optionName` is `logtrace`, IoTHubTransport_AMQP_Common_SetOption shall apply it using xio_setoption() to current SASL IO instance if it exists.]
   //         //if (transport_state->cbs_connection.sasl_io != NULL &&
   //         //    xio_setoption(transport_state->cbs_connection.sasl_io, OPTION_LOG_TRACE, &transport_state->is_trace_on) != RESULT_OK)
   //         //{
   //         //    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_204: [If xio_setoption() fails, IoTHubTransport_AMQP_Common_SetOption shall fail and return IOTHUB_CLIENT_ERROR.]
   //         //    LogError("IoTHubTransport_AMQP_Common_SetOption failed (xio_setoption failed to set logging on SASL IO)");
   //         //    result = IOTHUB_CLIENT_ERROR;
   //         //}
   //         //else
   //         //{
   //         //    // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_205: [If xio_setoption() succeeds, IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_OK.]
   //         //    result = IOTHUB_CLIENT_OK;
   //         //}
   //     }
   //     // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_047: [If the option name does not match one of the options handled by this module, IoTHubTransport_AMQP_Common_SetOption shall pass the value and name to the XIO using xio_setoption().] 
   //     else
   //     {
   //         result = IOTHUB_CLIENT_OK;

   //         // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_02_007: [ If optionName is x509certificate and the authentication method is not x509 then IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_INVALID_ARG. ]
   //         if (strcmp(OPTION_X509_CERT, option) == 0)
   //         {
   //             if (transport_state->preferred_authentication_mode == CREDENTIAL_NOT_BUILD)
   //             {
   //                 transport_state->preferred_authentication_mode = X509;
   //             }
   //             else if (transport_state->preferred_authentication_mode != X509)
   //             {
   //                 LogError("x509certificate specified, but authentication method is not x509");
   //                 result = IOTHUB_CLIENT_INVALID_ARG;
   //             }
   //         }
   //         /*Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_02_008: [ If optionName is x509privatekey and the authentication method is not x509 then IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_INVALID_ARG. ]*/
   //         else if (strcmp(OPTION_X509_PRIVATE_KEY, option) == 0)
   //         {
   //             if (transport_state->preferred_authentication_mode == CREDENTIAL_NOT_BUILD)
   //             {
   //                 transport_state->preferred_authentication_mode = X509;
   //             }
   //             else if (transport_state->preferred_authentication_mode != X509)
   //             {
   //                 LogError("x509privatekey specified, but authentication method is not x509");
   //                 result = IOTHUB_CLIENT_INVALID_ARG;
   //             }
   //         }

   //         if (result != IOTHUB_CLIENT_INVALID_ARG)
   //         {
   //             // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_206: [If the TLS IO does not exist, IoTHubTransport_AMQP_Common_SetOption shall create it and save it on the transport instance.]
   //             if (transport_state->tls_io == NULL &&
   //                 (transport_state->tls_io = transport_state->underlying_io_transport_provider(STRING_c_str(transport_state->iothub_host_fqdn))) == NULL)
   //             {
   //                 // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_207: [If IoTHubTransport_AMQP_Common_SetOption fails creating the TLS IO instance, it shall fail and return IOTHUB_CLIENT_ERROR.]
   //                 result = IOTHUB_CLIENT_ERROR;
   //                 LogError("IoTHubTransport_AMQP_Common_SetOption failed (failed to obtain a TLS I/O transport layer).");
   //             }
   //             else
   //             {
   //                 // Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_09_208: [When a new TLS IO instance is created, IoTHubTransport_AMQP_Common_SetOption shall apply the TLS I/O Options with OptionHandler_FeedOptions() if it is has any saved.]
   //                 if (transport_state->xioOptions != NULL)
   //                 {
   //                     if (OptionHandler_FeedOptions(transport_state->xioOptions, transport_state->tls_io) != 0)
   //                     {
   //                         LogError("IoTHubTransport_AMQP_Common_SetOption failed (unable to replay options to TLS)");
   //                     }
   //                     else
   //                     {
   //                         OptionHandler_Destroy(transport_state->xioOptions);
   //                         transport_state->xioOptions = NULL;
   //                     }
   //                 }

   //                 if (xio_setoption(transport_state->tls_io, option, value) != RESULT_OK)
   //                 {
   //                     /* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_03_001: [If xio_setoption fails, IoTHubTransport_AMQP_Common_SetOption shall return IOTHUB_CLIENT_ERROR.] */
   //                     result = IOTHUB_CLIENT_ERROR;
   //                     LogError("Invalid option (%s) passed to IoTHubTransport_AMQP_Common_SetOption", option);
   //                 }
   //                 else
   //                 {
   //                     result = IOTHUB_CLIENT_OK;
   //                 }
   //             }
   //         }
   //     }
		result = IOTHUB_CLIENT_OK;
	}

    return result;
}

IOTHUB_DEVICE_HANDLE IoTHubTransport_AMQP_Common_Register(TRANSPORT_LL_HANDLE handle, const IOTHUB_DEVICE_CONFIG* device, IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, PDLIST_ENTRY waitingToSend)
{
#ifdef NO_LOGGING
    UNUSED(iotHubClientHandle);
#endif

    IOTHUB_DEVICE_HANDLE result;

	if ((handle == NULL) || (device == NULL) || (waitingToSend == NULL) || (iotHubClientHandle == NULL))
    {
        LogError("invalid parameter TRANSPORT_LL_HANDLE handle=%p, const IOTHUB_DEVICE_CONFIG* device=%p, IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle=%p, PDLIST_ENTRY waiting_to_send=%p",
            handle, device, iotHubClientHandle, waitingToSend);
		result = NULL;
    }
	else if (device->deviceId == NULL)
	{
		LogError("Transport failed to register device (device_id provided is NULL)");
		result = NULL;
	}
    else
    {
		LIST_ITEM_HANDLE list_item;
        AMQP_TRANSPORT_INSTANCE* transport_instance = (AMQP_TRANSPORT_INSTANCE*)handle;

		if (is_device_registered(transport_instance->registered_devices, device->deviceId, &list_item))
		{
			LogError("IoTHubTransport_AMQP_Common_Register failed (device '%s' already registered on this transport instance)", device->deviceId);
			result = NULL;
		}
        else if (!is_device_credential_acceptable(device, transport_instance->preferred_authentication_mode))
        {
            LogError("Transport failed to register device '%s' (device credential was not accepted)", device->deviceId);
			result = NULL;
		}
        else
        {
            AMQP_TRANSPORT_DEVICE_INSTANCE* device_state;

            if ((device_state = (AMQP_TRANSPORT_DEVICE_INSTANCE*)malloc(sizeof(AMQP_TRANSPORT_DEVICE_INSTANCE))) == NULL)
            {
				LogError("Transport failed to register device '%s' (failed to create the device state instance; malloc failed)", device->deviceId);
				result = NULL;
			}
            else
            {
				memset(device_state, 0, sizeof(AMQP_TRANSPORT_DEVICE_INSTANCE));
                
				device_state->iothub_client_handle = iotHubClientHandle;
                device_state->transport_state = transport_instance;
                device_state->waiting_to_send = waitingToSend;
				device_state->device_state = DEVICE_STATE_STOPPED;
				device_state->max_state_change_timeout_secs = DEFAULT_DEVICE_STATE_CHANGE_TIMEOUT_SECS;
     
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
                device_state->subscribe_methods_needed = 0;
                device_state->subscribed_for_methods = 0;
#endif

                if ((device_state->device_id = STRING_construct(device->deviceId)) == NULL)
                {
					LogError("Transport failed to register device '%s' (failed to copy the deviceId)", device->deviceId);
					result = NULL;
				}
                else
                {
					DEVICE_CONFIG device_config;
					memset(&device_config, 0, sizeof(DEVICE_CONFIG));
					device_config.device_id = (char*)device->deviceId;
					device_config.iothub_host_fqdn = (char*)STRING_c_str(transport_instance->iothub_host_fqdn);
					device_config.device_primary_key = (char*)device->deviceKey;
					device_config.device_sas_token = (char*)device->deviceSasToken;
					device_config.authentication_mode = (device->deviceKey != NULL || device->deviceSasToken != NULL ? DEVICE_AUTH_MODE_CBS : DEVICE_AUTH_MODE_X509);
					device_config.on_state_changed_callback = on_device_state_changed_callback;
					device_config.on_state_changed_context = device_state;

					if ((device_state->device_handle = device_create(&device_config)) == NULL)
					{
						LogError("Transport failed to register device '%s' (failed to create the DEVICE_HANDLE instance)", device->deviceId);
						result = NULL;
					}
					else
					{
#ifdef WIP_C2D_METHODS_AMQP /* This feature is WIP, do not use yet */
						/* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_010: [ `IoTHubTransport_AMQP_Common_Create` shall create a new iothubtransportamqp_methods instance by calling `iothubtransportamqp_methods_create` while passing to it the the fully qualified domain name and the device Id. ]*/
						device_state->methods_handle = iothubtransportamqp_methods_create(STRING_c_str(transport_instance->iothub_host_fqdn), deviceId);
						if (device_state->methods_handle == NULL)
						{
							/* Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_01_011: [ If `iothubtransportamqp_methods_create` fails, `IoTHubTransport_AMQP_Common_Create` shall fail and return NULL. ]*/
							LogError("Transport failed to register device '%s' (Cannot create the methods module)", device->deviceId);
							cleanup_required = true;
							result = NULL;
						}
						else
#endif
							bool is_first_device_being_registered = (singlylinkedlist_get_head_item(transport_instance->registered_devices) == NULL);

							if (singlylinkedlist_add(transport_instance->registered_devices, device_state) == NULL)
							{
								LogError("Transport failed to register device '%s' (singlylinkedlist_add failed)", device->deviceId);
								result = NULL;
							}
							else
							{
								if (is_first_device_being_registered)
								{
									if (device_config.authentication_mode == DEVICE_AUTH_MODE_CBS)
									{
										transport_instance->preferred_authentication_mode = AMQP_TRANSPORT_AUTHENTICATION_MODE_CBS;
									}
									else
									{
										transport_instance->preferred_authentication_mode = AMQP_TRANSPORT_AUTHENTICATION_MODE_X509;
									}
								}

								result = (IOTHUB_DEVICE_HANDLE)device_state;
							}
					}
                }

                if (result == NULL)
                {
					destroy_registered_device_instance(device_state);
				}
            }
        }
    }

    return result;
}

void IoTHubTransport_AMQP_Common_Unregister(IOTHUB_DEVICE_HANDLE deviceHandle)
{
    if (deviceHandle == NULL)
    {
        LogError("Failed to unregister device (deviceHandle is NULL).");
    }
    else
    {
        AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device = (AMQP_TRANSPORT_DEVICE_INSTANCE*)deviceHandle;
		const char* device_id = STRING_c_str(registered_device->device_id);
		LIST_ITEM_HANDLE list_item;

        if (registered_device->transport_state == NULL)
        {
			LogError("Failed to unregister device '%s' (deviceHandle does not have a transport state associated to).", device_id);
        }
        else if (!is_device_registered(registered_device->transport_state->registered_devices, device_id, &list_item))
		{
			LogError("Failed to unregister device '%s' (device is not registered within this transport).", device_id);
		}
		else
		{
			// Removing it first so the race hazzard is reduced between this function and DoWork. Best would be to use locks.
			if (singlylinkedlist_remove(registered_device->transport_state->registered_devices, list_item) != RESULT_OK)
			{
				LogError("Failed to unregister device '%s' (singlylinkedlist_remove failed).", device_id);
			}
			else
			{
				// TODO: Q: should we go through waiting_to_send list and raise on_event_send_complete with BECAUSE_DESTROY ?

				destroy_registered_device_instance(registered_device);
			}
		}
    }
}

void IoTHubTransport_AMQP_Common_Destroy(TRANSPORT_LL_HANDLE handle)
{
    if (handle != NULL)
    {
        AMQP_TRANSPORT_INSTANCE* transport_state = (AMQP_TRANSPORT_INSTANCE*)handle;

		LIST_ITEM_HANDLE list_item = singlylinkedlist_get_head_item(transport_state->registered_devices);

		while (list_item != NULL)
		{
			AMQP_TRANSPORT_DEVICE_INSTANCE* registered_device = (AMQP_TRANSPORT_DEVICE_INSTANCE*)singlylinkedlist_item_get_value(list_item);
			list_item = singlylinkedlist_get_next_item(list_item);
			IoTHubTransport_AMQP_Common_Unregister(registered_device);
		}

		singlylinkedlist_destroy(transport_state->registered_devices);

		amqp_connection_destroy(transport_state->amqp_connection);

		destroy_underlying_io_transport(transport_state);


        STRING_delete(transport_state->iothub_host_fqdn);

        free(transport_state);
    }
}

int IoTHubTransport_AMQP_Common_SetRetryPolicy(TRANSPORT_LL_HANDLE handle, IOTHUB_CLIENT_RETRY_POLICY retryPolicy, size_t retryTimeoutLimitInSeconds)
{
    int result;
    (void)handle;
    (void)retryPolicy;
    (void)retryTimeoutLimitInSeconds;

    /* Retry Policy is currently not available for AMQP */

    result = 0;
    return result;
}

STRING_HANDLE IoTHubTransport_AMQP_Common_GetHostname(TRANSPORT_LL_HANDLE handle)
{
    STRING_HANDLE result;

	// Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_02_001: [If `handle` is NULL, `IoTHubTransport_AMQP_Common_GetHostname` shall return NULL.]
	if (handle == NULL)
    {
		LogError("Cannot provide the target host name (transport handle is NULL).");

		result = NULL;
    }
	// Codes_SRS_IOTHUBTRANSPORT_AMQP_COMMON_02_002: [IoTHubTransport_AMQP_Common_GetHostname shall return a copy of `instance->iothub_target_fqdn`.]
	else if ((result = STRING_clone(((AMQP_TRANSPORT_INSTANCE*)(handle))->iothub_host_fqdn)) == NULL)
	{
		LogError("Cannot provide the target host name (STRING_clone failed).");
	}

    return result;
}

