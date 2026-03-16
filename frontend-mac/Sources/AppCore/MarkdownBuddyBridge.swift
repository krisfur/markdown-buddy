import Foundation
import CMarkdownBuddy

public enum MarkdownBuddyBridgeError: Error, LocalizedError {
    case processingFailed(MbStatus)
    case editingFailed(MbStatus)

    public var errorDescription: String? {
        switch self {
        case .processingFailed(let status):
            return "Failed to process markdown document (status \(status.rawValue))."
        case .editingFailed(let status):
            return "Failed to apply markdown edit command (status \(status.rawValue))."
        }
    }
}

public struct MarkdownBuddyBridge {
    public init() {}

    public func processDocument(_ text: String) throws -> MarkdownDocument {
        var result = MbDocumentResult()
        let status = text.withCString { raw in
            mb_process_document(raw, strlen(raw), &result)
        }

        guard status == MB_STATUS_OK else {
            mb_free_document_result(&result)
            throw MarkdownBuddyBridgeError.processingFailed(status)
        }

        defer { mb_free_document_result(&result) }

        let spans: [MarkdownInlineSpan] = (0..<Int(result.span_count)).map { index in
            let span = result.spans[index]
            return MarkdownInlineSpan(
                kind: MarkdownInlineKind(rawValue: span.kind) ?? .text,
                text: Self.string(from: span.text),
                href: span.href.data == nil ? nil : Self.string(from: span.href)
            )
        }

        let sections: [MarkdownSection] = (0..<Int(result.section_count)).map { index in
            let section = result.sections[index]
            return MarkdownSection(
                id: index,
                level: Int(section.level),
                sourceOffset: Int(section.source_offset),
                title: Self.string(from: section.title)
            )
        }

        let blocks: [MarkdownPreviewBlock] = (0..<Int(result.block_count)).map { index in
            let block = result.blocks[index]
            let start = Int(block.span_start)
            let end = start + Int(block.span_count)
            return MarkdownPreviewBlock(
                id: index,
                kind: MarkdownBlockKind(rawValue: block.kind) ?? .paragraph,
                level: Int(block.level),
                sourceOffset: Int(block.source_offset),
                spans: Array(spans[start..<min(end, spans.count)]),
                text: Self.string(from: block.text)
            )
        }

        return MarkdownDocument(sections: sections, blocks: blocks)
    }

    public func apply(command: MarkdownEditCommand, text: String, selectionStart: Int, selectionEnd: Int) throws -> MarkdownEditResult {
        var result = MbEditResult()
        let status = text.withCString { raw in
            mb_apply_edit_command(raw, strlen(raw), Int32(selectionStart), Int32(selectionEnd), command.rawValue, &result)
        }

        guard status == MB_STATUS_OK else {
            mb_free_edit_result(&result)
            throw MarkdownBuddyBridgeError.editingFailed(status)
        }

        defer { mb_free_edit_result(&result) }

        return MarkdownEditResult(
            text: Self.string(from: result.text),
            selectionStart: Int(result.selection_start),
            selectionEnd: Int(result.selection_end)
        )
    }

    private static func string(from value: MbString) -> String {
        guard let data = value.data else {
            return ""
        }
        let raw = UnsafeRawPointer(data)
        let bytes = [UInt8](UnsafeBufferPointer(start: raw.assumingMemoryBound(to: UInt8.self), count: Int(value.length)))
        return String(decoding: bytes, as: UTF8.self)
    }
}
