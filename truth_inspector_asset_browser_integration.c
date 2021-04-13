
extern struct tm_localizer_api *tm_localizer_api;
extern struct tm_the_truth_api *tm_the_truth_api;
extern struct tm_temp_allocator_api *tm_temp_allocator_api;

static struct tm_the_machinery_api *tm_the_machinery_api;
static struct tm_asset_browser_tab_api *tm_asset_browser_tab_api_opt;

#include <foundation/api_registry.h>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>
#include <foundation/temp_allocator.h>
#include <foundation/undo.h>
#include <foundation/localizer.h>

#include <plugins/ui/docking.h>
#include <plugins/editor_views/asset_browser.h>
#include <plugins/ui/ui.h>

#include <the_machinery/the_machinery.h>
#include <the_machinery/asset_browser_tab.h>

#include "truth_inspector.h"

#include <inttypes.h>

extern void show_this_object(tm_tab_o *data, tm_tt_id_t object);
extern void show_this_type(tm_tab_o *data, tm_tt_type_t type);
extern const char *get_name_if_exists(const tm_the_truth_o *tt, tm_tt_id_t object);

typedef struct pair_t
{
    tm_tt_id_t asset_object;
    tm_tt_id_t asset;
} pair_t;

static pair_t private__asset_browser_selected_asset(tm_the_truth_o *tt, tm_tt_id_t asset_browser_object)
{
    if (!tm_asset_browser_tab_api_opt->selected_object)
        return (pair_t){0};

    const tm_tt_id_t selected = tm_asset_browser_tab_api_opt->selected_object(tt, asset_browser_object);
    if (!selected.u64)
        return (pair_t){0};

    const bool is_dir = selected.type == tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__ASSET_DIRECTORY).u64;

    if (!is_dir && selected.type != tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__ASSET).u64)
        return (pair_t){0};

    if (is_dir)
    {
        return (pair_t){0, selected};
    }

    const tm_tt_id_t asset_object = tm_the_truth_api->get_subobject(tt, tm_tt_read(tt, selected), TM_TT_PROP__ASSET__OBJECT);
    if (!asset_object.u64)
        return (pair_t){0};

    return (pair_t){asset_object, selected};
}

#define TM_U64_VALID(value) (value.u64 != 0)

static void asset_browser__menu__select_asset(void *inst, tm_asset_browser_o *ab, tm_ui_o *ui, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object, tm_undo_stack_i *undo_stack)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);

    if (!TM_U64_VALID(selected.asset_object) && !TM_U64_VALID(selected.asset))
        return;
    const bool is_dir = TM_U64_VALID(selected.asset);

    tm_tab_i *tab = tm_the_machinery_api->create_or_select_tab(tm_the_machinery_api->app, ui, TM_TRUTH_INSPECTOR_TAB_VT_NAME, &(tm_docking_find_tab_opt_t){.exclude_pinned = true});
    show_this_object(tab->inst, is_dir ? selected.asset : selected.asset_object);
}
static void asset_browser__menu__inspect__item(struct tm_ui_menu_item_t *item, void *ud, tm_asset_browser_o *ab, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (TM_U64_VALID(selected.asset_object) || TM_U64_VALID(selected.asset))
    {
        const bool is_dir = TM_U64_VALID(selected.asset);
        const char *name = get_name_if_exists(tt, is_dir ? selected.asset : selected.asset_object);
        name = name && *name != '\0' ? name : tm_temp_allocator_api->frame_printf("#%" PRIu64, (is_dir ? selected.asset.u64 : selected.asset_object.u64));
        item->text = tm_temp_allocator_api->frame_printf(TM_LOCALIZE("Inspect \"%s\""), name);
    }
}
static void asset_browser__menu__select_asset_object(void *inst, tm_asset_browser_o *ab, tm_ui_o *ui, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object, tm_undo_stack_i *undo_stack)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (!TM_U64_VALID(selected.asset_object))
        return;

    tm_tab_i *tab = tm_the_machinery_api->create_or_select_tab(tm_the_machinery_api->app, ui, TM_TRUTH_INSPECTOR_TAB_VT_NAME, &(tm_docking_find_tab_opt_t){.exclude_pinned = true});
    show_this_object(tab->inst, selected.asset_object);
}
static void asset_browser__menu__inspect__asset_object(struct tm_ui_menu_item_t *item, void *ud, tm_asset_browser_o *ab, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (TM_U64_VALID(selected.asset_object))
    {
        const char *name = get_name_if_exists(tt, selected.asset_object);
        name = name && *name != '\0' ? name : tm_temp_allocator_api->frame_printf("#%" PRIu64, (selected.asset_object.u64));
        item->text = tm_temp_allocator_api->frame_printf(TM_LOCALIZE("Inspect Object \"%s\""), name);
    }
}

static void asset_browser__menu__select_asset_object_type(void *inst, tm_asset_browser_o *ab, tm_ui_o *ui, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object, tm_undo_stack_i *undo_stack)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (!TM_U64_VALID(selected.asset_object))
        return;

    tm_tab_i *tab = tm_the_machinery_api->create_or_select_tab(tm_the_machinery_api->app, ui, TM_TRUTH_INSPECTOR_TAB_VT_NAME, &(tm_docking_find_tab_opt_t){.exclude_pinned = true});
    show_this_type(tab->inst, tm_tt_type(selected.asset_object));
}

static void asset_browser__menu__inspect__asset_object_type(struct tm_ui_menu_item_t *item, void *ud, tm_asset_browser_o *ab, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (TM_U64_VALID(selected.asset_object))
    {
        const char *name = tm_the_truth_api->type_name(tt, tm_tt_type(selected.asset_object));
        item->text = tm_temp_allocator_api->frame_printf(TM_LOCALIZE("Inspect Type %s"), name);
    }
}

static void asset_browser__menu__select_asset_type(void *inst, tm_asset_browser_o *ab, tm_ui_o *ui, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object, tm_undo_stack_i *undo_stack)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (!TM_U64_VALID(selected.asset_object) && !TM_U64_VALID(selected.asset))
        return;
    const bool is_dir = TM_U64_VALID(selected.asset);
    tm_tab_i *tab = tm_the_machinery_api->create_or_select_tab(tm_the_machinery_api->app, ui, TM_TRUTH_INSPECTOR_TAB_VT_NAME, &(tm_docking_find_tab_opt_t){.exclude_pinned = true});
    show_this_type(tab->inst, tm_tt_type(is_dir ? selected.asset : selected.asset_object));
}

static void asset_browser__menu__inspect__asset_type(struct tm_ui_menu_item_t *item, void *ud, tm_asset_browser_o *ab, struct tm_the_truth_o *tt, tm_tt_id_t asset_browser_object)
{
    const pair_t selected = private__asset_browser_selected_asset(tt, asset_browser_object);
    if (TM_U64_VALID(selected.asset_object) || TM_U64_VALID(selected.asset))
    {
        const bool is_dir = TM_U64_VALID(selected.asset);
        const char *name = tm_the_truth_api->type_name(tt, tm_tt_type(is_dir ? selected.asset : selected.asset_object));
        item->text = tm_temp_allocator_api->frame_printf(TM_LOCALIZE("Inspect Type %s"), name);
    }
}

static tm_asset_browser_custom_menu_item_i asset_browser__menu__inspect_asset = {
    .menu_item = asset_browser__menu__inspect__item,
    .menu_select = asset_browser__menu__select_asset,
};

static tm_asset_browser_custom_menu_item_i asset_browser__menu__inspect_asset_object = {
    .menu_item = asset_browser__menu__inspect__asset_object,
    .menu_select = asset_browser__menu__select_asset_object,
};

static tm_asset_browser_custom_menu_item_i asset_browser__menu__inspect_asset_object_type = {
    .menu_item = asset_browser__menu__inspect__asset_object_type,
    .menu_select = asset_browser__menu__select_asset_object_type,
};

static tm_asset_browser_custom_menu_item_i asset_browser__menu__inspect_asset_type = {
    .menu_item = asset_browser__menu__inspect__asset_type,
    .menu_select = asset_browser__menu__select_asset_type,
};

void load_asset_browser_integration(struct tm_api_registry_api *reg, bool load)
{
    tm_the_machinery_api = reg->get(TM_THE_MACHINERY_API_NAME);

    tm_asset_browser_tab_api_opt = reg->get_optional(TM_ASSET_BROWSER_TAB_API_NAME);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_TAB_CUSTOM_MENU_ITEMS_INTERFACE_NAME, &asset_browser__menu__inspect_asset);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_TAB_CUSTOM_MENU_ITEMS_INTERFACE_NAME, &asset_browser__menu__inspect_asset_object);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_TAB_CUSTOM_MENU_ITEMS_INTERFACE_NAME, &asset_browser__menu__inspect_asset_type);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_TAB_CUSTOM_MENU_ITEMS_INTERFACE_NAME, &asset_browser__menu__inspect_asset_object_type);
}