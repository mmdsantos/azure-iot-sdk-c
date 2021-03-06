// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdbool>
#include <cstdint>
#else
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#endif

void* real_malloc(size_t size)
{
    return malloc(size);
}

void real_free(void* ptr)
{
    free(ptr);
}

#include "testrunnerswitcher.h"
#include "azure_c_shared_utility/macro_utils.h"
#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "umocktypes_bool.h"
#include "umocktypes_stdint.h"
#include "umock_c_negative_tests.h"
#include "umocktypes.h"
#include "umocktypes_c.h"

#define ENABLE_MOCKS

#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/uniqueid.h"
#include "azure_c_shared_utility/optionhandler.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/message_receiver.h"
#include "iothub_client_private.h"
#include "iothub_client_version.h"
#include "uamqp_messaging.h"

#undef ENABLE_MOCKS

#include "iothubtransport_amqp_messenger.h"


static TEST_MUTEX_HANDLE g_testByTest;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

#define MESSENGER_IS_STARTED             true
#define MESSENGER_IS_NOT_STARTED         false

#define MESSENGER_IS_SUBSCRIBED          true
#define MESSENGER_IS_NOT_SUBSCRIBED      false

#define MESSAGE_RECEIVER_IS_CREATED      true
#define MESSAGE_RECEIVER_IS_NOT_CREATED  false

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

#define DEFAULT_EVENT_SEND_RETRY_LIMIT                    10
#define DEFAULT_EVENT_SEND_TIMEOUT_SECS 600

#define UNIQUE_ID_BUFFER_SIZE                             37
#define TEST_UNIQUE_ID                                    "A1234DE234A1234DE234A1234DE234A1234DEA1234DE234A1234DE234A1234DE234A1234DEA1234DE234A1234DE234A1234DE234A1234DE"

#define TEST_DEVICE_ID                                    "my_device"
#define TEST_DEVICE_ID_STRING_HANDLE                      (STRING_HANDLE)0x4442
#define TEST_IOTHUB_HOST_FQDN_STRING_HANDLE               (STRING_HANDLE)0x4443
#define TEST_IOTHUB_HOST_FQDN                             "some.fqdn.com"
#define TEST_ON_STATE_CHANGED_CB_CONTEXT                  (void*)0x4445
#define TEST_STRING_HANDLE                                (STRING_HANDLE)0x4446
#define TEST_SESSION_HANDLE                               (SESSION_HANDLE)0x4447
#define TEST_MESSENGER_HANDLE                             (MESSENGER_HANDLE)0x4448
#define TEST_DEVICES_PATH_STRING_HANDLE                   (STRING_HANDLE)0x4449
#define TEST_DEVICES_PATH_CHAR_PTR                        "iothub.azure-devices.net/devices/some-device-id"
#define TEST_EVENT_SEND_ADDRESS_STRING_HANDLE             (STRING_HANDLE)0x4450
#define TEST_EVENT_SEND_ADDRESS_CHAR_PTR                  "amqps:/iothub.azure-devices.net/devices/some-device-id/messages/events"

#define TEST_MESSAGE_RECEIVE_ADDRESS_STRING_HANDLE        (STRING_HANDLE)0x4451
#define TEST_MESSAGE_RECEIVE_ADDRESS_CHAR_PTR             "amqps://iothub.azure-devices.net/devices/some-device-id/messages/devicebound"

#define TEST_EVENT_SENDER_SOURCE_NAME_STRING_HANDLE       (STRING_HANDLE)0x4452
#define TEST_EVENT_SENDER_SOURCE_NAME_CHAR_PTR            "event_sender_source_name"
#define TEST_MESSAGE_RECEIVER_TARGET_NAME_STRING_HANDLE   (STRING_HANDLE)0x4453
#define TEST_MESSAGE_RECEIVER_TARGET_NAME_CHAR_PTR        "message_receiver_target_name"

#define TEST_EVENT_SENDER_SOURCE_AMQP_VALUE               (AMQP_VALUE)0x4454
#define TEST_EVENT_SENDER_TARGET_AMQP_VALUE               (AMQP_VALUE)0x4455

#define TEST_EVENT_SENDER_LINK_NAME_STRING_HANDLE         (STRING_HANDLE)0x4456
#define TEST_EVENT_SENDER_LINK_NAME_CHAR_PTR              "event_sender_link_name"
#define TEST_MESSAGE_RECEIVER_LINK_NAME_STRING_HANDLE     (STRING_HANDLE)0x4457
#define TEST_MESSAGE_RECEIVER_LINK_NAME_CHAR_PTR          "message_receiver_link_name"
#define TEST_EVENT_SENDER_LINK_HANDLE                     (LINK_HANDLE)0x4458
#define MESSAGE_SENDER_MAX_LINK_SIZE                      UINT64_MAX
#define MESSAGE_RECEIVER_MAX_LINK_SIZE                    65536
#define TEST_MESSAGE_SENDER_HANDLE                        (MESSAGE_SENDER_HANDLE)0x4459
#define TEST_LINK_ATTACH_PROPERTIES                       (fields)0x4460
#define TEST_LINK_DEVICE_TYPE_NAME_AMQP_VALUE             (AMQP_VALUE)0x4461
#define TEST_LINK_DEVICE_TYPE_VALUE_AMQP_VALUE            (AMQP_VALUE)0x4462

#define TEST_MESSAGE_RECEIVER_HANDLE                      (MESSAGE_RECEIVER_HANDLE)0x4463
#define TEST_MESSAGE_RECEIVER_LINK_HANDLE                 (LINK_HANDLE)0x4464

#define TEST_IOTHUB_MESSAGE_HANDLE                        (IOTHUB_MESSAGE_HANDLE)0x4466
#define TEST_MESSAGE_HANDLE                               (MESSAGE_HANDLE)0x4467
#define TEST_MESSAGE_RECEIVER_SOURCE_AMQP_VALUE           (AMQP_VALUE)0x4468
#define TEST_MESSAGE_RECEIVER_TARGET_AMQP_VALUE           (AMQP_VALUE)0x4469
#define TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT           (void*)0x4470
#define TEST_MESSAGE_DISPOSITION_ACCEPTED_AMQP_VALUE      (AMQP_VALUE)0x4471
#define TEST_MESSAGE_DISPOSITION_ABANDONED_AMQP_VALUE     (AMQP_VALUE)0x4472
#define TEST_MESSAGE_DISPOSITION_REJECTED_AMQP_VALUE      (AMQP_VALUE)0x4473
#define TEST_SINGLYLINKEDLIST_HANDLE                      (SINGLYLINKEDLIST_HANDLE)0x4476
#define TEST_LIST_ITEM_HANDLE                             (LIST_ITEM_HANDLE)0x4477
#define TEST_SEND_EVENT_TASK                              (const void*)0x4478
#define TEST_IOTHUB_CLIENT_HANDLE                         (void*)0x4479
static IOTHUB_MESSAGE_LIST* TEST_IOTHUB_MESSAGE_LIST_HANDLE;
static SINGLYLINKEDLIST_HANDLE TEST_WAIT_TO_SEND_LIST;
static SINGLYLINKEDLIST_HANDLE TEST_IN_PROGRESS_LIST;
#define TEST_WAIT_TO_SEND_LIST1                           (SINGLYLINKEDLIST_HANDLE)0x4481
#define TEST_WAIT_TO_SEND_LIST2                           (SINGLYLINKEDLIST_HANDLE)0x4482
#define TEST_IN_PROGRESS_LIST1                            (SINGLYLINKEDLIST_HANDLE)0x4483
#define TEST_IN_PROGRESS_LIST2                            (SINGLYLINKEDLIST_HANDLE)0x4484
#define TEST_OPTIONHANDLER_HANDLE                         (OPTIONHANDLER_HANDLE)0x4485
#define INDEFINITE_TIME                                   ((time_t)-1)

// Helpers

#ifdef __cplusplus
extern "C"
{
#endif

static int TEST_link_set_max_message_size_result;
int TEST_amqpvalue_set_map_value_result;
int TEST_link_set_attach_properties_result;

static int g_STRING_sprintf_call_count;
static int g_STRING_sprintf_fail_on_count;
static STRING_HANDLE saved_STRING_sprintf_handle;

int STRING_sprintf(STRING_HANDLE handle, const char* format, ...)
{
    int result;
    saved_STRING_sprintf_handle = handle;
    (void)format;

    g_STRING_sprintf_call_count++;

    if (g_STRING_sprintf_call_count == g_STRING_sprintf_fail_on_count)
    {
        result = __LINE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

#ifdef __cplusplus
}
#endif


static int saved_malloc_returns_count = 0;
static void* saved_malloc_returns[20];

static void* TEST_malloc(size_t size)
{
    saved_malloc_returns[saved_malloc_returns_count] = real_malloc(size);

    return saved_malloc_returns[saved_malloc_returns_count++];
}

static void TEST_free(void* ptr)
{
    int i, j;
    for (i = 0, j = 0; j < saved_malloc_returns_count; i++, j++)
    {
        if (saved_malloc_returns[i] == ptr) j++;

        saved_malloc_returns[i] = saved_malloc_returns[j];
    }

    if (i != j) saved_malloc_returns_count--;

    real_free(ptr);
}


static int saved_wait_to_send_list_count;
static const void* saved_wait_to_send_list[20];

static int saved_wait_to_send_list_count2;
static const void* saved_wait_to_send_list2[20];

static int saved_in_progress_list_count;
static const void* saved_in_progress_list[20];

static int saved_in_progress_list_count2;
static const void* saved_in_progress_list2[20];

static void* saved_on_state_changed_callback_context;
static MESSENGER_STATE saved_on_state_changed_callback_previous_state;
static MESSENGER_STATE saved_on_state_changed_callback_new_state;

static void TEST_on_state_changed_callback(void* context, MESSENGER_STATE previous_state, MESSENGER_STATE new_state)
{
    saved_on_state_changed_callback_context = context;
    saved_on_state_changed_callback_previous_state = previous_state;
    saved_on_state_changed_callback_new_state = new_state;
}


static LINK_HANDLE saved_messagesender_create_link;
static ON_MESSAGE_SENDER_STATE_CHANGED saved_messagesender_create_on_message_sender_state_changed;
static void* saved_messagesender_create_context;

static MESSAGE_SENDER_HANDLE TEST_messagesender_create(LINK_HANDLE link, ON_MESSAGE_SENDER_STATE_CHANGED on_message_sender_state_changed, void* context)
{
    saved_messagesender_create_link = link;
    saved_messagesender_create_on_message_sender_state_changed = on_message_sender_state_changed;
    saved_messagesender_create_context = context;

    return TEST_MESSAGE_SENDER_HANDLE;
}

static MESSAGE_RECEIVER_HANDLE TEST_messagereceiver_create_result;
static LINK_HANDLE saved_messagereceiver_create_link;
static ON_MESSAGE_RECEIVER_STATE_CHANGED saved_messagereceiver_create_on_message_receiver_state_changed;
static void* saved_messagereceiver_create_context;
static MESSAGE_RECEIVER_HANDLE TEST_messagereceiver_create(LINK_HANDLE link, ON_MESSAGE_RECEIVER_STATE_CHANGED on_message_receiver_state_changed, void* context)
{
    saved_messagereceiver_create_link = link;
    saved_messagereceiver_create_on_message_receiver_state_changed = on_message_receiver_state_changed;
    saved_messagereceiver_create_context = context;

    return TEST_messagereceiver_create_result;
}


static IOTHUB_MESSAGE_HANDLE saved_on_new_message_received_callback_message;
static void* saved_on_new_message_received_callback_context;
static MESSENGER_DISPOSITION_RESULT TEST_on_new_message_received_callback_result;
static MESSENGER_DISPOSITION_RESULT TEST_on_new_message_received_callback(IOTHUB_MESSAGE_HANDLE message, void* context)
{
    saved_on_new_message_received_callback_message = message;
    saved_on_new_message_received_callback_context = context;
    return TEST_on_new_message_received_callback_result;
}


static int TEST_messagereceiver_open_result;
static MESSAGE_RECEIVER_HANDLE saved_messagereceiver_open_message_receiver;
static ON_MESSAGE_RECEIVED saved_messagereceiver_open_on_message_received;
static const void* saved_messagereceiver_open_callback_context;
static int TEST_messagereceiver_open(MESSAGE_RECEIVER_HANDLE message_receiver, ON_MESSAGE_RECEIVED on_message_received, const void* callback_context)
{
    saved_messagereceiver_open_message_receiver = message_receiver;
    saved_messagereceiver_open_on_message_received = on_message_received;
    saved_messagereceiver_open_callback_context = callback_context;

    return TEST_messagereceiver_open_result;
}


static MESSENGER_CONFIG g_messenger_config;

static MESSENGER_CONFIG* get_messenger_config()
{
    memset(&g_messenger_config, 0, sizeof(MESSENGER_CONFIG));

    g_messenger_config.device_id = TEST_DEVICE_ID;
    g_messenger_config.iothub_host_fqdn = TEST_IOTHUB_HOST_FQDN;
    g_messenger_config.on_state_changed_callback = TEST_on_state_changed_callback;
    g_messenger_config.on_state_changed_context = TEST_ON_STATE_CHANGED_CB_CONTEXT;
    
    return &g_messenger_config;
}

typedef struct MESSENGER_DO_WORK_EXP_CALL_PROFILE_STRUCT
{
	MESSENGER_STATE current_state;
	bool create_message_sender;
	bool destroy_message_sender;
	bool create_message_receiver;
	bool destroy_message_receiver;
	int wait_to_send_list_length;
	int in_progress_list_length;
	size_t send_event_timeout_secs;
	time_t current_time;
} MESSENGER_DO_WORK_EXP_CALL_PROFILE;

static MESSENGER_DO_WORK_EXP_CALL_PROFILE g_do_work_profile;

static MESSENGER_DO_WORK_EXP_CALL_PROFILE* get_msgr_do_work_exp_call_profile(MESSENGER_STATE current_state, bool is_subscribed_for_messages, bool is_msg_rcvr_created, int wts_list_length, int ip_list_length, time_t current_time, size_t event_send_timeout_secs)
{
	memset(&g_do_work_profile, 0, sizeof(MESSENGER_DO_WORK_EXP_CALL_PROFILE));
	g_do_work_profile.current_state = current_state;
	g_do_work_profile.wait_to_send_list_length = wts_list_length;
	g_do_work_profile.in_progress_list_length = ip_list_length;
	g_do_work_profile.current_time = current_time;
	g_do_work_profile.send_event_timeout_secs = event_send_timeout_secs;

	if (g_do_work_profile.current_state == MESSENGER_STATE_STARTING)
	{
		g_do_work_profile.create_message_sender = true;
	}
	else if (g_do_work_profile.current_state == MESSENGER_STATE_STOPPING)
	{
		g_do_work_profile.destroy_message_sender = true;

		if (is_msg_rcvr_created)
		{
			g_do_work_profile.destroy_message_receiver = true;
		}
	}
	else if (g_do_work_profile.current_state == MESSENGER_STATE_STARTED)
	{
		if (is_subscribed_for_messages && !is_msg_rcvr_created)
		{
			g_do_work_profile.create_message_receiver = true;
		}
		else if (!is_subscribed_for_messages && is_msg_rcvr_created)
		{
			g_do_work_profile.destroy_message_receiver = true;
		}
	}

	return &g_do_work_profile;
}

static IOTHUB_MESSAGE_HANDLE saved_message_create_from_iothub_message;
static int TEST_message_create_from_iothub_message_return;
static int TEST_message_create_from_iothub_message(IOTHUB_MESSAGE_HANDLE iothub_message, MESSAGE_HANDLE* uamqp_message)
{
    saved_message_create_from_iothub_message = iothub_message;

    if (TEST_message_create_from_iothub_message_return == 0)
    {
        *uamqp_message = TEST_MESSAGE_HANDLE;
    }

    return TEST_message_create_from_iothub_message_return;
}


static MESSAGE_HANDLE saved_IoTHubMessage_CreateFromUamqpMessage_uamqp_message;
static int TEST_IoTHubMessage_CreateFromUamqpMessage_return;
static int TEST_IoTHubMessage_CreateFromUamqpMessage(MESSAGE_HANDLE uamqp_message, IOTHUB_MESSAGE_HANDLE* iothub_message)
{
    saved_IoTHubMessage_CreateFromUamqpMessage_uamqp_message = uamqp_message;

    if (TEST_IoTHubMessage_CreateFromUamqpMessage_return == 0)
    {
        *iothub_message = TEST_IOTHUB_MESSAGE_HANDLE;
    }

    return TEST_IoTHubMessage_CreateFromUamqpMessage_return;
}

static int TEST_messagesender_send_result;
static MESSAGE_SENDER_HANDLE saved_messagesender_send_message_sender;
static MESSAGE_HANDLE saved_messagesender_send_message;
static ON_MESSAGE_SEND_COMPLETE saved_messagesender_send_on_message_send_complete;
static void* saved_messagesender_send_callback_context;

static int TEST_messagesender_send(MESSAGE_SENDER_HANDLE message_sender, MESSAGE_HANDLE message, ON_MESSAGE_SEND_COMPLETE on_message_send_complete, void* callback_context)
{
    saved_messagesender_send_message_sender = message_sender;
    saved_messagesender_send_message = message;
    saved_messagesender_send_on_message_send_complete = on_message_send_complete;
    saved_messagesender_send_callback_context = callback_context;

    return TEST_messagesender_send_result;
}


static bool TEST_singlylinkedlist_add_fail_return = false;
static LIST_ITEM_HANDLE TEST_singlylinkedlist_add(SINGLYLINKEDLIST_HANDLE list, const void* item)
{
	if (list == TEST_WAIT_TO_SEND_LIST1)
	{
		saved_wait_to_send_list[saved_wait_to_send_list_count++] = item;
	}
	else if (list == TEST_WAIT_TO_SEND_LIST2)
	{
		saved_wait_to_send_list2[saved_wait_to_send_list_count2++] = item;
	}
	else if (list == TEST_IN_PROGRESS_LIST1)
	{
		saved_in_progress_list[saved_in_progress_list_count++] = item;
	}
	else if (list == TEST_IN_PROGRESS_LIST2)
	{
		saved_in_progress_list2[saved_in_progress_list_count2++] = item;
	}

	return TEST_singlylinkedlist_add_fail_return ? NULL : (LIST_ITEM_HANDLE)item;
}

static int TEST_singlylinkedlist_remove_return = 0;
static int TEST_singlylinkedlist_remove(SINGLYLINKEDLIST_HANDLE list, LIST_ITEM_HANDLE item)
{
	const void** TEST_list = NULL;
	int* TEST_list_count = NULL;

	if (list == TEST_WAIT_TO_SEND_LIST1)
	{
		TEST_list = saved_wait_to_send_list;
		TEST_list_count = &saved_wait_to_send_list_count;
	}
	else if (list == TEST_WAIT_TO_SEND_LIST2)
	{
		TEST_list = saved_wait_to_send_list2;
		TEST_list_count = &saved_wait_to_send_list_count2;
	}
	else if (list == TEST_IN_PROGRESS_LIST1)
	{
		TEST_list = saved_in_progress_list;
		TEST_list_count = &saved_in_progress_list_count;
	}
	else // i.e., "if (list == TEST_IN_PROGRESS_LIST2)"
	{
		TEST_list = saved_in_progress_list2;
		TEST_list_count = &saved_in_progress_list_count2;
	}

	int i;
	int item_found = 0;
	for (i = 0; i < *TEST_list_count; i++)
	{
		if (item_found)
		{
			TEST_list[i - 1] = TEST_list[i];
		}
		else if (TEST_list[i] == item)
		{
			item_found = 1;
		}
	}

	if (item_found)
	{
		(*TEST_list_count)--;
	}

	return item_found == 1 ? 0 : 1;
}

static LIST_ITEM_HANDLE TEST_singlylinkedlist_find(SINGLYLINKEDLIST_HANDLE list, LIST_MATCH_FUNCTION match_function, const void* match_context)
{
	(void)list;
	(void)match_function;
	return (LIST_ITEM_HANDLE)match_context;
}

static const void* TEST_singlylinkedlist_item_get_value(LIST_ITEM_HANDLE item_handle)
{
	return (const void*)item_handle;
}

static LIST_ITEM_HANDLE TEST_singlylinkedlist_get_head_item(SINGLYLINKEDLIST_HANDLE list)
{
	LIST_ITEM_HANDLE list_item;

	if (list == TEST_WAIT_TO_SEND_LIST1)
	{
		if (saved_wait_to_send_list_count <= 0)
		{
			list_item = NULL;
		}
		else
		{
			list_item = (LIST_ITEM_HANDLE)saved_wait_to_send_list[0];
		}
	}
	else if (list == TEST_WAIT_TO_SEND_LIST2)
	{
		if (saved_wait_to_send_list_count2 <= 0)
		{
			list_item = NULL;
		}
		else
		{
			list_item = (LIST_ITEM_HANDLE)saved_wait_to_send_list2[0];
		}
	}
	else if (list == TEST_IN_PROGRESS_LIST1)
	{
		if (saved_in_progress_list_count <= 0)
		{
			list_item = NULL;
		}
		else
		{
			list_item = (LIST_ITEM_HANDLE)saved_in_progress_list[0];
		}
	}
	else if (list == TEST_IN_PROGRESS_LIST2)
	{
		if (saved_in_progress_list_count2 <= 0)
		{
			list_item = NULL;
		}
		else
		{
			list_item = (LIST_ITEM_HANDLE)saved_in_progress_list2[0];
		}
	}
	else
	{
		list_item = NULL;
	}

	return list_item;
}

static LIST_ITEM_HANDLE TEST_singlylinkedlist_get_next_item(LIST_ITEM_HANDLE item_handle)
{
	LIST_ITEM_HANDLE next_item = NULL;

	int i;
	int item_found = 0;
	for (i = 0; i < saved_in_progress_list_count; i++)
	{
		if (item_found)
		{
			next_item = (LIST_ITEM_HANDLE)saved_in_progress_list[i];
			break;
		}
		else if (saved_in_progress_list[i] == (void*)item_handle)
		{
			item_found = 1;
		}
	}

	if (item_found == 0)
	{
		for (i = 0; i < saved_in_progress_list_count2; i++)
		{
			if (item_found)
			{
				next_item = (LIST_ITEM_HANDLE)saved_in_progress_list2[i];
				break;
			}
			else if (saved_in_progress_list2[i] == (void*)item_handle)
			{
				item_found = 1;
			}
		}
	}

	if (item_found == 0)
	{
		for (i = 0; i < saved_wait_to_send_list_count2; i++)
		{
			if (item_found)
			{
				next_item = (LIST_ITEM_HANDLE)saved_wait_to_send_list2[i];
				break;
			}
			else if (saved_wait_to_send_list2[i] == (void*)item_handle)
			{
				item_found = 1;
			}
		}
	}

	if (item_found == 0)
	{
		for (i = 0; i < saved_wait_to_send_list_count; i++)
		{
			if (item_found)
			{
				next_item = (LIST_ITEM_HANDLE)saved_wait_to_send_list[i];
				break;
			}
			else if (saved_wait_to_send_list[i] == (void*)item_handle)
			{
				item_found = 1;
			}
		}
	}

	return next_item;
}


static void set_expected_calls_for_messenger_create(MESSENGER_CONFIG* config)
{
    EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    // memset() - not mocked.
    STRICT_EXPECTED_CALL(STRING_construct(config->device_id)).SetReturn(TEST_DEVICE_ID_STRING_HANDLE);
    STRICT_EXPECTED_CALL(STRING_construct(config->iothub_host_fqdn)).SetReturn(TEST_IOTHUB_HOST_FQDN_STRING_HANDLE);
	STRICT_EXPECTED_CALL(singlylinkedlist_create()).SetReturn(TEST_WAIT_TO_SEND_LIST);
	STRICT_EXPECTED_CALL(singlylinkedlist_create()).SetReturn(TEST_IN_PROGRESS_LIST);
}

static void set_expected_calls_for_attach_device_client_type_to_link(LINK_HANDLE link_handle, int amqpvalue_set_map_value_result, int link_set_attach_properties_result)
{
    STRICT_EXPECTED_CALL(amqpvalue_create_map());
    STRICT_EXPECTED_CALL(amqpvalue_create_symbol("com.microsoft:client-version"));
    STRICT_EXPECTED_CALL(amqpvalue_create_string(CLIENT_DEVICE_TYPE_PREFIX CLIENT_DEVICE_BACKSLASH IOTHUB_SDK_VERSION));
    
    STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_LINK_ATTACH_PROPERTIES, TEST_LINK_DEVICE_TYPE_NAME_AMQP_VALUE, TEST_LINK_DEVICE_TYPE_VALUE_AMQP_VALUE)).SetReturn(amqpvalue_set_map_value_result);
    
    if (amqpvalue_set_map_value_result == 0)
    {
        STRICT_EXPECTED_CALL(link_set_attach_properties(link_handle, TEST_LINK_ATTACH_PROPERTIES)).SetReturn(link_set_attach_properties_result);
    }

    STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_LINK_DEVICE_TYPE_VALUE_AMQP_VALUE));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_LINK_DEVICE_TYPE_NAME_AMQP_VALUE));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_LINK_ATTACH_PROPERTIES));
}

static void set_expected_calls_for_message_receiver_create()
{
    // create_event_sender()
    // create_devices_path()
    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_DEVICES_PATH_STRING_HANDLE);
    STRICT_EXPECTED_CALL(STRING_c_str(TEST_IOTHUB_HOST_FQDN_STRING_HANDLE)).SetReturn(TEST_IOTHUB_HOST_FQDN);
    STRICT_EXPECTED_CALL(STRING_c_str(TEST_DEVICE_ID_STRING_HANDLE)).SetReturn(TEST_DEVICE_ID);
    // EXPECTED: STRING_sprintf

    // create_message_receive_address()
    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_MESSAGE_RECEIVE_ADDRESS_STRING_HANDLE);
    STRICT_EXPECTED_CALL(STRING_c_str(TEST_DEVICES_PATH_STRING_HANDLE)).SetReturn(TEST_DEVICES_PATH_CHAR_PTR);
    // EXPECTED: STRING_sprintf

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_DEVICE_ID_STRING_HANDLE)).SetReturn(TEST_DEVICE_ID);

    // create_link_name()
    EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(UniqueId_Generate(IGNORED_PTR_ARG, UNIQUE_ID_BUFFER_SIZE)).IgnoreArgument_uid();
    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_MESSAGE_RECEIVER_LINK_NAME_STRING_HANDLE);
    // EXPECTED: STRING_sprintf
    EXPECTED_CALL(free(IGNORED_PTR_ARG));

    // create_message_receiver_target_name()
    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_MESSAGE_RECEIVER_TARGET_NAME_STRING_HANDLE);
    STRICT_EXPECTED_CALL(STRING_c_str(TEST_MESSAGE_RECEIVER_LINK_NAME_STRING_HANDLE)).SetReturn(TEST_MESSAGE_RECEIVER_LINK_NAME_CHAR_PTR);
    // EXPECTED: STRING_sprintf

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_MESSAGE_RECEIVER_TARGET_NAME_STRING_HANDLE)).SetReturn(TEST_MESSAGE_RECEIVER_TARGET_NAME_CHAR_PTR);
    STRICT_EXPECTED_CALL(messaging_create_target(TEST_MESSAGE_RECEIVER_TARGET_NAME_CHAR_PTR)).SetReturn(TEST_MESSAGE_RECEIVER_TARGET_AMQP_VALUE);
    STRICT_EXPECTED_CALL(STRING_c_str(TEST_MESSAGE_RECEIVE_ADDRESS_STRING_HANDLE)).SetReturn(TEST_MESSAGE_RECEIVE_ADDRESS_CHAR_PTR);
    STRICT_EXPECTED_CALL(messaging_create_source(TEST_MESSAGE_RECEIVE_ADDRESS_CHAR_PTR)).SetReturn(TEST_MESSAGE_RECEIVER_SOURCE_AMQP_VALUE);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_MESSAGE_RECEIVER_LINK_NAME_STRING_HANDLE)).SetReturn(TEST_MESSAGE_RECEIVER_LINK_NAME_CHAR_PTR);
    STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, TEST_MESSAGE_RECEIVER_LINK_NAME_CHAR_PTR, role_receiver, TEST_MESSAGE_RECEIVER_SOURCE_AMQP_VALUE, TEST_MESSAGE_RECEIVER_TARGET_AMQP_VALUE))
        .SetReturn(TEST_MESSAGE_RECEIVER_LINK_HANDLE);

    STRICT_EXPECTED_CALL(link_set_rcv_settle_mode(TEST_MESSAGE_RECEIVER_LINK_HANDLE, receiver_settle_mode_first)).IgnoreArgument(2);

    STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_MESSAGE_RECEIVER_LINK_HANDLE, MESSAGE_RECEIVER_MAX_LINK_SIZE));

    set_expected_calls_for_attach_device_client_type_to_link(TEST_MESSAGE_RECEIVER_LINK_HANDLE, 0, 0);

    STRICT_EXPECTED_CALL(messagereceiver_create(TEST_MESSAGE_RECEIVER_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .IgnoreArgument(2)
        .IgnoreArgument(3);
    STRICT_EXPECTED_CALL(messagereceiver_open(TEST_MESSAGE_RECEIVER_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .IgnoreArgument(2)
        .IgnoreArgument(3);

    STRICT_EXPECTED_CALL(STRING_delete(TEST_DEVICES_PATH_STRING_HANDLE));
    STRICT_EXPECTED_CALL(STRING_delete(TEST_MESSAGE_RECEIVE_ADDRESS_STRING_HANDLE));
    STRICT_EXPECTED_CALL(STRING_delete(TEST_MESSAGE_RECEIVER_LINK_NAME_STRING_HANDLE));
    STRICT_EXPECTED_CALL(STRING_delete(TEST_MESSAGE_RECEIVER_TARGET_NAME_STRING_HANDLE));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_MESSAGE_RECEIVER_SOURCE_AMQP_VALUE));
    STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_MESSAGE_RECEIVER_TARGET_AMQP_VALUE));
}

static void set_expected_calls_for_message_receiver_destroy()
{
    STRICT_EXPECTED_CALL(messagereceiver_close(TEST_MESSAGE_RECEIVER_HANDLE));
    STRICT_EXPECTED_CALL(messagereceiver_destroy(TEST_MESSAGE_RECEIVER_HANDLE));
    STRICT_EXPECTED_CALL(link_destroy(TEST_MESSAGE_RECEIVER_LINK_HANDLE));
}

static void set_expected_calls_for_messenger_send_async()
{
	EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
	EXPECTED_CALL(singlylinkedlist_add(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG));
}

static IOTHUB_MESSAGE_LIST* TEST_on_event_send_complete_message;
static MESSENGER_EVENT_SEND_COMPLETE_RESULT TEST_on_event_send_complete_result;
static void* TEST_on_event_send_complete_context;
static void TEST_on_event_send_complete(IOTHUB_MESSAGE_LIST* message, MESSENGER_EVENT_SEND_COMPLETE_RESULT result, void* context)
{
	TEST_on_event_send_complete_message = message;
	TEST_on_event_send_complete_result = result;
	TEST_on_event_send_complete_context = context;
}

static int send_events(MESSENGER_HANDLE handle, int number_of_events)
{
	int events_sent = 0;

	while (number_of_events > 0)
	{
		set_expected_calls_for_messenger_send_async();
		if (messenger_send_async(handle, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete, TEST_IOTHUB_CLIENT_HANDLE) == 0)
		{
			events_sent++;
		}

		number_of_events--;
	}

	return events_sent;
}

static void set_expected_calls_for_message_sender_create()
{
	// create_event_sender()
	// create_devices_path()
	STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_DEVICES_PATH_STRING_HANDLE);
	STRICT_EXPECTED_CALL(STRING_c_str(TEST_IOTHUB_HOST_FQDN_STRING_HANDLE)).SetReturn(TEST_IOTHUB_HOST_FQDN);
	STRICT_EXPECTED_CALL(STRING_c_str(TEST_DEVICE_ID_STRING_HANDLE)).SetReturn(TEST_DEVICE_ID);
	// EXPECTED: STRING_sprintf

	// create_event_send_address()
	STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_EVENT_SEND_ADDRESS_STRING_HANDLE);
	STRICT_EXPECTED_CALL(STRING_c_str(TEST_DEVICES_PATH_STRING_HANDLE)).SetReturn(TEST_DEVICES_PATH_CHAR_PTR);
	// EXPECTED: STRING_sprintf

	STRICT_EXPECTED_CALL(STRING_c_str(TEST_DEVICE_ID_STRING_HANDLE));

	// create_link_name()
	EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
	STRICT_EXPECTED_CALL(UniqueId_Generate(IGNORED_PTR_ARG, UNIQUE_ID_BUFFER_SIZE)).IgnoreArgument_uid();
	STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_EVENT_SENDER_LINK_NAME_STRING_HANDLE);
	// EXPECTED: STRING_sprintf
	EXPECTED_CALL(free(IGNORED_PTR_ARG));

	// create_event_sender_source_name()
	STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_EVENT_SENDER_SOURCE_NAME_STRING_HANDLE);
	STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENT_SENDER_LINK_NAME_STRING_HANDLE)).SetReturn(TEST_EVENT_SENDER_LINK_NAME_CHAR_PTR);
	// EXPECTED: STRING_sprintf

	STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENT_SENDER_SOURCE_NAME_STRING_HANDLE)).SetReturn(TEST_EVENT_SENDER_SOURCE_NAME_CHAR_PTR);
	STRICT_EXPECTED_CALL(messaging_create_source(TEST_EVENT_SENDER_SOURCE_NAME_CHAR_PTR)).SetReturn(TEST_EVENT_SENDER_SOURCE_AMQP_VALUE);
	STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENT_SEND_ADDRESS_STRING_HANDLE)).SetReturn(TEST_EVENT_SEND_ADDRESS_CHAR_PTR);
	STRICT_EXPECTED_CALL(messaging_create_target(TEST_EVENT_SEND_ADDRESS_CHAR_PTR)).SetReturn(TEST_EVENT_SENDER_TARGET_AMQP_VALUE);
	STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENT_SENDER_LINK_NAME_STRING_HANDLE)).SetReturn(TEST_EVENT_SENDER_LINK_NAME_CHAR_PTR);
	STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, TEST_EVENT_SENDER_LINK_NAME_CHAR_PTR, role_sender, TEST_EVENT_SENDER_SOURCE_AMQP_VALUE, TEST_EVENT_SENDER_TARGET_AMQP_VALUE))
		.SetReturn(TEST_EVENT_SENDER_LINK_HANDLE);

	STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_EVENT_SENDER_LINK_HANDLE, MESSAGE_SENDER_MAX_LINK_SIZE)).SetReturn(TEST_link_set_max_message_size_result);

	// attach_device_client_type_to_link()
	set_expected_calls_for_attach_device_client_type_to_link(TEST_EVENT_SENDER_LINK_HANDLE, 0, 0);

	STRICT_EXPECTED_CALL(messagesender_create(TEST_EVENT_SENDER_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument(2)
		.IgnoreArgument(3);
	STRICT_EXPECTED_CALL(messagesender_open(TEST_MESSAGE_SENDER_HANDLE));

	STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENT_SENDER_LINK_NAME_STRING_HANDLE));
	STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENT_SENDER_SOURCE_NAME_STRING_HANDLE));
	STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_EVENT_SENDER_SOURCE_AMQP_VALUE));
	STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_EVENT_SENDER_TARGET_AMQP_VALUE));
	STRICT_EXPECTED_CALL(STRING_delete(TEST_DEVICES_PATH_STRING_HANDLE));
	STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENT_SEND_ADDRESS_STRING_HANDLE));
}

static void set_expected_calls_for_messenger_start(MESSENGER_CONFIG* config, MESSENGER_HANDLE messenger_handle)
{
    (void)config;
    (void)messenger_handle;
}

static void set_expected_calls_for_message_sender_destroy()
{
	STRICT_EXPECTED_CALL(messagesender_destroy(TEST_MESSAGE_SENDER_HANDLE));
	STRICT_EXPECTED_CALL(link_destroy(TEST_EVENT_SENDER_LINK_HANDLE));
}

static void set_expected_calls_for_messenger_stop(int wait_to_send_list_length, int in_progress_list_length, bool destroy_message_receiver)
{
	set_expected_calls_for_message_sender_destroy();

	if (destroy_message_receiver)
	{
		set_expected_calls_for_message_receiver_destroy();
	}

	// remove timed out events
	if (in_progress_list_length <= 0)
	{
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST)).SetReturn(NULL);
	}
	else
	{
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST));

		int i;
		for (i = 0; i < in_progress_list_length; i++)
		{
			EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
			// por fazer: adicionar codigo para remover items prescritos.

			if (i < (in_progress_list_length - 1))
			{
				EXPECTED_CALL(singlylinkedlist_get_next_item(IGNORED_PTR_ARG));
			}
			else
			{
				EXPECTED_CALL(singlylinkedlist_get_next_item(IGNORED_PTR_ARG)).SetReturn(NULL);
			}
		}
	}

	// Move events to wts list
	if (in_progress_list_length <= 0)
	{
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST)).SetReturn(NULL);
	}
	else
	{
		SINGLYLINKEDLIST_HANDLE new_wts_list = (TEST_WAIT_TO_SEND_LIST == TEST_WAIT_TO_SEND_LIST1 ? TEST_WAIT_TO_SEND_LIST2 : TEST_WAIT_TO_SEND_LIST1);
		SINGLYLINKEDLIST_HANDLE new_ip_list = (TEST_IN_PROGRESS_LIST == TEST_IN_PROGRESS_LIST1 ? TEST_IN_PROGRESS_LIST2 : TEST_IN_PROGRESS_LIST1);

		// rest of function
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST));
		STRICT_EXPECTED_CALL(singlylinkedlist_create()).SetReturn(new_wts_list);

		// Moving in_progress_list items to the new wts list.
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST));
		int i;
		for (i = 0; i < in_progress_list_length; i++)
		{
			EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
			EXPECTED_CALL(singlylinkedlist_add(new_wts_list, IGNORED_PTR_ARG));
			EXPECTED_CALL(singlylinkedlist_get_next_item(IGNORED_PTR_ARG));
		}

		// Moving wts (wait to send) list items to the new wts list.
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST));
		for (i = 0; i < wait_to_send_list_length; i++)
		{
			EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
			EXPECTED_CALL(singlylinkedlist_add(new_wts_list, IGNORED_PTR_ARG));
			EXPECTED_CALL(singlylinkedlist_get_next_item(IGNORED_PTR_ARG));
		}

		STRICT_EXPECTED_CALL(singlylinkedlist_create()).SetReturn(new_ip_list);
		STRICT_EXPECTED_CALL(singlylinkedlist_destroy(TEST_WAIT_TO_SEND_LIST));
		STRICT_EXPECTED_CALL(singlylinkedlist_destroy(TEST_IN_PROGRESS_LIST));
		TEST_WAIT_TO_SEND_LIST = new_wts_list;
		TEST_IN_PROGRESS_LIST = new_ip_list;
	}
}

static void set_expected_calls_for_on_message_send_complete()
{
	STRICT_EXPECTED_CALL(singlylinkedlist_find(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument_match_context()
		.IgnoreArgument_match_function();
	STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG)).IgnoreArgument_item_handle();
	EXPECTED_CALL(free(IGNORED_PTR_ARG));
}

static void set_expected_calls_for_message_do_work_send_pending_events(int number_of_events_pending, time_t current_time)
{
	int i;
	for (i = 0; i < number_of_events_pending; i++)
    {
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST));
		EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
		STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_WAIT_TO_SEND_LIST, IGNORED_PTR_ARG)).IgnoreArgument(2);
		STRICT_EXPECTED_CALL(singlylinkedlist_add(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG)).IgnoreArgument(2);

        STRICT_EXPECTED_CALL(message_create_from_iothub_message(TEST_IOTHUB_MESSAGE_HANDLE, IGNORED_PTR_ARG))
            .IgnoreArgument(2);

        STRICT_EXPECTED_CALL(messagesender_send(TEST_MESSAGE_SENDER_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .IgnoreArgument(2).IgnoreArgument(3).IgnoreArgument(4);
		STRICT_EXPECTED_CALL(get_time(NULL)).SetReturn(current_time);

        EXPECTED_CALL(message_destroy(IGNORED_PTR_ARG));
    }

	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST));
}

static time_t add_seconds(time_t base_time, int seconds)
{
	time_t new_time;
	struct tm *bd_new_time;

	if ((bd_new_time = localtime(&base_time)) == NULL)
	{
		new_time = INDEFINITE_TIME;
	}
	else
	{
		bd_new_time->tm_sec += seconds;
		new_time = mktime(bd_new_time);
	}

	return new_time;
}

static void set_expected_calls_for_process_event_send_timeouts(size_t in_progress_list_length, size_t send_event_timeout_secs, time_t current_time)
{
	if (in_progress_list_length <= 0)
	{
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST)).SetReturn(NULL);
	}
	else
	{
		time_t send_time = add_seconds(current_time, -1 * (int)send_event_timeout_secs);

		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST));
		
		for (; in_progress_list_length > 0; in_progress_list_length--)
		{
			EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
			STRICT_EXPECTED_CALL(get_time(NULL)).SetReturn(current_time);
			EXPECTED_CALL(get_difftime(current_time, send_time)).SetReturn(difftime(current_time, send_time));
			EXPECTED_CALL(singlylinkedlist_get_next_item(IGNORED_PTR_ARG));
		}

		EXPECTED_CALL(singlylinkedlist_get_next_item(IGNORED_PTR_ARG)).SetReturn(NULL);
	}
}

static void set_expected_calls_for_messenger_do_work(MESSENGER_DO_WORK_EXP_CALL_PROFILE *profile)
{
	if (profile->current_state == MESSENGER_STATE_STARTING)
	{
		set_expected_calls_for_message_sender_create();
	}
	else if (profile->current_state == MESSENGER_STATE_STARTED)
	{
		if (profile->create_message_receiver)
		{
			set_expected_calls_for_message_receiver_create();
		}
		else if (profile->destroy_message_receiver)
		{
			set_expected_calls_for_message_receiver_destroy();
		}

		set_expected_calls_for_process_event_send_timeouts(profile->in_progress_list_length, profile->send_event_timeout_secs, profile->current_time);

		set_expected_calls_for_message_do_work_send_pending_events(profile->wait_to_send_list_length, profile->current_time);
	}
}

static void set_expected_calls_for_messenger_destroy(MESSENGER_CONFIG* config, MESSENGER_HANDLE messenger_handle, bool destroy_message_sender, bool destroy_message_receiver, int wait_to_send_list_length, int in_progress_list_length)
{
	(void)config;

	set_expected_calls_for_messenger_stop(wait_to_send_list_length, in_progress_list_length, destroy_message_receiver);

	time_t current_time = time(NULL);

	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STOPPING, false, false, wait_to_send_list_length, in_progress_list_length, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	do_work_profile->destroy_message_sender = destroy_message_sender;
	do_work_profile->destroy_message_receiver = destroy_message_receiver;
	set_expected_calls_for_messenger_do_work(do_work_profile);

	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST)).SetReturn(NULL);

	wait_to_send_list_length += in_progress_list_length; // all events from in_progress_list should have been moved to wts list.

	while (wait_to_send_list_length > 0)
	{
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST)); 
		EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
		STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_WAIT_TO_SEND_LIST, IGNORED_PTR_ARG)).IgnoreArgument(2);
		EXPECTED_CALL(free(IGNORED_PTR_ARG)); // Freeing the SEND_EVENT_TASK instance.

		wait_to_send_list_length--;
	}

	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST)).SetReturn(NULL);

	STRICT_EXPECTED_CALL(singlylinkedlist_destroy(TEST_WAIT_TO_SEND_LIST));
	STRICT_EXPECTED_CALL(singlylinkedlist_destroy(TEST_IN_PROGRESS_LIST));

	STRICT_EXPECTED_CALL(STRING_delete(TEST_IOTHUB_HOST_FQDN_STRING_HANDLE));
	STRICT_EXPECTED_CALL(STRING_delete(TEST_DEVICE_ID_STRING_HANDLE));
	STRICT_EXPECTED_CALL(free(messenger_handle));
}

static void set_expected_calls_for_on_message_received_internal_callback(MESSENGER_DISPOSITION_RESULT disposition_result)
{
    TEST_on_new_message_received_callback_result = disposition_result;
    STRICT_EXPECTED_CALL(IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG)).IgnoreArgument(2);
    STRICT_EXPECTED_CALL(IoTHubMessage_Destroy(TEST_IOTHUB_MESSAGE_HANDLE));
    
    if (disposition_result == MESSENGER_DISPOSITION_RESULT_ACCEPTED)
    {
        STRICT_EXPECTED_CALL(messaging_delivery_accepted());
    }
    else if (disposition_result == MESSENGER_DISPOSITION_RESULT_ABANDONED)
    {
        STRICT_EXPECTED_CALL(messaging_delivery_released());
    }
    else if (disposition_result == MESSENGER_DISPOSITION_RESULT_REJECTED)
    {
        STRICT_EXPECTED_CALL(messaging_delivery_rejected("Rejected by application", "Rejected by application"));
    }
}

static MESSENGER_HANDLE create_and_start_messenger(MESSENGER_CONFIG* config)
{
    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_create(config);
    MESSENGER_HANDLE handle = messenger_create(config);

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_start(config, handle);
    (void)messenger_start(handle, TEST_SESSION_HANDLE);

    return handle;
}

static void crank_messenger_do_work(MESSENGER_HANDLE handle, MESSENGER_DO_WORK_EXP_CALL_PROFILE *profile)
{
	umock_c_reset_all_calls();
	set_expected_calls_for_messenger_do_work(profile);
	(void)messenger_do_work(handle);

	if (profile->create_message_sender && saved_messagesender_create_on_message_sender_state_changed != NULL)
	{
		STRICT_EXPECTED_CALL(get_time(NULL)).SetReturn(profile->current_time);
		saved_messagesender_create_on_message_sender_state_changed(saved_messagesender_create_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
	}

	if (profile->create_message_receiver && saved_messagereceiver_create_on_message_receiver_state_changed != NULL)
	{
		STRICT_EXPECTED_CALL(get_time(NULL)).SetReturn(profile->current_time);
		saved_messagereceiver_create_on_message_receiver_state_changed(saved_messagereceiver_create_context, MESSAGE_RECEIVER_STATE_OPEN, MESSAGE_RECEIVER_STATE_IDLE);
	}
}

static MESSENGER_HANDLE create_and_start_messenger2(MESSENGER_CONFIG* config, bool subscribe_for_messages)
{
	MESSENGER_HANDLE handle = create_and_start_messenger(config);

	time_t current_time = time(NULL);

	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTING, false, false, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	do_work_profile->create_message_sender = true;
	crank_messenger_do_work(handle, do_work_profile);

	if (subscribe_for_messages)
	{
		(void)messenger_subscribe_for_messages(handle, TEST_on_new_message_received_callback, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);

		do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, true, false, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
		do_work_profile->create_message_receiver = true;
		crank_messenger_do_work(handle, do_work_profile);
	}

	return handle;
}

BEGIN_TEST_SUITE(iothubtransport_amqp_messenger_ut)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(on_umock_c_error);

    int result = umocktypes_charptr_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);
    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);
    result = umocktypes_bool_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);


    REGISTER_UMOCK_ALIAS_TYPE(STRING_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(UNIQUEID_RESULT, int);
    REGISTER_UMOCK_ALIAS_TYPE(SESSION_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_SENDER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_RECEIVER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LINK_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(AMQP_VALUE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(role, bool);
    REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_SENDER_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(fields, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(IOTHUB_MESSAGE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_SEND_COMPLETE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_RECEIVED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_RECEIVER_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(receiver_settle_mode, int);
	REGISTER_UMOCK_ALIAS_TYPE(SINGLYLINKEDLIST_HANDLE, void*);
	REGISTER_UMOCK_ALIAS_TYPE(LIST_ITEM_HANDLE, void*);
	REGISTER_UMOCK_ALIAS_TYPE(LIST_MATCH_FUNCTION, void*);
	REGISTER_UMOCK_ALIAS_TYPE(MESSENGER_SEND_STATUS, int);
	REGISTER_UMOCK_ALIAS_TYPE(OPTIONHANDLER_HANDLE, void*);
	REGISTER_UMOCK_ALIAS_TYPE(OPTIONHANDLER_RESULT, int);
	REGISTER_UMOCK_ALIAS_TYPE(pfCloneOption, void*);
	REGISTER_UMOCK_ALIAS_TYPE(pfDestroyOption, void*);
	REGISTER_UMOCK_ALIAS_TYPE(pfSetOption, void*);
	REGISTER_UMOCK_ALIAS_TYPE(time_t, int);

    REGISTER_GLOBAL_MOCK_HOOK(malloc, TEST_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(free, TEST_free);
    REGISTER_GLOBAL_MOCK_HOOK(messagesender_create, TEST_messagesender_create);
    REGISTER_GLOBAL_MOCK_HOOK(messagesender_send, TEST_messagesender_send);
    REGISTER_GLOBAL_MOCK_HOOK(messagereceiver_create, TEST_messagereceiver_create);
    REGISTER_GLOBAL_MOCK_HOOK(messagereceiver_open, TEST_messagereceiver_open);
    REGISTER_GLOBAL_MOCK_HOOK(message_create_from_iothub_message, TEST_message_create_from_iothub_message);
    REGISTER_GLOBAL_MOCK_HOOK(IoTHubMessage_CreateFromUamqpMessage, TEST_IoTHubMessage_CreateFromUamqpMessage);
	REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_add, TEST_singlylinkedlist_add);
	REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_get_head_item, TEST_singlylinkedlist_get_head_item);
	REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_remove, TEST_singlylinkedlist_remove);
	REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_item_get_value, TEST_singlylinkedlist_item_get_value);
	REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_get_next_item, TEST_singlylinkedlist_get_next_item);
	REGISTER_GLOBAL_MOCK_HOOK(singlylinkedlist_find, TEST_singlylinkedlist_find);
	

	REGISTER_GLOBAL_MOCK_RETURN(singlylinkedlist_remove, 0);
	REGISTER_GLOBAL_MOCK_FAIL_RETURN(singlylinkedlist_remove, 555);

	REGISTER_GLOBAL_MOCK_RETURN(singlylinkedlist_add, TEST_LIST_ITEM_HANDLE);
	REGISTER_GLOBAL_MOCK_FAIL_RETURN(singlylinkedlist_add, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(STRING_construct, TEST_STRING_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_construct, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(STRING_c_str, TEST_IOTHUB_HOST_FQDN);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_c_str, NULL);
    
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_new, NULL);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messaging_create_source, NULL);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_create, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(link_set_max_message_size, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_set_max_message_size, 1);

    REGISTER_GLOBAL_MOCK_RETURN(messagesender_create, TEST_MESSAGE_SENDER_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagesender_create, NULL);
    
    REGISTER_GLOBAL_MOCK_RETURN(messagesender_open, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagesender_open, 1);

    REGISTER_GLOBAL_MOCK_RETURN(messagesender_send, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagesender_send, 1);

    REGISTER_GLOBAL_MOCK_RETURN(messagereceiver_create, TEST_MESSAGE_RECEIVER_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagereceiver_create, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(messagereceiver_open, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagereceiver_open, 1);

    REGISTER_GLOBAL_MOCK_RETURN(message_create_from_iothub_message, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_create_from_iothub_message, 1);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_map, TEST_LINK_ATTACH_PROPERTIES);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_map, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_symbol, TEST_LINK_DEVICE_TYPE_NAME_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_symbol, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_string, TEST_LINK_DEVICE_TYPE_VALUE_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_string, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_set_map_value, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_set_map_value, 1);

    REGISTER_GLOBAL_MOCK_RETURN(link_set_attach_properties, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_set_attach_properties, 1);

    REGISTER_GLOBAL_MOCK_RETURN(UniqueId_Generate, UNIQUEID_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(UniqueId_Generate, UNIQUEID_ERROR);

    REGISTER_GLOBAL_MOCK_RETURN(link_set_rcv_settle_mode, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_set_rcv_settle_mode, 1);

    REGISTER_GLOBAL_MOCK_RETURN(messaging_delivery_accepted, TEST_MESSAGE_DISPOSITION_ACCEPTED_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messaging_delivery_accepted, NULL);
    
    REGISTER_GLOBAL_MOCK_RETURN(messaging_delivery_released, TEST_MESSAGE_DISPOSITION_ABANDONED_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messaging_delivery_released, NULL);
    
    REGISTER_GLOBAL_MOCK_RETURN(messaging_delivery_rejected, TEST_MESSAGE_DISPOSITION_REJECTED_AMQP_VALUE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messaging_delivery_rejected, NULL);

	TEST_IOTHUB_MESSAGE_LIST_HANDLE = (IOTHUB_MESSAGE_LIST*)real_malloc(sizeof(IOTHUB_MESSAGE_LIST));
	ASSERT_IS_NOT_NULL(TEST_IOTHUB_MESSAGE_LIST_HANDLE);
	TEST_IOTHUB_MESSAGE_LIST_HANDLE->messageHandle = TEST_IOTHUB_MESSAGE_HANDLE;
}

TEST_SUITE_CLEANUP(TestClassCleanup)
{
	free(TEST_IOTHUB_MESSAGE_LIST_HANDLE);

    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(TestMethodInitialize)
{
    if (TEST_MUTEX_ACQUIRE(g_testByTest))
    {
        ASSERT_FAIL("our mutex is ABANDONED. Failure in test framework");
    }

    umock_c_reset_all_calls();

    g_STRING_sprintf_call_count = 0;
    g_STRING_sprintf_fail_on_count = -1;

    saved_malloc_returns_count = 0;

	TEST_WAIT_TO_SEND_LIST = TEST_WAIT_TO_SEND_LIST1;
	TEST_IN_PROGRESS_LIST = TEST_IN_PROGRESS_LIST1;

	TEST_singlylinkedlist_add_fail_return = false;
	saved_wait_to_send_list_count = 0;
	saved_wait_to_send_list_count2 = 0;
	saved_in_progress_list_count = 0;
	saved_in_progress_list_count2 = 0;
	
    saved_messagesender_create_link = NULL;
    saved_messagesender_create_on_message_sender_state_changed = NULL;
    saved_messagesender_create_context = NULL;

    saved_message_create_from_iothub_message = NULL;
    TEST_message_create_from_iothub_message_return = 0;

    saved_IoTHubMessage_CreateFromUamqpMessage_uamqp_message = NULL;
    TEST_IoTHubMessage_CreateFromUamqpMessage_return = 0;


    TEST_messagesender_send_result = 0;
    saved_messagesender_send_message_sender = NULL;
    saved_messagesender_send_message = NULL;
    saved_messagesender_send_on_message_send_complete = NULL;
    saved_messagesender_send_callback_context = NULL;

    saved_messagereceiver_create_link = NULL;
    saved_messagereceiver_create_on_message_receiver_state_changed = NULL;
    saved_messagereceiver_create_context = NULL;
    TEST_messagereceiver_create_result = TEST_MESSAGE_RECEIVER_HANDLE;

    saved_messagereceiver_open_message_receiver = NULL;
    saved_messagereceiver_open_on_message_received = NULL;
    saved_messagereceiver_open_callback_context = NULL;
    TEST_messagereceiver_open_result = 0;

    saved_on_new_message_received_callback_message = NULL;
    saved_on_new_message_received_callback_context = NULL;
    TEST_on_new_message_received_callback_result = MESSENGER_DISPOSITION_RESULT_ACCEPTED;

    TEST_link_set_max_message_size_result = 0;
    TEST_amqpvalue_set_map_value_result = 0;
    TEST_link_set_attach_properties_result = 0;

	TEST_on_event_send_complete_message = NULL;
	TEST_on_event_send_complete_result = MESSENGER_EVENT_SEND_COMPLETE_RESULT_OK;
	TEST_on_event_send_complete_context = NULL;
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_001: [If parameter `messenger_config` is NULL, messenger_create() shall return NULL]  
TEST_FUNCTION(messenger_create_NULL_config)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    MESSENGER_HANDLE handle = messenger_create(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, handle, NULL);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_002: [If `messenger_config->device_id` is NULL, messenger_create() shall return NULL]  
TEST_FUNCTION(messenger_create_config_NULL_device_id)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    config->device_id = NULL;

    umock_c_reset_all_calls();

    // act
    MESSENGER_HANDLE handle = messenger_create(config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, handle, NULL);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_003: [If `messenger_config->iothub_host_fqdn` is NULL, messenger_create() shall return NULL]  
TEST_FUNCTION(messenger_create_config_NULL_iothub_host_fqdn)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    config->iothub_host_fqdn = NULL;

    umock_c_reset_all_calls();

    // act
    MESSENGER_HANDLE handle = messenger_create(config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, handle, NULL);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_006: [messenger_create() shall allocate memory for the messenger instance structure (aka `instance`)]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_008: [messenger_create() shall save a copy of `messenger_config->device_id` into `instance->device_id`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_010: [messenger_create() shall save a copy of `messenger_config->iothub_host_fqdn` into `instance->iothub_host_fqdn`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_165: [`instance->wait_to_send_list` shall be set using singlylinkedlist_create()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_132: [`instance->in_progress_list` shall be set using singlylinkedlist_create()]   
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_013: [`messenger_config->on_state_changed_callback` shall be saved into `instance->on_state_changed_callback`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_014: [`messenger_config->on_state_changed_context` shall be saved into `instance->on_state_changed_context`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_015: [If no failures occurr, messenger_create() shall return a handle to `instance`]
TEST_FUNCTION(messenger_create_success)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_create(config);

    // act
    MESSENGER_HANDLE handle = messenger_create(config);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(handle);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_007: [If malloc() fails, messenger_create() shall fail and return NULL]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_009: [If STRING_construct() fails, messenger_create() shall fail and return NULL]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_011: [If STRING_construct() fails, messenger_create() shall fail and return NULL] 
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_166: [If singlylinkedlist_create() fails, messenger_create() shall fail and return NULL]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_133: [If singlylinkedlist_create() fails, messenger_create() shall fail and return NULL]
TEST_FUNCTION(messenger_create_failure_checks)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

    MESSENGER_CONFIG* config = get_messenger_config();

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_create(config);
    umock_c_negative_tests_snapshot();

    // act
    size_t i;
    for (i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (i == 3)
        {
            // These expected calls do not cause the API to fail.
            continue;
        }

        // arrange
        char error_msg[64];

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        MESSENGER_HANDLE handle = messenger_create(config);

        // assert
        sprintf(error_msg, "On failed call %zu", i);
        ASSERT_IS_NULL_WITH_MSG(handle, error_msg);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_029: [If `messenger_handle` is NULL, messenger_start() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_start_NULL_messenger_handle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    int result = messenger_start(NULL, TEST_SESSION_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_030: [If `session_handle` is NULL, messenger_start() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_start_NULL_session_handle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    int result = messenger_start(TEST_MESSENGER_HANDLE, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_031: [If `instance->state` is not MESSENGER_STATE_STOPPED, messenger_start() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_start_messenger_not_stopped)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger(config);

    // act
    int result = messenger_start(handle, TEST_SESSION_HANDLE);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_032: [`session_handle` shall be saved on `instance->session_handle`]   
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_115: [If no failures occurr, `instance->state` shall be set to MESSENGER_STATE_STARTING, and `instance->on_state_changed_callback` invoked if provided]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_056: [If no failures occurr, messenger_start() shall return 0]
TEST_FUNCTION(messenger_start_succeeds)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_create(config);
    MESSENGER_HANDLE handle = messenger_create(config);

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_start(config, handle);

    TEST_link_set_attach_properties_result = 1;

    // act
    int result = messenger_start(handle, TEST_SESSION_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);
	ASSERT_ARE_EQUAL(int, MESSENGER_STATE_STOPPED, saved_on_state_changed_callback_previous_state);
	ASSERT_ARE_EQUAL(int, MESSENGER_STATE_STARTING, saved_on_state_changed_callback_new_state);

    // cleanup
    messenger_destroy(handle);
}


// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_118: [If the messagesender new state is MESSAGE_SENDER_STATE_OPEN, `instance->state` shall be set to MESSENGER_STATE_STARTED, and `instance->on_state_changed_callback` invoked if provided]  
TEST_FUNCTION(messenger_state_on_event_sender_state_changed_callback_OPEN)
{
    // arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger(config);

	time_t current_time = time(NULL);

	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTING, false, false, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	set_expected_calls_for_messenger_do_work(do_work_profile);
	messenger_do_work(handle);
    
    // act
    ASSERT_IS_NOT_NULL(saved_messagesender_create_on_message_sender_state_changed);

    saved_messagesender_create_on_message_sender_state_changed((void*)handle, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
	crank_messenger_do_work(handle, do_work_profile);

    // assert
    ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_previous_state, MESSENGER_STATE_STARTING);
    ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_new_state, MESSENGER_STATE_STARTED);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_119: [If the messagesender new state is MESSAGE_SENDER_STATE_ERROR, `instance->state` shall be set to MESSENGER_STATE_ERROR, and `instance->on_state_changed_callback` invoked if provided]  
TEST_FUNCTION(messenger_state_on_event_sender_state_changed_callback_ERROR)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger(config);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTING, false, false, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	set_expected_calls_for_messenger_do_work(do_work_profile);
	messenger_do_work(handle);

    // act
    ASSERT_IS_NOT_NULL(saved_messagesender_create_on_message_sender_state_changed);

    saved_messagesender_create_on_message_sender_state_changed((void*)handle, MESSAGE_SENDER_STATE_ERROR, MESSAGE_SENDER_STATE_IDLE);
	crank_messenger_do_work(handle, do_work_profile);

    // assert
    ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_previous_state, MESSENGER_STATE_STARTING);
    ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_new_state, MESSENGER_STATE_ERROR);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_057: [If `messenger_handle` is NULL, messenger_stop() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_stop_NULL_handle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    int result = messenger_stop(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_058: [If `instance->state` is MESSENGER_STATE_STOPPED, messenger_stop() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_stop_messenger_not_started)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_create(config);
    MESSENGER_HANDLE handle = messenger_create(config);

    umock_c_reset_all_calls();

    // act
    int result = messenger_stop(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_116: [`instance->state` shall be set to MESSENGER_STATE_STOPPING, and `instance->on_state_changed_callback` invoked if provided]  
TEST_FUNCTION(messenger_stop_succeeds)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_stop(0, 0, true);

    // act
    int result = messenger_stop(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, result, 0);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_109: [If `messenger_handle` is NULL, messenger_destroy() shall fail and return]  
TEST_FUNCTION(messenger_destroy_NULL_handle)
{
    // arrange
    umock_c_reset_all_calls();
    
    // act
    messenger_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_152: [If `instance->state` is MESSENGER_STATE_STOPPING, messenger_do_work() shall close and destroy `instance->message_sender` and `instance->message_receiver`] 
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_060: [`instance->message_sender` shall be destroyed using messagesender_destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_061: [`instance->message_receiver` shall be closed using messagereceiver_close()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_062: [`instance->message_receiver` shall be destroyed using messagereceiver_destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_063: [`instance->sender_link` shall be destroyed using link_destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_064: [`instance->receiver_link` shall be destroyed using link_destroy()] 
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_162: [If `instance->state` is MESSENGER_STATE_STOPPING, messenger_do_work() shall move all items from `instance->in_progress_list` to the beginning of `instance->wait_to_send_list`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_164: [If all items get successfuly moved back to `instance->wait_to_send_list`, `instance->state` shall be set to MESSENGER_STATE_STOPPED, and `instance->on_state_changed_callback` invoked]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_110: [If the `instance->state` is not MESSENGER_STATE_STOPPED, messenger_destroy() shall invoke messenger_stop() and messenger_do_work() once]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_111: [All elements of `instance->in_progress_list` and `instance->wait_to_send_list` shall be removed, invoking `task->on_event_send_complete_callback` for each with MESSENGER_EVENT_SEND_COMPLETE_RESULT_MESSENGER_DESTROYED]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_150: [`instance->in_progress_list` and `instance->wait_to_send_list` shall be destroyed using singlylinkedlist_destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_112: [`instance->iothub_host_fqdn` shall be destroyed using STRING_delete()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_113: [`instance->device_id` shall be destroyed using STRING_delete()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_114: [messenger_destroy() shall destroy `instance` with free()] 
TEST_FUNCTION(messenger_destroy_succeeds)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	ASSERT_ARE_EQUAL(int, 1, send_events(handle, 1));

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE* mdecp = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, true, true, 1, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	crank_messenger_do_work(handle, mdecp);

	ASSERT_ARE_EQUAL(int, 1, send_events(handle, 1));

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_destroy(config, handle, true, true, 1, 1);

    // act
    messenger_destroy(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_163: [If not all items from `instance->in_progress_list` can be moved back to `instance->wait_to_send_list`, `instance->state` shall be set to MESSENGER_STATE_ERROR, and `instance->on_state_changed_callback` invoked]
TEST_FUNCTION(messenger_destroy_FAIL_TO_ROLLBACK_EVENTS)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	ASSERT_ARE_EQUAL(int, 1, send_events(handle, 1));

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE* mdecp = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, true, true, 1, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	crank_messenger_do_work(handle, mdecp);

	ASSERT_ARE_EQUAL(int, 1, send_events(handle, 1));

	umock_c_reset_all_calls();
	set_expected_calls_for_message_sender_destroy();
	set_expected_calls_for_message_receiver_destroy();
	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST));
	STRICT_EXPECTED_CALL(singlylinkedlist_create()).SetReturn(NULL);

	// act
	messenger_destroy(handle);

	// assert
	ASSERT_ARE_EQUAL(int, MESSENGER_STATE_STOPPING, saved_on_state_changed_callback_previous_state);
	ASSERT_ARE_EQUAL(int, MESSENGER_STATE_ERROR, saved_on_state_changed_callback_new_state);

	// cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_065: [If `messenger_handle` is NULL, messenger_do_work() shall fail and return]
TEST_FUNCTION(messenger_do_work_NULL_handle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    messenger_do_work(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_066: [If `instance->state` is not MESSENGER_STATE_STARTED, messenger_do_work() shall return]
TEST_FUNCTION(messenger_do_work_not_started)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();

    umock_c_reset_all_calls();
    set_expected_calls_for_messenger_create(config);
    MESSENGER_HANDLE handle = messenger_create(config);

    umock_c_reset_all_calls();

    // act
    messenger_do_work(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_151: [If `instance->state` is MESSENGER_STATE_STARTING, messenger_do_work() shall create and open `instance->message_sender`]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_033: [A variable, named `devices_path`, shall be created concatenating `instance->iothub_host_fqdn`, "/devices/" and `instance->device_id`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_035: [A variable, named `event_send_address`, shall be created concatenating "amqps://", `devices_path` and "/messages/events"]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_037: [A `link_name` variable shall be created using an unique string label per AMQP session]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_039: [A `source` variable shall be created with messaging_create_source() using an unique string label per AMQP session]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_041: [A `target` variable shall be created with messaging_create_target() using `event_send_address`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_043: [`instance->sender_link` shall be set using link_create(), passing `instance->session_handle`, `link_name`, "role_sender", `source` and `target` as parameters]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_047: [`instance->sender_link` maximum message size shall be set to UINT64_MAX using link_set_max_message_size()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_049: [`instance->sender_link` should have a property "com.microsoft:client-version" set as `CLIENT_DEVICE_TYPE_PREFIX/IOTHUB_SDK_VERSION`, using amqpvalue_set_map_value() and link_set_attach_properties()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_051: [`instance->message_sender` shall be created using messagesender_create(), passing the `instance->sender_link` and `on_event_sender_state_changed_callback`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_053: [`instance->message_sender` shall be opened using messagesender_open()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_055: [Before returning, messenger_do_work() shall release all the temporary memory it has allocated]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_153: [messenger_do_work() shall move each event to be sent from `instance->wait_to_send_list` to `instance->in_progress_list`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_154: [A MESSAGE_HANDLE shall be obtained out of the event's IOTHUB_MESSAGE_HANDLE instance by using message_create_from_iothub_message()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_157: [The MESSAGE_HANDLE shall be submitted for sending using messagesender_send(), passing `internal_on_event_send_complete_callback`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_159: [The MESSAGE_HANDLE shall be destroyed using message_destroy().] 
TEST_FUNCTION(messenger_do_work_send_events_success)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	set_expected_calls_for_messenger_send_async();
	int result = messenger_send_async(handle, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete, TEST_IOTHUB_CLIENT_HANDLE);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, false, false, 1, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);

	umock_c_reset_all_calls();
	set_expected_calls_for_messenger_do_work(do_work_profile);

    // act
    messenger_do_work(handle);

    // assert
	ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_067: [If `instance->receive_messages` is true and `instance->message_receiver` is NULL, a message_receiver shall be created]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_068: [A variable, named `devices_path`, shall be created concatenating `instance->iothub_host_fqdn`, "/devices/" and `instance->device_id`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_070: [A variable, named `message_receive_address`, shall be created concatenating "amqps://", `devices_path` and "/messages/devicebound"]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_072: [A `link_name` variable shall be created using an unique string label per AMQP session]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_074: [A `target` variable shall be created with messaging_create_target() using an unique string label per AMQP session]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_076: [A `source` variable shall be created with messaging_create_source() using `message_receive_address`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_078: [`instance->receiver_link` shall be set using link_create(), passing `instance->session_handle`, `link_name`, "role_receiver", `source` and `target` as parameters]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_080: [`instance->receiver_link` settle mode shall be set to "receiver_settle_mode_first" using link_set_rcv_settle_mode(), ]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_082: [`instance->receiver_link` maximum message size shall be set to 65536 using link_set_max_message_size()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_084: [`instance->receiver_link` should have a property "com.microsoft:client-version" set as `CLIENT_DEVICE_TYPE_PREFIX/IOTHUB_SDK_VERSION`, using amqpvalue_set_map_value() and link_set_attach_properties()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_086: [`instance->message_receiver` shall be created using messagereceiver_create(), passing the `instance->receiver_link` and `on_messagereceiver_state_changed_callback`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_088: [`instance->message_receiver` shall be opened using messagereceiver_open(), passing `on_message_received_internal_callback`]
TEST_FUNCTION(messenger_do_work_create_message_receiver)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

    (void)messenger_subscribe_for_messages(handle, TEST_on_new_message_received_callback, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, true, false, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	umock_c_reset_all_calls();
	set_expected_calls_for_messenger_do_work(do_work_profile);

    // act
    messenger_do_work(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_069: [If `devices_path` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_071: [If `message_receive_address` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_073: [If `link_name` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_075: [If `target` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_077: [If `source` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_079: [If link_create() fails, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_081: [If link_set_rcv_settle_mode() fails, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_083: [If link_set_max_message_size() fails, it shall be logged and ignored.]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_085: [If amqpvalue_set_map_value() or link_set_attach_properties() fail, the failure shall be ignored]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_087: [If messagereceiver_create() fails, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_089: [If messagereceiver_open() fails, messenger_do_work() shall fail and return]
TEST_FUNCTION(messenger_do_work_create_message_receiver_failure_checks)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger(config);

    (void)messenger_subscribe_for_messages(handle, TEST_on_new_message_received_callback, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);

    umock_c_reset_all_calls();
    set_expected_calls_for_message_receiver_create();
    umock_c_negative_tests_snapshot();

    saved_messagereceiver_open_on_message_received = NULL;

    // act
    size_t i;
    for (i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (i == 1 || i == 2 || i == 4 || i == 5 || i == 9 || i == 11 || i == 12 || 
            i == 14 || i == 16 || i == 19 || (i >= 20 && i <= 27) || (i >= 30 && i <= 35) )
        {
            // These expected calls do not cause the API to fail.
            continue;
        }

        // arrange
        char error_msg[64];

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        // act
        messenger_do_work(handle);

        // assert
        sprintf(error_msg, "On failed call %zu", i);
        ASSERT_IS_TRUE_WITH_MSG(saved_messagereceiver_open_on_message_received == NULL, error_msg);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_034: [If `devices_path` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_036: [If `event_send_address` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_038: [If `link_name` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_040: [If `source` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_042: [If `target` fails to be created, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_044: [If link_create() fails, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_048: [If link_set_max_message_size() fails, it shall be logged and ignored.]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_050: [If amqpvalue_set_map_value() or link_set_attach_properties() fail, the failure shall be ignored]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_052: [If messagesender_create() fails, messenger_do_work() shall fail and return]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_054: [If messagesender_open() fails, messenger_do_work() shall fail and return]  
TEST_FUNCTION(messenger_do_work_create_message_sender_failure_checks)
{
	// arrange
	ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

	MESSENGER_CONFIG* config = get_messenger_config();

	// act
	size_t i;
	size_t n = 10;
	for (i = 0; i < n; i++)
	{
		if (i == 1 || i == 2 || i == 4 || i == 5 || i == 9 || i == 11 || i == 12 || 
			i == 14 || i == 16 || i == 18 || (i >= 19 && i <= 26) || (i >= 29 && i <= 34)) 
		{
			continue; // These expected calls do not cause the API to fail.
		}

		// arrange
		set_expected_calls_for_messenger_create(config);
		MESSENGER_HANDLE handle = create_and_start_messenger(config);
		
		umock_c_reset_all_calls();
		set_expected_calls_for_message_sender_create();
		umock_c_negative_tests_snapshot();
		n = umock_c_negative_tests_call_count();

		char error_msg[64];

		umock_c_negative_tests_reset();
		umock_c_negative_tests_fail_call(i);

		// act
		messenger_do_work(handle);

		// assert
		sprintf(error_msg, "On failed call %zu", i);
		
		ASSERT_ARE_EQUAL_WITH_MSG(int, MESSENGER_STATE_STARTING, saved_on_state_changed_callback_previous_state, error_msg);
		ASSERT_ARE_EQUAL_WITH_MSG(int, MESSENGER_STATE_ERROR, saved_on_state_changed_callback_new_state, error_msg);

		// cleanup
		messenger_destroy(handle);
	}

	// cleanup
	umock_c_negative_tests_deinit();
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_120: [If the messagereceiver new state is MESSAGE_RECEIVER_STATE_ERROR, `instance->state` shall be set to MESSENGER_STATE_ERROR, and `instance->on_state_changed_callback` invoked if provided]
TEST_FUNCTION(messenger_state_on_message_receiver_state_changed_callback_ERROR)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	STRICT_EXPECTED_CALL(get_time(NULL)).SetReturn(time(NULL));

    // act
    ASSERT_IS_NOT_NULL(saved_messagereceiver_create_on_message_receiver_state_changed);

    saved_messagereceiver_create_on_message_receiver_state_changed(saved_messagereceiver_create_context, MESSAGE_RECEIVER_STATE_ERROR, MESSAGE_RECEIVER_STATE_OPEN);
	
	umock_c_reset_all_calls();
	messenger_do_work(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_previous_state, MESSENGER_STATE_STARTED);
    ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_new_state, MESSENGER_STATE_ERROR);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_092: [If `instance->receive_messages` is false and `instance->message_receiver` is not NULL, it shall be destroyed]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_093: [`instance->message_receiver` shall be closed using messagereceiver_close()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_094: [If messagereceiver_close() fails, it shall be logged and ignored]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_095: [`instance->message_receiver` shall be destroyed using messagereceiver_destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_096: [`instance->message_receiver` shall be set to NULL]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_097: [`instance->receiver_link` shall be destroyed using link_destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_098: [`instance->receiver_link` shall be set to NULL]
TEST_FUNCTION(messenger_do_work_destroy_message_receiver)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    (void)messenger_unsubscribe_for_messages(handle);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, false, true, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	umock_c_reset_all_calls();
	set_expected_calls_for_messenger_do_work(do_work_profile);

    // act
    messenger_do_work(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_107: [If no failure occurs, `task->on_event_send_complete_callback` shall be invoked with result EVENT_SEND_COMPLETE_RESULT_OK]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_128: [`task` shall be removed from `instance->in_progress_list`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_130: [`task` shall be destroyed using free()]  
TEST_FUNCTION(messenger_do_work_on_event_send_complete_OK)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	send_events(handle, 1);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *mdwp = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, false, false, 1, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	crank_messenger_do_work(handle, mdwp);

	umock_c_reset_all_calls();
	set_expected_calls_for_on_message_send_complete();

    // act
    ASSERT_IS_NOT_NULL(saved_messagesender_send_on_message_send_complete);

    saved_messagesender_send_on_message_send_complete(saved_messagesender_send_callback_context, MESSAGE_SEND_OK);

    // assert
	ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
	ASSERT_ARE_EQUAL(void_ptr, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete_message);
	ASSERT_ARE_EQUAL(int, MESSENGER_EVENT_SEND_COMPLETE_RESULT_OK, TEST_on_event_send_complete_result);
	ASSERT_ARE_EQUAL(void_ptr, TEST_IOTHUB_CLIENT_HANDLE, TEST_on_event_send_complete_context);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_108: [If a failure occurred, `task->on_event_send_complete_callback` shall be invoked with result EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_128: [`task` shall be removed from `instance->in_progress_list`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_130: [`task` shall be destroyed using free()]  
TEST_FUNCTION(messenger_do_work_on_event_send_complete_ERROR)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	send_events(handle, 1);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *mdwp = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, false, false, 1, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	crank_messenger_do_work(handle, mdwp);

    umock_c_reset_all_calls();
    set_expected_calls_for_on_message_send_complete();

    // act
    ASSERT_IS_NOT_NULL(saved_messagesender_send_on_message_send_complete);

    saved_messagesender_send_on_message_send_complete(saved_messagesender_send_callback_context, MESSAGE_SEND_ERROR);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
	ASSERT_ARE_EQUAL(void_ptr, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete_message);
	ASSERT_ARE_EQUAL(int, MESSENGER_EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING, TEST_on_event_send_complete_result);
    ASSERT_ARE_EQUAL(void_ptr, TEST_IOTHUB_CLIENT_HANDLE, TEST_on_event_send_complete_context);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_155: [If message_create_from_iothub_message() fails, `task->on_event_send_complete_callback` shall be invoked with result EVENT_SEND_COMPLETE_RESULT_ERROR_CANNOT_PARSE]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_156: [If message_create_from_iothub_message() fails, messenger_do_work() shall skip to the next event to be sent]
TEST_FUNCTION(messenger_do_work_send_events_message_create_from_iothub_message_fails)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	ASSERT_ARE_EQUAL(int, 1, send_events(handle, 1));

	umock_c_reset_all_calls();
	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST)).SetReturn(NULL);
	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST));
	EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
	STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_WAIT_TO_SEND_LIST, IGNORED_PTR_ARG))
		.IgnoreArgument(2);
	STRICT_EXPECTED_CALL(singlylinkedlist_add(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG))
		.IgnoreArgument(2);
	STRICT_EXPECTED_CALL(message_create_from_iothub_message(TEST_IOTHUB_MESSAGE_HANDLE, IGNORED_PTR_ARG))
		.IgnoreArgument(2).SetReturn(1);
	STRICT_EXPECTED_CALL(singlylinkedlist_find(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
		.IgnoreArgument_match_context()
		.IgnoreArgument_match_function();
	STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG))
		.IgnoreArgument(2);
	EXPECTED_CALL(free(IGNORED_PTR_ARG));
	STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST)).SetReturn(NULL);

    // act
    messenger_do_work(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
	ASSERT_ARE_EQUAL(void_ptr, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete_message);
	ASSERT_ARE_EQUAL(int, MESSENGER_EVENT_SEND_COMPLETE_RESULT_ERROR_CANNOT_PARSE, TEST_on_event_send_complete_result);
	ASSERT_ARE_EQUAL(void_ptr, TEST_IOTHUB_CLIENT_HANDLE, TEST_on_event_send_complete_context);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_158: [If messagesender_send() fails, `task->on_event_send_complete_callback` shall be invoked with result EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_160: [If any failure occurs the event shall be removed from `instance->in_progress_list` and destroyed]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_161: [If messenger_do_work() fail sending events for `instance->event_send_retry_limit` times in a row, it shall invoke `instance->on_state_changed_callback`, if provided, with error code MESSENGER_STATE_ERROR]
TEST_FUNCTION(messenger_do_work_send_events_messagesender_send_fails)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

    umock_c_reset_all_calls();

    int i;
    for (i = 0; i < DEFAULT_EVENT_SEND_RETRY_LIMIT; i++)
    {
        // arrange
		ASSERT_ARE_EQUAL(int, 1, send_events(handle, 1));

		umock_c_reset_all_calls();
		// timeout checks
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_IN_PROGRESS_LIST)).SetReturn(NULL);
		
		// send events
		STRICT_EXPECTED_CALL(singlylinkedlist_get_head_item(TEST_WAIT_TO_SEND_LIST));
		EXPECTED_CALL(singlylinkedlist_item_get_value(IGNORED_PTR_ARG));
		STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_WAIT_TO_SEND_LIST, IGNORED_PTR_ARG))
			.IgnoreArgument(2);
		STRICT_EXPECTED_CALL(singlylinkedlist_add(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG))
			.IgnoreArgument(2);
		STRICT_EXPECTED_CALL(message_create_from_iothub_message(TEST_IOTHUB_MESSAGE_HANDLE, IGNORED_PTR_ARG))
			.IgnoreArgument(2);
		STRICT_EXPECTED_CALL(messagesender_send(TEST_MESSAGE_SENDER_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
			.IgnoreArgument(2).IgnoreArgument(3).IgnoreArgument(4).SetReturn(1);
		EXPECTED_CALL(get_time(NULL)).SetReturn(INDEFINITE_TIME);
		EXPECTED_CALL(message_destroy(IGNORED_PTR_ARG));
		STRICT_EXPECTED_CALL(singlylinkedlist_find(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
			.IgnoreArgument_match_function()
			.IgnoreArgument_match_context();
		STRICT_EXPECTED_CALL(singlylinkedlist_remove(TEST_IN_PROGRESS_LIST, IGNORED_PTR_ARG)).IgnoreArgument(2);
		EXPECTED_CALL(free(IGNORED_PTR_ARG));

        // act
        messenger_do_work(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
		ASSERT_ARE_EQUAL(int, MESSENGER_EVENT_SEND_COMPLETE_RESULT_ERROR_FAIL_SENDING, TEST_on_event_send_complete_result);

        if (i < (DEFAULT_EVENT_SEND_RETRY_LIMIT - 1))
        {
            ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_new_state, MESSENGER_STATE_STARTED);
        }
        else
        {
            ASSERT_ARE_EQUAL(int, saved_on_state_changed_callback_new_state, MESSENGER_STATE_ERROR);
        }
    }

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_016: [If `messenger_handle` is NULL, messenger_subscribe_for_messages() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_subscribe_for_messages_NULL_handle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    int result = messenger_subscribe_for_messages(NULL, TEST_on_new_message_received_callback, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_018: [If `on_message_received_callback` is NULL, messenger_subscribe_for_messages() shall fail and return __LINE__] 
TEST_FUNCTION(messenger_subscribe_for_messages_NULL_callback)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger(config);

    umock_c_reset_all_calls();

    // act
    int result = messenger_subscribe_for_messages(handle, NULL, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_017: [If `instance->receive_messages` is already true, messenger_subscribe_for_messages() shall fail and return __LINE__]  
TEST_FUNCTION(messenger_subscribe_for_messages_already_subscribed)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger(config);

    (void)messenger_subscribe_for_messages(handle, TEST_on_new_message_received_callback, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);
    umock_c_reset_all_calls();

    // act
    int result = messenger_subscribe_for_messages(handle, TEST_on_new_message_received_callback, TEST_ON_NEW_MESSAGE_RECEIVED_CB_CONTEXT);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_023: [If `messenger_handle` is NULL, messenger_unsubscribe_for_messages() shall fail and return __LINE__]
TEST_FUNCTION(messenger_unsubscribe_for_messages_NULL_handle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    int result = messenger_unsubscribe_for_messages(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_024: [If `instance->receive_messages` is already false, messenger_unsubscribe_for_messages() shall fail and return __LINE__]
TEST_FUNCTION(messenger_unsubscribe_for_messages_not_subscribed)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger(config);

    umock_c_reset_all_calls();

    // act
    int result = messenger_unsubscribe_for_messages(handle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, result, 0);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_019: [`on_message_received_callback` shall be saved on `instance->on_message_received_callback`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_020: [`context` shall be saved on `instance->on_message_received_context`]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_021: [messenger_subscribe_for_messages() shall set `instance->receive_messages` to true]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_022: [If no failures occurr, messenger_subscribe_for_messages() shall return 0]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_121: [An IOTHUB_MESSAGE_HANDLE shall be obtained from MESSAGE_HANDLE using IoTHubMessage_CreateFromUamqpMessage()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_123: [`instance->on_message_received_callback` shall be invoked passing the IOTHUB_MESSAGE_HANDLE]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_124: [The IOTHUB_MESSAGE_HANDLE instance shall be destroyed using IoTHubMessage_Destroy()]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_125: [If `instance->on_message_received_callback` returns MESSENGER_DISPOSITION_RESULT_ACCEPTED, on_message_received_internal_callback shall return the result of messaging_delivery_accepted()]
TEST_FUNCTION(messenger_on_message_received_internal_callback_ACCEPTED)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    umock_c_reset_all_calls();
    set_expected_calls_for_on_message_received_internal_callback(MESSENGER_DISPOSITION_RESULT_ACCEPTED);

    // act
    ASSERT_IS_NOT_NULL(saved_messagereceiver_open_on_message_received);

    AMQP_VALUE result = saved_messagereceiver_open_on_message_received(saved_messagereceiver_open_callback_context, TEST_MESSAGE_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, result, TEST_MESSAGE_DISPOSITION_ACCEPTED_AMQP_VALUE);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_126: [If `instance->on_message_received_callback` returns MESSENGER_DISPOSITION_RESULT_ABANDONED, on_message_received_internal_callback shall return the result of messaging_delivery_released()]
TEST_FUNCTION(messenger_on_message_received_internal_callback_ABANDONED)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    umock_c_reset_all_calls();
    set_expected_calls_for_on_message_received_internal_callback(MESSENGER_DISPOSITION_RESULT_ABANDONED);

    // act
    ASSERT_IS_NOT_NULL(saved_messagereceiver_open_on_message_received);

    AMQP_VALUE result = saved_messagereceiver_open_on_message_received(saved_messagereceiver_open_callback_context, TEST_MESSAGE_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, result, TEST_MESSAGE_DISPOSITION_ABANDONED_AMQP_VALUE);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_127: [If `instance->on_message_received_callback` returns MESSENGER_DISPOSITION_RESULT_REJECTED, on_message_received_internal_callback shall return the result of messaging_delivery_rejected()]  
TEST_FUNCTION(messenger_on_message_received_internal_callback_REJECTED)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    umock_c_reset_all_calls();
    set_expected_calls_for_on_message_received_internal_callback(MESSENGER_DISPOSITION_RESULT_REJECTED);

    // act
    ASSERT_IS_NOT_NULL(saved_messagereceiver_open_on_message_received);

    AMQP_VALUE result = saved_messagereceiver_open_on_message_received(saved_messagereceiver_open_callback_context, TEST_MESSAGE_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, result, TEST_MESSAGE_DISPOSITION_REJECTED_AMQP_VALUE);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_122: [If IoTHubMessage_CreateFromUamqpMessage() fails, on_message_received_internal_callback shall return the result of messaging_delivery_rejected()]
TEST_FUNCTION(messenger_on_message_received_internal_callback_IoTHubMessage_CreateFromUamqpMessage_fails)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    umock_c_reset_all_calls();
    TEST_on_new_message_received_callback_result = MESSENGER_DISPOSITION_RESULT_ACCEPTED;
    STRICT_EXPECTED_CALL(IoTHubMessage_CreateFromUamqpMessage(TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(2)
        .SetReturn(1);
    STRICT_EXPECTED_CALL(messaging_delivery_rejected("Rejected due to failure reading AMQP message", "Failed reading AMQP message"));

    // act
    ASSERT_IS_NOT_NULL(saved_messagereceiver_open_on_message_received);

    AMQP_VALUE result = saved_messagereceiver_open_on_message_received(saved_messagereceiver_open_callback_context, TEST_MESSAGE_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, result, TEST_MESSAGE_DISPOSITION_REJECTED_AMQP_VALUE);

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_025: [messenger_unsubscribe_for_messages() shall set `instance->receive_messages` to false]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_026: [messenger_unsubscribe_for_messages() shall set `instance->on_message_received_callback` to NULL]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_027: [messenger_unsubscribe_for_messages() shall set `instance->on_message_received_context` to NULL]  
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_028: [If no failures occurr, messenger_unsubscribe_for_messages() shall return 0]
TEST_FUNCTION(messenger_unsubscribe_for_messages_success)
{
    // arrange
    MESSENGER_CONFIG* config = get_messenger_config();
    MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // act
    int unsubscription_result = messenger_unsubscribe_for_messages(handle);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *do_work_profile = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, false, true, 0, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	crank_messenger_do_work(handle, do_work_profile);

    // assert
    ASSERT_ARE_EQUAL(int, unsubscription_result, 0);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_134: [If `messenger_handle` is NULL, messenger_send_async() shall fail and return a non-zero value]  
TEST_FUNCTION(messenger_send_async_NULL_handle)
{
	// arrange

	// act
	int result = messenger_send_async(NULL, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete, TEST_IOTHUB_CLIENT_HANDLE);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);

	// cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_135: [If `message` is NULL, messenger_send_async() shall fail and return a non-zero value]  
TEST_FUNCTION(messenger_send_async_NULL_message)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	// act
	int result = messenger_send_async(handle, NULL, TEST_on_event_send_complete, TEST_IOTHUB_CLIENT_HANDLE);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_136: [If `on_event_send_complete_callback` is NULL, messenger_send_async() shall fail and return a non-zero value] 
TEST_FUNCTION(messenger_send_async_NULL_callback)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	// act
	int result = messenger_send_async(handle, TEST_IOTHUB_MESSAGE_LIST_HANDLE, NULL, TEST_IOTHUB_CLIENT_HANDLE);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_138: [If malloc() fails, messenger_send_async() shall fail and return a non-zero value]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_139: [If singlylinkedlist_add() fails, messenger_send_async() shall fail and return a non-zero value]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_142: [If any failure occurs, messenger_send_async() shall free any memory it has allocated]
TEST_FUNCTION(messenger_send_async_failure_checks)
{
	// arrange
	ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	umock_c_reset_all_calls();
	set_expected_calls_for_messenger_send_async();
	umock_c_negative_tests_snapshot();

	// act
	size_t i;
	for (i = 0; i < umock_c_negative_tests_call_count(); i++)
	{
		// arrange
		char error_msg[64];

		umock_c_negative_tests_reset();
		umock_c_negative_tests_fail_call(i);

		// act
		int result = messenger_send_async(handle, TEST_IOTHUB_MESSAGE_LIST_HANDLE, TEST_on_event_send_complete, TEST_IOTHUB_CLIENT_HANDLE);
		
		// assert
		sprintf(error_msg, "On failed call %zu", i);
		ASSERT_ARE_NOT_EQUAL_WITH_MSG(int, 0, result, error_msg);
	}

	// cleanup
	umock_c_negative_tests_deinit();
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_144: [If `messenger_handle` is NULL, messenger_get_send_status() shall fail and return a non-zero value] 
TEST_FUNCTION(messenger_get_send_status_NULL_handle)
{
	// act
	MESSENGER_SEND_STATUS send_status;
	int result = messenger_get_send_status(NULL, &send_status);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_145: [If `send_status` is NULL, messenger_get_send_status() shall fail and return a non-zero value] 
TEST_FUNCTION(messenger_get_send_status_NULL_send_status)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	// act
	int result = messenger_get_send_status(handle, NULL);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_147: [If `instance->in_progress_list` and `instance->wait_to_send_list` are empty, send_status shall be set to MESSENGER_SEND_STATUS_IDLE] 
TEST_FUNCTION(messenger_get_send_status_IDLE_succeeds)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, true);

	// act
	MESSENGER_SEND_STATUS send_status;
	int result = messenger_get_send_status(handle, &send_status);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	ASSERT_ARE_EQUAL(int, MESSENGER_SEND_STATUS_IDLE, send_status);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_148: [Otherwise, send_status shall be set to MESSENGER_SEND_STATUS_BUSY] 
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_149: [If no failures occur, messenger_get_send_status() shall return 0]
TEST_FUNCTION(messenger_get_send_status_BUSY_succeeds)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	send_events(handle, 1);

	// act
	MESSENGER_SEND_STATUS send_status_wts;
	int result_wts = messenger_get_send_status(handle, &send_status_wts);

	time_t current_time = time(NULL);
	MESSENGER_DO_WORK_EXP_CALL_PROFILE *mdwp = get_msgr_do_work_exp_call_profile(MESSENGER_STATE_STARTED, false, false, 1, 0, current_time, DEFAULT_EVENT_SEND_TIMEOUT_SECS);
	crank_messenger_do_work(handle, mdwp);

	ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

	MESSENGER_SEND_STATUS send_status_ip;
	int result_ip = messenger_get_send_status(handle, &send_status_ip);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result_wts);
	ASSERT_ARE_EQUAL(int, MESSENGER_SEND_STATUS_BUSY, send_status_wts);
	ASSERT_ARE_EQUAL(int, 0, result_ip);
	ASSERT_ARE_EQUAL(int, MESSENGER_SEND_STATUS_BUSY, send_status_ip);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_167: [If `messenger_handle` or `name` or `value` is NULL, messenger_set_option shall fail and return a non-zero value]
TEST_FUNCTION(messenger_set_option_NULL_handle)
{
	// arrange
	size_t value = 100;

	// act
	int result = messenger_set_option(NULL, MESSENGER_OPTION_EVENT_SEND_TIMEOUT_SECS, &value);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);

	// cleanup
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_167: [If `messenger_handle` or `name` or `value` is NULL, messenger_set_option shall fail and return a non-zero value]
TEST_FUNCTION(messenger_set_option_NULL_name)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	size_t value = 100;

	// act
	int result = messenger_set_option(handle, NULL, &value);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	ASSERT_IS_NOT_NULL(handle);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_167: [If `messenger_handle` or `name` or `value` is NULL, messenger_set_option shall fail and return a non-zero value]
TEST_FUNCTION(messenger_set_option_NULL_value)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	// act
	int result = messenger_set_option(handle, MESSENGER_OPTION_EVENT_SEND_TIMEOUT_SECS, NULL);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	ASSERT_IS_NOT_NULL(handle);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_168: [If name matches MESSENGER_OPTION_EVENT_SEND_TIMEOUT_SECS, `value` shall be saved on `instance->event_send_timeout_secs`]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_172: [If no errors occur, messenger_set_option shall return 0]
TEST_FUNCTION(messenger_set_option_EVENT_SEND_TIMEOUT_SECS)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	size_t value = 100;

	// act
	int result = messenger_set_option(handle, MESSENGER_OPTION_EVENT_SEND_TIMEOUT_SECS, &value);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	ASSERT_IS_NOT_NULL(handle);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_169: [If name matches MESSENGER_OPTION_SAVED_OPTIONS, `value` shall be applied using OptionHandler_FeedOptions]
TEST_FUNCTION(messenger_set_option_SAVED_OPTIONS)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	OPTIONHANDLER_HANDLE value = TEST_OPTIONHANDLER_HANDLE;
	STRICT_EXPECTED_CALL(OptionHandler_FeedOptions(value, handle)).SetReturn(OPTIONHANDLER_OK);

	// act
	int result = messenger_set_option(handle, MESSENGER_OPTION_SAVED_OPTIONS, value);

	// assert
	ASSERT_ARE_EQUAL(int, 0, result);
	ASSERT_IS_NOT_NULL(handle);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_170: [If OptionHandler_FeedOptions fails, messenger_set_option shall fail and return a non-zero value]
TEST_FUNCTION(messenger_set_option_OptionHandler_FeedOptions_fails)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	OPTIONHANDLER_HANDLE value = TEST_OPTIONHANDLER_HANDLE;
	STRICT_EXPECTED_CALL(OptionHandler_FeedOptions(value, handle)).SetReturn(OPTIONHANDLER_ERROR);

	// act
	int result = messenger_set_option(handle, MESSENGER_OPTION_SAVED_OPTIONS, value);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	ASSERT_IS_NOT_NULL(handle);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_171: [If name does not match any supported option, authentication_set_option shall fail and return a non-zero value]
TEST_FUNCTION(messenger_set_option_name_not_supported)
{
	// arrange
	MESSENGER_CONFIG* config = get_messenger_config();
	MESSENGER_HANDLE handle = create_and_start_messenger2(config, false);

	size_t value = 100;

	// act
	int result = messenger_set_option(handle, "Bernie Sanders Forever!", &value);

	// assert
	ASSERT_ARE_NOT_EQUAL(int, 0, result);
	ASSERT_IS_NOT_NULL(handle);

	// cleanup
	messenger_destroy(handle);
}

// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_173: [If `messenger_handle` is NULL, messenger_retrieve_options shall fail and return NULL]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_174: [An OPTIONHANDLER_HANDLE instance shall be created using OptionHandler_Create]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_175: [If an OPTIONHANDLER_HANDLE instance fails to be created, messenger_retrieve_options shall fail and return NULL]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_176: [Each option of `instance` shall be added to the OPTIONHANDLER_HANDLE instance using OptionHandler_AddOption]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_177: [If OptionHandler_AddOption fails, messenger_retrieve_options shall fail and return NULL]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_178: [If messenger_retrieve_options fails, any allocated memory shall be freed]
// Tests_SRS_IOTHUBTRANSPORT_AMQP_MESSENGER_09_179: [If no failures occur, messenger_retrieve_options shall return the OPTIONHANDLER_HANDLE instance]

END_TEST_SUITE(iothubtransport_amqp_messenger_ut)
