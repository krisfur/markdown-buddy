package main

import "core:c"
import libc "core:c/libc"
import "core:mem"

PREVIEW_EMPTY :: "Start writing markdown in the editor."

Mb_Status :: enum c.int {
    OK = 0,
    INVALID_ARGUMENT = 1,
    INTERNAL_ERROR = 2,
}

Mb_Inline_Kind :: enum c.int {
    TEXT = 0,
    EMPHASIS = 1,
    STRONG = 2,
    CODE = 3,
    LINK = 4,
}

Mb_Block_Kind :: enum c.int {
    PARAGRAPH = 0,
    HEADING = 1,
    LIST_ITEM = 2,
    BLOCKQUOTE = 3,
    CODE_BLOCK = 4,
}

Mb_Edit_Command :: enum c.int {
    BOLD = 1,
    ITALIC = 2,
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

Mb_Inline_Span :: struct {
    kind: c.int,
    text: Mb_String,
    href: Mb_String,
}

Mb_Preview_Block :: struct {
    kind: c.int,
    level: c.int,
    source_offset: c.int,
    span_start: c.size_t,
    span_count: c.size_t,
    text: Mb_String,
}

Mb_Document_Result :: struct {
    sections: ^Mb_Section,
    section_count: c.size_t,
    spans: ^Mb_Inline_Span,
    span_count: c.size_t,
    blocks: ^Mb_Preview_Block,
    block_count: c.size_t,
}

Mb_Edit_Result :: struct {
    text: Mb_String,
    selection_start: c.int,
    selection_end: c.int,
}

Internal_Section :: struct {
    level: int,
    source_offset: int,
    title: string,
}

Internal_Span :: struct {
    kind: Mb_Inline_Kind,
    text: string,
    href: string,
}

Internal_Block :: struct {
    kind: Mb_Block_Kind,
    level: int,
    source_offset: int,
    span_start: int,
    span_count: int,
    text: string,
}

Section_Buffer :: struct {
    data: [^]Internal_Section,
    count: int,
    capacity: int,
}

Span_Buffer :: struct {
    data: [^]Internal_Span,
    count: int,
    capacity: int,
}

Block_Buffer :: struct {
    data: [^]Internal_Block,
    count: int,
    capacity: int,
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

concat3 :: proc(a, b, c_: string) -> Mb_String {
    total := len(a) + len(b) + len(c_)
    result := Mb_String{}
    if total == 0 {
        return result
    }

    buffer := alloc_bytes(total)
    if buffer == nil {
        return result
    }

    cursor := 0
    if len(a) > 0 {
        mem.copy_non_overlapping(&buffer[cursor], raw_data(a), len(a))
        cursor += len(a)
    }
    if len(b) > 0 {
        mem.copy_non_overlapping(&buffer[cursor], raw_data(b), len(b))
        cursor += len(b)
    }
    if len(c_) > 0 {
        mem.copy_non_overlapping(&buffer[cursor], raw_data(c_), len(c_))
    }

    result.data = &buffer[0]
    result.length = c.size_t(total)
    return result
}

concat4 :: proc(a, b, c_, d: string) -> Mb_String {
    total := len(a) + len(b) + len(c_) + len(d)
    result := Mb_String{}
    if total == 0 {
        return result
    }

    buffer := alloc_bytes(total)
    if buffer == nil {
        return result
    }

    cursor := 0
    if len(a) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(a), len(a)); cursor += len(a) }
    if len(b) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(b), len(b)); cursor += len(b) }
    if len(c_) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(c_), len(c_)); cursor += len(c_) }
    if len(d) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(d), len(d)) }

    result.data = &buffer[0]
    result.length = c.size_t(total)
    return result
}

concat5 :: proc(a, b, c_, d, e: string) -> Mb_String {
    total := len(a) + len(b) + len(c_) + len(d) + len(e)
    result := Mb_String{}
    if total == 0 {
        return result
    }

    buffer := alloc_bytes(total)
    if buffer == nil {
        return result
    }

    cursor := 0
    if len(a) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(a), len(a)); cursor += len(a) }
    if len(b) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(b), len(b)); cursor += len(b) }
    if len(c_) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(c_), len(c_)); cursor += len(c_) }
    if len(d) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(d), len(d)); cursor += len(d) }
    if len(e) > 0 { mem.copy_non_overlapping(&buffer[cursor], raw_data(e), len(e)) }

    result.data = &buffer[0]
    result.length = c.size_t(total)
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

blockquote_text :: proc(line: string) -> (text: string, ok: bool) {
    current := trim_ascii_space(line)
    if len(current) == 0 || current[0] != '>' {
        return "", false
    }
    current = current[1:]
    if len(current) > 0 && current[0] == ' ' {
        current = current[1:]
    }
    return current, true
}

line_is_fence :: proc(line: string) -> bool {
    current := trim_ascii_space(line)
    return len(current) >= 3 && current[0] == '`' && current[1] == '`' && current[2] == '`'
}

grow_buffer :: proc(ptr: rawptr, elem_size, old_cap: int) -> (rawptr, bool) {
    new_cap := 8
    if old_cap > 0 {
        new_cap = old_cap * 2
    }
    mem_ptr := libc.realloc(ptr, c.size_t(new_cap * elem_size))
    return mem_ptr, mem_ptr != nil
}

append_section_internal :: proc(buffer: ^Section_Buffer, value: Internal_Section) -> bool {
    if buffer.count == buffer.capacity {
        mem_ptr, ok := grow_buffer(buffer.data, size_of(Internal_Section), buffer.capacity)
        if !ok {
            return false
        }
        buffer.data = cast([^]Internal_Section)mem_ptr
        buffer.capacity = max(8, buffer.capacity * 2)
    }
    buffer.data[buffer.count] = value
    buffer.count += 1
    return true
}

append_span_internal :: proc(buffer: ^Span_Buffer, value: Internal_Span) -> bool {
    if buffer.count == buffer.capacity {
        mem_ptr, ok := grow_buffer(buffer.data, size_of(Internal_Span), buffer.capacity)
        if !ok {
            return false
        }
        buffer.data = cast([^]Internal_Span)mem_ptr
        buffer.capacity = max(8, buffer.capacity * 2)
    }
    buffer.data[buffer.count] = value
    buffer.count += 1
    return true
}

append_block_internal :: proc(buffer: ^Block_Buffer, value: Internal_Block) -> bool {
    if buffer.count == buffer.capacity {
        mem_ptr, ok := grow_buffer(buffer.data, size_of(Internal_Block), buffer.capacity)
        if !ok {
            return false
        }
        buffer.data = cast([^]Internal_Block)mem_ptr
        buffer.capacity = max(8, buffer.capacity * 2)
    }
    buffer.data[buffer.count] = value
    buffer.count += 1
    return true
}

free_section_buffer :: proc(buffer: ^Section_Buffer) {
    if buffer.data != nil {
        libc.free(buffer.data)
    }
    buffer^ = Section_Buffer{}
}

free_span_buffer :: proc(buffer: ^Span_Buffer) {
    if buffer.data != nil {
        libc.free(buffer.data)
    }
    buffer^ = Span_Buffer{}
}

free_block_buffer :: proc(buffer: ^Block_Buffer) {
    if buffer.data != nil {
        libc.free(buffer.data)
    }
    buffer^ = Block_Buffer{}
}

append_text_span :: proc(spans: ^Span_Buffer, text: string) -> bool {
    if len(text) == 0 {
        return true
    }
    return append_span_internal(spans, Internal_Span{kind = .TEXT, text = text})
}

parse_inline_into :: proc(input: string, spans: ^Span_Buffer) -> bool {
    cursor := 0
    text_start := 0
    for cursor < len(input) {
        if cursor+1 < len(input) && input[cursor] == '*' && input[cursor+1] == '*' {
            end := cursor + 2
            for end+1 < len(input) {
                if input[end] == '*' && input[end+1] == '*' {
                    if !append_text_span(spans, input[text_start:cursor]) { return false }
                    if !append_span_internal(spans, Internal_Span{kind = .STRONG, text = input[cursor+2:end]}) { return false }
                    cursor = end + 2
                    text_start = cursor
                    break
                }
                end += 1
            }
            if text_start == cursor {
                continue
            }
        }

        if input[cursor] == '*' {
            end := cursor + 1
            for end < len(input) && input[end] != '*' {
                end += 1
            }
            if end < len(input) && end > cursor + 1 {
                if !append_text_span(spans, input[text_start:cursor]) { return false }
                if !append_span_internal(spans, Internal_Span{kind = .EMPHASIS, text = input[cursor+1:end]}) { return false }
                cursor = end + 1
                text_start = cursor
                continue
            }
        }

        if input[cursor] == '`' {
            end := cursor + 1
            for end < len(input) && input[end] != '`' {
                end += 1
            }
            if end < len(input) && end > cursor + 1 {
                if !append_text_span(spans, input[text_start:cursor]) { return false }
                if !append_span_internal(spans, Internal_Span{kind = .CODE, text = input[cursor+1:end]}) { return false }
                cursor = end + 1
                text_start = cursor
                continue
            }
        }

        if input[cursor] == '[' {
            close_label := cursor + 1
            for close_label < len(input) && input[close_label] != ']' {
                close_label += 1
            }
            if close_label+1 < len(input) && input[close_label+1] == '(' {
                close_href := close_label + 2
                for close_href < len(input) && input[close_href] != ')' {
                    close_href += 1
                }
                if close_href < len(input) && close_label > cursor + 1 {
                    if !append_text_span(spans, input[text_start:cursor]) { return false }
                    if !append_span_internal(spans, Internal_Span{
                        kind = .LINK,
                        text = input[cursor+1:close_label],
                        href = input[close_label+2:close_href],
                    }) { return false }
                    cursor = close_href + 1
                    text_start = cursor
                    continue
                }
            }
        }

        cursor += 1
    }
    return append_text_span(spans, input[text_start:])
}

make_preview_model :: proc(input: string, sections: ^Section_Buffer, spans: ^Span_Buffer, blocks: ^Block_Buffer) -> bool {
    if line_is_blank(input) {
        local_start := spans.count
        if !parse_inline_into(PREVIEW_EMPTY, spans) { return false }
        return append_block_internal(blocks, Internal_Block{kind = .PARAGRAPH, source_offset = 0, span_start = local_start, span_count = spans.count - local_start})
    }

    index := 0
    for index < len(input) {
        line_start := index
        line_end := index
        for line_end < len(input) && input[line_end] != '\n' {
            line_end += 1
        }
        line := input[line_start:line_end]
        if len(line) > 0 && line[len(line)-1] == '\r' {
            line = line[:len(line)-1]
        }

        if line_is_blank(line) {
            index = line_end + 1
            continue
        }

        if line_is_fence(line) {
            code_start := line_end + 1
            index = code_start
            code_end := code_start
            for index < len(input) {
                sub_start := index
                sub_end := index
                for sub_end < len(input) && input[sub_end] != '\n' {
                    sub_end += 1
                }
                sub_line := input[sub_start:sub_end]
                if len(sub_line) > 0 && sub_line[len(sub_line)-1] == '\r' {
                    sub_line = sub_line[:len(sub_line)-1]
                }
                if line_is_fence(sub_line) {
                    code_end = sub_start
                    index = sub_end + 1
                    break
                }
                index = sub_end + 1
                code_end = sub_end
            }
            if !append_block_internal(blocks, Internal_Block{kind = .CODE_BLOCK, source_offset = line_start, text = input[code_start:code_end]}) { return false }
            continue
        }

        level, title, is_heading := line_section_info(line)
        if is_heading {
            if !append_section_internal(sections, Internal_Section{level = level, source_offset = line_start, title = title}) { return false }
            span_start := spans.count
            if !parse_inline_into(title, spans) { return false }
            if !append_block_internal(blocks, Internal_Block{kind = .HEADING, level = level, source_offset = line_start, span_start = span_start, span_count = spans.count - span_start}) { return false }
            index = line_end + 1
            continue
        }

        item, is_list := unordered_list_item(line)
        if is_list {
            span_start := spans.count
            if !parse_inline_into(item, spans) { return false }
            if !append_block_internal(blocks, Internal_Block{kind = .LIST_ITEM, source_offset = line_start, span_start = span_start, span_count = spans.count - span_start}) { return false }
            index = line_end + 1
            continue
        }

        quote, is_quote := blockquote_text(line)
        if is_quote {
            span_start := spans.count
            if !parse_inline_into(quote, spans) { return false }
            if !append_block_internal(blocks, Internal_Block{kind = .BLOCKQUOTE, source_offset = line_start, span_start = span_start, span_count = spans.count - span_start}) { return false }
            index = line_end + 1
            continue
        }

        normalized := trim_ascii_space(line)
        span_start := spans.count
        if !parse_inline_into(normalized, spans) { return false }
        if !append_block_internal(blocks, Internal_Block{kind = .PARAGRAPH, source_offset = line_start, span_start = span_start, span_count = spans.count - span_start}) { return false }
        index = line_end + 1
    }

    return true
}

copy_sections :: proc(values: Section_Buffer, out_result: ^Mb_Document_Result) -> bool {
    if values.count == 0 {
        return true
    }
    buffer := cast(^Mb_Section)libc.malloc(c.size_t(values.count * size_of(Mb_Section)))
    if buffer == nil {
        return false
    }
    items := (cast([^]Mb_Section)buffer)[:values.count]
    for i in 0..<values.count {
        value := values.data[i]
        items[i] = Mb_Section{level = c.int(value.level), source_offset = c.int(value.source_offset), title = clone_string(value.title)}
        if len(value.title) > 0 && items[i].title.data == nil {
            out_result.sections = buffer
            out_result.section_count = c.size_t(i)
            mb_free_document_result(out_result)
            return false
        }
    }
    out_result.sections = buffer
    out_result.section_count = c.size_t(values.count)
    return true
}

copy_spans :: proc(values: Span_Buffer, out_result: ^Mb_Document_Result) -> bool {
    if values.count == 0 {
        return true
    }
    buffer := cast(^Mb_Inline_Span)libc.malloc(c.size_t(values.count * size_of(Mb_Inline_Span)))
    if buffer == nil {
        return false
    }
    items := (cast([^]Mb_Inline_Span)buffer)[:values.count]
    for i in 0..<values.count {
        value := values.data[i]
        items[i] = Mb_Inline_Span{kind = c.int(value.kind), text = clone_string(value.text), href = clone_string(value.href)}
        if (len(value.text) > 0 && items[i].text.data == nil) || (len(value.href) > 0 && items[i].href.data == nil) {
            out_result.spans = buffer
            out_result.span_count = c.size_t(i + 1)
            mb_free_document_result(out_result)
            return false
        }
    }
    out_result.spans = buffer
    out_result.span_count = c.size_t(values.count)
    return true
}

copy_blocks :: proc(values: Block_Buffer, out_result: ^Mb_Document_Result) -> bool {
    if values.count == 0 {
        return true
    }
    buffer := cast(^Mb_Preview_Block)libc.malloc(c.size_t(values.count * size_of(Mb_Preview_Block)))
    if buffer == nil {
        return false
    }
    items := (cast([^]Mb_Preview_Block)buffer)[:values.count]
    for i in 0..<values.count {
        value := values.data[i]
        items[i] = Mb_Preview_Block{
            kind = c.int(value.kind),
            level = c.int(value.level),
            source_offset = c.int(value.source_offset),
            span_start = c.size_t(value.span_start),
            span_count = c.size_t(value.span_count),
            text = clone_string(value.text),
        }
        if len(value.text) > 0 && items[i].text.data == nil {
            out_result.blocks = buffer
            out_result.block_count = c.size_t(i + 1)
            mb_free_document_result(out_result)
            return false
        }
    }
    out_result.blocks = buffer
    out_result.block_count = c.size_t(values.count)
    return true
}

has_wrapper :: proc(input, wrapper: string, start: int) -> bool {
    if start < 0 || start+len(wrapper) > len(input) {
        return false
    }
    return input[start:start+len(wrapper)] == wrapper
}

apply_wrapping_edit :: proc(input, wrapper: string, selection_start, selection_end: int, out_result: ^Mb_Edit_Result) -> bool {
    w := len(wrapper)
    start := min(selection_start, selection_end)
    end := max(selection_start, selection_end)

    if start < 0 || end < 0 || start > len(input) || end > len(input) {
        return false
    }

    if start != end {
        if end-start >= w*2 && has_wrapper(input, wrapper, start) && has_wrapper(input, wrapper, end-w) {
            out_result.text = concat3(input[:start], input[start+w:end-w], input[end:])
            out_result.selection_start = c.int(start)
            out_result.selection_end = c.int(end - w*2)
            return out_result.text.length > 0 || len(input) - w*2 == 0
        }

        if has_wrapper(input, wrapper, start-w) && has_wrapper(input, wrapper, end) {
            out_result.text = concat3(input[:start-w], input[start:end], input[end+w:])
            out_result.selection_start = c.int(start - w)
            out_result.selection_end = c.int(end - w)
            return out_result.text.length > 0 || len(input) - w*2 == 0
        }

        out_result.text = concat5(input[:start], wrapper, input[start:end], wrapper, input[end:])
        out_result.selection_start = c.int(start + w)
        out_result.selection_end = c.int(end + w)
        return out_result.text.length > 0
    }

    if has_wrapper(input, wrapper, start-w) && has_wrapper(input, wrapper, start) {
        out_result.text = concat3(input[:start-w], "", input[start+w:])
        out_result.selection_start = c.int(start - w)
        out_result.selection_end = c.int(start - w)
        return out_result.text.length > 0 || len(input) - w*2 == 0
    }

    out_result.text = concat4(input[:start], wrapper, wrapper, input[end:])
    out_result.selection_start = c.int(start + w)
    out_result.selection_end = c.int(start + w)
    return out_result.text.length > 0 || len(input)+w*2 == 0
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
    sections := Section_Buffer{}
    spans := Span_Buffer{}
    blocks := Block_Buffer{}
    defer free_section_buffer(&sections)
    defer free_span_buffer(&spans)
    defer free_block_buffer(&blocks)

    if !make_preview_model(input, &sections, &spans, &blocks) {
        return .INTERNAL_ERROR
    }

    if !copy_sections(sections, out_result) {
        return .INTERNAL_ERROR
    }
    if !copy_spans(spans, out_result) {
        return .INTERNAL_ERROR
    }
    if !copy_blocks(blocks, out_result) {
        return .INTERNAL_ERROR
    }

    return .OK
}

@(export)
mb_apply_edit_command :: proc(input_utf8: ^u8, input_length: c.size_t, selection_start, selection_end, command: c.int, out_result: ^Mb_Edit_Result) -> Mb_Status {
    if out_result == nil {
        return .INVALID_ARGUMENT
    }

    out_result^ = Mb_Edit_Result{}

    if input_utf8 == nil && input_length > 0 {
        return .INVALID_ARGUMENT
    }

    input := to_string(input_utf8, input_length)
    ok := false

    switch Mb_Edit_Command(command) {
    case .BOLD:
        ok = apply_wrapping_edit(input, "**", int(selection_start), int(selection_end), out_result)
    case .ITALIC:
        ok = apply_wrapping_edit(input, "*", int(selection_start), int(selection_end), out_result)
    case:
        return .INVALID_ARGUMENT
    }

    if !ok && len(input) > 0 {
        mb_free_edit_result(out_result)
        return .INTERNAL_ERROR
    }

    return .OK
}

@(export)
mb_free_document_result :: proc(result: ^Mb_Document_Result) {
    if result == nil {
        return
    }

    if result.sections != nil {
        sections := (cast([^]Mb_Section)result.sections)[:int(result.section_count)]
        for item in sections {
            free_string(item.title)
        }
        libc.free(result.sections)
    }

    if result.spans != nil {
        spans := (cast([^]Mb_Inline_Span)result.spans)[:int(result.span_count)]
        for item in spans {
            free_string(item.text)
            free_string(item.href)
        }
        libc.free(result.spans)
    }

    if result.blocks != nil {
        blocks := (cast([^]Mb_Preview_Block)result.blocks)[:int(result.block_count)]
        for item in blocks {
            free_string(item.text)
        }
        libc.free(result.blocks)
    }

    result^ = Mb_Document_Result{}
}

@(export)
mb_free_edit_result :: proc(result: ^Mb_Edit_Result) {
    if result == nil {
        return
    }

    free_string(result.text)
    result^ = Mb_Edit_Result{}
}
