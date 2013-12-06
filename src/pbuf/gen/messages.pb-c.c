/* Generated by the protocol buffer compiler.  DO NOT EDIT! */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C_NO_DEPRECATED
#define PROTOBUF_C_NO_DEPRECATED
#endif

#include "messages.pb-c.h"
void   vdcapi__message__init
                     (Vdcapi__Message         *message)
{
  static Vdcapi__Message init_value = VDCAPI__MESSAGE__INIT;
  *message = init_value;
}
size_t vdcapi__message__get_packed_size
                     (const Vdcapi__Message *message)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__message__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t vdcapi__message__pack
                     (const Vdcapi__Message *message,
                      uint8_t       *out)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__message__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t vdcapi__message__pack_to_buffer
                     (const Vdcapi__Message *message,
                      ProtobufCBuffer *buffer)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__message__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Vdcapi__Message *
       vdcapi__message__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Vdcapi__Message *)
     protobuf_c_message_unpack (&vdcapi__message__descriptor,
                                allocator, len, data);
}
void   vdcapi__message__free_unpacked
                     (Vdcapi__Message *message,
                      ProtobufCAllocator *allocator)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__message__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   vdcapi__generic_response__init
                     (Vdcapi__GenericResponse         *message)
{
  static Vdcapi__GenericResponse init_value = VDCAPI__GENERIC_RESPONSE__INIT;
  *message = init_value;
}
size_t vdcapi__generic_response__get_packed_size
                     (const Vdcapi__GenericResponse *message)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__generic_response__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t vdcapi__generic_response__pack
                     (const Vdcapi__GenericResponse *message,
                      uint8_t       *out)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__generic_response__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t vdcapi__generic_response__pack_to_buffer
                     (const Vdcapi__GenericResponse *message,
                      ProtobufCBuffer *buffer)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__generic_response__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Vdcapi__GenericResponse *
       vdcapi__generic_response__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Vdcapi__GenericResponse *)
     protobuf_c_message_unpack (&vdcapi__generic_response__descriptor,
                                allocator, len, data);
}
void   vdcapi__generic_response__free_unpacked
                     (Vdcapi__GenericResponse *message,
                      ProtobufCAllocator *allocator)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &vdcapi__generic_response__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const Vdcapi__Type vdcapi__message__type__default_value = VDCAPI__TYPE__GENERIC_RESPONSE;
static const uint32_t vdcapi__message__message_id__default_value = 0;
static const ProtobufCFieldDescriptor vdcapi__message__field_descriptors[22] =
{
  {
    "type",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, type),
    &vdcapi__type__descriptor,
    &vdcapi__message__type__default_value,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "message_id",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT32,
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, has_message_id),
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, message_id),
    NULL,
    &vdcapi__message__message_id__default_value,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "generic_response",
    3,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, generic_response),
    &vdcapi__generic_response__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_request_hello",
    100,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_request_hello),
    &vdcapi__vdsm__request_hello__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdc_response_hello",
    101,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdc_response_hello),
    &vdcapi__vdc__response_hello__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_request_get_property",
    102,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_request_get_property),
    &vdcapi__vdsm__request_get_property__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdc_response_get_property",
    103,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdc_response_get_property),
    &vdcapi__vdc__response_get_property__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdc_send_announce",
    106,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdc_send_announce),
    &vdcapi__vdc__send_announce__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_set_property",
    107,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_set_property),
    &vdcapi__vdsm__send_set_property__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdc_send_vanish",
    108,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdc_send_vanish),
    &vdcapi__vdc__notification_vanish__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdc_send_push_property",
    109,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdc_send_push_property),
    &vdcapi__vdc__notification_push_property__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_remove",
    110,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_remove),
    &vdcapi__vdsm__send_remove__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_bye",
    111,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_bye),
    &vdcapi__vdsm__send_bye__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_call_scene",
    112,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_call_scene),
    &vdcapi__vdsm__notification_call_scene__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_save_scene",
    113,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_save_scene),
    &vdcapi__vdsm__notification_save_scene__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_undo_scene",
    114,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_undo_scene),
    &vdcapi__vdsm__notification_undo_scene__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_set_local_prio",
    115,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_set_local_prio),
    &vdcapi__vdsm__notification_set_local_prio__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_call_min_scene",
    116,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_call_min_scene),
    &vdcapi__vdsm__notification_call_min_scene__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_identify",
    117,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_identify),
    &vdcapi__vdsm__notification_identify__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_ping",
    118,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_ping),
    &vdcapi__vdsm__notification_ping__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdc_send_pong",
    119,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdc_send_pong),
    &vdcapi__vdc__notification_pong__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "vdsm_send_set_control_value",
    120,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__Message, vdsm_send_set_control_value),
    &vdcapi__vdsm__notification_set_control_value__descriptor,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned vdcapi__message__field_indices_by_name[] = {
  2,   /* field[2] = generic_response */
  1,   /* field[1] = message_id */
  0,   /* field[0] = type */
  6,   /* field[6] = vdc_response_get_property */
  4,   /* field[4] = vdc_response_hello */
  7,   /* field[7] = vdc_send_announce */
  20,   /* field[20] = vdc_send_pong */
  10,   /* field[10] = vdc_send_push_property */
  9,   /* field[9] = vdc_send_vanish */
  5,   /* field[5] = vdsm_request_get_property */
  3,   /* field[3] = vdsm_request_hello */
  12,   /* field[12] = vdsm_send_bye */
  17,   /* field[17] = vdsm_send_call_min_scene */
  13,   /* field[13] = vdsm_send_call_scene */
  18,   /* field[18] = vdsm_send_identify */
  19,   /* field[19] = vdsm_send_ping */
  11,   /* field[11] = vdsm_send_remove */
  14,   /* field[14] = vdsm_send_save_scene */
  21,   /* field[21] = vdsm_send_set_control_value */
  16,   /* field[16] = vdsm_send_set_local_prio */
  8,   /* field[8] = vdsm_send_set_property */
  15,   /* field[15] = vdsm_send_undo_scene */
};
static const ProtobufCIntRange vdcapi__message__number_ranges[3 + 1] =
{
  { 1, 0 },
  { 100, 3 },
  { 106, 7 },
  { 0, 22 }
};
const ProtobufCMessageDescriptor vdcapi__message__descriptor =
{
  PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC,
  "vdcapi.Message",
  "Message",
  "Vdcapi__Message",
  "vdcapi",
  sizeof(Vdcapi__Message),
  22,
  vdcapi__message__field_descriptors,
  vdcapi__message__field_indices_by_name,
  3,  vdcapi__message__number_ranges,
  (ProtobufCMessageInit) vdcapi__message__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const Vdcapi__ResultCode vdcapi__generic_response__result__default_value = VDCAPI__RESULT_CODE__RESULT_OK;
static const ProtobufCFieldDescriptor vdcapi__generic_response__field_descriptors[3] =
{
  {
    "result",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__GenericResponse, result),
    &vdcapi__result_code__descriptor,
    &vdcapi__generic_response__result__default_value,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "description",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__GenericResponse, description),
    NULL,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "message_id",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    PROTOBUF_C_OFFSETOF(Vdcapi__GenericResponse, message_id),
    NULL,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned vdcapi__generic_response__field_indices_by_name[] = {
  1,   /* field[1] = description */
  2,   /* field[2] = message_id */
  0,   /* field[0] = result */
};
static const ProtobufCIntRange vdcapi__generic_response__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor vdcapi__generic_response__descriptor =
{
  PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC,
  "vdcapi.GenericResponse",
  "GenericResponse",
  "Vdcapi__GenericResponse",
  "vdcapi",
  sizeof(Vdcapi__GenericResponse),
  3,
  vdcapi__generic_response__field_descriptors,
  vdcapi__generic_response__field_indices_by_name,
  1,  vdcapi__generic_response__number_ranges,
  (ProtobufCMessageInit) vdcapi__generic_response__init,
  NULL,NULL,NULL    /* reserved[123] */
};
const ProtobufCEnumValue vdcapi__type__enum_values_by_number[21] =
{
  { "GENERIC_RESPONSE", "VDCAPI__TYPE__GENERIC_RESPONSE", 1 },
  { "VDSM_REQUEST_HELLO", "VDCAPI__TYPE__VDSM_REQUEST_HELLO", 2 },
  { "VDC_RESPONSE_HELLO", "VDCAPI__TYPE__VDC_RESPONSE_HELLO", 3 },
  { "VDSM_REQUEST_GET_PROPERTY", "VDCAPI__TYPE__VDSM_REQUEST_GET_PROPERTY", 4 },
  { "VDC_RESPONSE_GET_PROPERTY", "VDCAPI__TYPE__VDC_RESPONSE_GET_PROPERTY", 5 },
  { "VDSM_NOTIFICATION_PING", "VDCAPI__TYPE__VDSM_NOTIFICATION_PING", 6 },
  { "VDC_NOTIFICATION_PONG", "VDCAPI__TYPE__VDC_NOTIFICATION_PONG", 7 },
  { "VDC_SEND_ANNOUNCE", "VDCAPI__TYPE__VDC_SEND_ANNOUNCE", 8 },
  { "VDC_NOTIFICATION_VANISH", "VDCAPI__TYPE__VDC_NOTIFICATION_VANISH", 9 },
  { "VDC_NOTIFICATION_PUSH_PROPERTY", "VDCAPI__TYPE__VDC_NOTIFICATION_PUSH_PROPERTY", 10 },
  { "VDSM_SEND_SET_PROPERTY", "VDCAPI__TYPE__VDSM_SEND_SET_PROPERTY", 11 },
  { "VDSM_SEND_REMOVE", "VDCAPI__TYPE__VDSM_SEND_REMOVE", 12 },
  { "VDSM_SEND_BYE", "VDCAPI__TYPE__VDSM_SEND_BYE", 13 },
  { "VDSM_NOTIFICATION_CALL_SCENE", "VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_SCENE", 14 },
  { "VDSM_NOTIFICATION_SAVE_SCENE", "VDCAPI__TYPE__VDSM_NOTIFICATION_SAVE_SCENE", 15 },
  { "VDSM_NOTIFICATION_UNDO_SCENE", "VDCAPI__TYPE__VDSM_NOTIFICATION_UNDO_SCENE", 16 },
  { "VDSM_NOTIFICATION_SET_LOCAL_PRIO", "VDCAPI__TYPE__VDSM_NOTIFICATION_SET_LOCAL_PRIO", 17 },
  { "VDSM_NOTIFICATION_CALL_MIN_SCENE", "VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_MIN_SCENE", 18 },
  { "VDSM_NOTIFICATION_IDENTIFY", "VDCAPI__TYPE__VDSM_NOTIFICATION_IDENTIFY", 19 },
  { "VDSM_NOTIFICATION_PING_DEVICE", "VDCAPI__TYPE__VDSM_NOTIFICATION_PING_DEVICE", 20 },
  { "VDSM_NOTIFICATION_SET_CONTROL_VALUE", "VDCAPI__TYPE__VDSM_NOTIFICATION_SET_CONTROL_VALUE", 21 },
};
static const ProtobufCIntRange vdcapi__type__value_ranges[] = {
{1, 0},{0, 21}
};
const ProtobufCEnumValueIndex vdcapi__type__enum_values_by_name[21] =
{
  { "GENERIC_RESPONSE", 0 },
  { "VDC_NOTIFICATION_PONG", 6 },
  { "VDC_NOTIFICATION_PUSH_PROPERTY", 9 },
  { "VDC_NOTIFICATION_VANISH", 8 },
  { "VDC_RESPONSE_GET_PROPERTY", 4 },
  { "VDC_RESPONSE_HELLO", 2 },
  { "VDC_SEND_ANNOUNCE", 7 },
  { "VDSM_NOTIFICATION_CALL_MIN_SCENE", 17 },
  { "VDSM_NOTIFICATION_CALL_SCENE", 13 },
  { "VDSM_NOTIFICATION_IDENTIFY", 18 },
  { "VDSM_NOTIFICATION_PING", 5 },
  { "VDSM_NOTIFICATION_PING_DEVICE", 19 },
  { "VDSM_NOTIFICATION_SAVE_SCENE", 14 },
  { "VDSM_NOTIFICATION_SET_CONTROL_VALUE", 20 },
  { "VDSM_NOTIFICATION_SET_LOCAL_PRIO", 16 },
  { "VDSM_NOTIFICATION_UNDO_SCENE", 15 },
  { "VDSM_REQUEST_GET_PROPERTY", 3 },
  { "VDSM_REQUEST_HELLO", 1 },
  { "VDSM_SEND_BYE", 12 },
  { "VDSM_SEND_REMOVE", 11 },
  { "VDSM_SEND_SET_PROPERTY", 10 },
};
const ProtobufCEnumDescriptor vdcapi__type__descriptor =
{
  PROTOBUF_C_ENUM_DESCRIPTOR_MAGIC,
  "vdcapi.Type",
  "Type",
  "Vdcapi__Type",
  "vdcapi",
  21,
  vdcapi__type__enum_values_by_number,
  21,
  vdcapi__type__enum_values_by_name,
  1,
  vdcapi__type__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
const ProtobufCEnumValue vdcapi__result_code__enum_values_by_number[14] =
{
  { "RESULT_OK", "VDCAPI__RESULT_CODE__RESULT_OK", 1 },
  { "RESULT_MESSAGE_UNKNOWN", "VDCAPI__RESULT_CODE__RESULT_MESSAGE_UNKNOWN", 2 },
  { "RESULT_INCOMPATIBLE_API", "VDCAPI__RESULT_CODE__RESULT_INCOMPATIBLE_API", 3 },
  { "RESULT_SERVICE_NOT_AVAILABLE", "VDCAPI__RESULT_CODE__RESULT_SERVICE_NOT_AVAILABLE", 4 },
  { "RESULT_INSUFFICIENT_STORAGE", "VDCAPI__RESULT_CODE__RESULT_INSUFFICIENT_STORAGE", 5 },
  { "RESULT_FORBIDDEN", "VDCAPI__RESULT_CODE__RESULT_FORBIDDEN", 6 },
  { "RESULT_NOT_IMPLEMENTED", "VDCAPI__RESULT_CODE__RESULT_NOT_IMPLEMENTED", 7 },
  { "RESULT_NO_CONTENT_FOR_ARRAY", "VDCAPI__RESULT_CODE__RESULT_NO_CONTENT_FOR_ARRAY", 8 },
  { "RESULT_INVALID_VALUE_TYPE", "VDCAPI__RESULT_CODE__RESULT_INVALID_VALUE_TYPE", 9 },
  { "RESULT_MISSING_SUBMESSAGE", "VDCAPI__RESULT_CODE__RESULT_MISSING_SUBMESSAGE", 10 },
  { "RESULT_MISSING_DATA", "VDCAPI__RESULT_CODE__RESULT_MISSING_DATA", 11 },
  { "RESULT_NOT_FOUND", "VDCAPI__RESULT_CODE__RESULT_NOT_FOUND", 12 },
  { "RESULT_NOT_AUTHORIZED", "VDCAPI__RESULT_CODE__RESULT_NOT_AUTHORIZED", 13 },
  { "RESULT_GENERAL_ERROR", "VDCAPI__RESULT_CODE__RESULT_GENERAL_ERROR", 14 },
};
static const ProtobufCIntRange vdcapi__result_code__value_ranges[] = {
{1, 0},{0, 14}
};
const ProtobufCEnumValueIndex vdcapi__result_code__enum_values_by_name[14] =
{
  { "RESULT_FORBIDDEN", 5 },
  { "RESULT_GENERAL_ERROR", 13 },
  { "RESULT_INCOMPATIBLE_API", 2 },
  { "RESULT_INSUFFICIENT_STORAGE", 4 },
  { "RESULT_INVALID_VALUE_TYPE", 8 },
  { "RESULT_MESSAGE_UNKNOWN", 1 },
  { "RESULT_MISSING_DATA", 10 },
  { "RESULT_MISSING_SUBMESSAGE", 9 },
  { "RESULT_NOT_AUTHORIZED", 12 },
  { "RESULT_NOT_FOUND", 11 },
  { "RESULT_NOT_IMPLEMENTED", 6 },
  { "RESULT_NO_CONTENT_FOR_ARRAY", 7 },
  { "RESULT_OK", 0 },
  { "RESULT_SERVICE_NOT_AVAILABLE", 3 },
};
const ProtobufCEnumDescriptor vdcapi__result_code__descriptor =
{
  PROTOBUF_C_ENUM_DESCRIPTOR_MAGIC,
  "vdcapi.ResultCode",
  "ResultCode",
  "Vdcapi__ResultCode",
  "vdcapi",
  14,
  vdcapi__result_code__enum_values_by_number,
  14,
  vdcapi__result_code__enum_values_by_name,
  1,
  vdcapi__result_code__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
