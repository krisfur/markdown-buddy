package main

import "core:c"
import libc "core:c/libc"
import "core:mem"
import "core:strings"

PREVIEW_EMPTY :: "Start writing markdown in the editor."

Mb_Status :: enum c.int {
    OK = 0,
    INVALID_ARGUMENT = 1,
    INTERNAL_ERROR = 2,
}

Mb_String :: struct {
    data: ^u8,
    length: c.size_t,
}

Mb_Section :: struct {
    level: c.int,
    source_offset: c.int,
    title: Mb_String,
}

Mb_Document_Result :: struct {
    preview_text: Mb_String,
    sections: ^Mb_Section,
    section_count: c.size_t,
}

alloc_bytes :: proc(size: int) -> []u8 {
    if size <= 0 {
        return nil
    }

    data := cast([^]u8)libc.malloc(c.size_t(size))
    if data == nil {
        return nil
    }

    return data[:size]
}

clone_string :: proc(value: string) -> Mb_String {
    result := Mb_String{}
    if len(value) == 0 {
        return result
    }

    buffer := alloc_bytes(len(value))
    if buffer == nil {
        return result
    }

    mem.copy_non_overlapping(&buffer[0], raw_data(value), len(value))
    result.data = &buffer[0]
    result.length = c.size_t(len(value))
    return result
}

free_string :: proc(value: Mb_String) {
    if value.data != nil {
        libc.free(value.data)
    }
}

to_string :: proc(input_utf8: ^u8, input_length: c.size_t) -> string {
    if input_utf8 == nil || input_length == 0 {
        return ""
    }

    data := ([^]u8)(input_utf8)
    return string(data[:int(input_length)])
}

trim_ascii_space :: proc(value: string) -> string {
    start := 0
    end := len(value)

    for start < end {
        ch := value[start]
        if ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' {
            start += 1
            continue
        }
        break
    }

    for end > start {
        ch := value[end-1]
        if ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' {
            end -= 1
            continue
        }
        break
    }

    return value[start:end]
}

line_is_blank :: proc(line: string) -> bool {
    return len(trim_ascii_space(line)) == 0
}

line_section_info :: proc(line: string) -> (level: int, title: string, ok: bool) {
    current := line
    if len(current) > 0 && current[len(current)-1] == '\r' {
        current = current[:len(current)-1]
    }

    hashes := 0
    for hashes < len(current) && current[hashes] == '#' {
        hashes += 1
    }

    if hashes == 0 || hashes > 6 || len(current) <= hashes || current[hashes] != ' ' {
        return 0, "", false
    }

    return hashes, trim_ascii_space(current[hashes+1:]), true
}

unordered_list_item :: proc(line: string) -> (item: string, ok: bool) {
    current := trim_ascii_space(line)
    if len(current) >= 2 && (current[0] == '-' || current[0] == '*') && current[1] == ' ' {
        return trim_ascii_space(current[2:]), true
    }

    return "", false
}

make_preview_text :: proc(input: string) -> Mb_String {
    if line_is_blank(input) {
        return clone_string(PREVIEW_EMPTY)
    }

    return clone_string(input)
}

count_sections :: proc(input: string) -> int {
    count := 0
    index := 0

    for index < len(input) {
        line_start := index
        line_end := index

        for line_end < len(input) && input[line_end] != '\n' {
            line_end += 1
        }

        _, _, ok := line_section_info(input[line_start:line_end])
        if ok {
            count += 1
        }

        index = line_end + 1
    }

    return count
}

parse_sections :: proc(input: string, out_result: ^Mb_Document_Result) -> bool {
    section_count := count_sections(input)
    if section_count == 0 {
        out_result.sections = nil
        out_result.section_count = 0
        return true
    }

    size := section_count * size_of(Mb_Section)
    buffer := cast(^Mb_Section)libc.malloc(c.size_t(size))
    if buffer == nil {
        return false
    }

    sections := (cast([^]Mb_Section)buffer)[:section_count]
    for i in 0..<section_count {
        sections[i] = Mb_Section{}
    }

    index := 0
    section_index := 0
    for index < len(input) {
        line_start := index
        line_end := index

        for line_end < len(input) && input[line_end] != '\n' {
            line_end += 1
        }

        level, title, ok := line_section_info(input[line_start:line_end])
        if ok {
            cloned := clone_string(title)
            if len(title) > 0 && cloned.data == nil {
                for i in 0..<section_index {
                    free_string(sections[i].title)
                }
                libc.free(buffer)
                return false
            }

            sections[section_index] = Mb_Section{
                level = c.int(level),
                source_offset = c.int(line_start),
                title = cloned,
            }
            section_index += 1
        }

        index = line_end + 1
    }

    out_result.sections = buffer
    out_result.section_count = c.size_t(section_count)
    return true
}

@(export)
mb_process_document :: proc(input_utf8: ^u8, input_length: c.size_t, out_result: ^Mb_Document_Result) -> Mb_Status {
    if out_result == nil {
        return .INVALID_ARGUMENT
    }

    out_result^ = Mb_Document_Result{}

    if input_utf8 == nil && input_length > 0 {
        return .INVALID_ARGUMENT
    }

    input := to_string(input_utf8, input_length)
    preview := make_preview_text(input)
    if preview.length > 0 && preview.data == nil {
        return .INTERNAL_ERROR
    }

    out_result.preview_text = preview

    if !parse_sections(input, out_result) {
        mb_free_document_result(out_result)
        return .INTERNAL_ERROR
    }

    return .OK
}

@(export)
mb_free_document_result :: proc(result: ^Mb_Document_Result) {
    if result == nil {
        return
    }

    free_string(result.preview_text)

    if result.sections != nil {
        sections := (cast([^]Mb_Section)result.sections)[:int(result.section_count)]
        for item in sections {
            free_string(item.title)
        }
        libc.free(result.sections)
    }

    result^ = Mb_Document_Result{}
}

main :: proc() {}
