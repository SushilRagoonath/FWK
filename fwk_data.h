// -----------------------------------------------------------------------------
// data framework (json5) @todo:xml,kvdb
// - rlyeh, public domain
//
// @todo: vec2,vec3,vec4

// data api

API bool    data_push(const char *source);
API int         data_count(const char *keypath);
#define         data_int(...)    data_get(0,stringf(__VA_ARGS__)).i
#define         data_float(...)  data_get(0,stringf(__VA_ARGS__)).f
#define         data_string(...) data_get(1,stringf(__VA_ARGS__)).s
API bool    data_pop();

// internal api

typedef union data_t {
    char* s;
    double f;
    int64_t i;
    uintptr_t p;
    array(union data_t) arr;
} data_t;

API data_t data_get(bool is_string, const char *keypath); // @todo, array(data_t) data_array();
