#include <stdlib.h>

#include <jml.h>


static uint64_t
hash_fnv0(const uint8_t *octects, uint32_t len)
{
    uint64_t hash = 0;

    for (uint32_t i= 0; i < len; i++) {
        hash *= 1099511628211;
        hash ^= octects[i];
    }

    return hash;
}


static uint64_t
hash_fnv1(const uint8_t *octects, uint32_t len)
{
    uint64_t hash = 14695981039346656037u;

    for (uint32_t i= 0; i < len; i++) {
        hash *= 1099511628211;
        hash ^= octects[i];
    }

    return hash;
}


static uint64_t
hash_fnv1a(const uint8_t *octects, uint32_t len)
{
    uint64_t hash = 14695981039346656037u;

    for (uint32_t i= 0; i < len; i++) {
        hash ^= octects[i];
        hash *= 1099511628211;
    }

    return hash;
}


static uint64_t
hash_djb2(const uint8_t *octects, uint32_t len)
{
    uint64_t hash = 5381;

    for (uint32_t i= 0; i < len; i++) {
        hash = ((hash << 5) + hash) + octects[i];
    }

    return hash;
}


static uint64_t
hash_sdbm(const uint8_t *octects, uint32_t len)
{
    uint64_t hash = 0;

    for (uint32_t i= 0; i < len; i++) {
        hash = octects[i] + (hash << 6) + (hash << 16) - hash;
    }

    return hash;

}


static uint64_t
hash_lose_lose(const uint8_t *octects, uint32_t len)
{
    uint64_t hash = 0;

    for (uint32_t i= 0; i < len; i++) {
        hash += octects[i];
    }

    return hash;
}


static uint32_t
hash_murmur3(const uint8_t *octects, uint32_t len, uint32_t seed)
{
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;
    uint32_t r1 = 15;
    uint32_t r2 = 13;

    uint32_t m = 5;
    uint32_t n = 0xe6546b64;
    uint32_t k = 0;

    uint8_t *d = (uint8_t*)octects;
    uint32_t i= 0, l = len / 4;
    uint32_t h = seed;

    const uint32_t *chunks = (const uint32_t *)(d + l * 4);
    const uint8_t *tail = (d + l * 4);

    for (i = -l; i != 0; ++i) {
        k = chunks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;
        h ^= k;
        h = (h << r2) | (h >> (32 - r2));
        h = h * m + n;
    }

    k = 0;

    switch (len & 3) {
        case 3: k ^= (tail[2] << 16); /*fallthrough*/
        case 2: k ^= (tail[1] << 8); /*fallthrough*/

        case 1:
        k ^= tail[0];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;
        h ^= k;
    }

    h ^= len;

    h ^= (h >> 16);
    h *= 0x85ebca6b;
    h ^= (h >> 13);
    h *= 0xc2b2ae35;
    h ^= (h >> 16);

    return h;
}


static uint32_t
hash_murmur3_noseed(const uint8_t *octects, uint32_t len)
{
    static int seed = -1;
    if (seed < 0) seed = rand();
    return hash_murmur3(octects, len, seed);
}


static uint32_t
hash_crc32b(const uint8_t *octects, uint32_t len)
{
    uint32_t mask, hash = 0xffffffff;

    for (uint32_t i= 0; i < len; i++) {
        hash ^= octects[i];
        for (int j = 7; j >= 0; j--) {
            mask = -(hash & 1);
            hash = (hash >> 1) ^ (0xedb88320 & mask);
        }
    }

    return ~hash;
}


#define HASH_FUNC1(name, func)                          \
    static jml_value_t                                  \
    name(int arg_count, jml_value_t *args)              \
    {                                                   \
        jml_obj_exception_t *exc = jml_error_args(      \
            arg_count, 1);                              \
                                                        \
        if (exc != NULL)                                \
            goto err;                                   \
                                                        \
        if (!IS_STRING(args[0])) {                      \
            exc = jml_error_types(false, 1, "string");  \
            goto err;                                   \
        }                                               \
        jml_obj_string_t *string = AS_STRING(args[0]);  \
        uint8_t *bytes = (uint8_t*)string->chars;       \
        return NUM_VAL(func(bytes, string->length));    \
                                                        \
    err:                                                \
        return OBJ_VAL(exc);                            \
    }


HASH_FUNC1(jml_std_hash_fnv0, hash_fnv0)
HASH_FUNC1(jml_std_hash_fnv1, hash_fnv1)
HASH_FUNC1(jml_std_hash_fnv1a, hash_fnv1a)
HASH_FUNC1(jml_std_hash_djb2, hash_djb2)
HASH_FUNC1(jml_std_hash_sdbm, hash_sdbm)
HASH_FUNC1(jml_std_hash_lose_lose, hash_lose_lose)
HASH_FUNC1(jml_std_hash_murmur3, hash_murmur3_noseed)
HASH_FUNC1(jml_std_hash_crc32b, hash_crc32b)


#undef HASH_FUNC1


/*module table*/
MODULE_TABLE_HEAD module_table[] = {
    {"fnv0",                        &jml_std_hash_fnv0},
    {"fnv1",                        &jml_std_hash_fnv1},
    {"fnv1a",                       &jml_std_hash_fnv1a},
    {"djb2",                        &jml_std_hash_djb2},
    {"sdbm",                        &jml_std_hash_sdbm},
    {"lose_lose",                   &jml_std_hash_lose_lose},
    {"murmur3",                     &jml_std_hash_murmur3},
    {"crc32b",                      &jml_std_hash_crc32b},
    {NULL,                          NULL}
};
