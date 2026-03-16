#if os(macOS)
import AppCore
import SwiftUI

struct PreviewView: View {
    let document: MarkdownDocument

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header("Preview")
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    if document.blocks.isEmpty {
                        Text("Start writing markdown in the editor.")
                            .italic()
                            .foregroundStyle(Color(red: 0.5, green: 0.55, blue: 0.64))
                    } else {
                        ForEach(document.blocks) { block in
                            PreviewBlockView(block: block)
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(20)
            }
        }
        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(Color(red: 0.16, green: 0.19, blue: 0.26)))
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }

    private func header(_ title: String) -> some View {
        HStack {
            Text(title)
                .font(.system(size: 11, weight: .bold))
                .textCase(.uppercase)
                .tracking(1.6)
                .foregroundStyle(Color(red: 0.62, green: 0.67, blue: 0.80))
            Spacer()
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 12)
        .background(Color(red: 0.17, green: 0.22, blue: 0.31))
    }
}

private struct PreviewBlockView: View {
    let block: MarkdownPreviewBlock

    var body: some View {
        switch block.kind {
        case .heading:
            inlineText(block.spans)
                .font(block.level <= 1 ? .system(size: 28, weight: .bold, design: .serif) : block.level == 2 ? .system(size: 22, weight: .bold, design: .serif) : .system(size: 18, weight: .bold, design: .serif))
                .foregroundStyle(Color(red: 0.84, green: 0.87, blue: 0.92))
        case .listItem:
            HStack(alignment: .top, spacing: 8) {
                Text("•")
                    .foregroundStyle(Color(red: 0.51, green: 0.63, blue: 0.76))
                inlineText(block.spans)
            }
        case .blockquote:
            HStack(alignment: .top, spacing: 8) {
                Text("│")
                    .foregroundStyle(Color(red: 0.44, green: 0.61, blue: 0.84))
                inlineText(block.spans)
                    .italic()
                    .foregroundStyle(Color(red: 0.64, green: 0.71, blue: 0.82))
            }
        case .codeBlock:
            Text(block.text)
                .font(.system(.body, design: .monospaced))
                .foregroundStyle(Color(red: 0.66, green: 0.78, blue: 0.50))
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(14)
                .background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(Color(red: 0.12, green: 0.16, blue: 0.22)))
        case .paragraph:
            inlineText(block.spans)
                .foregroundStyle(Color(red: 0.84, green: 0.87, blue: 0.92))
        }
    }

    private func inlineText(_ spans: [MarkdownInlineSpan]) -> Text {
        spans.reduce(Text("")) { partial, span in
            partial + styledText(for: span)
        }
    }

    private func styledText(for span: MarkdownInlineSpan) -> Text {
        switch span.kind {
        case .emphasis:
            return Text(span.text).italic()
        case .strong:
            return Text(span.text).bold()
        case .code:
            return Text(span.text)
                .font(.system(.body, design: .monospaced))
                .foregroundStyle(Color(red: 0.55, green: 0.67, blue: 0.93))
        case .link:
            return Text(span.text)
                .underline()
                .foregroundStyle(Color(red: 0.50, green: 0.76, blue: 1.0))
        case .text:
            return Text(span.text)
        }
    }
}
#endif
