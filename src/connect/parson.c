/*
Parson ( http://kgabis.github.com/parson/ )
Copyright (c) 2012 - 2016 Krzysztof Gabis
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */

#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define STARTING_CAPACITY         15
#define ARRAY_MAX_CAPACITY    122880 /* 15*(2^13) */
#define OBJECT_MAX_CAPACITY      960 /* 15*(2^6)  */
#define MAX_NESTING               19
#define DOUBLE_SERIALIZATION_FORMAT "%f"

#define SIZEOF_TOKEN(a)       (sizeof(a) - 1)
#define SKIP_CHAR(str)        ((*str)++)
#define SKIP_WHITESPACES(str) while (isspace(**str)) { SKIP_CHAR(str); }
#define MAX(a, b)             ((a) > (b) ? (a) : (b))

#undef malloc
#undef free

static x_JSON_Malloc_Function parson_malloc = malloc;
static x_JSON_Free_Function parson_free = free;

#define IS_CONT(b) (((unsigned char)(b) & 0xC0) == 0x80) /* is utf-8 continuation byte */

/* Type definitions */
typedef union x_json_value_value {
    char        *string;
    double       number;
    x_JSON_Object *object;
    x_JSON_Array  *array;
    int          boolean;
    int          null;
} x_JSON_Value_Value;

struct x_json_value_t {
    x_JSON_Value_Type     type;
    x_JSON_Value_Value    value;
};

struct x_json_object_t {
    char       **names;
    x_JSON_Value **values;
    size_t       count;
    size_t       capacity;
};

struct x_json_array_t {
    x_JSON_Value **items;
    size_t       count;
    size_t       capacity;
};

/* Various */
static char * read_file(const char *filename);
static void   remove_comments(char *string, const char *start_token, const char *end_token);
static char * parson_strndup(const char *string, size_t n);
static char * parson_strdup(const char *string);
static int    is_utf16_hex(const unsigned char *string);
static int    num_bytes_in_utf8_sequence(unsigned char c);
static int    verify_utf8_sequence(const unsigned char *string, int *len);
static int    is_valid_utf8(const char *string, size_t string_len);
static int    is_decimal(const char *string, size_t length);

/* JSON Object */
static x_JSON_Object * x_json_object_init(void);
static x_JSON_Status   x_json_object_add(x_JSON_Object *object, const char *name, x_JSON_Value *value);
static x_JSON_Status   x_json_object_resize(x_JSON_Object *object, size_t new_capacity);
static x_JSON_Value  * x_json_object_nget_value(const x_JSON_Object *object, const char *name, size_t n);
static void            x_json_object_free(x_JSON_Object *object);

/* JSON Array */
static x_JSON_Array * x_json_array_init(void);
static x_JSON_Status  x_json_array_add(x_JSON_Array *array, x_JSON_Value *value);
static x_JSON_Status  x_json_array_resize(x_JSON_Array *array, size_t new_capacity);
static void           x_json_array_free(x_JSON_Array *array);

/* JSON Value */
static x_JSON_Value * x_json_value_init_string_no_copy(char *string);

/* Parser */
static void         skip_quotes(const char **string);
static int          parse_utf_16(const char **unprocessed, char **processed);
static char *       process_string(const char *input, size_t len);
static char *       get_quoted_string(const char **string);
static x_JSON_Value * parse_object_value(const char **string, size_t nesting);
static x_JSON_Value * parse_array_value(const char **string, size_t nesting);
static x_JSON_Value * parse_string_value(const char **string);
static x_JSON_Value * parse_boolean_value(const char **string);
static x_JSON_Value * parse_number_value(const char **string);
static x_JSON_Value * parse_null_value(const char **string);
static x_JSON_Value * parse_value(const char **string, size_t nesting);

/* Serialization */
static int    x_json_serialize_to_buffer_r(const x_JSON_Value *value, char *buf, int level, int is_pretty, char *num_buf);
static int    x_json_serialize_string(const char *string, char *buf);
static int    append_indent(char *buf, int level);
static int    append_string(char *buf, const char *string);

/* Various */
static char * parson_strndup(const char *string, size_t n) {
    char *output_string = (char*)parson_malloc(n + 1);
    if (!output_string)
        return NULL;
    output_string[n] = '\0';
    strncpy(output_string, string, n);
    return output_string;
}

static char * parson_strdup(const char *string) {
    return parson_strndup(string, strlen(string));
}

static int is_utf16_hex(const unsigned char *s) {
    return isxdigit(s[0]) && isxdigit(s[1]) && isxdigit(s[2]) && isxdigit(s[3]);
}

static int num_bytes_in_utf8_sequence(unsigned char c) {
    if (c == 0xC0 || c == 0xC1 || c > 0xF4 || IS_CONT(c)) {
        return 0;
    } else if ((c & 0x80) == 0) {    /* 0xxxxxxx */
        return 1;
    } else if ((c & 0xE0) == 0xC0) { /* 110xxxxx */
        return 2;
    } else if ((c & 0xF0) == 0xE0) { /* 1110xxxx */
        return 3;
    } else if ((c & 0xF8) == 0xF0) { /* 11110xxx */
        return 4;
    }
    return 0; /* won't happen */
}

static int verify_utf8_sequence(const unsigned char *string, int *len) {
    unsigned int cp = 0;
    *len = num_bytes_in_utf8_sequence(string[0]);

    if (*len == 1) {
        cp = string[0];
    } else if (*len == 2 && IS_CONT(string[1])) {
        cp = string[0] & 0x1F;
        cp = (cp << 6) | (string[1] & 0x3F);
    } else if (*len == 3 && IS_CONT(string[1]) && IS_CONT(string[2])) {
        cp = ((unsigned char)string[0]) & 0xF;
        cp = (cp << 6) | (string[1] & 0x3F);
        cp = (cp << 6) | (string[2] & 0x3F);
    } else if (*len == 4 && IS_CONT(string[1]) && IS_CONT(string[2]) && IS_CONT(string[3])) {
        cp = string[0] & 0x7;
        cp = (cp << 6) | (string[1] & 0x3F);
        cp = (cp << 6) | (string[2] & 0x3F);
        cp = (cp << 6) | (string[3] & 0x3F);
    } else {
        return 0;
    }

    /* overlong encodings */
    if ((cp < 0x80    && *len > 1) ||
        (cp < 0x800   && *len > 2) ||
        (cp < 0x10000 && *len > 3)) {
        return 0;
    }

    /* invalid unicode */
    if (cp > 0x10FFFF) {
        return 0;
    }

    /* surrogate halves */
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        return 0;
    }

    return 1;
}

static int is_valid_utf8(const char *string, size_t string_len) {
    int len = 0;
    const char *string_end =  string + string_len;
    while (string < string_end) {
        if (!verify_utf8_sequence((const unsigned char*)string, &len)) {
            return 0;
        }
        string += len;
    }
    return 1;
}

static int is_decimal(const char *string, size_t length) {
    if (length > 1 && string[0] == '0' && string[1] != '.')
        return 0;
    if (length > 2 && !strncmp(string, "-0", 2) && string[2] != '.')
        return 0;
    while (length--)
        if (strchr("xX", string[length]))
            return 0;
    return 1;
}

static char * read_file(const char * filename) {
    FILE *fp = fopen(filename, "r");
    size_t file_size;
    long pos;
    char *file_contents;
    if (!fp)
        return NULL;
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return NULL;
    }
    file_size = pos;
    rewind(fp);
    file_contents = (char*)parson_malloc(sizeof(char) * (file_size + 1));
    if (!file_contents) {
        fclose(fp);
        return NULL;
    }
    if (fread(file_contents, file_size, 1, fp) < 1) {
        if (ferror(fp)) {
            fclose(fp);
            parson_free(file_contents);
            return NULL;
        }
    }
    fclose(fp);
    file_contents[file_size] = '\0';
    return file_contents;
}

static void remove_comments(char *string, const char *start_token, const char *end_token) {
    int in_string = 0, escaped = 0;
    size_t i;
    char *ptr = NULL, current_char;
    size_t start_token_len = strlen(start_token);
    size_t end_token_len = strlen(end_token);
    if (start_token_len == 0 || end_token_len == 0)
    	return;
    while ((current_char = *string) != '\0') {
        if (current_char == '\\' && !escaped) {
            escaped = 1;
            string++;
            continue;
        } else if (current_char == '\"' && !escaped) {
            in_string = !in_string;
        } else if (!in_string && strncmp(string, start_token, start_token_len) == 0) {
			for(i = 0; i < start_token_len; i++)
                string[i] = ' ';
        	string = string + start_token_len;
            ptr = strstr(string, end_token);
            if (!ptr)
                return;
            for (i = 0; i < (ptr - string) + end_token_len; i++)
                string[i] = ' ';
          	string = ptr + end_token_len - 1;
        }
        escaped = 0;
        string++;
    }
}

/* JSON Object */
static x_JSON_Object * x_json_object_init(void) {
    x_JSON_Object *new_obj = (x_JSON_Object*)parson_malloc(sizeof(x_JSON_Object));
    if (!new_obj)
        return NULL;
    new_obj->names = (char**)NULL;
    new_obj->values = (x_JSON_Value**)NULL;
    new_obj->capacity = 0;
    new_obj->count = 0;
    return new_obj;
}

static x_JSON_Status x_json_object_add(x_JSON_Object *object, const char *name, x_JSON_Value *value) {
    size_t index = 0;
    if (object == NULL || name == NULL || value == NULL) {
        return JSONFailure;
    }
    if (x_json_object_get_value(object, name) != NULL) {
        return JSONFailure;
    }
    if (object->count >= object->capacity) {
        size_t new_capacity = MAX(object->capacity * 2, STARTING_CAPACITY);
        if (new_capacity > OBJECT_MAX_CAPACITY)
            return JSONFailure;
        if (x_json_object_resize(object, new_capacity) == JSONFailure)
            return JSONFailure;
    }
    index = object->count;
    object->names[index] = parson_strdup(name);
    if (object->names[index] == NULL)
        return JSONFailure;
    object->values[index] = value;
    object->count++;
    return JSONSuccess;
}

static x_JSON_Status x_json_object_resize(x_JSON_Object *object, size_t new_capacity) {
    char **temp_names = NULL;
    x_JSON_Value **temp_values = NULL;

    if ((object->names == NULL && object->values != NULL) ||
        (object->names != NULL && object->values == NULL) ||
        new_capacity == 0) {
            return JSONFailure; /* Shouldn't happen */
    }

    temp_names = (char**)parson_malloc(new_capacity * sizeof(char*));
    if (temp_names == NULL)
        return JSONFailure;

    temp_values = (x_JSON_Value**)parson_malloc(new_capacity * sizeof(x_JSON_Value*));
    if (temp_values == NULL) {
        parson_free(temp_names);
        return JSONFailure;
    }

    if (object->names != NULL && object->values != NULL && object->count > 0) {
        memcpy(temp_names, object->names, object->count * sizeof(char*));
        memcpy(temp_values, object->values, object->count * sizeof(x_JSON_Value*));
    }
    parson_free(object->names);
    parson_free(object->values);
    object->names = temp_names;
    object->values = temp_values;
    object->capacity = new_capacity;
    return JSONSuccess;
}

static x_JSON_Value * x_json_object_nget_value(const x_JSON_Object *object, const char *name, size_t n) {
    size_t i, name_length;
    for (i = 0; i < x_json_object_get_count(object); i++) {
        name_length = strlen(object->names[i]);
        if (name_length != n)
            continue;
        if (strncmp(object->names[i], name, n) == 0)
            return object->values[i];
    }
    return NULL;
}

static void x_json_object_free(x_JSON_Object *object) {
    while(object->count--) {
        parson_free(object->names[object->count]);
        x_json_value_free(object->values[object->count]);
    }
    parson_free(object->names);
    parson_free(object->values);
    parson_free(object);
}

/* JSON Array */
static x_JSON_Array * x_json_array_init(void) {
    x_JSON_Array *new_array = (x_JSON_Array*)parson_malloc(sizeof(x_JSON_Array));
    if (!new_array)
        return NULL;
    new_array->items = (x_JSON_Value**)NULL;
    new_array->capacity = 0;
    new_array->count = 0;
    return new_array;
}

static x_JSON_Status x_json_array_add(x_JSON_Array *array, x_JSON_Value *value) {
    if (array->count >= array->capacity) {
        size_t new_capacity = MAX(array->capacity * 2, STARTING_CAPACITY);
        if (new_capacity > ARRAY_MAX_CAPACITY)
            return JSONFailure;
        if (x_json_array_resize(array, new_capacity) == JSONFailure)
            return JSONFailure;
    }
    array->items[array->count] = value;
    array->count++;
    return JSONSuccess;
}

static x_JSON_Status x_json_array_resize(x_JSON_Array *array, size_t new_capacity) {
    x_JSON_Value **new_items = NULL;
    if (new_capacity == 0) {
        return JSONFailure;
    }
    new_items = (x_JSON_Value**)parson_malloc(new_capacity * sizeof(x_JSON_Value*));
    if (new_items == NULL) {
        return JSONFailure;
    }
    if (array->items != NULL && array->count > 0) {
        memcpy(new_items, array->items, array->count * sizeof(x_JSON_Value*));
    }
    parson_free(array->items);
    array->items = new_items;
    array->capacity = new_capacity;
    return JSONSuccess;
}

static void x_json_array_free(x_JSON_Array *array) {
    while (array->count--)
        x_json_value_free(array->items[array->count]);
    parson_free(array->items);
    parson_free(array);
}

/* JSON Value */
static x_JSON_Value * x_json_value_init_string_no_copy(char *string) {
    x_JSON_Value *new_value = (x_JSON_Value*)parson_malloc(sizeof(x_JSON_Value));
    if (!new_value)
        return NULL;
    new_value->type = JSONString;
    new_value->value.string = string;
    return new_value;
}

/* Parser */
static void skip_quotes(const char **string) {
    SKIP_CHAR(string);
    while (**string != '\"') {
        if (**string == '\0')
            return;
        if (**string == '\\') {
            SKIP_CHAR(string);
            if (**string == '\0')
                return;
        }
        SKIP_CHAR(string);
    }
    SKIP_CHAR(string);
}

static int parse_utf_16(const char **unprocessed, char **processed) {
    unsigned int cp, lead, trail;
    char *processed_ptr = *processed;
    const char *unprocessed_ptr = *unprocessed;
    unprocessed_ptr++; /* skips u */
    if (!is_utf16_hex((const unsigned char*)unprocessed_ptr) || sscanf(unprocessed_ptr, "%4x", &cp) == EOF)
            return JSONFailure;
    if (cp < 0x80) {
        *processed_ptr = cp; /* 0xxxxxxx */
    } else if (cp < 0x800) {
        *processed_ptr++ = ((cp >> 6) & 0x1F) | 0xC0; /* 110xxxxx */
        *processed_ptr   = ((cp     ) & 0x3F) | 0x80; /* 10xxxxxx */
    } else if (cp < 0xD800 || cp > 0xDFFF) {
        *processed_ptr++ = ((cp >> 12) & 0x0F) | 0xE0; /* 1110xxxx */
        *processed_ptr++ = ((cp >> 6)  & 0x3F) | 0x80; /* 10xxxxxx */
        *processed_ptr   = ((cp     )  & 0x3F) | 0x80; /* 10xxxxxx */
    } else if (cp >= 0xD800 && cp <= 0xDBFF) { /* lead surrogate (0xD800..0xDBFF) */
        lead = cp;
        unprocessed_ptr += 4; /* should always be within the buffer, otherwise previous sscanf would fail */
        if (*unprocessed_ptr++ != '\\' || *unprocessed_ptr++ != 'u' || /* starts with \u? */
            !is_utf16_hex((const unsigned char*)unprocessed_ptr)          ||
            sscanf(unprocessed_ptr, "%4x", &trail) == EOF           ||
            trail < 0xDC00 || trail > 0xDFFF) { /* valid trail surrogate? (0xDC00..0xDFFF) */
                return JSONFailure;
        }
        cp = ((((lead-0xD800)&0x3FF)<<10)|((trail-0xDC00)&0x3FF))+0x010000;
        *processed_ptr++ = (((cp >> 18) & 0x07) | 0xF0); /* 11110xxx */
        *processed_ptr++ = (((cp >> 12) & 0x3F) | 0x80); /* 10xxxxxx */
        *processed_ptr++ = (((cp >> 6)  & 0x3F) | 0x80); /* 10xxxxxx */
        *processed_ptr   = (((cp     )  & 0x3F) | 0x80); /* 10xxxxxx */
    } else { /* trail surrogate before lead surrogate */
        return JSONFailure;
    }
    unprocessed_ptr += 3;
    *processed = processed_ptr;
    *unprocessed = unprocessed_ptr;
    return JSONSuccess;
}


/* Copies and processes passed string up to supplied length.
Example: "\u006Corem ipsum" -> lorem ipsum */
static char* process_string(const char *input, size_t len) {
    const char *input_ptr = input;
    size_t initial_size = (len + 1) * sizeof(char);
    size_t final_size = 0;
    char *output = (char*)parson_malloc(initial_size);
    char *output_ptr = output;
    char *resized_output = NULL;
    while ((*input_ptr != '\0') && (size_t)(input_ptr - input) < len) {
        if (*input_ptr == '\\') {
            input_ptr++;
            switch (*input_ptr) {
                case '\"': *output_ptr = '\"'; break;
                case '\\': *output_ptr = '\\'; break;
                case '/':  *output_ptr = '/';  break;
                case 'b':  *output_ptr = '\b'; break;
                case 'f':  *output_ptr = '\f'; break;
                case 'n':  *output_ptr = '\n'; break;
                case 'r':  *output_ptr = '\r'; break;
                case 't':  *output_ptr = '\t'; break;
                case 'u':
                    if (parse_utf_16(&input_ptr, &output_ptr) == JSONFailure)
                        goto error;
                    break;
                default:
                    goto error;
            }
        } else if ((unsigned char)*input_ptr < 0x20) {
            goto error; /* 0x00-0x19 are invalid characters for json string (http://www.ietf.org/rfc/rfc4627.txt) */
        } else {
            *output_ptr = *input_ptr;
        }
        output_ptr++;
        input_ptr++;
    }
    *output_ptr = '\0';
    /* resize to new length */
    final_size = (size_t)(output_ptr-output) + 1;
    resized_output = (char*)parson_malloc(final_size);
    if (resized_output == NULL)
        goto error;
    memcpy(resized_output, output, final_size);
    parson_free(output);
    return resized_output;
error:
    parson_free(output);
    return NULL;
}

/* Return processed contents of a string between quotes and
   skips passed argument to a matching quote. */
static char * get_quoted_string(const char **string) {
    const char *string_start = *string;
    size_t string_len = 0;
    skip_quotes(string);
    string_len = *string - string_start - 2; /* length without quotes */
    return process_string(string_start + 1, string_len);
}

static x_JSON_Value * parse_value(const char **string, size_t nesting) {
    if (nesting > MAX_NESTING)
        return NULL;
    SKIP_WHITESPACES(string);
    switch (**string) {
        case '{':
            return parse_object_value(string, nesting + 1);
        case '[':
            return parse_array_value(string, nesting + 1);
        case '\"':
            return parse_string_value(string);
        case 'f': case 't':
            return parse_boolean_value(string);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number_value(string);
        case 'n':
            return parse_null_value(string);
        default:
            return NULL;
    }
}

static x_JSON_Value * parse_object_value(const char **string, size_t nesting) {
    x_JSON_Value *output_value = x_json_value_init_object(), *new_value = NULL;
    x_JSON_Object *output_object = x_json_value_get_object(output_value);
    char *new_key = NULL;
    if (output_value == NULL)
        return NULL;
    SKIP_CHAR(string);
    SKIP_WHITESPACES(string);
    if (**string == '}') { /* empty object */
        SKIP_CHAR(string);
        return output_value;
    }
    while (**string != '\0') {
        new_key = get_quoted_string(string);
        SKIP_WHITESPACES(string);
        if (new_key == NULL || **string != ':') {
            x_json_value_free(output_value);
            return NULL;
        }
        SKIP_CHAR(string);
        new_value = parse_value(string, nesting);
        if (new_value == NULL) {
            parson_free(new_key);
            x_json_value_free(output_value);
            return NULL;
        }
        if(x_json_object_add(output_object, new_key, new_value) == JSONFailure) {
            parson_free(new_key);
            parson_free(new_value);
            x_json_value_free(output_value);
            return NULL;
        }
        parson_free(new_key);
        SKIP_WHITESPACES(string);
        if (**string != ',')
            break;
        SKIP_CHAR(string);
        SKIP_WHITESPACES(string);
    }
    SKIP_WHITESPACES(string);
    if (**string != '}' || /* Trim object after parsing is over */
        x_json_object_resize(output_object, x_json_object_get_count(output_object)) == JSONFailure) {
            x_json_value_free(output_value);
            return NULL;
    }
    SKIP_CHAR(string);
    return output_value;
}

static x_JSON_Value * parse_array_value(const char **string, size_t nesting) {
    x_JSON_Value *output_value = x_json_value_init_array(), *new_array_value = NULL;
    x_JSON_Array *output_array = x_json_value_get_array(output_value);
    if (!output_value)
        return NULL;
    SKIP_CHAR(string);
    SKIP_WHITESPACES(string);
    if (**string == ']') { /* empty array */
        SKIP_CHAR(string);
        return output_value;
    }
    while (**string != '\0') {
        new_array_value = parse_value(string, nesting);
        if (!new_array_value) {
            x_json_value_free(output_value);
            return NULL;
        }
        if(x_json_array_add(output_array, new_array_value) == JSONFailure) {
            parson_free(new_array_value);
            x_json_value_free(output_value);
            return NULL;
        }
        SKIP_WHITESPACES(string);
        if (**string != ',')
            break;
        SKIP_CHAR(string);
        SKIP_WHITESPACES(string);
    }
    SKIP_WHITESPACES(string);
    if (**string != ']' || /* Trim array after parsing is over */
        x_json_array_resize(output_array, x_json_array_get_count(output_array)) == JSONFailure) {
            x_json_value_free(output_value);
            return NULL;
    }
    SKIP_CHAR(string);
    return output_value;
}

static x_JSON_Value * parse_string_value(const char **string) {
    x_JSON_Value *value = NULL;
    char *new_string = get_quoted_string(string);
    if (new_string == NULL)
        return NULL;
    value = x_json_value_init_string_no_copy(new_string);
    if (value == NULL) {
        parson_free(new_string);
        return NULL;
    }
    return value;
}

static x_JSON_Value * parse_boolean_value(const char **string) {
    size_t true_token_size = SIZEOF_TOKEN("true");
    size_t false_token_size = SIZEOF_TOKEN("false");
    if (strncmp("true", *string, true_token_size) == 0) {
        *string += true_token_size;
        return x_json_value_init_boolean(1);
    } else if (strncmp("false", *string, false_token_size) == 0) {
        *string += false_token_size;
        return x_json_value_init_boolean(0);
    }
    return NULL;
}

static x_JSON_Value * parse_number_value(const char **string) {
    char *end;
    double number = strtod(*string, &end);
    x_JSON_Value *output_value;
    if (is_decimal(*string, end - *string)) {
        *string = end;
        output_value = x_json_value_init_number(number);
    } else {
        output_value = NULL;
    }
    return output_value;
}

static x_JSON_Value * parse_null_value(const char **string) {
    size_t token_size = SIZEOF_TOKEN("null");
    if (strncmp("null", *string, token_size) == 0) {
        *string += token_size;
        return x_json_value_init_null();
    }
    return NULL;
}

/* Serialization */
#define APPEND_STRING(str) do { written = append_string(buf, (str));\
                                if (written < 0) { return -1; }\
                                if (buf != NULL) { buf += written; }\
                                written_total += written; } while(0)

#define APPEND_INDENT(level) do { written = append_indent(buf, (level));\
                                  if (written < 0) { return -1; }\
                                  if (buf != NULL) { buf += written; }\
                                  written_total += written; } while(0)

static int x_json_serialize_to_buffer_r(const x_JSON_Value *value, char *buf, int level, int is_pretty, char *num_buf)
{
    const char *key = NULL, *string = NULL;
    x_JSON_Value *temp_value = NULL;
    x_JSON_Array *array = NULL;
    x_JSON_Object *object = NULL;
    size_t i = 0, count = 0;
    double num = 0.0;
    int written = -1, written_total = 0;

    switch (x_json_value_get_type(value)) {
        case JSONArray:
            array = x_json_value_get_array(value);
            count = x_json_array_get_count(array);
            APPEND_STRING("[");
            if (count > 0 && is_pretty)
                APPEND_STRING("\n");
            for (i = 0; i < count; i++) {
                if (is_pretty)
                    APPEND_INDENT(level+1);
                temp_value = x_json_array_get_value(array, i);
                written = x_json_serialize_to_buffer_r(temp_value, buf, level+1, is_pretty, num_buf);
                if (written < 0)
                    return -1;
                if (buf != NULL)
                    buf += written;
                written_total += written;
                if (i < (count - 1))
                    APPEND_STRING(",");
                if (is_pretty)
                    APPEND_STRING("\n");
            }
            if (count > 0 && is_pretty)
                APPEND_INDENT(level);
            APPEND_STRING("]");
            return written_total;
        case JSONObject:
            object = x_json_value_get_object(value);
            count  = x_json_object_get_count(object);
            APPEND_STRING("{");
            if (count > 0 && is_pretty)
                APPEND_STRING("\n");
            for (i = 0; i < count; i++) {
                key = x_json_object_get_name(object, i);
                if (is_pretty)
                    APPEND_INDENT(level+1);
                written = x_json_serialize_string(key, buf);
                if (written < 0)
                    return -1;
                if (buf != NULL)
                    buf += written;
                written_total += written;
                APPEND_STRING(":");
                if (is_pretty)
                    APPEND_STRING(" ");
                temp_value = x_json_object_get_value(object, key);
                written = x_json_serialize_to_buffer_r(temp_value, buf, level+1, is_pretty, num_buf);
                if (written < 0)
                    return -1;
                if (buf != NULL)
                    buf += written;
                written_total += written;
                if (i < (count - 1))
                    APPEND_STRING(",");
                if (is_pretty)
                    APPEND_STRING("\n");
            }
            if (count > 0 && is_pretty)
                APPEND_INDENT(level);
            APPEND_STRING("}");
            return written_total;
        case JSONString:
            string = x_json_value_get_string(value);
            written = x_json_serialize_string(string, buf);
            if (written < 0)
                return -1;
            if (buf != NULL)
                buf += written;
            written_total += written;
            return written_total;
        case JSONBoolean:
            if (x_json_value_get_boolean(value))
                APPEND_STRING("true");
            else
                APPEND_STRING("false");
            return written_total;
        case JSONNumber:
            num = x_json_value_get_number(value);
            if (buf != NULL)
                num_buf = buf;
            if (num == ((double)(int)num)) /*  check if num is integer */
                written = sprintf(num_buf, "%d", (int)num);
            else
                written = sprintf(num_buf, DOUBLE_SERIALIZATION_FORMAT, num);
            if (written < 0)
                return -1;
            if (buf != NULL)
                buf += written;
            written_total += written;
            return written_total;
        case JSONNull:
            APPEND_STRING("null");
            return written_total;
        case JSONError:
            return -1;
        default:
            return -1;
    }
}

static int x_json_serialize_string(const char *string, char *buf) {
    size_t i = 0, len = strlen(string);
    char c = '\0';
    int written = -1, written_total = 0;
    APPEND_STRING("\"");
    for (i = 0; i < len; i++) {
        c = string[i];
        switch (c) {
            case '\"': APPEND_STRING("\\\""); break;
            case '\\': APPEND_STRING("\\\\"); break;
            case '/':  APPEND_STRING("\\/"); break; /* to make json embeddable in xml\/html */
            case '\b': APPEND_STRING("\\b"); break;
            case '\f': APPEND_STRING("\\f"); break;
            case '\n': APPEND_STRING("\\n"); break;
            case '\r': APPEND_STRING("\\r"); break;
            case '\t': APPEND_STRING("\\t"); break;
            default:
                if (buf != NULL) {
                    buf[0] = c;
                    buf += 1;
                }
                written_total += 1;
                break;
        }
    }
    APPEND_STRING("\"");
    return written_total;
}

static int append_indent(char *buf, int level) {
    int i;
    int written = -1, written_total = 0;
    for (i = 0; i < level; i++) {
        APPEND_STRING("    ");
    }
    return written_total;
}

static int append_string(char *buf, const char *string) {
    if (buf == NULL) {
        return (int)strlen(string);
    }
    return sprintf(buf, "%s", string);
}

#undef APPEND_STRING
#undef APPEND_INDENT

/* Parser API */
x_JSON_Value * x_json_parse_file(const char *filename) {
    char *file_contents = read_file(filename);
    x_JSON_Value *output_value = NULL;
    if (file_contents == NULL)
        return NULL;
    output_value = x_json_parse_string(file_contents);
    parson_free(file_contents);
    return output_value;
}

x_JSON_Value * x_json_parse_file_with_comments(const char *filename) {
    char *file_contents = read_file(filename);
    x_JSON_Value *output_value = NULL;
    if (file_contents == NULL)
        return NULL;
    output_value = x_json_parse_string_with_comments(file_contents);
    parson_free(file_contents);
    return output_value;
}

x_JSON_Value * x_json_parse_string(const char *string) {
    if (string == NULL)
        return NULL;
    return parse_value((const char**)&string, 0);
}

x_JSON_Value * x_json_parse_string_with_comments(const char *string) {
    x_JSON_Value *result = NULL;
    char *string_mutable_copy = NULL, *string_mutable_copy_ptr = NULL;
    string_mutable_copy = parson_strdup(string);
    if (string_mutable_copy == NULL)
        return NULL;
    remove_comments(string_mutable_copy, "/*", "*/");
    remove_comments(string_mutable_copy, "//", "\n");
    string_mutable_copy_ptr = string_mutable_copy;
    result = parse_value((const char**)&string_mutable_copy_ptr, 0);
    parson_free(string_mutable_copy);
    return result;
}

/* JSON Object API */

x_JSON_Value * x_json_object_get_value(const x_JSON_Object *object, const char *name) {
    if (object == NULL || name == NULL)
        return NULL;
    return x_json_object_nget_value(object, name, strlen(name));
}

const char * x_json_object_get_string(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_string(x_json_object_get_value(object, name));
}

double x_json_object_get_number(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_number(x_json_object_get_value(object, name));
}

x_JSON_Object * x_json_object_get_object(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_object(x_json_object_get_value(object, name));
}

x_JSON_Array * x_json_object_get_array(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_array(x_json_object_get_value(object, name));
}

int x_json_object_get_boolean(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_boolean(x_json_object_get_value(object, name));
}

x_JSON_Value * x_json_object_dotget_value(const x_JSON_Object *object, const char *name) {
    const char *dot_position = strchr(name, '.');
    if (!dot_position)
        return x_json_object_get_value(object, name);
    object = x_json_value_get_object(x_json_object_nget_value(object, name, dot_position - name));
    return x_json_object_dotget_value(object, dot_position + 1);
}

const char * x_json_object_dotget_string(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_string(x_json_object_dotget_value(object, name));
}

double x_json_object_dotget_number(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_number(x_json_object_dotget_value(object, name));
}

x_JSON_Object * x_json_object_dotget_object(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_object(x_json_object_dotget_value(object, name));
}

x_JSON_Array * x_json_object_dotget_array(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_array(x_json_object_dotget_value(object, name));
}

int x_json_object_dotget_boolean(const x_JSON_Object *object, const char *name) {
    return x_json_value_get_boolean(x_json_object_dotget_value(object, name));
}

size_t x_json_object_get_count(const x_JSON_Object *object) {
    return object ? object->count : 0;
}

const char * x_json_object_get_name(const x_JSON_Object *object, size_t index) {
    if (object == NULL || index >= x_json_object_get_count(object))
        return NULL;
    return object->names[index];
}

x_JSON_Value * x_json_object_get_value_at(const x_JSON_Object *object, size_t index) {
    if (object == NULL || index >= x_json_object_get_count(object))
        return NULL;
    return object->values[index];
}

int x_json_object_has_value (const x_JSON_Object *object, const char *name) {
    return json_object_get_value(object, name) != NULL;
}

int x_json_object_has_value_of_type(const x_JSON_Object *object, const char *name, x_JSON_Value_Type type) {
    x_JSON_Value *val = json_object_get_value(object, name);
    return val != NULL && json_value_get_type(val) == type;
}

int x_json_object_dothas_value (const x_JSON_Object *object, const char *name) {
    return json_object_dotget_value(object, name) != NULL;
}

int x_json_object_dothas_value_of_type(const x_JSON_Object *object, const char *name, x_JSON_Value_Type type) {
    x_JSON_Value *val = json_object_dotget_value(object, name);
    return val != NULL && json_value_get_type(val) == type;
}

/* JSON Array API */
x_JSON_Value * x_json_array_get_value(const x_JSON_Array *array, size_t index) {
    if (array == NULL || index >= x_json_array_get_count(array))
        return NULL;
    return array->items[index];
}

const char * x_json_array_get_string(const x_JSON_Array *array, size_t index) {
    return x_json_value_get_string(x_json_array_get_value(array, index));
}

double x_json_array_get_number(const x_JSON_Array *array, size_t index) {
    return x_json_value_get_number(x_json_array_get_value(array, index));
}

x_JSON_Object * x_json_array_get_object(const x_JSON_Array *array, size_t index) {
    return x_json_value_get_object(x_json_array_get_value(array, index));
}

x_JSON_Array * x_json_array_get_array(const x_JSON_Array *array, size_t index) {
    return x_json_value_get_array(x_json_array_get_value(array, index));
}

int x_json_array_get_boolean(const x_JSON_Array *array, size_t index) {
    return x_json_value_get_boolean(x_json_array_get_value(array, index));
}

size_t x_json_array_get_count(const x_JSON_Array *array) {
    return array ? array->count : 0;
}

/* JSON Value API */
x_JSON_Value_Type x_json_value_get_type(const x_JSON_Value *value) {
    return value ? value->type : JSONError;
}

x_JSON_Object * x_json_value_get_object(const x_JSON_Value *value) {
    return x_json_value_get_type(value) == JSONObject ? value->value.object : NULL;
}

x_JSON_Array * x_json_value_get_array(const x_JSON_Value *value) {
    return x_json_value_get_type(value) == JSONArray ? value->value.array : NULL;
}

const char * x_json_value_get_string(const x_JSON_Value *value) {
    return x_json_value_get_type(value) == JSONString ? value->value.string : NULL;
}

double x_json_value_get_number(const x_JSON_Value *value) {
    return x_json_value_get_type(value) == JSONNumber ? value->value.number : 0;
}

int x_json_value_get_boolean(const x_JSON_Value *value) {
    return x_json_value_get_type(value) == JSONBoolean ? value->value.boolean : -1;
}

void x_json_value_free(x_JSON_Value *value) {
    switch (x_json_value_get_type(value)) {
        case JSONObject:
            x_json_object_free(value->value.object);
            break;
        case JSONString:
            if (value->value.string) { parson_free(value->value.string); }
            break;
        case JSONArray:
            x_json_array_free(value->value.array);
            break;
        default:
            break;
    }
    parson_free(value);
}

x_JSON_Value * x_json_value_init_object(void) {
    x_JSON_Value *new_value = (x_JSON_Value*)parson_malloc(sizeof(x_JSON_Value));
    if (!new_value)
        return NULL;
    new_value->type = JSONObject;
    new_value->value.object = x_json_object_init();
    if (!new_value->value.object) {
        parson_free(new_value);
        return NULL;
    }
    return new_value;
}

x_JSON_Value * x_json_value_init_array(void) {
    x_JSON_Value *new_value = (x_JSON_Value*)parson_malloc(sizeof(x_JSON_Value));
    if (!new_value)
        return NULL;
    new_value->type = JSONArray;
    new_value->value.array = x_json_array_init();
    if (!new_value->value.array) {
        parson_free(new_value);
        return NULL;
    }
    return new_value;
}

x_JSON_Value * x_json_value_init_string(const char *string) {
    char *copy = NULL;
    x_JSON_Value *value;
    size_t string_len = 0;
    if (string == NULL)
        return NULL;
    string_len = strlen(string);
    if (!is_valid_utf8(string, string_len))
        return NULL;
    copy = parson_strndup(string, string_len);
    if (copy == NULL)
        return NULL;
    value = x_json_value_init_string_no_copy(copy);
    if (value == NULL)
        parson_free(copy);
    return value;
}

x_JSON_Value * x_json_value_init_number(double number) {
    x_JSON_Value *new_value = (x_JSON_Value*)parson_malloc(sizeof(x_JSON_Value));
    if (!new_value)
        return NULL;
    new_value->type = JSONNumber;
    new_value->value.number = number;
    return new_value;
}

x_JSON_Value * x_json_value_init_boolean(int boolean) {
    x_JSON_Value *new_value = (x_JSON_Value*)parson_malloc(sizeof(x_JSON_Value));
    if (!new_value)
        return NULL;
    new_value->type = JSONBoolean;
    new_value->value.boolean = boolean ? 1 : 0;
    return new_value;
}

x_JSON_Value * x_json_value_init_null(void) {
    x_JSON_Value *new_value = (x_JSON_Value*)parson_malloc(sizeof(x_JSON_Value));
    if (!new_value)
        return NULL;
    new_value->type = JSONNull;
    return new_value;
}

x_JSON_Value * x_json_value_deep_copy(const x_JSON_Value *value) {
    size_t i = 0;
    x_JSON_Value *return_value = NULL, *temp_value_copy = NULL, *temp_value = NULL;
    const char *temp_string = NULL, *temp_key = NULL;
    char *temp_string_copy = NULL;
    x_JSON_Array *temp_array = NULL, *temp_array_copy = NULL;
    x_JSON_Object *temp_object = NULL, *temp_object_copy = NULL;

    switch (x_json_value_get_type(value)) {
        case JSONArray:
            temp_array = x_json_value_get_array(value);
            return_value = x_json_value_init_array();
            if (return_value == NULL)
                return NULL;
            temp_array_copy = x_json_value_get_array(return_value);
            for (i = 0; i < x_json_array_get_count(temp_array); i++) {
                temp_value = x_json_array_get_value(temp_array, i);
                temp_value_copy = x_json_value_deep_copy(temp_value);
                if (temp_value_copy == NULL) {
                    x_json_value_free(return_value);
                    return NULL;
                }
                if (x_json_array_add(temp_array_copy, temp_value_copy) == JSONFailure) {
                    x_json_value_free(return_value);
                    x_json_value_free(temp_value_copy);
                    return NULL;
                }
            }
            return return_value;
        case JSONObject:
            temp_object = x_json_value_get_object(value);
            return_value = x_json_value_init_object();
            if (return_value == NULL)
                return NULL;
            temp_object_copy = x_json_value_get_object(return_value);
            for (i = 0; i < x_json_object_get_count(temp_object); i++) {
                temp_key = x_json_object_get_name(temp_object, i);
                temp_value = x_json_object_get_value(temp_object, temp_key);
                temp_value_copy = x_json_value_deep_copy(temp_value);
                if (temp_value_copy == NULL) {
                    x_json_value_free(return_value);
                    return NULL;
                }
                if (x_json_object_add(temp_object_copy, temp_key, temp_value_copy) == JSONFailure) {
                    x_json_value_free(return_value);
                    x_json_value_free(temp_value_copy);
                    return NULL;
                }
            }
            return return_value;
        case JSONBoolean:
            return x_json_value_init_boolean(x_json_value_get_boolean(value));
        case JSONNumber:
            return x_json_value_init_number(x_json_value_get_number(value));
        case JSONString:
            temp_string = x_json_value_get_string(value);
            temp_string_copy = parson_strdup(temp_string);
            if (temp_string_copy == NULL)
                return NULL;
            return_value = x_json_value_init_string_no_copy(temp_string_copy);
            if (return_value == NULL)
                parson_free(temp_string_copy);
            return return_value;
        case JSONNull:
            return x_json_value_init_null();
        case JSONError:
            return NULL;
        default:
            return NULL;
    }
}

size_t x_json_serialization_size(const x_JSON_Value *value) {
    char num_buf[1100]; /* recursively allocating buffer on stack is a bad idea, so let's do it only once */
    int res = x_json_serialize_to_buffer_r(value, NULL, 0, 0, num_buf);
    return res < 0 ? 0 : (size_t)(res + 1);
}

x_JSON_Status x_json_serialize_to_buffer(const x_JSON_Value *value, char *buf, size_t buf_size_in_bytes) {
    int written = -1;
    size_t needed_size_in_bytes = x_json_serialization_size(value);
    if (needed_size_in_bytes == 0 || buf_size_in_bytes < needed_size_in_bytes) {
        return JSONFailure;
    }
    written = x_json_serialize_to_buffer_r(value, buf, 0, 0, NULL);
    if (written < 0)
        return JSONFailure;
    return JSONSuccess;
}

x_JSON_Status x_json_serialize_to_file(const x_JSON_Value *value, const char *filename) {
    x_JSON_Status return_code = JSONSuccess;
    FILE *fp = NULL;
    char *serialized_string = x_json_serialize_to_string(value);
    if (serialized_string == NULL) {
        return JSONFailure;
    }
    fp = fopen (filename, "w");
    if (fp == NULL) {
        x_json_free_serialized_string(serialized_string);
        return JSONFailure;
    }
    if (fputs(serialized_string, fp) == EOF) {
        return_code = JSONFailure;
    }
    if (fclose(fp) == EOF) {
        return_code = JSONFailure;
    }
    x_json_free_serialized_string(serialized_string);
    return return_code;
}

char * x_json_serialize_to_string(const x_JSON_Value *value) {
    x_JSON_Status serialization_result = JSONFailure;
    size_t buf_size_bytes = x_json_serialization_size(value);
    char *buf = NULL;
    if (buf_size_bytes == 0) {
        return NULL;
    }
    buf = (char*)parson_malloc(buf_size_bytes);
    if (buf == NULL)
        return NULL;
    serialization_result = x_json_serialize_to_buffer(value, buf, buf_size_bytes);
    if (serialization_result == JSONFailure) {
        x_json_free_serialized_string(buf);
        return NULL;
    }
    return buf;
}

size_t x_json_serialization_size_pretty(const x_JSON_Value *value) {
    char num_buf[1100]; /* recursively allocating buffer on stack is a bad idea, so let's do it only once */
    int res = x_json_serialize_to_buffer_r(value, NULL, 0, 1, num_buf);
    return res < 0 ? 0 : (size_t)(res + 1);
}

x_JSON_Status x_json_serialize_to_buffer_pretty(const x_JSON_Value *value, char *buf, size_t buf_size_in_bytes) {
    int written = -1;
    size_t needed_size_in_bytes = x_json_serialization_size_pretty(value);
    if (needed_size_in_bytes == 0 || buf_size_in_bytes < needed_size_in_bytes)
        return JSONFailure;
    written = x_json_serialize_to_buffer_r(value, buf, 0, 1, NULL);
    if (written < 0)
        return JSONFailure;
    return JSONSuccess;
}

x_JSON_Status x_json_serialize_to_file_pretty(const x_JSON_Value *value, const char *filename) {
    x_JSON_Status return_code = JSONSuccess;
    FILE *fp = NULL;
    char *serialized_string = x_json_serialize_to_string_pretty(value);
    if (serialized_string == NULL) {
        return JSONFailure;
    }
    fp = fopen (filename, "w");
    if (fp == NULL) {
        x_json_free_serialized_string(serialized_string);
        return JSONFailure;
    }
    if (fputs(serialized_string, fp) == EOF) {
        return_code = JSONFailure;
    }
    if (fclose(fp) == EOF) {
        return_code = JSONFailure;
    }
    x_json_free_serialized_string(serialized_string);
    return return_code;
}

char * x_json_serialize_to_string_pretty(const x_JSON_Value *value) {
    x_JSON_Status serialization_result = JSONFailure;
    size_t buf_size_bytes = x_json_serialization_size_pretty(value);
    char *buf = NULL;
    if (buf_size_bytes == 0) {
        return NULL;
    }
    buf = (char*)parson_malloc(buf_size_bytes);
    if (buf == NULL)
        return NULL;
    serialization_result = x_json_serialize_to_buffer_pretty(value, buf, buf_size_bytes);
    if (serialization_result == JSONFailure) {
        x_json_free_serialized_string(buf);
        return NULL;
    }
    return buf;
}

void x_json_free_serialized_string(char *string) {
    parson_free(string);
}

x_JSON_Status x_json_array_remove(x_JSON_Array *array, size_t ix) {
    x_JSON_Value *temp_value = NULL;
    size_t last_element_ix = 0;
    if (array == NULL || ix >= x_json_array_get_count(array)) {
        return JSONFailure;
    }
    last_element_ix = x_json_array_get_count(array) - 1;
    x_json_value_free(x_json_array_get_value(array, ix));
    if (ix != last_element_ix) { /* Replace value with one from the end of array */
        temp_value = x_json_array_get_value(array, last_element_ix);
        if (temp_value == NULL) {
            return JSONFailure;
        }
        array->items[ix] = temp_value;
    }
    array->count -= 1;
    return JSONSuccess;
}

x_JSON_Status x_json_array_replace_value(x_JSON_Array *array, size_t ix, x_JSON_Value *value) {
    if (array == NULL || value == NULL || ix >= x_json_array_get_count(array)) {
        return JSONFailure;
    }
    x_json_value_free(x_json_array_get_value(array, ix));
    array->items[ix] = value;
    return JSONSuccess;
}

x_JSON_Status x_json_array_replace_string(x_JSON_Array *array, size_t i, const char* string) {
    x_JSON_Value *value = x_json_value_init_string(string);
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_replace_value(array, i, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_replace_number(x_JSON_Array *array, size_t i, double number) {
    x_JSON_Value *value = x_json_value_init_number(number);
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_replace_value(array, i, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_replace_boolean(x_JSON_Array *array, size_t i, int boolean) {
    x_JSON_Value *value = x_json_value_init_boolean(boolean);
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_replace_value(array, i, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_replace_null(x_JSON_Array *array, size_t i) {
    x_JSON_Value *value = x_json_value_init_null();
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_replace_value(array, i, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_clear(x_JSON_Array *array) {
    size_t i = 0;
    if (array == NULL)
        return JSONFailure;
    for (i = 0; i < x_json_array_get_count(array); i++) {
        x_json_value_free(x_json_array_get_value(array, i));
    }
    array->count = 0;
    return JSONSuccess;
}

x_JSON_Status x_json_array_append_value(x_JSON_Array *array, x_JSON_Value *value) {
    if (array == NULL || value == NULL)
        return JSONFailure;
    return x_json_array_add(array, value);
}

x_JSON_Status x_json_array_append_string(x_JSON_Array *array, const char *string) {
    x_JSON_Value *value = x_json_value_init_string(string);
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_append_value(array, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_append_number(x_JSON_Array *array, double number) {
    x_JSON_Value *value = x_json_value_init_number(number);
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_append_value(array, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_append_boolean(x_JSON_Array *array, int boolean) {
    x_JSON_Value *value = x_json_value_init_boolean(boolean);
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_append_value(array, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_array_append_null(x_JSON_Array *array) {
    x_JSON_Value *value = x_json_value_init_null();
    if (value == NULL)
        return JSONFailure;
    if (x_json_array_append_value(array, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_object_set_value(x_JSON_Object *object, const char *name, x_JSON_Value *value) {
    size_t i = 0;
    x_JSON_Value *old_value;
    if (object == NULL || name == NULL || value == NULL)
        return JSONFailure;
    old_value = x_json_object_get_value(object, name);
    if (old_value != NULL) { /* free and overwrite old value */
        x_json_value_free(old_value);
        for (i = 0; i < x_json_object_get_count(object); i++) {
            if (strcmp(object->names[i], name) == 0) {
                object->values[i] = value;
                return JSONSuccess;
            }
        }
    }
    /* add new key value pair */
    return x_json_object_add(object, name, value);
}

x_JSON_Status x_json_object_set_string(x_JSON_Object *object, const char *name, const char *string) {
    return x_json_object_set_value(object, name, x_json_value_init_string(string));
}

x_JSON_Status x_json_object_set_number(x_JSON_Object *object, const char *name, double number) {
    return x_json_object_set_value(object, name, x_json_value_init_number(number));
}

x_JSON_Status x_json_object_set_boolean(x_JSON_Object *object, const char *name, int boolean) {
    return x_json_object_set_value(object, name, x_json_value_init_boolean(boolean));
}

x_JSON_Status x_json_object_set_null(x_JSON_Object *object, const char *name) {
    return x_json_object_set_value(object, name, x_json_value_init_null());
}

x_JSON_Status x_json_object_dotset_value(x_JSON_Object *object, const char *name, x_JSON_Value *value) {
    const char *dot_pos = NULL;
    char *current_name = NULL;
    x_JSON_Object *temp_obj = NULL;
    x_JSON_Value *new_value = NULL;
    if (value == NULL || name == NULL || value == NULL)
        return JSONFailure;
    dot_pos = strchr(name, '.');
    if (dot_pos == NULL) {
        return x_json_object_set_value(object, name, value);
    } else {
        current_name = parson_strndup(name, dot_pos - name);
        temp_obj = x_json_object_get_object(object, current_name);
        if (temp_obj == NULL) {
            new_value = x_json_value_init_object();
            if (new_value == NULL) {
                parson_free(current_name);
                return JSONFailure;
            }
            if (x_json_object_add(object, current_name, new_value) == JSONFailure) {
                x_json_value_free(new_value);
                parson_free(current_name);
                return JSONFailure;
            }
            temp_obj = x_json_object_get_object(object, current_name);
        }
        parson_free(current_name);
        return x_json_object_dotset_value(temp_obj, dot_pos + 1, value);
    }
}

x_JSON_Status x_json_object_dotset_string(x_JSON_Object *object, const char *name, const char *string) {
    x_JSON_Value *value = x_json_value_init_string(string);
    if (value == NULL)
        return JSONFailure;
    if (x_json_object_dotset_value(object, name, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_object_dotset_number(x_JSON_Object *object, const char *name, double number) {
    x_JSON_Value *value = x_json_value_init_number(number);
    if (value == NULL)
        return JSONFailure;
    if (x_json_object_dotset_value(object, name, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_object_dotset_boolean(x_JSON_Object *object, const char *name, int boolean) {
    x_JSON_Value *value = x_json_value_init_boolean(boolean);
    if (value == NULL)
        return JSONFailure;
    if (x_json_object_dotset_value(object, name, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_object_dotset_null(x_JSON_Object *object, const char *name) {
    x_JSON_Value *value = x_json_value_init_null();
    if (value == NULL)
        return JSONFailure;
    if (x_json_object_dotset_value(object, name, value) == JSONFailure) {
        x_json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

x_JSON_Status x_json_object_remove(x_JSON_Object *object, const char *name) {
    size_t i = 0, last_item_index = 0;
    if (object == NULL || x_json_object_get_value(object, name) == NULL)
        return JSONFailure;
    last_item_index = x_json_object_get_count(object) - 1;
    for (i = 0; i < x_json_object_get_count(object); i++) {
        if (strcmp(object->names[i], name) == 0) {
            parson_free(object->names[i]);
            x_json_value_free(object->values[i]);
            if (i != last_item_index) { /* Replace key value pair with one from the end */
                object->names[i] = object->names[last_item_index];
                object->values[i] = object->values[last_item_index];
            }
            object->count -= 1;
            return JSONSuccess;
        }
    }
    return JSONFailure; /* No execution path should end here */
}

x_JSON_Status x_json_object_dotremove(x_JSON_Object *object, const char *name) {
    const char *dot_pos = strchr(name, '.');
    char *current_name = NULL;
    x_JSON_Object *temp_obj = NULL;
    if (dot_pos == NULL) {
        return x_json_object_remove(object, name);
    } else {
        current_name = parson_strndup(name, dot_pos - name);
        temp_obj = x_json_object_get_object(object, current_name);
        if (temp_obj == NULL) {
            parson_free(current_name);
            return JSONFailure;
        }
        parson_free(current_name);
        return x_json_object_dotremove(temp_obj, dot_pos + 1);
    }
}

x_JSON_Status x_json_object_clear(x_JSON_Object *object) {
    size_t i = 0;
    if (object == NULL) {
        return JSONFailure;
    }
    for (i = 0; i < x_json_object_get_count(object); i++) {
        parson_free(object->names[i]);
        x_json_value_free(object->values[i]);
    }
    object->count = 0;
    return JSONSuccess;
}

x_JSON_Status x_json_validate(const x_JSON_Value *schema, const x_JSON_Value *value) {
    x_JSON_Value *temp_schema_value = NULL, *temp_value = NULL;
    x_JSON_Array *schema_array = NULL, *value_array = NULL;
    x_JSON_Object *schema_object = NULL, *value_object = NULL;
    x_JSON_Value_Type schema_type = JSONError, value_type = JSONError;
    const char *key = NULL;
    size_t i = 0, count = 0;
    if (schema == NULL || value == NULL)
        return JSONFailure;
    schema_type = x_json_value_get_type(schema);
    value_type = x_json_value_get_type(value);
    if (schema_type != value_type && schema_type != JSONNull) /* null represents all values */
        return JSONFailure;
    switch (schema_type) {
        case JSONArray:
            schema_array = x_json_value_get_array(schema);
            value_array = x_json_value_get_array(value);
            count = x_json_array_get_count(schema_array);
            if (count == 0)
                return JSONSuccess; /* Empty array allows all types */
            /* Get first value from array, rest is ignored */
            temp_schema_value = x_json_array_get_value(schema_array, 0);
            for (i = 0; i < x_json_array_get_count(value_array); i++) {
                temp_value = x_json_array_get_value(value_array, i);
                if (x_json_validate(temp_schema_value, temp_value) == 0) {
                    return JSONFailure;
                }
            }
            return JSONSuccess;
        case JSONObject:
            schema_object = x_json_value_get_object(schema);
            value_object = x_json_value_get_object(value);
            count = x_json_object_get_count(schema_object);
            if (count == 0)
                return JSONSuccess; /* Empty object allows all objects */
            else if (x_json_object_get_count(value_object) < count)
                return JSONFailure; /* Tested object mustn't have less name-value pairs than schema */
            for (i = 0; i < count; i++) {
                key = x_json_object_get_name(schema_object, i);
                temp_schema_value = x_json_object_get_value(schema_object, key);
                temp_value = x_json_object_get_value(value_object, key);
                if (temp_value == NULL)
                    return JSONFailure;
                if (x_json_validate(temp_schema_value, temp_value) == JSONFailure)
                    return JSONFailure;
            }
            return JSONSuccess;
        case JSONString: case JSONNumber: case JSONBoolean: case JSONNull:
            return JSONSuccess; /* equality already tested before switch */
        case JSONError: default:
            return JSONFailure;
    }
}

x_JSON_Status x_json_value_equals(const x_JSON_Value *a, const x_JSON_Value *b) {
    x_JSON_Object *a_object = NULL, *b_object = NULL;
    x_JSON_Array *a_array = NULL, *b_array = NULL;
    const char *a_string = NULL, *b_string = NULL;
    const char *key = NULL;
    size_t a_count = 0, b_count = 0, i = 0;
    x_JSON_Value_Type a_type, b_type;
    a_type = x_json_value_get_type(a);
    b_type = x_json_value_get_type(b);
    if (a_type != b_type) {
        return 0;
    }
    switch (a_type) {
        case JSONArray:
            a_array = x_json_value_get_array(a);
            b_array = x_json_value_get_array(b);
            a_count = x_json_array_get_count(a_array);
            b_count = x_json_array_get_count(b_array);
            if (a_count != b_count) {
                return 0;
            }
            for (i = 0; i < a_count; i++) {
                if (!x_json_value_equals(x_json_array_get_value(a_array, i),
                                       x_json_array_get_value(b_array, i))) {
                    return 0;
                }
            }
            return 1;
        case JSONObject:
            a_object = x_json_value_get_object(a);
            b_object = x_json_value_get_object(b);
            a_count = x_json_object_get_count(a_object);
            b_count = x_json_object_get_count(b_object);
            if (a_count != b_count) {
                return 0;
            }
            for (i = 0; i < a_count; i++) {
                key = x_json_object_get_name(a_object, i);
                if (!x_json_value_equals(x_json_object_get_value(a_object, key),
                                       x_json_object_get_value(b_object, key))) {
                    return 0;
                }
            }
            return 1;
        case JSONString:
            a_string = x_json_value_get_string(a);
            b_string = x_json_value_get_string(b);
            return strcmp(a_string, b_string) == 0;
        case JSONBoolean:
            return x_json_value_get_boolean(a) == x_json_value_get_boolean(b);
        case JSONNumber:
            return fabs(x_json_value_get_number(a) - x_json_value_get_number(b)) < 0.000001; /* EPSILON */
        case JSONError:
            return 1;
        case JSONNull:
            return 1;
        default:
            return 1;
    }
}

x_JSON_Value_Type x_json_type(const x_JSON_Value *value) {
    return x_json_value_get_type(value);
}

x_JSON_Object * x_json_object (const x_JSON_Value *value) {
    return x_json_value_get_object(value);
}

x_JSON_Array * x_json_array  (const x_JSON_Value *value) {
    return x_json_value_get_array(value);
}

const char * x_json_string (const x_JSON_Value *value) {
    return x_json_value_get_string(value);
}

double x_json_number (const x_JSON_Value *value) {
    return x_json_value_get_number(value);
}

int x_json_boolean(const x_JSON_Value *value) {
    return x_json_value_get_boolean(value);
}

void x_json_set_allocation_functions(x_JSON_Malloc_Function malloc_fun, x_JSON_Free_Function free_fun) {
    parson_malloc = malloc_fun;
    parson_free = free_fun;
}