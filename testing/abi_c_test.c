#include <stdio.h>
#include <string.h>

#include "markdown_buddy.h"

static int failures = 0;

static void report_size(const char *label, size_t actual, size_t expected) {
    printf("%s actual=%zu expected=%zu %s\n", label, actual, expected, actual == expected ? "OK" : "FAIL");
    if (actual != expected) {
        failures += 1;
    }
}

static void report_int(const char *label, int actual, int expected) {
    printf("%s actual=%d expected=%d %s\n", label, actual, expected, actual == expected ? "OK" : "FAIL");
    if (actual != expected) {
        failures += 1;
    }
}

static void report_string(const char *label, const char *actual, const char *expected) {
    int ok = strcmp(actual, expected) == 0;
    printf("%s actual=%s expected=%s %s\n", label, actual, expected, ok ? "OK" : "FAIL");
    if (!ok) {
        failures += 1;
    }
}

static void copy_mb_string(char *dest, size_t dest_size, MbString value) {
    size_t count = value.length < dest_size - 1 ? value.length : dest_size - 1;
    if (count > 0 && value.data != NULL) {
        memcpy(dest, value.data, count);
    }
    dest[count] = '\0';
}

static void check_document(void) {
    const char *input = "# Title\n\nHello **world**\n\n- Item\n";
    MbDocumentResult result = {0};
    char first_section[64] = {0};

    if (mb_process_document(input, strlen(input), &result) != MB_STATUS_OK) {
        fprintf(stderr, "document.process status=FAIL\n");
        failures += 1;
        return;
    }

    copy_mb_string(first_section, sizeof(first_section), result.sections[0].title);
    report_size("document.sections", result.section_count, 1);
    report_size("document.blocks", result.block_count, 3);
    report_size("document.spans", result.span_count, 4);
    report_string("document.first_section", first_section, "Title");
    report_int("document.first_block_kind", result.blocks[0].kind, MB_BLOCK_HEADING);

    mb_free_document_result(&result);
}

static void check_edit_commands(void) {
    MbEditResult result = {0};
    char text[64] = {0};

    if (mb_apply_edit_command("hello", 5, 0, 5, MB_EDIT_BOLD, &result) != MB_STATUS_OK) {
        fprintf(stderr, "edit.bold status=FAIL\n");
        failures += 1;
        return;
    }

    copy_mb_string(text, sizeof(text), result.text);
    report_string("edit.bold.text", text, "**hello**");
    report_int("edit.bold.selection_start", result.selection_start, 2);
    report_int("edit.bold.selection_end", result.selection_end, 7);
    mb_free_edit_result(&result);

    if (mb_apply_edit_command("*hello*", 7, 0, 7, MB_EDIT_ITALIC, &result) != MB_STATUS_OK) {
        fprintf(stderr, "edit.italic status=FAIL\n");
        failures += 1;
        return;
    }

    copy_mb_string(text, sizeof(text), result.text);
    report_string("edit.italic.text", text, "hello");
    report_int("edit.italic.selection_start", result.selection_start, 0);
    report_int("edit.italic.selection_end", result.selection_end, 5);
    mb_free_edit_result(&result);
}

int main(void) {
    check_document();
    check_edit_commands();

    if (failures != 0) {
        fprintf(stderr, "summary failures=%d\n", failures);
        return 1;
    }

    puts("summary failures=0");
    return 0;
}
