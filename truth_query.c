extern struct tm_api_registry_api *tm_global_api_registry;
extern struct tm_properties_view_api *tm_properties_view_api;
extern struct tm_ui_api *tm_ui_api;
extern struct tm_temp_allocator_api *tm_temp_allocator_api;
extern struct tm_os_clipboard_api *tm_os_clipboard_api;
extern struct tm_localizer_api *tm_localizer_api;
extern struct tm_the_truth_api *tm_the_truth_api;

#include <foundation/api_registry.h>
#include <foundation/temp_allocator.h>
#include <foundation/api_types.h>
#include <foundation/macros.h>
#include <foundation/localizer.h>
#include <foundation/the_truth.h>
#include <foundation/string.inl>
#include <foundation/log.h>
#include <foundation/carray.inl>
#include <foundation/murmurhash64a.inl>
#include <foundation/hash.inl>

#include "truth_query.h"
#include "truth_inspector.h"

typedef struct tm_strhash_to_uint32 TM_HASH_T(tm_strhash_t, uint32_t) tm_strhash_to_uint32;

enum token_e
{
    NONE,
    GET,
    IF,
    SEMICOLON,
    EQUAL,
    NOT,
    LESS_THAN,
    GREATER_THAN,
    LESS_THAN_INCLUSIV,
    GREATER_THAN_INCLUSIV,
    AND,
    OR,
    OBJECTS,
    IDENTIFIER,
    NUMBER,
    COLLUM,
    HAS,
    TYPE,
    DEFINE,
    PROPERTY,
    ASPECT,
    INDEX,
};

typedef struct token_t
{
    enum token_e t;
    const char *v;
    char data[64];
    uint32_t size;
} token_t;

enum value_type_e
{
    VALUE_T_NONE,
    VALUE_T_STRING,
    VALUE_T_HASH,
    VALUE_T_DEFINE,
    VALUE_T_PROPERTY_INDEX,
};

enum context_e
{
    CONTEXT_NONE,
    CONTEXT_TYPE,
    CONTEXT_PROP,
    CONTEXT_ASPECT,
};

typedef struct get_t
{
    const char *value;
    enum context_e context;
    enum value_type_e value_type;
    tm_strhash_t hash;
    bool show_objects;
} get_t;

enum method_e
{
    METHOD_NONE,
    METHOD_EQUAL,
    METHOD_NOT_EUQAL,
    METHOD_LESS_THAN,
    METHOD_NOT_LESS_THAN,
    METHOD_GREATER_THAN,
    METHOD_NOT_GREATER_THAN,
    METHOD_LESS_THAN_INCLUSIV,
    METHOD_NOT_LESS_THAN_INCLUSIV,
    METHOD_GREATER_THAN_INCLUSIV,
    METHOD_NOT_GREATER_THAN_INCLUSIV,
    METHOD_HAS,
    METHOD_HAS_NOT,
};

enum connection_e
{
    CON_NONE,
    CON_AND,
    CON_OR,
};

typedef struct condition_t
{
    // how it will be interpreted
    enum context_e context;
    // comparison
    enum method_e method;
    // next condition
    enum connection_e and_or;
    // index 0 --> 64 bytes data type
    // index 1 --> next 64 data type
    enum value_type_e data_type[2];
    char value[128];
} condition_t;

typedef struct if_t
{
    uint32_t num_conditions;
    condition_t *conditions;
} if_t;

typedef struct tm_truth_query_o
{
    tm_allocator_i allocator;
    const tm_the_truth_o *tt;
    tm_strhash_to_uint32 defines;
    tm_strhash_to_uint32 aspects;
    tm_hash32_t type_to_aspect;
    tm_tt_type_t **aspect_types;
} tm_truth_query_o;

token_t query_find(const char *start, const char **end, const char *token, enum token_e te)
{
    TM_INIT_TEMP_ALLOCATOR(ta);
    char *str = tm_temp_alloc(ta, strlen(token) + 1);
    memset(str, 0, strlen(token) + 1);
    uint32_t i = 0;
    token_t t = {0};
    while (*start)
    {
        bool found = false;
        const char *ptr = token;
        while (*ptr)
        {
            if (*start == *ptr)
            {
                found = true;
                break;
            }
            ++ptr;
        }
        if (found)
        {
            str[i] = *ptr;
            ++i;
        }
        else
        {
            if (!strcmp(str, token))
            {
                if (*start == ' ' || *start == ';')
                {
                    *end = start;
                    t.t = te;
                    t.v = start;
                }

                break;
            }
            break;
        }
        ++start;
    }
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return t;
};

static tm_truth_query_result_t query_consume_token(const tm_truth_query_o *data, const token_t *token, tm_temp_allocator_i *ta);

const tm_truth_query_result_t query_parse(const tm_truth_query_o *data, const char *p)
{
    tm_truth_query_result_t res = {0};
    TM_INIT_TEMP_ALLOCATOR(ta);
    token_t *token = 0;
    bool has_get = false;
    bool error = false;
    while (*p)
    {
        if (*p == 'G' && !has_get)
        {
            const char *end = NULL;
            token_t t = query_find(p, &end, "GET", GET);
            if (t.v)
            {
                has_get = true;
                tm_carray_temp_push(token, t, ta);
                p = end;
            }
            else
            {
                TM_LOG("Could not find GET the query must be incorrect.");
                return res;
            }
        }
        else if (*p == ' ')
        {
            ++p;
        }
        else if (*p == ';')
        {
            token_t t = (token_t){.t = SEMICOLON, .v = p};
            tm_carray_temp_push(token, t, ta);
            ++p;
        }
        else if (*p == '#')
        {
            token_t t = (token_t){.t = DEFINE, .v = p};
            const char *end = strchr(p, ' ');
            end = end ? end : strchr(p, ';');
            const char *v = tm_temp_allocator_api->printf(ta, "%.*s", (end) - (p), p);
            strcpy(t.data, v);
            tm_carray_temp_push(token, t, ta);
            if (end)
            {
                p = end;
            }
            else
            {
                error = true;
            }
        }
        else if (*p == '=')
        {
            size_t l = strlen(p);
            if (l > 1)
            {
                const char *next = p;
                ++next;
                if (*next == '=')
                {
                    ++next;
                    token_t t = (token_t){.t = EQUAL, .v = next};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
                else
                {
                    token_t t = (token_t){.t = EQUAL, .v = p};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
            }
            else
            {
                error = true;
            }
        }
        else if (*p == ':')
        {
            size_t l = strlen(p);
            if (l > 1)
            {
                const char *next = p;
                ++next;
                if (*next == ':')
                {
                    token_t t = (token_t){.t = COLLUM, .v = next};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
                else
                {
                    error = true;
                }
            }
            else
            {
                error = true;
            }
        }
        else if (*p == '>')
        {
            size_t l = strlen(p);
            if (l > 1 || l > 2)
            {
                const char *next = p;
                ++next;
                if (*next != '=')
                {
                    token_t t = (token_t){.t = GREATER_THAN, .v = next};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
                else if (l > 2 && *next == '=')
                {
                    token_t t = (token_t){.t = GREATER_THAN_INCLUSIV, .v = next};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
                else
                {
                    error = true;
                }
            }
            else
            {
                error = true;
            }
        }
        else if (*p == '<')
        {
            size_t l = strlen(p);
            if (l > 1 || l > 2)
            {
                const char *next = p;
                ++next;
                if (*next != '=')
                {
                    token_t t = (token_t){.t = LESS_THAN, .v = next};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
                else if (l > 2 && *next == '=')
                {
                    token_t t = (token_t){.t = LESS_THAN_INCLUSIV, .v = next};
                    tm_carray_temp_push(token, t, ta);
                    p = next;
                }
                else
                {
                    error = true;
                }
            }
            else
            {
                error = true;
            }
        }
        else if (*p == '[') //[0]
        {
            size_t l = strlen(p);
            if (l >= 3)
            {
                const char *end = strchr(p, ']');
                if (end)
                {
                    ++p;
                    char *end_of_number;
                    const float v = (float)strtod(p, &end_of_number);
                    token_t t = (token_t){.t = INDEX, .v = p};
                    memcpy(t.data, &v, sizeof(float));
                    tm_carray_temp_push(token, t, ta);
                    ++end;
                    p = end;
                }
                else
                {
                    error = true;
                }
            }
            else
            {
                error = true;
            }
        }
        else if (*p >= '0' && *p <= '9')
        {
            char *end_of_number;
            const float v = (float)strtod(p, &end_of_number);
            token_t t = (token_t){.t = NUMBER, .v = p};
            memcpy(t.data, &v, sizeof(float));
            tm_carray_temp_push(token, t, ta);
            p = end_of_number;
        }
        else if (*p >= 'a' && *p <= 'z' || *p == '_' || *p == '-' || *p >= 'A' && *p <= 'Z')
        {

            if (*p == 'N')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "NOT", NOT);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            else if (*p == 'A')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "AND", AND);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
                end = NULL;
                t = query_find(p, &end, "ASPECT", ASPECT);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            else if (*p == 'O')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "OR", OR);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
                t = query_find(p, &end, "OBJECTS", OBJECTS);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            else if (*p == 'H')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "HAS", HAS);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            else if (*p == 'I')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "IF", IF);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            else if (*p == 'T')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "TYPE", TYPE);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            else if (*p == 'P')
            {
                const char *end = NULL;
                token_t t = query_find(p, &end, "PROPERTY", PROPERTY);
                if (t.v)
                {
                    tm_carray_temp_push(token, t, ta);
                    p = end;
                    continue;
                }
            }
            token_t t = (token_t){.t = IDENTIFIER, .v = p};
            const char *end = strchr(p, ' ');
            end = end ? end : strchr(p, ':');
            end = end ? end : strchr(p, ';');
            end = end ? end : strchr(p, '\0');
            const char *v = tm_temp_allocator_api->printf(ta, "%.*s", (end) - (p), p);
            strcpy(t.data, v);
            tm_carray_temp_push(token, t, ta);
            if (end)
            {
                p = end;
            }
            else
            {
                p++;
            }
        }
        if (error)
        {
            TM_LOG("Unknown Token!");
            return res;
        }
    }
    if (!has_get)
    {
        TM_LOG("Could not find GET the query must be incorrect.");
    }
    res = query_consume_token(data, token, ta);
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return res;
}

static get_t query_consume_token_get(const token_t *token, const token_t *show, tm_temp_allocator_i *ta)
{
    get_t s = {0};
    bool found_aspect = false;
    bool found_type = false;
    for (const token_t *t = show; t != tm_carray_end(token); ++t)
    {
        bool b = false;
        switch (t->t)
        {
        case NONE:
        case GET:
        case HAS:
            break;
        case OBJECTS:
            s.show_objects = true;
            break;
        case ASPECT:
        {
            if (!found_aspect)
            {
                found_aspect = true;
                s.context = CONTEXT_ASPECT;
            }
            else
            {
                TM_LOG("Illegal Statement: ASPECT can only be defined once");
                return (get_t){0};
            }
        }
        break;
        case TYPE:
        {
            if (!found_aspect && !found_type)
            {
                found_type = true;
                s.context = CONTEXT_TYPE;
            }
            else
            {
                TM_LOG("Illegal Statement: TYPE can only be defined once");
                return (get_t){0};
            }
        }
        break;
        case DEFINE:
        case IDENTIFIER:
        {
            if (found_type || found_aspect)
            {
                s.value = t->data;
            }
            else if (!found_type && !found_aspect)
            {
                found_type = true;
                s.value = t->data;
                s.context = CONTEXT_TYPE;
            }

            if (t->t == DEFINE)
            {
                TM_INIT_TEMP_ALLOCATOR(a);
                s.hash = tm_murmur_hash_string(tm_temp_allocator_api->printf(a, "TM_TT_TYPE__%s", s.value + 1));
                s.value_type = VALUE_T_DEFINE;
                TM_SHUTDOWN_TEMP_ALLOCATOR(a);
            }
            else
            {
                s.value_type = VALUE_T_STRING;
                s.hash = tm_murmur_hash_string(s.value);
            }
        }
        break;
        case NUMBER:
            s.hash = (tm_strhash_t)(t->data);
            if (!found_type && !found_aspect)
            {
                found_type = true;
                s.value = t->data;
                s.value_type = VALUE_T_HASH;
                s.context = CONTEXT_TYPE;
            }
            else if (found_aspect || found_type)
            {
                s.value_type = VALUE_T_HASH;
            }
            break;
        case IF:
        default:
            if (!found_type && !found_aspect)
            {
                TM_LOG("Illegal Statement: A GET statement needs a TYPE or ASPECT!");
                return (get_t){0};
            }
            b = true;
            break;
        }

        if (b)
            break;
    }
    return s;
};

static condition_t get_identifier(const token_t *begin, const token_t **end, bool has_not, tm_temp_allocator_i *ta)
{
    const token_t *next = begin;
    ++next;

    condition_t cond = {
        .context = CONTEXT_PROP,
    };

    // IDENTIFIER COLLUM IDENTIFIER
    // example: SHOW tm_asset IF object::name = test;
    // check if subobject:
    if (next->t == COLLUM)
    {
        ++next;
        if (next->t == IDENTIFIER)
        {
            cond.data_type[1] = VALUE_T_STRING;
            memcpy(cond.value + 64, next->data, strlen(next->data));
        }
        else if (next->t == NUMBER)
        {
            cond.data_type[1] = VALUE_T_PROPERTY_INDEX;
            memcpy(cond.value + 64, next->data, strlen(next->data));
        }
        else
        {
            TM_LOG("There has been a syntax error with your IF comparison statement when trying to access a subobject.");
            return (condition_t){0};
        }
        ++next;
    }

    if (begin->t == IDENTIFIER)
    {
        cond.data_type[0] = VALUE_T_STRING;
        memcpy(cond.value, begin->data, strlen(begin->data));
    }
    else if (begin->t == NUMBER)
    {
        cond.data_type[0] = VALUE_T_PROPERTY_INDEX;
        memcpy(cond.value, begin->data, sizeof(float));
    }
    else if (begin->t == DEFINE)
    {
        cond.data_type[0] = VALUE_T_DEFINE;
        memcpy(cond.value, begin->data + 1, strlen(begin->data + 1)); // +1 to get ride of #
    }
    else
    {
        TM_LOG("There has been a syntax error with your IF comparison statement");
        return (condition_t){0};
    }
    if (next != begin)
    {
        *end = begin;
    }
    else
    {
        *end = next;
    }
    return cond;
}

// SHOW TYPE IF HAS ASPECT #TM_TT_ASPECT__PROPERTIES
// SHOW TYPE IF HAS PROPERTY name AND HAS ASPECT #TM_TT_ASPECT__PROPERTIES
// SHOW tm_vec3_t IF x > 0.4
// TODO: NO NAIVE IF!
static if_t query_consume_token_if(enum context_e context, const token_t *token, const token_t *if_v, tm_temp_allocator_i *ta)
{
    bool has_has = false;
    bool has_not = false;
    enum connection_e connection = {0};
    if_t res = {0};
    for (const token_t *t = if_v; t != tm_carray_end(token); ++t)
    {
        bool end = false;
        switch (t->t)
        {
        case OR:
        case AND:
        {
            if (!tm_carray_size(res.conditions))
            {
                connection = t->t == AND ? CON_AND : CON_OR;
            }
            else
            {
                res.conditions[tm_carray_size(res.conditions) - 1].and_or = t->t == AND ? CON_AND : CON_OR;
            }
        }
        case IF:
            break;
        case NOT:
            has_not = true;
            break;
        case HAS:
            has_has = true;
            break;
        case PROPERTY:
        case ASPECT:
        {
            if (has_has)
            {
                const token_t *next = t;
                ++next;
                condition_t cond = {
                    .method = has_not ? METHOD_NOT_EUQAL : METHOD_EQUAL,
                    .context = t->t == ASPECT ? CONTEXT_ASPECT : CONTEXT_PROP,
                };
                if (next->t == IDENTIFIER)
                {
                    cond.data_type[0] = VALUE_T_STRING;
                    memcpy(cond.value, next->data, strlen(next->data));
                }
                else if (next->t == NUMBER)
                {
                    cond.data_type[0] = VALUE_T_HASH;
                    memcpy(cond.value, next->data, sizeof(float));
                }
                else if (next->t == DEFINE)
                {
                    cond.data_type[0] = VALUE_T_DEFINE;
                    memcpy(cond.value, next->data + 1, strlen(next->data + 1)); // +1 to get ride of #
                }
                else
                {
                    TM_LOG("There has been a syntax error with your IF %s statement", t->t == ASPECT ? "ASPECT" : "PROPERTY");
                    return (if_t){0};
                }
                if (connection != CON_NONE)
                {
                    cond.and_or = connection;
                    connection = CON_NONE;
                }
                t = next;
                tm_carray_temp_push(res.conditions, cond, ta);
                has_not = false;
            }
            else
            {
                TM_LOG("query: There has been a syntax error. A IF statement with a %s needs a HAS", t->t == ASPECT ? "ASPECT" : "PROPERTY");
                return (if_t){0};
            }
        }
        break;
        case GREATER_THAN:
        case LESS_THAN:
        case EQUAL:
        case LESS_THAN_INCLUSIV:
        case GREATER_THAN_INCLUSIV:
        {
            if (!tm_carray_size(res.conditions))
            {
                TM_LOG("Syntax error");
                return (if_t){0};
            }
            condition_t *cond = &res.conditions[tm_carray_size(res.conditions) - 1];
            switch (t->t)
            {
            case GREATER_THAN:
                cond->method = has_not ? METHOD_NOT_GREATER_THAN : METHOD_GREATER_THAN;
                break;
            case LESS_THAN:
                cond->method = has_not ? METHOD_NOT_LESS_THAN : METHOD_LESS_THAN;
                break;
            case EQUAL:
                cond->method = has_not ? METHOD_NOT_EUQAL : METHOD_EQUAL;
                break;
            case LESS_THAN_INCLUSIV:
                cond->method = has_not ? METHOD_NOT_LESS_THAN_INCLUSIV : METHOD_LESS_THAN_INCLUSIV;
                break;
            case GREATER_THAN_INCLUSIV:
                cond->method = has_not ? METHOD_NOT_GREATER_THAN_INCLUSIV : METHOD_GREATER_THAN_INCLUSIV;
                break;
            }
        }
        break;
        case IDENTIFIER:
        case INDEX:
        case NUMBER:
        {
            condition_t cond = get_identifier(t, &t, has_not, ta);
            if (connection != CON_NONE)
            {
                cond.and_or = connection;
                connection = CON_NONE;
            }
            tm_carray_temp_push(res.conditions, cond, ta);
            has_not = false;
        }
        break;
            break;
        default:
            end = true;
            break;
        }

        if (end)
            break;
    }
    return res;
}

static tm_truth_query_result_t run(const tm_truth_query_o *data, const get_t *get, const if_t *if_v, tm_temp_allocator_i *ta)
{
    tm_truth_query_result_t res = {0};

    const bool has_value = get->value_type != VALUE_T_NONE;
    const bool has_if = tm_carray_size(if_v->conditions) > 0;
    if (get->context == CONTEXT_TYPE)
    {
        if (!has_if && has_value)
        {
            tm_strhash_t type_hash = {0};
            if (get->value_type == VALUE_T_DEFINE)
            {
                uint32_t num_i;
                const tm_truth_query_define_t **define_data = (const tm_truth_query_define_t **)tm_global_api_registry->implementations(TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME, &num_i);
                uint32_t index = tm_hash_get(&data->defines, get->hash);
                if (index)
                {
                    type_hash = define_data[index]->type;
                }
                else
                {
                    TM_LOG("Defines \"%s\" unknown!", get->value);
                    return res;
                }
            }
            else
            {
                type_hash = get->hash;
            }
            res.type = tm_the_truth_api->object_type_from_name_hash(data->tt, type_hash);
            res.res_type = TM_TRUTH_QUERY_RESULT_TYPE;
            if (get->show_objects)
            {
                res.objects = tm_the_truth_api->all_objects_of_type(data->tt, res.type, ta);
                res.num_objects = tm_carray_size(res.objects);
                res.res_type = TM_TRUTH_QUERY_RESULT_OBJECTS;
            }
        }
        else if (has_if && !has_value)
        {
        }
    }
    else if (get->context == CONTEXT_ASPECT)
    {
    }
    else
    {
        TM_LOG("Inspect Query: Show statement can only be of type TYPE or ASPECT");
    }
    return res;
}

static tm_truth_query_result_t query_consume_token(const tm_truth_query_o *data, const token_t *token, tm_temp_allocator_i *ta)
{
    get_t get = {0};
    if_t if_v = {0};
    for (const token_t *t = token; t != tm_carray_end(token); ++t)
    {
        switch (t->t)
        {
        case GET:
            get = query_consume_token_get(token, t, ta);
            if (get.context == CONTEXT_NONE)
            {
                break;
            }
            break;
        case IF:
            if_v = query_consume_token_if(get.context, token, t, ta);
            break;
        default:
            break;
        }
    }
    return run(data, &get, &if_v, ta);
}

static tm_truth_query_o *create(struct tm_allocator_i *allocator, const tm_the_truth_o *tt)
{
    tm_allocator_i a = tm_allocator_api->create_child(allocator, "tm_truth_query_o");
    tm_truth_query_o *q = tm_alloc(&a, sizeof(tm_truth_query_o));
    *q = (tm_truth_query_o){
        .allocator = a,
        .tt = tt,
    };
    q->defines.allocator = &q->allocator;
    q->aspects.allocator = &q->allocator;
    uint32_t num_i;
    const tm_truth_query_define_t **define_data = (const tm_truth_query_define_t **)tm_global_api_registry->implementations(TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME, &num_i);
    for (uint32_t i = 0; i < num_i; ++i)
    {
        tm_hash_add_rv(&q->defines, i == 0 ? 0 : tm_murmur_hash_string(define_data[i]->name), i);
    }
    const tm_tt_inspector_aspect_t **aspect_data = (const tm_tt_inspector_aspect_t **)tm_global_api_registry->implementations(TM_THE_TRUTH_INSPECTOR_ASPECT_INTERFACE_NAME, &num_i);
    TM_INIT_TEMP_ALLOCATOR(ta)
    for (uint32_t i = 0; i < num_i; ++i)
    {
        tm_carray_push(q->aspect_types, 0, &q->allocator);
        tm_hash_add_rv(&q->aspects, i == 0 ? 0 : tm_murmur_hash_string(aspect_data[i]->define), i);
        tm_hash_add(&q->aspects, i == 0 ? 0 : aspect_data[i]->type_hash, i);
        tm_hash_add_rv(&q->aspects, i == 0 ? 0 : tm_murmur_hash_string(tm_temp_allocator_api->printf(ta, "%" PRIu64, aspect_data[i]->type_hash)), i);
        const tm_the_truth_get_types_with_aspect_t *twa = tm_the_truth_api->get_types_with_aspect(tt, aspect_data[i]->type_hash, ta);
        for (const tm_the_truth_get_types_with_aspect_t *t = twa; t != tm_carray_end(twa); ++t)
        {
            tm_carray_push(q->aspect_types[i], t->type, &q->allocator);
        }
    }
    TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
    return q;
}

static void destroy(struct tm_truth_query_o *query_api)
{
    tm_allocator_i a = query_api->allocator;
    tm_hash_free(&query_api->defines);
    tm_free(&a, query_api, sizeof(*query_api));
    tm_allocator_api->destroy_child(&a);
}

struct tm_truth_query_api *tm_truth_query_api = &(struct tm_truth_query_api){
    .create = create,
    .destroy = destroy,
    .run = query_parse,
};

static tm_truth_query_define_t *nill = &(tm_truth_query_define_t){0};

extern void load_query_defines(struct tm_api_registry_api *reg, bool load);
void load_query(struct tm_api_registry_api *reg, bool load)
{
    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_QUERY_REGISTER_DEFINE_INTERFACE_NAME, nill);
    load_query_defines(reg, load);
}