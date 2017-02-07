# IoTHubTransport_AMQP_Device Requirements
================

## Overview

This module encapsulates all the components that represent a logical device registered through the AMQP transport in Azure C SDK.
It is responsible for triggering the device authentication, messaging exchange.


## Dependencies

This module will depend on the following modules:

azure-c-shared-utility
azure-uamqp-c
iothubtransport_amqp_cbsauthentication
iothubtransport_amqp_messenger


## Exposed API

```c

typedef enum DEVICE_STATE_TAG
{
	DEVICE_STATE_STOPPED,
	DEVICE_STATE_STOPPING,
	DEVICE_STATE_STARTING,
	DEVICE_STATE_STARTED,
	DEVICE_STATE_STOPPED,
	DEVICE_STATE_ERROR_AUTH,
	DEVICE_STATE_ERROR_MSG
} DEVICE_STATE;

typedef enum DEVICE_AUTH_MODE_TAG
{
	DEVICE_AUTH_MODE_CBS,
	DEVICE_AUTH_MODE_X509
} DEVICE_AUTH_MODE;

typedef enum DEVICE_SEND_STATUS_TAG
{
	DEVICE_SEND_STATUS_IDLE,
	DEVICE_SEND_STATUS_BUSY
} DEVICE_SEND_STATUS;

typedef void(*ON_DEVICE_STATE_CHANGED)(void* context, DEVICE_STATE previous_state, DEVICE_STATE new_state);
typedef void(*ON_DEVICE_C2D_MESSAGE_RECEIVED)(IOTHUB_MESSAGE_HANDLE message, void* context);
typedef void(*ON_DEVICE_D2C_EVENT_SEND_COMPLETE)(IOTHUB_MESSAGE_HANDLE message, void* context);

typedef struct DEVICE_CONFIG_TAG
{
	const char* device_id;
	const char* iothub_host_fqdn;
	DEVICE_AUTH_MODE authentication_mode;
	ON_DEVICE_STATE_CHANGED on_state_changed_callback;
	void* on_state_changed_context;
} DEVICE_CONFIG;

typedef struct DEVICE_INSTANCE* DEVICE_HANDLE;

extern DEVICE_HANDLE device_create(DEVICE_CONFIG *config);
extern void device_destroy(DEVICE_HANDLE handle);

extern int device_start_async(DEVICE_HANDLE handle, SESSION_HANDLE session_handle, CBS_HANDLE cbs_handle);
extern void device_stop_async(DEVICE_HANDLE handle);
extern void device_do_work(DEVICE_HANDLE handle);

extern int device_send_event_async(DEVICE_HANDLE handle, IOTHUB_MESSAGE_LIST* message, ON_DEVICE_D2C_EVENT_SEND_COMPLETE on_event_send_complete_callback, const void* context);
extern int device_get_send_status(DEVICE_HANDLE handle, DEVICE_SEND_STATUS *send_status);

extern int device_subscribe_message(DEVICE_HANDLE handle, ON_DEVICE_C2D_MESSAGE_RECEIVED on_message_received_callback, void* context);
extern int device_unsubscribe_message(DEVICE_HANDLE handle);

extern int device_subscribe_device_twin(DEVICE_HANDLE handle);
extern void device_unsubscribe_device_twin(DEVICE_HANDLE handle);

extern int device_subscribe_device_method(DEVICE_HANDLE handle);
extern void device_unsubscribe_device_method(DEVICE_HANDLE handle);
extern int device_send_device_method_response(DEVICE_HANDLE handle, METHOD_HANDLE method_id, const unsigned char* response, size_t response_size, int status_response);


extern int device_set_retry_policy(DEVICE_HANDLE handle, IOTHUB_CLIENT_RETRY_POLICY policy, size_t retry_timeout_limit_in_seconds);
extern int device_set_option(DEVICE_HANDLE handle, const char* name, void* value);
extern OPTIONHANDLER_HANDLE device_retrieve_options(DEVICE_HANDLE handle);

```


Note: `instance` refers to the structure that holds the current state and control parameters of the device. 
In each function (other than amqp_device_create) it shall derive from the AMQP_DEVICE_HANDLE handle passed as argument.  


### device_create

```c
extern DEVICE_HANDLE device_create(DEVICE_CONFIG config);
```

**SRS_DEVICE_09_001: [**If `config` or device_id or iothub_host_fqdn or on_state_changed_callback are NULL then device_create shall fail and return NULL**]**
**SRS_DEVICE_09_001: [**device_create shall allocate memory for the device instance structure**]**
**SRS_DEVICE_09_001: [**If malloc fails, device_create shall fail and return NULL**]**
**SRS_DEVICE_09_001: [**All `config` parameters shall be saved into `instance`**]**
**SRS_DEVICE_09_001: [**If any `config` parameters fail to be saved into `instance`, device_create shall fail and return NULL**]**
**SRS_DEVICE_09_001: [**If `instance->authentication_mode` is DEVICE_AUTH_MODE_CBS, `instance->authentication_handle` shall be set using authentication_create()**]**
**SRS_DEVICE_09_001: [**If the AUTHENTICATION_HANDLE fails to be created, device_create shall fail and return NULL**]**
**SRS_DEVICE_09_001: [**`instance->messenger_handle` shall be set using messenger_create()**]**
**SRS_DEVICE_09_001: [**If the MESSENGER_HANDLE fails to be created, device_create shall fail and return NULL**]**
**SRS_DEVICE_09_001: [**If device_create fails it shall release all memory it has allocated**]**
**SRS_DEVICE_09_001: [**If device_create succeeds it shall return a handle to its `instance` structure**]**


### device_get_send_status

```c
extern int device_get_send_status(DEVICE_HANDLE handle, DEVICE_SEND_STATUS *send_status);
```

**SRS_DEVICE_09_001: [**If `handle` is NULL, device_get_send_status shall return a non-zero result**]**
**SRS_DEVICE_09_001: [**If `send_status` is NULL, device_get_send_status shall return a non-zero result**]**
**SRS_DEVICE_09_001: [**The status of ` instance->messenger_handle` shall be obtained using messenger_get_send_status**]**
**SRS_DEVICE_09_001: [**If messenger_get_send_status fails, device_get_send_status shall return a non-zero result**]**
**SRS_DEVICE_09_001: [**If messenger_get_send_status returns MESSENGER_SEND_STATUS_IDLE, device_get_send_status return status DEVICE_SEND_STATUS_IDLE**]**
**SRS_DEVICE_09_001: [**If messenger_get_send_status returns MESSENGER_SEND_STATUS_BUSY, device_get_send_status return status DEVICE_SEND_STATUS_BUSY**]**
**SRS_DEVICE_09_001: [**If device_get_send_status succeeds, it shall return zero as result**]**
