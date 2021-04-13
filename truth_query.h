#pragma once
#include <foundation/api_types.h>

// The Truth Query API allowes to write simple queries and to request objects,
// types. The Query Language accepted type indices, type ID's ([[tm_tt_type_t]]), hash, define
// name, [[tm_tt_id_t]]. Defines need to be registered via the
// [[TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME]] otherweise the API will
// not be able to find the define.
// Note: Its not functional. Might

struct tm_the_truth_o;

#define TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME "tm_the_truth_query_register_define_interface"

typedef struct tm_truth_query_define_t
{
    const char *name;
    tm_strhash_t type;
} tm_truth_query_define_t;

enum truth_query_result {
    TM_TRUTH_QUERY_RESULT_TYPE,
    TM_TRUTH_QUERY_RESULT_ASPECT,
    TM_TRUTH_QUERY_RESULT_OBJECT,
    TM_TRUTH_QUERY_RESULT_TYPES,
    TM_TRUTH_QUERY_RESULT_OBJECTS,
};

typedef struct tm_truth_query_result_t
{
    enum truth_query_result res_type;
    tm_tt_type_t type;
    tm_tt_id_t object;
    tm_tt_type_t *types;
    tm_tt_id_t *objects;
    uint64_t num_types;
    uint64_t num_objects;
} tm_truth_query_result_t;

typedef struct tm_truth_query_o tm_truth_query_o;

struct tm_truth_query_api
{
    tm_truth_query_o *(*create)(struct tm_allocator_i *allocator, const struct tm_the_truth_o *tt);
    void (*destroy)(struct tm_truth_query_o *query_api);

    const tm_truth_query_result_t (*run)(struct tm_truth_query_o *query_api, const char *query);
};

#ifdef TM_LINKS_TRUTH_QUERY
extern struct tm_truth_query_api *tm_truth_query_api;
#endif