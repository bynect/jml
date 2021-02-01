#ifndef JML_STRING_H_
#define JML_STRING_H_

#include <jml.h>


#define JML_MAXUNICODE              0x10ffffu
#define JML_MAXUTF                  0x7fffffffu


uint8_t jml_string_encode(char *buffer, uint32_t value);

const char *jml_string_decode(const char *str, uint32_t *val);

uint8_t jml_string_charbytes(const char *str, uint32_t i);


#endif /* JML_STRING_H_ */
