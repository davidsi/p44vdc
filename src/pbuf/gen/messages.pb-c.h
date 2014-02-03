/* Generated by the protocol buffer compiler.  DO NOT EDIT! */

#ifndef PROTOBUF_C_messages_2eproto__INCLUDED
#define PROTOBUF_C_messages_2eproto__INCLUDED

#include <google/protobuf-c/protobuf-c.h>

PROTOBUF_C_BEGIN_DECLS

#include "vdcapi.pb-c.h"

typedef struct _Vdcapi__Message Vdcapi__Message;
typedef struct _Vdcapi__GenericResponse Vdcapi__GenericResponse;


/* --- enums --- */

typedef enum _Vdcapi__Type {
  VDCAPI__TYPE__GENERIC_RESPONSE = 1,
  VDCAPI__TYPE__VDSM_REQUEST_HELLO = 2,
  VDCAPI__TYPE__VDC_RESPONSE_HELLO = 3,
  VDCAPI__TYPE__VDSM_REQUEST_GET_PROPERTY = 4,
  VDCAPI__TYPE__VDC_RESPONSE_GET_PROPERTY = 5,
  VDCAPI__TYPE__VDSM_REQUEST_SET_PROPERTY = 6,
  VDCAPI__TYPE__VDC_RESPONSE_SET_PROPERTY = 7,
  VDCAPI__TYPE__VDSM_SEND_PING = 8,
  VDCAPI__TYPE__VDC_SEND_PONG = 9,
  VDCAPI__TYPE__VDC_SEND_ANNOUNCE = 10,
  VDCAPI__TYPE__VDC_SEND_VANISH = 11,
  VDCAPI__TYPE__VDC_SEND_PUSH_PROPERTY = 12,
  VDCAPI__TYPE__VDSM_SEND_REMOVE = 13,
  VDCAPI__TYPE__VDSM_SEND_BYE = 14,
  VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_SCENE = 15,
  VDCAPI__TYPE__VDSM_NOTIFICATION_SAVE_SCENE = 16,
  VDCAPI__TYPE__VDSM_NOTIFICATION_UNDO_SCENE = 17,
  VDCAPI__TYPE__VDSM_NOTIFICATION_SET_LOCAL_PRIO = 18,
  VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_MIN_SCENE = 19,
  VDCAPI__TYPE__VDSM_NOTIFICATION_IDENTIFY = 20,
  VDCAPI__TYPE__VDSM_NOTIFICATION_SET_CONTROL_VALUE = 21
} Vdcapi__Type;
typedef enum _Vdcapi__ResultCode {
  VDCAPI__RESULT_CODE__ERR_OK = 0,
  VDCAPI__RESULT_CODE__ERR_MESSAGE_UNKNOWN = 1,
  VDCAPI__RESULT_CODE__ERR_INCOMPATIBLE_API = 2,
  VDCAPI__RESULT_CODE__ERR_SERVICE_NOT_AVAILABLE = 3,
  VDCAPI__RESULT_CODE__ERR_INSUFFICIENT_STORAGE = 4,
  VDCAPI__RESULT_CODE__ERR_FORBIDDEN = 5,
  VDCAPI__RESULT_CODE__ERR_NOT_IMPLEMENTED = 6,
  VDCAPI__RESULT_CODE__ERR_NO_CONTENT_FOR_ARRAY = 7,
  VDCAPI__RESULT_CODE__ERR_INVALID_VALUE_TYPE = 8,
  VDCAPI__RESULT_CODE__ERR_MISSING_SUBMESSAGE = 9,
  VDCAPI__RESULT_CODE__ERR_MISSING_DATA = 10,
  VDCAPI__RESULT_CODE__ERR_NOT_FOUND = 11,
  VDCAPI__RESULT_CODE__ERR_NOT_AUTHORIZED = 12
} Vdcapi__ResultCode;

/* --- messages --- */

struct  _Vdcapi__Message
{
  ProtobufCMessage base;
  Vdcapi__Type type;
  protobuf_c_boolean has_message_id;
  uint32_t message_id;
  Vdcapi__GenericResponse *generic_response;
  Vdcapi__VdsmRequestHello *vdsm_request_hello;
  Vdcapi__VdcResponseHello *vdc_response_hello;
  Vdcapi__VdsmRequestGetProperty *vdsm_request_get_property;
  Vdcapi__VdcResponseGetProperty *vdc_response_get_property;
  Vdcapi__VdsmRequestSetProperty *vdsm_request_set_property;
  Vdcapi__VdsmSendPing *vdsm_send_ping;
  Vdcapi__VdcSendPong *vdc_send_pong;
  Vdcapi__VdcSendAnnounce *vdc_send_announce;
  Vdcapi__VdcSendVanish *vdc_send_vanish;
  Vdcapi__VdcSendPushProperty *vdc_send_push_property;
  Vdcapi__VdsmSendRemove *vdsm_send_remove;
  Vdcapi__VdsmSendBye *vdsm_send_bye;
  Vdcapi__VdsmNotificationCallScene *vdsm_send_call_scene;
  Vdcapi__VdsmNotificationSaveScene *vdsm_send_save_scene;
  Vdcapi__VdsmNotificationUndoScene *vdsm_send_undo_scene;
  Vdcapi__VdsmNotificationSetLocalPrio *vdsm_send_set_local_prio;
  Vdcapi__VdsmNotificationCallMinScene *vdsm_send_call_min_scene;
  Vdcapi__VdsmNotificationIdentify *vdsm_send_identify;
  Vdcapi__VdsmNotificationSetControlValue *vdsm_send_set_control_value;
};
#define VDCAPI__MESSAGE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&vdcapi__message__descriptor) \
    , VDCAPI__TYPE__GENERIC_RESPONSE, 0,0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }


struct  _Vdcapi__GenericResponse
{
  ProtobufCMessage base;
  Vdcapi__ResultCode code;
  char *description;
};
#define VDCAPI__GENERIC_RESPONSE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&vdcapi__generic_response__descriptor) \
    , VDCAPI__RESULT_CODE__ERR_OK, NULL }


/* Vdcapi__Message methods */
void   vdcapi__message__init
                     (Vdcapi__Message         *message);
size_t vdcapi__message__get_packed_size
                     (const Vdcapi__Message   *message);
size_t vdcapi__message__pack
                     (const Vdcapi__Message   *message,
                      uint8_t             *out);
size_t vdcapi__message__pack_to_buffer
                     (const Vdcapi__Message   *message,
                      ProtobufCBuffer     *buffer);
Vdcapi__Message *
       vdcapi__message__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   vdcapi__message__free_unpacked
                     (Vdcapi__Message *message,
                      ProtobufCAllocator *allocator);
/* Vdcapi__GenericResponse methods */
void   vdcapi__generic_response__init
                     (Vdcapi__GenericResponse         *message);
size_t vdcapi__generic_response__get_packed_size
                     (const Vdcapi__GenericResponse   *message);
size_t vdcapi__generic_response__pack
                     (const Vdcapi__GenericResponse   *message,
                      uint8_t             *out);
size_t vdcapi__generic_response__pack_to_buffer
                     (const Vdcapi__GenericResponse   *message,
                      ProtobufCBuffer     *buffer);
Vdcapi__GenericResponse *
       vdcapi__generic_response__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   vdcapi__generic_response__free_unpacked
                     (Vdcapi__GenericResponse *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*Vdcapi__Message_Closure)
                 (const Vdcapi__Message *message,
                  void *closure_data);
typedef void (*Vdcapi__GenericResponse_Closure)
                 (const Vdcapi__GenericResponse *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCEnumDescriptor    vdcapi__type__descriptor;
extern const ProtobufCEnumDescriptor    vdcapi__result_code__descriptor;
extern const ProtobufCMessageDescriptor vdcapi__message__descriptor;
extern const ProtobufCMessageDescriptor vdcapi__generic_response__descriptor;

PROTOBUF_C_END_DECLS


#endif  /* PROTOBUF_messages_2eproto__INCLUDED */