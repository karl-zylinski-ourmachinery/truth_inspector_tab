extern struct tm_api_registry_api *tm_global_api_registry;
extern struct tm_properties_view_api *tm_properties_view_api;
extern struct tm_ui_api *tm_ui_api;
extern struct tm_temp_allocator_api *tm_temp_allocator_api;
extern struct tm_os_clipboard_api *tm_os_clipboard_api;
extern struct tm_localizer_api *tm_localizer_api;
extern struct tm_the_truth_api *tm_the_truth_api;

#include <foundation/api_registry.h>
#include <foundation/api_types.h>
#include <foundation/localizer.h>
#include <foundation/macros.h>
#include <foundation/string.inl>
#include <foundation/temp_allocator.h>
#include <foundation/the_truth.h>

#include <plugins/editor_views/properties.h>
#include <plugins/os_window/os_window.h>
#include <plugins/ui/docking.h>
#include <plugins/ui/ui.h>
#include <plugins/ui/ui_custom.h>

#include "truth_inspector.h"

// -- aspect headers
#include <foundation/the_truth_assets.h>
#include <plugins/animation/animation_state_machine.h>
#include <plugins/animation/state_graph.h>
#include <plugins/editor_views/graph.h>
#include <plugins/editor_views/tree_view.h>
#include <plugins/physics/physics_shape_component.h>
#include <plugins/shader_system/shader_system_creation_graph.h>
#include <plugins/the_machinery_shared/asset_aspects.h>
#include <plugins/the_machinery_shared/component_interfaces/editor_ui_interface.h>
#include <plugins/the_machinery_shared/component_interfaces/render_interface.h>
#include <plugins/the_machinery_shared/component_interfaces/shader_interface.h>
#include <plugins/ui/clipboard.h>

#include <inttypes.h>
#include <string.h>

struct tm_tab_o;

extern const float metrics_item_h;

extern float aspect_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_tt_inspector_aspect_t *aspect);
extern float copy_disabled_text(tm_properties_ui_args_t *args, tm_rect_t props_r, const char *label, const char *data);
extern float inspect_type(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_o *tt, tm_tt_type_t type, const char *type_name, bool *show_only_this_type);
extern void show_this_type(tm_tab_o *data, tm_tt_type_t type);
extern void show_all_object_of_this_type(tm_tab_o *data, tm_tt_type_t type);

//---

static float file_extension_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *);
static tm_tt_inspector_aspect_t *file_extension = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__FILE_EXTENSION",
    .name = "tm_tt_assets_file_extension_aspect_i",
    .type_hash = TM_TT_ASPECT__FILE_EXTENSION,
    .description = "Defines the file ending in the Asset Browser and on the HDD. Note that on the HDD tm_ is added.",
    .ui = file_extension_ui,
};

static float file_extension_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data)
{
    return tm_properties_view_api->ui_static_text(args, props_r, "File Extension", NULL, (const char *)aspect_data->data);
}

//--

static tm_tt_inspector_aspect_t *asm_state_interface = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__ASM_STATE_INTERFACE",
    .name = "tm_asm_state_interface_properties",
    .type_hash = TM_TT_ASPECT__ASM_STATE_INTERFACE,
    .description = "Aspect that indicates that a Truth Type implements the enum tm_asm_state_interface_properties",
};

static tm_tt_inspector_aspect_t *asm_transit_interface = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__ASM_TRANSITION_INTERFACE",
    .name = "tm_asm_transition_interface_properties",
    .type_hash = TM_TT_ASPECT__ASM_TRANSITION_INTERFACE,
    .description = " Aspect that indicates that a Truth Type implements the enum tm_asm_transition_interface_properties",
};

static tm_tt_inspector_aspect_t *asm_state_graph = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__STATE_GRAPH",
    .name = "tm_state_graph_aspect_i",
    .type_hash = TM_TT_ASPECT__STATE_GRAPH,
    .description = "Aspect that defines how a type interacts with the state graph.",
};

static tm_tt_inspector_aspect_t *asm_state_graph_node = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__STATE_GRAPH__NODE",
    .name = "tm_state_graph_node_aspect_i",
    .type_hash = TM_TT_ASPECT__STATE_GRAPH__NODE,
    .description = "Used to define a State Graph Node",
};

static tm_tt_inspector_aspect_t *state_graph_transtion = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__STATE_GRAPH__TRANSITION",
    .name = "tm_state_graph_transition_aspect_i",
    .type_hash = TM_TT_ASPECT__STATE_GRAPH__TRANSITION,
    .description = "Use for transition",
};

// ---
static float graph_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data);
static tm_tt_inspector_aspect_t *graph_aspect = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__GRAPH",
    .name = "tm_graph_aspect_i",
    .type_hash = TM_TT_ASPECT__GRAPH,
    .description = "Aspects that controls how a Truth type should be rendered by the graph view.",
    .ui = graph_ui,
};

static float graph_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data)
{
    TM_INIT_TEMP_ALLOCATOR(ta);
    const tm_graph_aspect_i *graph = (const tm_graph_aspect_i *)aspect_data->data;
    props_r.y = copy_disabled_text(args, props_r, "Node Interface Name", graph->node_interface_name);
    props_r.y = copy_disabled_text(args, props_r, "IO Type Interface Name", graph->io_type_interface_name);
    tm_tt_type_t type = tm_the_truth_api->object_type_from_name_hash(args->tt, graph->subgraph_type_hash);
    const char *org_name = tm_the_truth_api->type_name(args->tt, type);
    char *name = tm_temp_alloc(ta, strlen(org_name) + 1);
    tm_get_display_name(org_name, name, (uint32_t)strlen(org_name) + 1);
    props_r.y = copy_disabled_text(args, props_r, "Subgraph type Hash", tm_temp_allocator_api->printf(ta, "%" PRIu64, TM_STRHASH_U64(graph->subgraph_type_hash)));
    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = props_r, .text = tm_temp_allocator_api->printf(ta, "Show all objects of Subgraph Type (%s)", name) }))
        show_all_object_of_this_type(args->tab->inst, type);
    props_r.y += metrics_item_h;
    if (tm_ui_api->button(args->ui, args->uistyle, &(tm_ui_button_t){ .rect = props_r, .text = tm_temp_allocator_api->printf(ta, "Inspect Subgraph Type (%s)", name) }))
        show_this_type(args->tab->inst, type);
    props_r.y += metrics_item_h;
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return props_r.y;
}

// ---

static float properties_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data);
static tm_tt_inspector_aspect_t *properties_aspect = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__PROPERTIES",
    .name = "tm_properties_aspect_i",
    .type_hash = TM_TT_ASPECT__PROPERTIES,
    .description = "Aspect that can be used to customize the property view for a specific Truth type. (contains: tm_properties_aspect_i)",
    .ui = properties_ui,
};

static float properties_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data)
{
    const tm_properties_aspect_i *p = (const tm_properties_aspect_i *)aspect_data->data;
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Custom UI", NULL, p->custom_ui ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Custom Subobject UI", NULL, p->custom_subobject_ui ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Custom Child UI", NULL, p->custom_child_ui ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Custom Display Name", NULL, p->get_display_name ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Custom Type Display Name", NULL, p->get_type_display_name ? "Yes" : "No");
    if (p->get_type_display_name)
        props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Type Display Name", NULL, p->get_type_display_name());
    return props_r.y;
}
// ---
static tm_tt_inspector_aspect_t *properties_valid_aspect = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__VALIDATE",
    .name = "tm_validate_aspect_i",
    .type_hash = TM_TT_ASPECT__VALIDATE,
    .description = "Aspect used to validate property values.",
};
// ---
static float treeview_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data);
static tm_tt_inspector_aspect_t *tree_view_aspect = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__TREE_VIEW",
    .name = "tm_tree_view_aspect_i",
    .type_hash = TM_TT_ASPECT__TREE_VIEW,
    .description = "Aspect that controls how a Truth Type should be displayed in the tree.",
    .ui = treeview_ui,
};
static float treeview_ui(tm_properties_ui_args_t *args, tm_rect_t props_r, const tm_the_truth_get_aspects_t *aspect_data)
{
    const tm_tree_view_aspect_i *t = (const tm_tree_view_aspect_i *)aspect_data->data;
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a UI", NULL, t->ui ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a Setup", NULL, t->setup ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a Tooltip", NULL, t->tooltip ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides as Additional Elements", NULL, t->additional_elements ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a Icon", NULL, t->icon ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Can drop above", NULL, t->can_drop_above ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a drop above Func", NULL, t->drop_above ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a Context Menu", NULL, t->context_menu ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Provides a Display Name", NULL, t->display_name ? "Yes" : "No");
    props_r.y = tm_properties_view_api->ui_static_text(args, props_r, "Compute Node Properties", NULL, t->compute_node_properties ? "Yes" : "No");
    return props_r.y;
}
// ---
static tm_tt_inspector_aspect_t *physics_shape = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__PHYSICS_SHAPE_GEOMETRY",
    .name = "tm_physics_shape_geometry",
    .type_hash = TM_TT_ASPECT__PHYSICS_SHAPE_GEOMETRY,
    .description = "Aspect that provides geometry for physics shapes. The entity set with",
};
// ---
static tm_tt_inspector_aspect_t *shader_system = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__SHADER_SYSTEM_DATA_DRIVEN_SETTINGS",
    .name = "tm_shader_system_data_drive_settings_i",
    .type_hash = TM_TT_ASPECT__SHADER_SYSTEM_DATA_DRIVEN_SETTINGS,
    .description = "Implements a \" shader graph compiler \" on top of the Creation Graph concept by exposing HLSL snippets as nodes to the graph.",
};
// ---
static tm_tt_inspector_aspect_t *asset_preview = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__ASSET_PREVIEW",
    .name = "tm_asset_preview_aspect_i",
    .type_hash = TM_TT_ASPECT__ASSET_PREVIEW,
    .description = "Previews Asset",
};
// ---
static tm_tt_inspector_aspect_t *asset_scene = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__ASSET_SCENE",
    .name = "tm_asset_scene_aspect_i",
    .type_hash = TM_TT_ASPECT__ASSET_SCENE,
};
// ---
static tm_tt_inspector_aspect_t *asset_open = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_ASPECT__ASSET_OPEN",
    .name = "tm_asset_open_aspect_i",
    .type_hash = TM_TT_ASPECT__ASSET_OPEN,
    .description = "Assets with this aspect can be opened via double click."
};
// ---
static tm_tt_inspector_aspect_t *ci_editor_ui = &(tm_tt_inspector_aspect_t){
    .define = "TM_CI_EDITOR_UI",
    .name = "tm_ci_editor_ui_i",
    .type_hash = TM_CI_EDITOR_UI,
    .description = "Interface for UI manipulation of the component."
};
// ---
static tm_tt_inspector_aspect_t *ci_render = &(tm_tt_inspector_aspect_t){
    .define = "TM_CI_RENDER",
    .name = "tm_ci_render_i",
    .type_hash = TM_CI_RENDER,
    .description = "Defines the component interface for a rendering subpass."
};
// ---
static tm_tt_inspector_aspect_t *ci_shader = &(tm_tt_inspector_aspect_t){
    .define = "TM_CI_SHADER",
    .name = "tm_ci_shader_i",
    .type_hash = TM_CI_SHADER,
    .description = " Defines the component interface for a shading subpass."
};
// ---

// ------- property aspects
static tm_tt_inspector_aspect_t *props_aspect_props_asset_picker = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_PROP_ASPECT__PROPERTIES__ASSET_PICKER",
    .name = "tm_tt_prop_aspect__properties__asset_picker",
    .type_hash = TM_TT_PROP_ASPECT__PROPERTIES__ASSET_PICKER,
    .description = "Property aspect that specifies that the property should use the Asset Picker to set the value"
};
// ---
static tm_tt_inspector_aspect_t *props_aspect_props_local_entity_picker = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_PROP_ASPECT__PROPERTIES__USE_LOCAL_ENTITY_PICKER",
    .name = "tm_tt_prop_aspect__properties__use_local_entity_picker",
    .type_hash = TM_TT_PROP_ASPECT__PROPERTIES__USE_LOCAL_ENTITY_PICKER,
    .description = "If set to a non-zero value, a \"local entity picker\" will be used to pick the TM_THE_TRUTH_PROPERTY_TYPE_REFERENCE property."
};
// ---
static tm_tt_inspector_aspect_t *props_aspect_props_custom_ui = &(tm_tt_inspector_aspect_t){
    .define = "TM_TT_PROP_ASPECT__PROPERTIES__CUSTOM_UI",
    .name = "tm_tt_prop_aspect__properties__custom_ui",
    .type_hash = TM_TT_PROP_ASPECT__PROPERTIES__CUSTOM_UI,
    .description = "Used to implement a custom UI for a single property in an object"
};

void load_aspect_uis(struct tm_api_registry_api *reg, bool load)
{
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, file_extension);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, graph_aspect);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asm_state_graph);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asm_state_graph_node);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, state_graph_transtion);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asm_transit_interface);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asm_state_interface);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, properties_aspect);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, properties_valid_aspect);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, tree_view_aspect);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, physics_shape);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, shader_system);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asset_open);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asset_preview);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, asset_scene);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, ci_editor_ui);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, ci_render);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, ci_shader);

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME, props_aspect_props_asset_picker);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME, props_aspect_props_custom_ui);
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_INSPECTOR_PROPERTY_ASPECT_INTERFACE_NAME, props_aspect_props_local_entity_picker);
}