extern struct tm_api_registry_api *tm_global_api_registry;

#include <foundation/api_types.h>
#include <foundation/api_registry.h>

#include "truth_query.h"

#define TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(type_name)                                                        \
    static tm_truth_query_define_t *the_truth_query_define_TM_TT_TYPE__##type_name = &(tm_truth_query_define_t) \
    {                                                                                                           \
        "TM_TT_TYPE__" #type_name "",                                                                           \
            TM_TT_TYPE_HASH__##type_name,                                                                       \
    }

#define TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, define)                                                                                                   \
    {                                                                                                                                                      \
        if (load)                                                                                                                                          \
        {                                                                                                                                                  \
            tm_global_api_registry->add_implementation(TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME, the_truth_query_define_TM_TT_TYPE__##define);    \
        }                                                                                                                                                  \
        else                                                                                                                                               \
        {                                                                                                                                                  \
            tm_global_api_registry->remove_implementation(TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME, the_truth_query_define_TM_TT_TYPE__##define); \
        }                                                                                                                                                  \
    }
// files with defines:
#include <foundation/plugin_assets.h>
#include <foundation/the_truth_assets.h>
#include <foundation/the_truth_types.h>
#include <foundation/the_truth.h>

#include <plugins/entity/tag_component.h>

// -- defines:
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(TAG_COMPONENT);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(PLUGIN);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(ASSET_ROOT);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(ASSET);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(ASSET_DIRECTORY);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(ASSET_LABEL);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(BOOL);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(FLOAT);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(STRING);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(UINT32_T);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(UINT64_T);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(DOUBLE);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(VEC2);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(VEC3);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(VEC4);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(POSITION);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(ROTATION);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(SCALE);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(RECT);
TM_THE_TRUTH_QUERY_DEFINE_TYPE_DEFINE(ANYTHING);

void load_query_defines(struct tm_api_registry_api *reg, bool load)
{
    tm_global_api_registry = reg;
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, TAG_COMPONENT);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, PLUGIN);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, ASSET_ROOT);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, ASSET);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, ASSET_DIRECTORY);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, ASSET_LABEL);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, BOOL);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, FLOAT);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, STRING);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, UINT32_T);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, UINT64_T);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, DOUBLE);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, VEC2);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, VEC3);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, VEC4);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, POSITION);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, ROTATION);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, SCALE);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, RECT);
    TM_THE_TRUTH_QUERY_REGISTER_DEFINE(load, ANYTHING);
}