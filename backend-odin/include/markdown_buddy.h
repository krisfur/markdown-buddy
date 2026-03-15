#ifndef MARKDOWN_BUDDY_H
#define MARKDOWN_BUDDY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MbStatus {
    MB_STATUS_OK = 0,
    MB_STATUS_INVALID_ARGUMENT = 1,
    MB_STATUS_INTERNAL_ERROR = 2,
} MbStatus;

typedef struct MbString {
    char *data;
    size_t length;
} MbString;

typedef struct MbSection {
    int32_t level;
    int32_t source_offset;
    MbString title;
} MbSection;

typedef struct MbDocumentResult {
    MbString preview_text;
    MbSection *sections;
    size_t section_count;
} MbDocumentResult;

MbStatus mb_process_document(const char *input_utf8, size_t input_length, MbDocumentResult *out_result);
void mb_free_document_result(MbDocumentResult *result);

#ifdef __cplusplus
}
#endif

#endif
