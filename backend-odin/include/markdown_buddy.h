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

typedef enum MbInlineKind {
    MB_INLINE_TEXT = 0,
    MB_INLINE_EMPHASIS = 1,
    MB_INLINE_STRONG = 2,
    MB_INLINE_CODE = 3,
    MB_INLINE_LINK = 4,
} MbInlineKind;

typedef enum MbBlockKind {
    MB_BLOCK_PARAGRAPH = 0,
    MB_BLOCK_HEADING = 1,
    MB_BLOCK_LIST_ITEM = 2,
    MB_BLOCK_BLOCKQUOTE = 3,
    MB_BLOCK_CODE_BLOCK = 4,
} MbBlockKind;

typedef struct MbString {
    char *data;
    size_t length;
} MbString;

typedef struct MbSection {
    int32_t level;
    int32_t source_offset;
    MbString title;
} MbSection;

typedef struct MbInlineSpan {
    int32_t kind;
    MbString text;
    MbString href;
} MbInlineSpan;

typedef struct MbPreviewBlock {
    int32_t kind;
    int32_t level;
    int32_t source_offset;
    size_t span_start;
    size_t span_count;
    MbString text;
} MbPreviewBlock;

typedef struct MbDocumentResult {
    MbSection *sections;
    size_t section_count;
    MbInlineSpan *spans;
    size_t span_count;
    MbPreviewBlock *blocks;
    size_t block_count;
} MbDocumentResult;

MbStatus mb_process_document(const char *input_utf8, size_t input_length, MbDocumentResult *out_result);
void mb_free_document_result(MbDocumentResult *result);

#ifdef __cplusplus
}
#endif

#endif
