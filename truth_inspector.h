#pragma once

#include <foundation/api_types.h>

// The Truth Inspector Tab allowes the user to inspect Truth Objects or Truth
// types. The Tab will allow the user to  search for a Type and display its
// defintion and properties. The Tab has the following functions:
// - Search Objects by [[tm_tt_id_t]]
// - Search Types by hash, by [[tm_tt_id_t]] index, by name
// - Display all objects of a type
// - Display type properties, default values
// - Display owner
// - Display Aspects (if registered with
//   [[TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME]] or
//   [[TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME]])

struct tm_properties_ui_args_t;
struct tm_the_truth_get_aspects_t;

// Tab identifer
#define TM_TRUTH_INSPECTOR_TAB_VT_NAME "tm_truth_inspector_tab"
#define TM_TRUTH_INSPECTOR_TAB_VT_NAME_HASH TM_STATIC_HASH("tm_truth_inspector_tab", 0x5bcb2932ec6564cdULL)

// Expects a [[tm_tt_inspector_aspect_t]] for aspects
#define TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME "tm_the_truth_inspector_aspect_interface"
// Expects a [[tm_tt_inspector_aspect_t]] for property aspects
#define TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME "tm_the_truth_inspector_property_aspect_interface"

typedef struct tm_tt_inspector_aspect_t
{
    // Name of the Aspect the value of the define
    const char *name;
    // String version of the Define
    const char *define;
    // Option. Description of the Aspect
    const char *description;
    // The typehash of the define
    tm_strhash_t type_hash;
    // Optional. If provided the user can decide how the Aspect shall be rendered
    float (*ui)(struct tm_properties_ui_args_t *args, tm_rect_t item_rect, const struct tm_the_truth_get_aspects_t *);
} tm_tt_inspector_aspect_t;