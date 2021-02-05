#include <string.h>
#include <stdio.h>

#include <jml.h>

#include <jml_type.h>
#include <jml_value.h>
#include <jml_gc.h>
#include <jml_util.h>
#include <jml_vm.h>


void
jml_value_print(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_BOOL(value))
        printf(AS_BOOL(value) ? "true" : "false");

    else if (IS_NONE(value))
        printf("none");

    else if (IS_NUM(value))
        printf("%g", AS_NUM(value));

    else if (IS_OBJ(value))
        jml_obj_print(value);
#else
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;

        case VAL_NONE:
            printf("none");
            break;

        case VAL_NUM:
            printf("%g", AS_NUM(value));
            break;

        case VAL_OBJ:
            jml_obj_print(value);
            break;
    }
#endif
}


char *
jml_value_stringify(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_BOOL(value))
        return jml_strdup(AS_BOOL(value) ? "true" : "false");

    else if (IS_NONE(value))
        return jml_strdup("none");

    else if (IS_NUM(value)) {
        char str[13];
        sprintf(str, "%g", AS_NUM(value));
        return jml_strdup(str);

    } else if (IS_OBJ(value))
        return jml_obj_stringify(value);
#else
    switch (value.type) {
        case VAL_BOOL:
            return jml_strdup(AS_BOOL(value) ? "true" : "false");

        case VAL_NONE:
            return jml_strdup("none");

        case VAL_NUM: {
            char str[13];
            sprintf(str, "%g", AS_NUM(value));
            return jml_strdup(str);
        }

        case VAL_OBJ:
            return jml_obj_stringify(value);
    }
#endif
    return NULL;
}


const char *
jml_value_stringify_type(jml_value_t value)
{
#ifdef JML_NAN_TAGGING
    if (IS_BOOL(value))
        return "<type bool>";

    else if (IS_NONE(value))
        return "<type none>";

    else if (IS_NUM(value))
        return "<type number>";

    else if (IS_OBJ(value))
        return jml_obj_stringify_type(value);
#else
    switch (value.type) {
        case VAL_BOOL:
            return "<type bool>";

        case VAL_NONE:
            return "<type none>";

        case VAL_NUM:
            return "<type number>";

        case VAL_OBJ:
            return jml_obj_stringify_type(value);
    }
#endif
    return NULL;
}


bool
jml_value_equal(jml_value_t a, jml_value_t b)
{
#ifdef JML_NAN_TAGGING
    if (IS_NUM(a) && IS_NUM(b)) {
        return AS_NUM(a) == AS_NUM(b);
    }

    if (IS_ARRAY(a) && IS_ARRAY(b)) {
        if (AS_ARRAY(a)->values.count != AS_ARRAY(b)->values.count)
            return false;

        for (int i = 0; i < AS_ARRAY(a)->values.count; ++i) {
            if (!jml_value_equal(AS_ARRAY(a)->values.values[i],
                AS_ARRAY(b)->values.values[i]))
                return false;
        }
        return true;
    }

    return a == b;

#else
    if (a.type != b.type)   return false;

    switch (a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NONE:      return true;
        case VAL_NUM:       return AS_NUM(a) == AS_NUM(b);

        case VAL_OBJ: {
            if (IS_ARRAY(a)) {
                if (AS_ARRAY(a)->values.count != AS_ARRAY(b)->values.count)
                    return false;

                for (int i = 0; i < AS_ARRAY(a)->values.count; ++i) {
                    if (!jml_value_equal(AS_ARRAY(a)->values.values[i],
                        AS_ARRAY(b)->values.values[i]))
                        return false;
                }
                return true;
            }

            return AS_OBJ(a) == AS_OBJ(b);
        }

        default:            return false;
    }
#endif
}


void
jml_value_array_init(jml_value_array_t *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}


void
jml_value_array_write(jml_value_array_t *array,
    jml_value_t value)
{
    if (array->capacity < array->count + 1) {
        int old_capacity    = array->count;
        array->capacity     = GROW_CAPACITY(old_capacity);
        array->values       = GROW_ARRAY(jml_value_t,
            array->values, old_capacity, array->capacity);
    }

    array->values[array->count++] = value;
}


void
jml_value_array_free(jml_value_array_t *array)
{
    FREE_ARRAY(jml_value_t, array->values, array->capacity);
    jml_value_array_init(array);
}


void
jml_hashmap_init(jml_hashmap_t *map)
{
    map->count = 0;
    map->capacity = -1;
    map->entries = NULL;
}


void
jml_hashmap_free(jml_hashmap_t *map)
{
    FREE_ARRAY(jml_hashmap_entry_t,
        map->entries, map->capacity + 1);
    jml_hashmap_init(map);
}


static jml_hashmap_entry_t *
jml_hashmap_find_entry(jml_hashmap_entry_t *entries,
    int capacity, jml_obj_string_t *key)
{
    uint32_t index = key->hash & capacity;
    jml_hashmap_entry_t *tombstone = NULL;

    while (true) {
        jml_hashmap_entry_t *entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NONE(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL)
                    tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) & capacity;
    }
}


static void
jml_hashmap_adjust_capacity(jml_hashmap_t *map,
    int capacity)
{
    jml_hashmap_entry_t *entries = ALLOCATE(jml_hashmap_entry_t,
        capacity + 1);

    for (int i = 0; i <= capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NONE_VAL;
    }

    map->count = 0;
    for (int i = 0; i <= map->capacity; i++) {
        jml_hashmap_entry_t *entry = &map->entries[i];
        if (entry->key == NULL) continue;

        jml_hashmap_entry_t *dest = jml_hashmap_find_entry(
            entries, capacity, entry->key
        );
        dest->key = entry->key;
        dest->value = entry->value;
        map->count++;
    }

    FREE_ARRAY(jml_hashmap_entry_t, map->entries, map->capacity + 1);
    map->entries = entries;
    map->capacity = capacity;
}


bool
jml_hashmap_get(jml_hashmap_t *map,
    jml_obj_string_t *key, jml_value_t *value)
{
    if (map->count == 0)    return false;

    jml_hashmap_entry_t *entry = jml_hashmap_find_entry(
        map->entries, map->capacity, key
    );
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}


bool
jml_hashmap_set(jml_hashmap_t *map,
    jml_obj_string_t *key, jml_value_t value)
{
    if (map->count + 1 > (map->capacity + 1) * MAP_LOAD_MAX) {
        int capacity = GROW_CAPACITY(map->capacity + 1) - 1;
        jml_hashmap_adjust_capacity(map, capacity);
    }

    jml_hashmap_entry_t *entry = jml_hashmap_find_entry(
        map->entries, map->capacity, key
    );

    bool new_key = entry->key == NULL;
    if (new_key && IS_NONE(entry->value)) map->count++;

    entry->key = key;
    entry->value = value;
    return new_key;
}


bool
jml_hashmap_pop(jml_hashmap_t *map,
    jml_obj_string_t *key, jml_value_t *value)
{
    if (map->count == 0)
        return OBJ_VAL(vm->sentinel);

    jml_hashmap_entry_t *entry = jml_hashmap_find_entry(
        map->entries, map->capacity, key
    );

    if (entry->key == NULL)
        return false;

    *value = entry->value;

    entry->key = NULL;
    entry->value = TRUE_VAL;

    return true;
}


bool
jml_hashmap_del(jml_hashmap_t *map,
    jml_obj_string_t *key)
{
    if (map->count == 0)    return false;

    jml_hashmap_entry_t *entry = jml_hashmap_find_entry(
        map->entries, map->capacity, key
    );
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = TRUE_VAL;

    return true;
}


void
jml_hashmap_add(jml_hashmap_t *source,
    jml_hashmap_t *dest)
{
    for (int i = 0; i <= source->capacity; i++) {
        jml_hashmap_entry_t *entry = &source->entries[i];

        if (entry->key != NULL) {
            jml_hashmap_set(dest, entry->key, entry->value);
        }
    }
}


jml_obj_string_t *
jml_hashmap_find(jml_hashmap_t *map, const char *chars,
    size_t length, uint32_t hash)
{
    if (map->count == 0) return NULL;
    uint32_t index = hash & map->capacity;

    while (true) {
        jml_hashmap_entry_t *entry= &map->entries[index];

        if (entry->key == NULL) {
            if (IS_NONE(entry->value)) return NULL;

        } else if ((entry->key->length == length)
            && (entry->key->hash == hash)
            && (memcmp(entry->key->chars, chars, length) == 0))

                return entry->key;

        index = (index + 1) & map->capacity;
    }
}


void
jml_hashmap_remove_white(jml_hashmap_t *map)
{
    for (int i = 0; i <= map->capacity; i++) {
        jml_hashmap_entry_t *entry = &map->entries[i];
        if (entry->key != NULL && !(entry->key->obj.marked)) {
            jml_hashmap_del(map, entry->key);
        }
    }
}


void
jml_hashmap_mark(jml_hashmap_t *map)
{
    for (int i = 0; i <= map->capacity; i++) {
        jml_hashmap_entry_t *entry = &map->entries[i];
        jml_gc_mark_obj((jml_obj_t*)entry->key);
        jml_gc_mark_value(entry->value);
    }
}


jml_hashmap_entry_t *
jml_hashmap_iterator(jml_hashmap_t *map)
{
    jml_hashmap_entry_t *entries = jml_alloc(
        map->count * sizeof(jml_hashmap_entry_t));

    int count = 0;
    for (int i = 0; i <= map->capacity; i++) {
        jml_hashmap_entry_t entry = map->entries[i];

        if (entry.key == NULL)      continue;
        if (count == map->count)    break;

        entries[count++] = entry;
    }

    return entries;
}
