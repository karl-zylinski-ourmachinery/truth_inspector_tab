

static struct tm_ui_popup_item_picker_api *tm_ui_popup_item_picker_api;
static struct tm_api_registry_api *tm_global_api_registry;
static struct tm_draw2d_api *tm_draw2d_api;
static struct tm_application_api *tm_application_api;
static struct tm_logger_api *tm_logger_api;

struct tm_os_clipboard_api *tm_os_clipboard_api;
struct tm_localizer_api *tm_localizer_api;
struct tm_the_truth_api *tm_the_truth_api;
struct tm_temp_allocator_api *tm_temp_allocator_api;
struct tm_random_api *tm_random_api;
struct tm_ui_api *tm_ui_api;
struct tm_properties_view_api *tm_properties_view_api;
struct tm_error_api *tm_error_api;

#include <foundation/allocator.h>
#include <foundation/api_registry.h>
#include <foundation/application.h>
#include <foundation/carray.inl>
#include <foundation/error.h>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/macros.h>
#include <foundation/murmurhash64a.inl>
#include <foundation/random.h>
#include <foundation/rect.inl>
#include <foundation/string.inl>
#include <foundation/temp_allocator.h>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>

#include <plugins/editor_views/properties.h>
#include <plugins/editor_views/ui_popup_item_picker.h>
#include <plugins/os_window/os_window.h>
#include <plugins/ui/docking.h>
#include <plugins/ui/draw2d.h>
#include <plugins/ui/ui.h>
#include <plugins/ui/ui_custom.h>
#include <plugins/ui/ui_icon.h>

#include "truth_inspector.h"
#include <the_machinery/the_machinery_tab.h>

#include <stdio.h>

// extern functions:
extern void load_aspect_uis(struct tm_api_registry_api *reg, bool load);
extern void load_asset_browser_integration(struct tm_api_registry_api *reg, bool load);

const float metrics_item_h = 25.f;
const float metrics_list_h = 150.f;

// Metrics for drawing the UI.
static float default_metrics[] = {
    [TM_PROPERTIES_METRIC_LABEL_WIDTH] = 150.f,
    [TM_PROPERTIES_METRIC_MARGIN] = 5.0f,
    [TM_PROPERTIES_METRIC_ITEM_HEIGHT] = 20.0f,
    [TM_PROPERTIES_METRIC_INDENT] = 10.0f,
    [TM_PROPERTIES_METRIC_EDIT_WIDTH] = 60.0f,
    [TM_PROPERTIES_METRIC_CHECKBOX_CONTROL_WIDTH] = 10.0f,
    [TM_PROPERTIES_METRIC_SUBOBJECT_LABEL_MARGIN] = 15.0f,
    [TM_PROPERTIES_METRIC_GROUP_RECT_PADDING] = 10.0f,
    [TM_PROPERTIES_METRIC_GROUP_LABEL_LEFT_MARGIN] = 12.0f,
    [TM_PROPERTIES_METRIC_MENU_WIDTH] = 15.0f,
    [TM_PROPERTIES_METRIC_COLOR_PICKER_RGB_LABEL_SIZE] = 15.0f,
    [TM_PROPERTIES_METRIC_COLOR_PICKER_INPUT_SIZE] = 60.0f,
    [TM_PROPERTIES_METRIC_COLOR_PICKER_HSV_LABEL_SIZE] = 15.0f,
    [TM_PROPERTIES_METRIC_COLOR_PICKER_SLIDER_MARGIN] = 7.0f,
    [TM_PROPERTIES_METRIC_COLOR_PICKER_SLIDER_KNOB_SIZE] = 7.0f,
};

#pragma region type defentions

enum filter_mode_e {
    MODE_DISPLAY_SPECIFIC_OBJECT,
    MODE_DISPLAY_OBJECTS_OF_TYPE,
    MODE_DISPLAY_OBJECTS_OWNED_BY,
    MODE_DISPLAY_TYPE,
    MODE_DISPLAY_TYPE_BY_NUMBER,
    MODE_DISPLAY_ASPECT,
};

struct filter_t
{
    uint32_t picked;
    tm_tt_id_t object;
    enum filter_mode_e mode;
};

struct tm_tab_o
{
    tm_tab_i tm_tab_i;
    tm_allocator_i *allocator;
    float max_x;
    float max_y;
    float scroll_y;
    tm_the_truth_o *tt;
    char **type_names;
    char **search_items;
    char **search_aspect_names;
    tm_tt_type_t *tt_types;
    tm_properties_view_o *properties_view;
    tm_properties_ui_args_t prop_args;
    struct filter_t filter;
    char buffer[1024];
    tm_set_id_t expanded;
    tm_hash64_t index_or_hash_to_type;
    tm_hash64_t index_or_hash_to_aspect;
    const tm_tt_inspector_aspect_t **aspects;
    // index after which the prop aspects start
    uint32_t prop_aspect_begin;
};

#pragma endregion

#pragma region extern functions

void show_all_object_of_this_type(tm_tab_o *data, tm_tt_type_t type)
{
    data->filter.picked = (uint32_t)type.u64;
    data->filter.mode = MODE_DISPLAY_OBJECTS_OF_TYPE;
    snprintf(data->buffer, 1024, "%s", data->type_names[data->filter.picked]);
}

void show_all_object_owned_by_this(tm_tab_o *data, tm_tt_id_t owner)
{
    data->filter.picked = (uint32_t)owner.type;
    data->filter.object = owner;
    data->filter.mode = MODE_DISPLAY_OBJECTS_OWNED_BY;
    snprintf(data->buffer, 1024, "%s", data->type_names[data->filter.picked]);
}

void show_this_type(tm_tab_o *data, tm_tt_type_t type)
{
    data->filter.picked = (uint32_t)type.u64;
    data->filter.mode = MODE_DISPLAY_TYPE;
    snprintf(data->buffer, 1024, "%s", data->type_names[data->filter.picked]);
}

void show_this_object(tm_tab_o *data, tm_tt_id_t object)
{
    data->filter.mode = MODE_DISPLAY_SPECIFIC_OBJECT;
    data->filter.object = object;
    data->filter.picked = 0;
    snprintf(data->buffer, 1024, "%" PRIu64, object.u64);
}

#pragma endregion

#pragma region utility

static const char *type_to_string(enum tm_the_truth_property_type type)
{
    switch (type) {
    case TM_THE_TRUTH_PROPERTY_TYPE_NONE:
        return "None";
    case TM_THE_TRUTH_PROPERTY_TYPE_BOOL:
        return "Bool";
    case TM_THE_TRUTH_PROPERTY_TYPE_UINT32_T:
        return "Unit32";
    case TM_THE_TRUTH_PROPERTY_TYPE_UINT64_T:
        return "Unit64";
    case TM_THE_TRUTH_PROPERTY_TYPE_FLOAT:
        return "Float";
    case TM_THE_TRUTH_PROPERTY_TYPE_DOUBLE:
        return "Double";
    case TM_THE_TRUTH_PROPERTY_TYPE_STRING:
        return "String";
    case TM_THE_TRUTH_PROPERTY_TYPE_BUFFER:
        return "Buffer";
    case TM_THE_TRUTH_PROPERTY_TYPE_REFERENCE:
        return "Reference";
    case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT:
        return "Subobject";
    case TM_THE_TRUTH_PROPERTY_TYPE_REFERENCE_SET:
        return "Reference Set";
    case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT_SET:
        return "Subobject Set";
    default:
        return "Unknown";
    }
}

const char *get_name_if_exists(const tm_the_truth_o *tt, tm_tt_id_t object)
{
    const tm_the_truth_property_definition_t *props = tm_the_truth_api->properties(tt, tm_tt_type(object));
    uint32_t index = 0;
    for (const tm_the_truth_property_definition_t *prop = props; prop != tm_carray_end(props); ++prop, ++index) {
        if (!strcmp(prop->name, "name") && prop->type == TM_THE_TRUTH_PROPERTY_TYPE_STRING) {
            return tm_the_truth_api->get_string(tt, tm_tt_read(tt, object), index);
        }
    }
    return NULL;
}

// A text edit which is disabled but can be copied via right click
float copy_disabled_text(tm_properties_ui_args_t *args, tm_rect_t props_r, const char *label, const char *data)
{
    tm_ui_buffers_t uib = tm_ui_api->buffers(args->ui);
    TM_INIT_TEMP_ALLOCATOR(ta);
    tm_rect_t label_r = { props_r.x, props_r.y, args->metrics[TM_PROPERTIES_METRIC_LABEL_WIDTH], metrics_item_h };
    label_r.y = props_r.y;
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = label_r, .text = label });
    tm_rect_t textedit_r = { props_r.x + label_r.w + 10, props_r.y, props_r.w - label_r.w - 10, metrics_item_h };
    uint64_t text_id = tm_ui_api->make_id(args->ui);
    char *str = tm_temp_allocator_api->printf(ta, "%s", data);
    tm_ui_api->textedit(args->ui, args->uistyle, &(tm_ui_textedit_t){ .id = text_id, .rect = textedit_r, .is_disabled = true }, str, (uint32_t)strlen(str));
    if (tm_ui_api->is_hovering(args->ui, textedit_r, 0))
        uib.activation->next_hover = text_id;
    if (uib.activation->hover == text_id && tm_ui_api->right_mouse_pressed(args->ui, "Copy")) {
        tm_os_clipboard_item_t item = { .format = "text/plain", .data = str, .size = (uint32_t)strlen(str) };
        tm_os_clipboard_api->set(&item, 1);
    }
    if (uib.activation->hover == text_id) {
        tm_vec2_t pos = uib.input->mouse_pos;
        pos.x += args->metrics[TM_PROPERTIES_METRIC_MARGIN] * 2;
        uib.activation->tooltip_position = pos;
        tm_ui_api->tooltip(args->ui, args->uistyle, "Right Click to copy");
    }
    props_r.y += metrics_item_h + args->metrics[TM_PROPERTIES_METRIC_MARGIN];
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return props_r.y;
}

#pragma region aspect
// Tries to find the property aspect in the registered aspects
// depends on [[TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME]]
static tm_the_truth_get_aspects_t *get_property_aspects(const tm_the_truth_o *tt, tm_tt_type_t type, uint32_t prop, tm_temp_allocator_i *ta)
{
    uint32_t num_i;
    tm_the_truth_get_aspects_t *out = 0;
    tm_tt_inspector_aspect_t **aspect_data = (tm_tt_inspector_aspect_t **)tm_global_api_registry->implementations(TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME, &num_i);
    for (uint32_t i = 0; i < num_i; ++i) {
        void *data = tm_the_truth_api->get_property_aspect(tt, type, prop, aspect_data[i]->type_hash);
        if (data) {
            tm_the_truth_get_aspects_t a = { .data = data, .id = aspect_data[i]->type_hash };
            tm_carray_temp_push(out, a, ta);
        }
    }
    return out;
}

// Default base aspect UI if no custom UI is defined in [[TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME]]
// see truth_inspect_aspect_ui.c for more information
float aspect_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_tt_inspector_aspect_t *aspect)
{
    tm_ui_buffers_t uib = tm_ui_api->buffers(args->ui);
    TM_INIT_TEMP_ALLOCATOR(ta);
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Name", NULL, aspect->name);
    props_r.y = copy_disabled_text(args, props_r, "Define", tm_or(aspect->define, "Unknown"));
    props_r.y = copy_disabled_text(args, props_r, "Type hash", tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(aspect->type_hash)));
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = props_r, .text = TM_LOCALIZE("Aspect Description:") });
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = props_r, .text = TM_LOCALIZE("Aspect Description:") });
    props_r.y += metrics_item_h;
    props_r.y += tm_ui_api->wrapped_text(args->ui, args->uistyle, &(tm_ui_text_t){ .text = tm_or(aspect->description, TM_LOCALIZE("- None -")), .rect = props_r }).h;
    props_r.y += uib.metrics[TM_UI_METRIC_MARGIN];
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return props_r.y;
}
#pragma endregion

#pragma endregion

#pragma region inspect functions

static float inspect_aspect(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect)
{
    float y = 0;
    TM_INIT_TEMP_ALLOCATOR(ta);
    char *type_hash_str = tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(aspect->id));
    const uint64_t key = TM_STRHASH_U64(tm_murmur_hash_string(type_hash_str));
    const uint64_t idx = tm_hash_get(&args->tab->inst->index_or_hash_to_aspect, key);
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    if (idx != UINT32_MAX) {
        y = aspect_ui(args, props_r, args->tab->inst->aspects[idx]);
        if (args->tab->inst->aspects[idx]->ui) {
            y = args->tab->inst->aspects[idx]->ui(args, props_r, aspect);
        }
    }
    if (y == 0) {
        return aspect_ui(args, props_r, &(tm_tt_inspector_aspect_t){ .name = "Unknown", .type_hash = aspect->id, .description = "- None -", .define = "Unknown" });
    } else {
        return y;
    }
}

static float inspect_property(tm_properties_ui_args_t *args, tm_rect_t props_r, tm_tt_type_t type, const tm_the_truth_property_definition_t *prop, uint32_t index)
{
    TM_INIT_TEMP_ALLOCATOR(ta);
    tm_tt_id_t default_object = tm_the_truth_api->default_object(args->tt, type);
    tm_the_truth_get_aspects_t *porps_aspects = get_property_aspects(args->tt, type, index, ta);
    char name[1024];
    snprintf(name, 1024, "%s", prop->ui_name ? prop->ui_name : prop->name);
    tm_capitalize(name);
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Name", NULL, name);
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Type", NULL, type_to_string(prop->type));
    if (default_object.u64) {
        const char *default_value_name = "Default Value";
        switch (prop->type) {
        case TM_THE_TRUTH_PROPERTY_TYPE_BOOL: {
            const char *value = tm_the_truth_api->get_bool(args->tt, tm_tt_read(args->tt, default_object), index) ? "True" : "False";
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_UINT32_T: {
            const char *value = tm_temp_allocator_api->printf(ta, "%" PRIu32, tm_the_truth_api->get_uint32_t(args->tt, tm_tt_read(args->tt, default_object), index));
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_UINT64_T: {
            const char *value = tm_temp_allocator_api->printf(ta, "%" PRIu64, tm_the_truth_api->get_uint64_t(args->tt, tm_tt_read(args->tt, default_object), index));
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_FLOAT: {
            const char *value = tm_temp_allocator_api->printf(ta, "%f", tm_the_truth_api->get_float(args->tt, tm_tt_read(args->tt, default_object), index));
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_DOUBLE: {
            const char *value = tm_temp_allocator_api->printf(ta, "%f", tm_the_truth_api->get_double(args->tt, tm_tt_read(args->tt, default_object), index));
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_STRING: {
            const char *value = tm_the_truth_api->get_string(args->tt, tm_tt_read(args->tt, default_object), index);
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_BUFFER: {
            tm_tt_buffer_t buf = tm_the_truth_api->get_buffer(args->tt, tm_tt_read(args->tt, default_object), index);
            const char *value = tm_temp_allocator_api->printf(ta, "Id: %" PRIu32 " Size: %" PRIu64, buf.id, buf.size);
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_REFERENCE: {
            tm_tt_id_t ref = tm_the_truth_api->get_reference(args->tt, tm_tt_read(args->tt, default_object), index);
            const char *value = tm_temp_allocator_api->printf(ta, "Id: %" PRIu64, ref.u64);
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT: {
            tm_tt_id_t ref = tm_the_truth_api->get_subobject(args->tt, tm_tt_read(args->tt, default_object), index);
            const char *value = tm_temp_allocator_api->printf(ta, "Id: %" PRIu64, ref.u64);
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_REFERENCE_SET: {
            uint64_t size = tm_the_truth_api->get_reference_set_size(args->tt, tm_tt_read(args->tt, default_object), index);
            const char *value = tm_temp_allocator_api->printf(ta, "Size: %" PRIu64, size);
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT_SET: {
            uint64_t size = tm_the_truth_api->get_subobject_set_size(args->tt, tm_tt_read(args->tt, default_object), index);
            const char *value = tm_temp_allocator_api->printf(ta, "Size: %" PRIu64, size);
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, value);
        }
        default:
            props_r.y = tm_properties_view_api->ui_static_text(args, props_r, default_value_name, NULL, "Unknown");
            break;
        }
    }
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Aspects", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu64, tm_carray_size(porps_aspects)));
    if (tm_carray_size(porps_aspects) > 0) {
        props_r.x += 10;
        bool prop_aspect_expanded = false;
        props_r.y = tm_properties_view_api->ui_group(args, props_r, "Property Aspects:", NULL, (tm_tt_id_t){ TM_STRHASH_U64(prop->type_hash) }, 0, false, &prop_aspect_expanded);
        if (prop_aspect_expanded) {
            props_r.y += metrics_item_h;
            for (const tm_the_truth_get_aspects_t *aspect = porps_aspects; aspect != tm_carray_end(porps_aspects); ++aspect) {
                props_r.y = inspect_aspect(args, props_r, aspect);
                props_r.y = tm_properties_view_api->ui_horizontal_line(args, props_r);
            }
        }
        props_r.x -= 10;
    }
    if (prop->buffer_extension) {
        props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Buffer Extension", NULL, prop->buffer_extension);
    }
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return props_r.y;
}

float inspect_type(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_o *tt, tm_tt_type_t type, const char *type_name)
{
    TM_INIT_TEMP_ALLOCATOR(ta);
    props_r.x += 10;
    tm_tt_id_t default_object = tm_the_truth_api->default_object(tt, type);
    const tm_the_truth_property_definition_t *props = tm_the_truth_api->properties(tt, type);
    const tm_the_truth_get_aspects_t *aspects = tm_the_truth_api->get_aspects(tt, type);
    uint64_t objects = tm_carray_size(tm_the_truth_api->all_objects_of_type(tt, type, ta));
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Type", NULL, type_name);
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Type Index", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu64, type.u64));
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Objects", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu64, objects));
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Has Default", NULL, default_object.u64 ? "Yes" : "No");
    if (default_object.u64) {
        bool expanded_default = false;
        props_r.y = tm_properties_view_api->ui_group(args, props_r, "Inspect Default Object", NULL, default_object, 1, false, &expanded_default);
        if (expanded_default) {
            props_r.y = tm_properties_view_api->ui_object(args, props_r, default_object, 1);
        }
    }
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Aspects", NULL, tm_carray_size(aspects) > 0 ? tm_temp_allocator_api->printf(ta, "%" PRIu64, tm_carray_size(aspects)) : "None");
    char *type_hash = tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(tm_the_truth_api->type_name_hash(tt, type)));
    props_r.y = copy_disabled_text(args, props_r, TM_LOCALIZE("Type Hash"), type_hash);
    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = props_r, .text = tm_temp_allocator_api->printf(ta, "Show all %" PRIu64 " objects of type.", objects) }))

    {
        show_all_object_of_this_type(args->tab->inst, type);
    }
    props_r.y += metrics_item_h;
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = props_r, .text = TM_LOCALIZE("Type Properties:") });
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = props_r, .text = TM_LOCALIZE("Type Properties:") });
    props_r.y += metrics_item_h;
    props_r.y = tm_properties_view_api->ui_horizontal_line(args, props_r);
    uint32_t index = 0;
    for (const tm_the_truth_property_definition_t *prop = props; prop != tm_carray_end(props); ++prop, ++index) {
        props_r.y = inspect_property(args, props_r, type, prop, index);
        props_r.y = tm_properties_view_api->ui_horizontal_line(args, props_r);
    }
    if (tm_carray_size(aspects) > 0) {
        bool expanded = false;
        props_r.y = tm_properties_view_api->ui_group(args, props_r, "Type Aspects:", NULL, (tm_tt_id_t){ type.u64 }, 0, false, &expanded);
        if (expanded) {
            props_r.y += metrics_item_h;
            for (const tm_the_truth_get_aspects_t *aspect = aspects; aspect != tm_carray_end(aspects); ++aspect) {
                props_r.y = inspect_aspect(args, props_r, aspect);
                props_r.y = tm_properties_view_api->ui_horizontal_line(args, props_r);
            }
        }
    }
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return props_r.y;
}

static float inspect_object_memory_usage(tm_properties_ui_args_t *args, tm_rect_t object_r, const tm_the_truth_o *tt, tm_tt_id_t object)
{
    TM_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);
    tm_set_t buffer = { .allocator = a };
    const tm_tt_memory_use_t mu = tm_the_truth_api->memory_use(args->tt, object, &buffer);
    const uint64_t total = mu.resident + mu.unloaded;
    char si_buffer[TM_FORMAT_SI_SIZE];
    object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Currently in Memory", NULL, tm_format_si(mu.resident, si_buffer, "B"));
    object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "On Demand", NULL, tm_format_si(mu.unloaded, si_buffer, "B"));
    object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Total Size", NULL, tm_format_si(total, si_buffer, "B"));
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return object_r.y;
}

static float inspect_objects(tm_properties_ui_args_t *args, tm_rect_t group_r, const tm_the_truth_o *tt, tm_tt_id_t id, const char **type_names);

static float inspect_object_properties(tm_properties_ui_args_t *args, tm_rect_t group_r, const tm_the_truth_o *tt, tm_tt_type_t type, tm_tt_id_t object)
{
    tm_ui_buffers_t uib = tm_ui_api->buffers(args->ui);
    const tm_the_truth_property_definition_t *props = tm_the_truth_api->properties(tt, type);
    uint32_t index = 0;
    for (const tm_the_truth_property_definition_t *prop = props; prop != tm_carray_end(props); ++prop, ++index) {
        group_r.y = inspect_property(args, group_r, type, prop, index);
        tm_tt_id_t prot = tm_the_truth_api->prototype(args->tt, object);
        if (prot.u64) {
            tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = group_r, .text = TM_LOCALIZE("Prototype Information:") });
            group_r.y += metrics_item_h;
            group_r.y = inspect_objects(args, group_r, args->tt, prot, args->tab->inst->type_names);
            const bool overridden = tm_the_truth_api->is_overridden(tt, tm_tt_read(tt, object), index);
            if (overridden)
                group_r.y = tm_properties_view_api->ui_static_text(args, group_r, "Prototype", NULL, "Overridden");
            else
                group_r.y = tm_properties_view_api->ui_static_text(args, group_r, "Prototype", NULL, "Inherited");
        } else {
            group_r.y = tm_properties_view_api->ui_static_text(args, group_r, "Prototype", NULL, "No Prototype");
        }
        const bool has_data = tm_the_truth_api->has_data(args->tt, tm_tt_read(args->tt, object), index);
        group_r.y = tm_properties_view_api->ui_static_text(args, group_r, "Has data", NULL, has_data ? "Yes" : "No");
        // inspect value:
        if (has_data) {
            group_r.y = tm_properties_view_api->ui_property(args, group_r, object, 0, index);
        }
        switch (prop->type) {
        case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT: {
            TM_INIT_TEMP_ALLOCATOR(ta);
            tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = group_r, .text = TM_LOCALIZE("Subobject:") });
            group_r.y += metrics_item_h;
            tm_tt_id_t subobject = tm_the_truth_api->get_subobject(tt, tm_tt_read(tt, object), index);
            tm_draw2d_api->fill_rect(uib.vbuffer, *uib.ibuffers, &(tm_draw2d_style_t){ .color = uib.colors[TM_UI_COLOR_CONTROL_BACKGROUND_ACTIVE], .clip = args->uistyle->clip }, group_r);
            const char *sub_type = args->tab->inst->type_names[subobject.type];
            const char *sub_name = get_name_if_exists(args->tt, subobject);
            sub_name = sub_name ? tm_temp_allocator_api->printf(ta, "Name: %s", sub_name) : "";
            tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = tm_rect_divide_x(group_r, 5, 2, 0), .text = tm_temp_allocator_api->printf(ta, "ID: %" PRIu64 " Type: %s %s", subobject, sub_type, sub_name) });
            if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = tm_rect_divide_x(group_r, 5, 2, 1), .text = "Inspect" })) {
                args->tab->inst->filter.object = subobject;
                args->tab->inst->filter.mode = MODE_DISPLAY_SPECIFIC_OBJECT;
                args->tab->inst->filter.picked = 0;
                args->tab->inst->buffer[0] = 0;
            }
            TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
            group_r.y += metrics_item_h;
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT_SET: {
            group_r.x += 10;
            TM_INIT_TEMP_ALLOCATOR(ta);
            const tm_tt_id_t *subobjects = tm_the_truth_api->get_subobject_set(tt, tm_tt_read(tt, object), index, ta);
            bool expanded = false;
            group_r.y = tm_properties_view_api->ui_group(args, group_r, tm_temp_allocator_api->printf(ta, "Subobjects: %" PRIu64, tm_carray_size(subobjects)), NULL, (tm_tt_id_t){ index + object.u64 }, 0, false, &expanded);
            if (expanded) {
                for (const tm_tt_id_t *subobject = subobjects; subobject != tm_carray_end(subobjects); ++subobject) {
                    tm_draw2d_api->fill_rect(uib.vbuffer, *uib.ibuffers, &(tm_draw2d_style_t){ .color = uib.colors[TM_UI_COLOR_CONTROL_BACKGROUND_ACTIVE], .clip = args->uistyle->clip }, group_r);
                    const char *sub_type = args->tab->inst->type_names[subobject->type];
                    const char *sub_name = get_name_if_exists(args->tt, *subobject);
                    sub_name = sub_name ? tm_temp_allocator_api->printf(ta, "Name: %s", sub_name) : "";
                    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = tm_rect_divide_x(group_r, 5, 2, 0), .text = tm_temp_allocator_api->printf(ta, "ID: %" PRIu64 " Type: %s %s", subobject, sub_type, sub_name) });
                    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = tm_rect_divide_x(group_r, 5, 2, 1), .text = "Inspect" })) {
                        args->tab->inst->filter.object = *subobject;
                        args->tab->inst->filter.mode = MODE_DISPLAY_SPECIFIC_OBJECT;
                        args->tab->inst->filter.picked = 0;
                        args->tab->inst->buffer[0] = 0;
                    }
                    group_r.y += metrics_item_h;
                }
            }
            TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
            group_r.x -= 10;
        } break;
        default:
            break;
        }
        group_r.y = tm_properties_view_api->ui_horizontal_line(args, group_r);
    }
    return group_r.y;
}

static float inspect_objects(tm_properties_ui_args_t *args, tm_rect_t group_r, const tm_the_truth_o *tt, tm_tt_id_t id, const char **type_names)
{
    TM_INIT_TEMP_ALLOCATOR(ta);
    tm_ui_buffers_t uib = tm_ui_api->buffers(args->ui);
    tm_tt_type_t type = tm_tt_type(id);
    if (tm_the_truth_api->is_alive(tt, id)) {
        tm_tt_id_t owner = tm_the_truth_api->owner(tt, id);
        bool expanded = false;
        const char *object_name = get_name_if_exists(args->tt, id);
        const char *group_name = object_name ? tm_temp_allocator_api->printf(ta, "#%" PRIu64 " %s", id.u64, object_name) : tm_temp_allocator_api->printf(ta, "#%" PRIu64, (id.u64));
        group_r.y = tm_properties_view_api->ui_group(args, group_r, group_name, NULL, id, 0, false, &expanded);
        if (expanded) {
            tm_rect_t object_r = group_r;
            // 1. display basic information about object:
            tm_uuid_t uuid = tm_the_truth_api->uuid(tt, id);
            object_r.y = copy_disabled_text(args, object_r, "ID", tm_temp_allocator_api->printf(ta, "%" PRIu64, id.u64));
            object_r.y = copy_disabled_text(args, object_r, "UUID", tm_temp_allocator_api->printf(ta, "%" PRIu64 "-%" PRIu64, uuid.a, uuid.b));
            if (!uuid.a && !uuid.b) {
                tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = object_r, .text = TM_LOCALIZE("Object has never been saved.") });
                object_r.y += metrics_item_h + args->metrics[TM_PROPERTIES_METRIC_MARGIN];
            }
            object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Index", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu64, id.index));
            object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Generation", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu64, id.generation));
            object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Version", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu32, tm_the_truth_api->version(tt, id)));
            object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Type", NULL, tm_temp_allocator_api->printf(ta, "%s", type_names[id.type]));
            object_r.y = tm_properties_view_api->ui_static_text(args, object_r, "Type Index", NULL, tm_temp_allocator_api->printf(ta, "%" PRIu64, id.type));
            object_r.y = copy_disabled_text(args, object_r, "Type Hash", tm_temp_allocator_api->printf(ta, "%" PRIu64, tm_the_truth_api->type_name_hash(tt, (tm_tt_type_t){ id.type })));
            if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = object_r, .text = tm_temp_allocator_api->printf(ta, "Inspect type %s", type_names[id.type]) })) {
                show_this_type(args->tab->inst, tm_tt_type(id));
            }
            object_r.y += metrics_item_h;
            object_r.y = inspect_object_memory_usage(args, object_r, tt, id);
            // 2. Owner
            if (owner.u64) {
                tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = object_r, .text = TM_LOCALIZE("Owner:") });
                tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = object_r, .text = TM_LOCALIZE("Owner:") });
                object_r.y += metrics_item_h;
                bool owner_expanded = false;
                uint64_t tidx = tm_tt_type(owner).u64;
                object_r.x += 10;
                const char *owner_name = get_name_if_exists(args->tt, owner);
                const char *owner_group_name = owner_name ? tm_temp_allocator_api->printf(ta, "ID: %" PRIu64 " Type: %s Name: %s", id.u64, type_names[owner.type], owner_name) : tm_temp_allocator_api->printf(ta, "ID: %" PRIu64 " Type: %s", id.u64, type_names[owner.type]);
                object_r.y = tm_properties_view_api->ui_group(args, object_r, owner_group_name, NULL, owner, 0, false, &owner_expanded);
                if (owner_expanded) {
                    object_r.x += 10;
                    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = object_r, .text = "Inspect" })) {
                        args->tab->inst->filter.object = owner;
                        args->tab->inst->filter.mode = MODE_DISPLAY_SPECIFIC_OBJECT;
                        args->tab->inst->filter.picked = 0;
                        args->tab->inst->buffer[0] = 0;
                    }
                    object_r.y += metrics_item_h;
                    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = object_r, .text = tm_temp_allocator_api->printf(ta, "Inspect type %s", type_names[tidx]) })) {
                        show_this_type(args->tab->inst, tm_tt_type(owner));
                    }
                    object_r.y += metrics_item_h;
                    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = object_r, .text = tm_temp_allocator_api->printf(ta, "Show all objects of type %s", type_names[tidx]) })) {
                        show_all_object_of_this_type(args->tab->inst, tm_tt_type(owner));
                    }
                    object_r.y += metrics_item_h;
                    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = object_r, .text = "Show all objects it owns" })) {
                        show_all_object_owned_by_this(args->tab->inst, owner);
                    }
                    object_r.y += metrics_item_h;
                    object_r.x -= 10;
                }
                object_r.x -= 10;
            }
            // 3. Properties
            tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = object_r, .text = TM_LOCALIZE("Properties:") });
            tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = object_r, .text = TM_LOCALIZE("Properties:") });
            object_r.y += metrics_item_h;
            object_r.y = inspect_object_properties(args, object_r, tt, type, id);
            group_r.y = object_r.y + uib.metrics[TM_UI_METRIC_MARGIN];
        }
    }
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return group_r.y;
}

#pragma endregion

#pragma region tab

#pragma region ui

static float show_owned_objects(tm_properties_ui_args_t *args, tm_rect_t group_r, const tm_the_truth_o *tt, const struct filter_t *filter, const char **type_names)
{
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = group_r, .text = TM_LOCALIZE("Owens:") });
    tm_ui_api->label(args->ui, args->uistyle, &(tm_ui_label_t){ .rect = group_r, .text = TM_LOCALIZE("Owens:") });
    group_r.y += metrics_item_h;
    const tm_the_truth_property_definition_t *props = tm_the_truth_api->properties(tt, tm_tt_type(filter->object));
    uint32_t index = 0;
    for (const tm_the_truth_property_definition_t *prop = props; prop != tm_carray_end(props); ++prop, ++index) {
        switch (prop->type) {
        case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT: {
            tm_tt_id_t subobject = tm_the_truth_api->get_subobject(tt, tm_tt_read(tt, filter->object), index);
            group_r.y = inspect_objects(args, group_r, tt, subobject, type_names);
        } break;
        case TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT_SET: {
            TM_INIT_TEMP_ALLOCATOR(ta);
            const tm_tt_id_t *subobjects = tm_the_truth_api->get_subobject_set(tt, tm_tt_read(tt, filter->object), index, ta);
            for (const tm_tt_id_t *subobject = subobjects; subobject != tm_carray_end(subobjects); ++subobject) {
                group_r.y = inspect_objects(args, group_r, tt, *subobject, type_names);
            }
            TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
        } break;
        default:
            break;
        }
    }
    return group_r.y;
}

// TODO: add advanced filter
// TODO: add tagging
// TODO: add change
static void tab__ui(tm_tab_o *data, struct tm_ui_o *ui, const struct tm_ui_style_t *uistyle_in, tm_rect_t content_r)
{
    struct filter_t *filter = &data->filter;
    // -- set up
    tm_ui_buffers_t uib = tm_ui_api->buffers(ui);
    tm_ui_style_t *uistyle = (tm_ui_style_t[]){ *uistyle_in };
    tm_draw2d_style_t *style = &(tm_draw2d_style_t){ 0 };
    tm_ui_api->to_draw_style(ui, style, uistyle);
    // -- set property view settings:
    data->prop_args.ui = ui;
    data->prop_args.uistyle = uistyle;
    data->prop_args.tab = &data->tm_tab_i;
    const float content_y = content_r.y;
    content_r = tm_rect_pad(content_r, -uib.metrics[TM_UI_METRIC_MARGIN], -uib.metrics[TM_UI_METRIC_MARGIN]);
    // -- header / search bar:
    bool not_in_list = false;
    tm_rect_t label_r = { content_r.x, content_r.y, default_metrics[TM_PROPERTIES_METRIC_LABEL_WIDTH], metrics_item_h };
    TM_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);
    char **type_names = data->type_names;
    tm_tt_type_t *tt_types = data->tt_types;
    char **search_items = data->search_items;
    char **search_aspect_names = data->search_aspect_names;
    if (filter->mode != MODE_DISPLAY_SPECIFIC_OBJECT && filter->mode != MODE_DISPLAY_ASPECT) {
        tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = label_r, .text = TM_LOCALIZE("Truth Type Name") });
        tm_rect_t text_r = { content_r.x + label_r.w + 10, content_r.y, content_r.w - label_r.w - 10, metrics_item_h };
        if (tm_ui_popup_item_picker_api->pick_textedit(ui, uistyle, &(tm_ui_textedit_picker_t){ .rect = text_r, .default_text = "Enter Truth Type Name or Hash or Index", .num_of_strings = (uint32_t)tm_carray_size(search_items) - 1, .strings = search_items, .search_text = data->buffer, .search_text_bytes = 1024 }, &data->filter.picked, &not_in_list)) {
            uint64_t index = tm_hash_get_rv(&data->index_or_hash_to_type, TM_STRHASH_U64(tm_murmur_hash_string(search_items[data->filter.picked])));
            data->filter.picked = (uint32_t)index;
        }
        if (tm_vec2_in_rect(uib.input->mouse_pos, text_r) && (uib.input->right_mouse_pressed || uib.input->left_mouse_pressed)) {
            data->filter.object = (tm_tt_id_t){ 0 };
        }
        content_r.y += metrics_item_h;
    } else if (filter->mode != MODE_DISPLAY_SPECIFIC_OBJECT) {
        tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = label_r, .text = TM_LOCALIZE("Aspect Name") });
        tm_rect_t text_r = { content_r.x + label_r.w + 10, content_r.y, content_r.w - label_r.w - 10, metrics_item_h };
        if (tm_ui_popup_item_picker_api->pick_textedit(ui, uistyle, &(tm_ui_textedit_picker_t){ .rect = text_r, .default_text = "Enter Truth Type Name or Hash or Index", .num_of_strings = (uint32_t)tm_carray_size(search_aspect_names) - 1, .strings = search_aspect_names, .search_text = data->buffer, .search_text_bytes = 1024 }, &data->filter.picked, &not_in_list)) {
            uint64_t index = tm_hash_get_rv(&data->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(search_aspect_names[data->filter.picked])));
            data->filter.picked = (uint32_t)index;
        }
        if (tm_vec2_in_rect(uib.input->mouse_pos, text_r) && (uib.input->right_mouse_pressed || uib.input->left_mouse_pressed)) {
            data->filter.object = (tm_tt_id_t){ 0 };
        }
        content_r.y += metrics_item_h;
    } else {
        tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = label_r, .text = TM_LOCALIZE("Truth ID") });
        tm_rect_t text_r = { content_r.x + label_r.w + 10, content_r.y, content_r.w - label_r.w - 10, metrics_item_h };
        if (tm_ui_api->textedit(ui, uistyle, &(tm_ui_textedit_t){ .rect = tm_rect_split_right(text_r, 50, 0, 0), .default_text = "Enter a Truth ID (uint64_t)" }, data->buffer, 1024)) {
            uint64_t id = strtoull(data->buffer, 0, 10);
            filter->object = (tm_tt_id_t){ id };
        }
        if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .icon = TM_UI_ICON__FILTER, .rect = tm_rect_split_right(text_r, 50, 0, 1) })) {
            uint64_t id = strtoull(data->buffer, 0, 10);
            filter->object = (tm_tt_id_t){ id };
        }
        if (tm_vec2_in_rect(uib.input->mouse_pos, text_r) && (uib.input->right_mouse_pressed || uib.input->left_mouse_pressed)) {
            data->filter.object = (tm_tt_id_t){ 0 };
        }
        content_r.y += metrics_item_h;
    }
    //-- mode
    label_r.y = content_r.y;
    tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = label_r, .text = TM_LOCALIZE("Display Mode") });
    tm_rect_t checkbox_row = { content_r.x + label_r.w + 10, content_r.y + metrics_item_h, content_r.w - label_r.w - 10, metrics_item_h };

    if (tm_ui_api->radio(ui, uistyle, &(tm_ui_radio_t){ .rect = tm_rect_divide_x(checkbox_row, 5, 4, 0), .text = TM_LOCALIZE("Specific Object") }, filter->mode == MODE_DISPLAY_SPECIFIC_OBJECT)) {
        data->buffer[0] = 0;
        filter->object = (tm_tt_id_t){ 0 };
        filter->mode = MODE_DISPLAY_SPECIFIC_OBJECT;
    }

    if (tm_ui_api->radio(ui, uistyle, &(tm_ui_radio_t){ .rect = tm_rect_divide_x(checkbox_row, 5, 4, 1), .text = TM_LOCALIZE("All objects of type") }, filter->mode == MODE_DISPLAY_OBJECTS_OF_TYPE)) {
        filter->object = (tm_tt_id_t){ 0 };
        filter->mode = MODE_DISPLAY_OBJECTS_OF_TYPE;
    }

    if (tm_ui_api->radio(ui, uistyle, &(tm_ui_radio_t){ .rect = tm_rect_divide_x(checkbox_row, 5, 4, 2), .text = TM_LOCALIZE("Type defintion") }, filter->mode == MODE_DISPLAY_TYPE)) {
        filter->mode = MODE_DISPLAY_TYPE;
        filter->object = (tm_tt_id_t){ 0 };
    }

    if (tm_ui_api->radio(ui, uistyle, &(tm_ui_radio_t){ .rect = tm_rect_divide_x(checkbox_row, 5, 4, 3), .text = TM_LOCALIZE("Show Aspects") }, filter->mode == MODE_DISPLAY_ASPECT)) {
        filter->mode = MODE_DISPLAY_ASPECT;
        data->buffer[0] = 0;
        filter->object = (tm_tt_id_t){ 0 };
    }

    if (!data->buffer[0]) {
        filter->picked = 0;
    }

    const bool display_use_aspect = filter->mode == MODE_DISPLAY_ASPECT;
    const bool display_only_type = filter->mode == MODE_DISPLAY_TYPE;
    const bool display_object = filter->mode == MODE_DISPLAY_SPECIFIC_OBJECT;
    const bool display_objects_of_type = filter->mode == MODE_DISPLAY_OBJECTS_OF_TYPE;
    const bool display_owned_objects = filter->mode == MODE_DISPLAY_OBJECTS_OWNED_BY;

    tm_rect_t filter_r = { .x = content_r.x, .y = content_r.y + 2 * metrics_item_h, .w = content_r.w, .h = metrics_item_h };
    if (display_owned_objects) {
        filter_r.y = tm_properties_view_api->ui_static_text(&data->prop_args, filter_r, "Filter", NULL, tm_temp_allocator_api->printf(ta, "Show only objects owned by #%" PRIu64, data->filter.object.u64));
        if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .rect = filter_r, .text = "Reset filter" })) {
            data->filter.mode = MODE_DISPLAY_TYPE;
            data->filter.object = (tm_tt_id_t){ 0 };
        }
        filter_r.y += metrics_item_h + uib.metrics[TM_UI_METRIC_MARGIN];
    } else if (display_objects_of_type) {
        filter_r.y = tm_properties_view_api->ui_static_text(&data->prop_args, filter_r, "Filter", NULL, data->buffer[0] ? tm_temp_allocator_api->printf(ta, "Show objects of type %s", type_names[data->filter.picked]) : "No type selected");
    } else if (display_only_type && data->buffer[0]) {
        if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .rect = filter_r, .text = "Reset filter" })) {
            data->filter.mode = MODE_DISPLAY_TYPE;
            data->buffer[0] = 0;
            data->filter.object = (tm_tt_id_t){ 0 };
        }
        filter_r.y += metrics_item_h + uib.metrics[TM_UI_METRIC_MARGIN];
    } else if (display_object && data->buffer[0]) {
        if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .rect = filter_r, .text = "Reset Search" })) {
            data->buffer[0] = 0;
            data->filter.object = (tm_tt_id_t){ 0 };
        }
        filter_r.y += metrics_item_h + uib.metrics[TM_UI_METRIC_MARGIN];
    }

    //-- display entries:
    float offset = (filter_r.y - content_y) + uib.metrics[TM_UI_METRIC_MARGIN];
    tm_rect_t canvas_r = { .w = content_r.w, .h = data->max_y + offset };
    content_r.y = filter_r.y;
    content_r.h -= offset;
    tm_ui_scrollview_t scrollview = {
        .rect = content_r,
        .canvas = canvas_r,
        .visibility_y = TM_UI_SCROLLBAR_VISIBILITY_WHEN_NEEDED,
    };
    tm_rect_t scroll_content_r;
    tm_ui_api->begin_scrollview(ui, uistyle, &scrollview, NULL, &data->scroll_y, &scroll_content_r);
    style->clip = uistyle->clip = tm_draw2d_api->add_sub_clip_rect(uib.vbuffer, uistyle->clip, scroll_content_r);
    float max_y = 0;
    tm_rect_t row_r = {
        .x = scroll_content_r.x + 10,
        .y = scroll_content_r.y + 10 + canvas_r.y - data->scroll_y,
        .w = scroll_content_r.w - 20,
        .h = 20.0f,
    };
    if (filter->picked && !not_in_list && !display_use_aspect) {
        if (display_objects_of_type) {
            tm_tt_id_t *ids = tm_the_truth_api->all_objects_of_type(data->tt, tt_types[filter->picked], ta);
            for (tm_tt_id_t *id = ids; id != tm_carray_end(ids); ++id) {
                row_r.y = inspect_objects(&data->prop_args, row_r, data->tt, *id, type_names);
            }
            max_y = row_r.y;
        } else {
            max_y = inspect_type(&data->prop_args, row_r, data->tt, tt_types[filter->picked], type_names[filter->picked]);
        }
    } else if (display_only_type) {
        uint32_t tidx = 0;
        tm_rect_t type_r = row_r;
        for (tm_tt_type_t *type = tt_types; type != tm_carray_end(tt_types); ++type, ++tidx) {
            bool type_expanded = false;
            type_r.y = tm_properties_view_api->ui_group(&data->prop_args, type_r, tm_temp_allocator_api->printf(ta, "Name: %s - Index: %" PRIu32 " - Hash: %" PRIu64, type_names[tidx], tidx, TM_STRHASH_U64(tm_the_truth_api->type_name_hash(data->tt, (tm_tt_type_t){ tidx }))), NULL, (tm_tt_id_t){ tidx }, 0, false, &type_expanded);
            if (type_expanded) {
                type_r.y = inspect_type(&data->prop_args, type_r, data->tt, *type, type_names[tidx]);
            }
        }
        max_y = type_r.y;
    } else if (display_owned_objects && data->filter.object.u64) {
        tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = row_r, .text = TM_LOCALIZE("Owner:") });
        tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = row_r, .text = TM_LOCALIZE("Owner:") });
        row_r.y += metrics_item_h;
        row_r.y = inspect_objects(&data->prop_args, row_r, data->tt, data->filter.object, type_names);
        row_r.y = tm_properties_view_api->ui_horizontal_line(&data->prop_args, row_r);
        max_y = show_owned_objects(&data->prop_args, row_r, data->tt, &data->filter, type_names);
    } else if (display_object) {
        if (tm_the_truth_api->is_alive(data->tt, data->filter.object)) {
            tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = row_r, .text = TM_LOCALIZE("Found Object:") });
            tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = row_r, .text = TM_LOCALIZE("Found Object:") });
            row_r.y += metrics_item_h;
            max_y = inspect_objects(&data->prop_args, row_r, data->tt, data->filter.object, type_names);
        } else {
            tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = row_r, .text = TM_LOCALIZE("No object found") });
        }
    } else if (!filter->picked && display_use_aspect) {
        bool type_aspects_expanded = false;
        static tm_tt_id_t id = { 0 };
        if (!id.u64)
            id.u64 = tm_random_api->next();
        float y = tm_properties_view_api->ui_group(&data->prop_args, row_r, "Type Aspects", NULL, id, 0, false, &type_aspects_expanded);
        row_r.y = y;
        if (type_aspects_expanded) {
            for (uint32_t i = 0; i < data->prop_aspect_begin; ++i) {
                row_r.y = aspect_ui(&data->prop_args, row_r, data->aspects[i]);
                const tm_the_truth_get_types_with_aspect_t *types = tm_the_truth_api->get_types_with_aspect(data->tt, data->aspects[filter->picked]->type_hash, ta);
                bool show_types = false;
                static tm_tt_id_t type_id = { 0 };
                if (!type_id.u64)
                    type_id.u64 = tm_random_api->next();
                row_r.y = tm_properties_view_api->ui_group(&data->prop_args, row_r, tm_temp_allocator_api->printf(ta, "Types with aspect (%" PRIu64 ")", tm_carray_size(types)), NULL, type_id, 0, false, &show_types);
                if (show_types) {
                    for (uint32_t t = 0; t < (uint32_t)tm_carray_size(types); ++t) {
                        const char *type_name = type_names[types[t].type.u64];
                        tm_rect_t typename_r = tm_rect_split_right(row_r, 50, 0, 0);
                        tm_rect_t btn_r = tm_rect_split_right(row_r, 50, 0, 1);
                        typename_r.x += 10;
                        tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = typename_r, .text = type_name });
                        if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .rect = btn_r, .text = "Inspect" })) {
                            show_this_type(data, types[t].type);
                        }
                        row_r.y = row_r.y + row_r.h + uib.metrics[TM_UI_METRIC_MARGIN];
                    }
                }
                row_r.y = tm_properties_view_api->ui_horizontal_line(&data->prop_args, row_r);
            }
        }
        bool prop_aspects_expanded = false;
        static tm_tt_id_t prop_id = { 0 };
        if (!prop_id.u64)
            prop_id.u64 = tm_random_api->next();
        y = tm_properties_view_api->ui_group(&data->prop_args, row_r, "Property Aspects", NULL, prop_id, 0, false, &prop_aspects_expanded);
        row_r.y = y;
        if (prop_aspects_expanded) {
            for (uint32_t i = data->prop_aspect_begin + 1; i < (uint32_t)tm_carray_size(data->aspects); ++i) {
                row_r.y = aspect_ui(&data->prop_args, row_r, data->aspects[i]);
                row_r.y = tm_properties_view_api->ui_horizontal_line(&data->prop_args, row_r);
            }
        }
        max_y = row_r.y;
    } else if (filter->picked && display_use_aspect) {
        row_r.y = aspect_ui(&data->prop_args, row_r, data->aspects[filter->picked]);
        if (filter->picked <= data->prop_aspect_begin) {
            const tm_the_truth_get_types_with_aspect_t *types = tm_the_truth_api->get_types_with_aspect(data->tt, data->aspects[filter->picked]->type_hash, ta);
            bool show_types = false;
            static tm_tt_id_t prop_id = { 0 };
            if (!prop_id.u64)
                prop_id.u64 = tm_random_api->next();
            row_r.y = tm_properties_view_api->ui_group(&data->prop_args, row_r, tm_temp_allocator_api->printf(ta, "Types with aspect (%" PRIu64 ")", tm_carray_size(types)), NULL, prop_id, 0, false, &show_types);
            if (show_types) {
                for (uint32_t t = 0; t < (uint32_t)tm_carray_size(types); ++t) {
                    const char *type_name = type_names[types[t].type.u64];
                    tm_rect_t typename_r = tm_rect_split_right(row_r, 50, 0, 0);
                    tm_rect_t btn_r = tm_rect_split_right(row_r, 50, 0, 1);
                    typename_r.x += 10;
                    tm_ui_api->label(ui, uistyle, &(tm_ui_label_t){ .rect = typename_r, .text = type_name });
                    if (tm_ui_api->button(ui, uistyle, &(tm_ui_button_t){ .rect = btn_r, .text = "Inspect" })) {
                        show_this_type(data, types[t].type);
                    }
                    row_r.y = row_r.y + row_r.h + uib.metrics[TM_UI_METRIC_MARGIN];
                }
            }
        }
        max_y = row_r.y;
    }
    data->max_y = max_y + uib.metrics[TM_UI_METRIC_MARGIN];
    tm_ui_api->end_scrollview(ui, NULL, &data->scroll_y, true);
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
}

#pragma endregion

#pragma region definition

static const char *tab__create_menu_name(void)
{
    return "Truth Inspector Tab";
}

static const char *tab__title(tm_tab_o *tab, struct tm_ui_o *ui)
{
    return "Truth Inspector Tab";
}

static void tab__set_root(tm_tab_o *tab, struct tm_the_truth_o *tt, tm_tt_id_t root)
{
    if (tt == tab->prop_args.tt)
        return;

    tab->prop_args.tt = tt;

    if (tab->properties_view)
        tm_properties_view_api->destroy_properties_view(tab->properties_view);

    tab->properties_view = tm_properties_view_api->create_properties_view(tab->allocator,
        &(tm_properties_view_config_t){
            .tt = tt,
            .undo_stack = tab->prop_args.undo_stack,
        });

    tab->prop_args.pv = tab->properties_view;
}

static tm_tab_i *tab__create(tm_tab_create_context_t *context, tm_ui_o *ui)
{
    tm_allocator_i *allocator = context->allocator;
    uint64_t *id = context->id;
    tm_unused(ui);

    static tm_the_machinery_tab_vt *vt = 0;
    if (!vt)
        vt = tm_global_api_registry->get(TM_TRUTH_INSPECTOR_TAB_VT_NAME);

    tm_tab_o *tab = tm_alloc(allocator, sizeof(tm_tab_o));
    *tab = (tm_tab_o){
        .tm_tab_i = {
            .vt = (tm_tab_vt *)vt,
            .inst = (tm_tab_o *)tab,
            .root_id = *id,
        },
        .expanded = { .allocator = allocator },
        .index_or_hash_to_type = { .allocator = allocator, .default_value = UINT32_MAX },
        .index_or_hash_to_aspect = { .allocator = allocator, .default_value = UINT32_MAX },
        .allocator = allocator,
        .tt = context->tt,
        .prop_args = (tm_properties_ui_args_t){
            .metrics = default_metrics,
            .undo_stack = context->undo_stack,
            .asset_root = tm_application_api->asset_root(context->application),
        }
    };
    {
        uint32_t types = tm_the_truth_api->num_types(tab->tt);
        tm_carray_push(tab->type_names, "Nil", allocator);
        tm_hash_add_rv(&tab->index_or_hash_to_type, 0, 0);
        tm_carray_push(tab->tt_types, (tm_tt_type_t){ 0 }, allocator);
        TM_INIT_TEMP_ALLOCATOR(ta);
        for (uint32_t type = 1; type < types; ++type) {
            const char *org_name = tm_the_truth_api->type_name(tab->tt, (tm_tt_type_t){ type });
            char buffer[1024] = { 0 };
            tm_get_display_name(org_name, buffer, (uint32_t)strlen(org_name) + 1);
            char *name = tm_alloc(allocator, strlen(buffer) + 1);
            strcpy(name, buffer);
            tm_carray_push(tab->type_names, name, allocator);
            tm_hash_add_rv(&tab->index_or_hash_to_type, TM_STRHASH_U64(tm_murmur_hash_string(name)), tm_carray_size(tab->type_names) - 1);
            tm_carray_push(tab->search_items, name, allocator);
            tm_carray_push(tab->tt_types, (tm_tt_type_t){ type }, allocator);
            {
                char *type_index_str_tmp = tm_temp_allocator_api->printf(ta, "%" PRIu32, type);
                char *type_index_str = tm_alloc(allocator, strlen(type_index_str_tmp) + 1);
                strcpy(type_index_str, type_index_str_tmp);
                tm_carray_push(tab->search_items, type_index_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_type, TM_STRHASH_U64(tm_murmur_hash_string(type_index_str)), tm_carray_size(tab->type_names) - 1);
            }
            {
                tm_strhash_t hash = tm_the_truth_api->type_name_hash(tab->tt, (tm_tt_type_t){ type });
                char *type_hash_str_tmp = tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(hash));
                char *type_hash_str = tm_alloc(allocator, strlen(type_hash_str_tmp) + 1);
                strcpy(type_hash_str, type_hash_str_tmp);
                tm_carray_push(tab->search_items, type_hash_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_type, TM_STRHASH_U64(tm_murmur_hash_string(type_hash_str)), tm_carray_size(tab->type_names) - 1);
            }
        }
        TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    }
    // add aspects
    {
        TM_INIT_TEMP_ALLOCATOR(ta);
        uint32_t num_i = 0;
        tm_tt_inspector_aspect_t **aspect_data = (tm_tt_inspector_aspect_t **)tm_global_api_registry->implementations(TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, &num_i);
        for (uint32_t i = 0; i < num_i; ++i) {
            tm_carray_push(tab->aspects, aspect_data[i], allocator);
            const uint32_t idx = (uint32_t)tm_carray_size(tab->aspects) - 1;
            {
                char *type_index_str = tm_alloc(allocator, strlen(aspect_data[i]->name) + 1);
                strcpy(type_index_str, aspect_data[i]->name);
                tm_carray_push(tab->search_aspect_names, type_index_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(type_index_str)), idx);
            }
            {
                char *type_index_str = tm_alloc(allocator, strlen(aspect_data[i]->define) + 1);
                strcpy(type_index_str, aspect_data[i]->define);
                tm_carray_push(tab->search_aspect_names, type_index_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(type_index_str)), idx);
            }
            {
                char *type_hash_str_tmp = tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(aspect_data[i]->type_hash));
                char *type_hash_str = tm_alloc(allocator, strlen(type_hash_str_tmp) + 1);
                strcpy(type_hash_str, type_hash_str_tmp);
                tm_carray_push(tab->search_aspect_names, type_hash_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(type_hash_str)), idx);
            }
        }
        tab->prop_aspect_begin = (uint32_t)tm_carray_size(tab->aspects) - 1;
        tm_tt_inspector_aspect_t **prop_aspect_data = (tm_tt_inspector_aspect_t **)tm_global_api_registry->implementations(TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME, &num_i);
        for (uint32_t i = 0; i < num_i; ++i) {
            tm_carray_push(tab->aspects, prop_aspect_data[i], allocator);
            const uint32_t idx = (uint32_t)tm_carray_size(tab->aspects) - 1;
            {
                char *type_index_str = tm_alloc(allocator, strlen(prop_aspect_data[i]->name) + 1);
                strcpy(type_index_str, prop_aspect_data[i]->name);
                tm_carray_push(tab->search_aspect_names, type_index_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(type_index_str)), idx);
            }
            {
                char *type_index_str = tm_alloc(allocator, strlen(prop_aspect_data[i]->define) + 1);
                strcpy(type_index_str, prop_aspect_data[i]->name);
                tm_carray_push(tab->search_aspect_names, type_index_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(type_index_str)), idx);
            }
            {
                char *type_hash_str_tmp = tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(prop_aspect_data[i]->type_hash));
                char *type_hash_str = tm_alloc(allocator, strlen(type_hash_str_tmp) + 1);
                strcpy(type_hash_str, type_hash_str_tmp);
                tm_carray_push(tab->search_aspect_names, type_hash_str, allocator);
                tm_hash_add_rv(&tab->index_or_hash_to_aspect, TM_STRHASH_U64(tm_murmur_hash_string(type_hash_str)), idx);
            }
        }
        TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    }
    tab__set_root(tab, context->tt, (tm_tt_id_t){ 0 });
    *id += 1000000;
    return &tab->tm_tab_i;
}

static const char *tab__create_menu_category(void)
{
    return TM_LOCALIZE("Debugging");
}

static void tab__destroy(tm_tab_o *tab)
{
    tm_properties_view_api->destroy_properties_view(tab->properties_view);
    tm_carray_free(tab->type_names, tab->allocator);
    for (uint32_t i = 0; i < tm_carray_size(tab->search_items); ++i) {
        tm_free(tab->allocator, tab->search_items[i], strlen(tab->search_items[i]) + 1);
    }
    for (uint32_t i = 0; i < tm_carray_size(tab->search_aspect_names); ++i) {
        tm_free(tab->allocator, tab->search_aspect_names[i], strlen(tab->search_aspect_names[i]) + 1);
    }
    tm_carray_free(tab->search_aspect_names, tab->allocator);
    tm_carray_free(tab->search_items, tab->allocator);
    tm_carray_free(tab->tt_types, tab->allocator);
    tm_set_free(&tab->expanded);
    tm_hash_free(&tab->index_or_hash_to_type);
    tm_hash_free(&tab->index_or_hash_to_aspect);
    tm_carray_free(tab->aspects, tab->allocator);
    tm_free(tab->allocator, tab, sizeof(*tab));
}

static tm_the_machinery_tab_vt *truth_inspector_tab_vt = &(tm_the_machinery_tab_vt){
    .name = TM_TRUTH_INSPECTOR_TAB_VT_NAME,
    .name_hash = TM_TRUTH_INSPECTOR_TAB_VT_NAME_HASH,
    .create_menu_name = tab__create_menu_name,
    .create = tab__create,
    .destroy = tab__destroy,
    .title = tab__title,
    .set_root = tab__set_root,
    .run_as_job = false,
    .always_update = true,
    .create_menu_category = tab__create_menu_category,
    .ui = tab__ui
};

#pragma endregion

TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api *reg, bool load)
{
    tm_global_api_registry = reg;
    tm_draw2d_api = reg->get(TM_DRAW2D_API_NAME);
    tm_ui_api = reg->get(TM_UI_API_NAME);
    tm_ui_popup_item_picker_api = reg->get(TM_UI_POPUP_ITEM_PICKER_API_NAME);
    tm_properties_view_api = reg->get(TM_PROPERTIES_VIEW_API_NAME);
    tm_application_api = reg->get(TM_APPLICATION_API_NAME);
    tm_os_clipboard_api = reg->get(TM_OS_CLIPBOARD_API_NAME);
    tm_logger_api = reg->get(TM_LOGGER_API_NAME);

    tm_localizer_api = reg->get(TM_LOCALIZER_API_NAME);
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_temp_allocator_api = reg->get(TM_TEMP_ALLOCATOR_API_NAME);
    tm_random_api = reg->get(TM_RANDOM_API_NAME);
    tm_error_api = reg->get(TM_ERROR_API_NAME);

    tm_set_or_remove_api(reg, load, TM_TRUTH_INSPECTOR_TAB_VT_NAME, truth_inspector_tab_vt);
    tm_add_or_remove_implementation(reg, load, TM_TAB_VT_INTERFACE_NAME, truth_inspector_tab_vt);
    load_aspect_uis(reg, load);
    load_asset_browser_integration(reg, load);
}
