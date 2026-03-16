import AppCore
import Foundation

@main
struct MarkdownBuddyAbiProbe {
    static func main() throws {
        let bridge = MarkdownBuddyBridge()
        let path = CommandLine.arguments.dropFirst().first ?? "../example.md"
        let input = try String(contentsOfFile: path, encoding: .utf8)
        let document = try bridge.processDocument(input)
        var failures = 0

        failures += report("document.sections", actual: String(document.sections.count), expected: "12")
        failures += report("document.blocks", actual: String(document.blocks.count), expected: "49")
        failures += report("document.first_section", actual: document.sections.first?.title ?? "<none>", expected: "Markdown Buddy Demo")
        failures += report("document.first_block_kind", actual: String(document.blocks.first?.kind.rawValue ?? -1), expected: String(MarkdownBlockKind.heading.rawValue))
        failures += report("document.first_block_spans", actual: String(document.blocks.first?.spans.count ?? -1), expected: "1")

        let boldResult = try bridge.apply(command: .bold, text: "hello", selectionStart: 0, selectionEnd: 5)
        failures += report("edit.bold.text", actual: boldResult.text, expected: "**hello**")
        failures += report("edit.bold.selection_start", actual: String(boldResult.selectionStart), expected: "2")
        failures += report("edit.bold.selection_end", actual: String(boldResult.selectionEnd), expected: "7")

        let italicResult = try bridge.apply(command: .italic, text: "*hello*", selectionStart: 0, selectionEnd: 7)
        failures += report("edit.italic.text", actual: italicResult.text, expected: "hello")
        failures += report("edit.italic.selection_start", actual: String(italicResult.selectionStart), expected: "0")
        failures += report("edit.italic.selection_end", actual: String(italicResult.selectionEnd), expected: "5")

        print("summary failures=\(failures)")
        if failures != 0 {
            Foundation.exit(1)
        }
    }

    @discardableResult
    static func report(_ label: String, actual: String, expected: String) -> Int {
        let ok = actual == expected
        print("\(label) actual=\(actual) expected=\(expected) \(ok ? "OK" : "FAIL")")
        return ok ? 0 : 1
    }
}
