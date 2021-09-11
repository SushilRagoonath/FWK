static array(json5) roots;
static array(char*) sources;

bool data_push(const char *source) {
    char *source_rw = STRDUP(source);
    json5 root = {0};
    char *error = json5_parse(&root, source_rw, 0);
    if( error ) {
        FREE(source_rw);
        return false;
    } else {
        array_push(sources, source_rw);
        array_push(roots, root);
        return true;
    }
}

bool data_pop() {
    if( array_count(roots) > 0 ) {
        FREE(*array_back(sources));
        array_pop(sources);

        json5_free(array_back(roots));
        array_pop(roots);
        return true;
    }
    return false;
}

json5* data_node(const char *keypath) {
    json5 *j = array_back(roots), *r = j;
    for each_substring( keypath, "/[.]", key ) {
        r = 0;
        /**/ if( j->type == JSON5_ARRAY ) r = j = &j->array[atoi(key)];
        else if( j->type == JSON5_OBJECT )
        for( int i = 0; !r && i < j->count; ++i ) {
            if( j->nodes[i].name && !strcmp(j->nodes[i].name, key) ) {
                r = j = &j->nodes[i];
                break;
            }
        }
        if( !j ) break;
    }
    return r;
}

int data_count(const char *keypath) {
    json5* j = data_node(keypath);
    return j ? j->count : 0;
}

data_t data_get(bool is_string, const char *keypath) {
    json5 *j = data_node(keypath);
    data_t v = {0};
    v.i = j ? j->integer : 0; // ( j->name && !strcmp(j->name, key) ) ? j->integer : 0;
    if(is_string && !v.p) v.s = "";
    //if(is_string) printf("s:%s\n", v.s); else printf("f:%f\n", v.f);
    return v;
}
