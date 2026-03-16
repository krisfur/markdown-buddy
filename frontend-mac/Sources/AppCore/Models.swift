import Foundation

public enum MarkdownInlineKind: Int32, Sendable {
    case text = 0
    case emphasis = 1
    case strong = 2
    case code = 3
    case link = 4
}

public enum MarkdownBlockKind: Int32, Sendable {
    case paragraph = 0
    case heading = 1
    case listItem = 2
    case blockquote = 3
    case codeBlock = 4
}

public enum MarkdownEditCommand: Int32, Sendable {
    case bold = 1
    case italic = 2
}

public struct MarkdownSection: Identifiable, Hashable, Sendable {
    public let id: Int
    public let level: Int
    public let sourceOffset: Int
    public let title: String
}

public struct MarkdownInlineSpan: Hashable, Sendable {
    public let kind: MarkdownInlineKind
    public let text: String
    public let href: String?
}

public struct MarkdownPreviewBlock: Identifiable, Hashable, Sendable {
    public let id: Int
    public let kind: MarkdownBlockKind
    public let level: Int
    public let sourceOffset: Int
    public let spans: [MarkdownInlineSpan]
    public let text: String
}

public struct MarkdownDocument: Sendable {
    public let sections: [MarkdownSection]
    public let blocks: [MarkdownPreviewBlock]
}

public struct MarkdownEditResult: Sendable {
    public let text: String
    public let selectionStart: Int
    public let selectionEnd: Int
}
