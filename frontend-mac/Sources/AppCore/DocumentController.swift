import Foundation

@MainActor
public final class DocumentController {
    public private(set) var bridge = MarkdownBuddyBridge()
    public private(set) var currentPath: String?
    public private(set) var isDirty = false
    public var text: String {
        didSet {
            guard !isProgrammaticChange else { return }
            isDirty = true
            refreshPreview()
        }
    }
    public private(set) var document = MarkdownDocument(sections: [], blocks: [])
    public var selectedRange: NSRange = .init(location: 0, length: 0)

    private var isProgrammaticChange = false

    public init(text: String = "") {
        self.text = text
        refreshPreview()
    }

    public var displayName: String {
        currentPath.map { URL(fileURLWithPath: $0).lastPathComponent } ?? "Untitled"
    }

    public func setText(_ newText: String, markDirty: Bool = true) {
        isProgrammaticChange = true
        text = newText
        isProgrammaticChange = false
        refreshPreview()
        if markDirty {
            isDirty = true
        }
    }

    public func refreshPreview() {
        do {
            document = try bridge.processDocument(text)
        } catch {
            document = MarkdownDocument(sections: [], blocks: [])
        }
    }

    public func newDocument() {
        currentPath = nil
        isDirty = false
        selectedRange = .init(location: 0, length: 0)
        setText("", markDirty: false)
    }

    public func load(from path: String) throws {
        let contents = try String(contentsOfFile: path, encoding: .utf8)
        currentPath = URL(fileURLWithPath: path).standardized.path
        isDirty = false
        selectedRange = .init(location: 0, length: 0)
        setText(contents, markDirty: false)
    }

    public func save() throws {
        guard let currentPath else {
            throw CocoaError(.fileNoSuchFile)
        }
        try text.write(toFile: currentPath, atomically: true, encoding: .utf8)
        isDirty = false
    }

    public func save(as path: String) throws {
        try text.write(toFile: path, atomically: true, encoding: .utf8)
        currentPath = URL(fileURLWithPath: path).standardized.path
        isDirty = false
    }

    public func jumpToSection(_ section: MarkdownSection) {
        selectedRange = NSRange(location: section.sourceOffset, length: 0)
    }

    public func applyEditCommand(_ command: MarkdownEditCommand) throws {
        let result = try bridge.apply(
            command: command,
            text: text,
            selectionStart: selectedRange.location,
            selectionEnd: selectedRange.location + selectedRange.length
        )
        isProgrammaticChange = true
        text = result.text
        isProgrammaticChange = false
        selectedRange = NSRange(location: result.selectionStart, length: result.selectionEnd - result.selectionStart)
        isDirty = true
        refreshPreview()
    }
}
