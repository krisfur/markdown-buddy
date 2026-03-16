#if os(macOS)
import AppCore
import AppKit
import SwiftUI

struct ContentView: View {
    @ObservedObject var controller: MacDocumentController

    var body: some View {
        NavigationSplitView {
            sidebar
                .navigationSplitViewColumnWidth(min: 220, ideal: 240, max: 280)
        } detail: {
            HSplitView {
                editorPane
                previewPane
            }
            .padding(12)
            .background(
                LinearGradient(
                    colors: [Color(red: 0.10, green: 0.14, blue: 0.20), Color(red: 0.07, green: 0.10, blue: 0.14)],
                    startPoint: .top,
                    endPoint: .bottom
                )
            )
        }
        .alert("Save changes before closing?", isPresented: $controller.showingUnsavedAlert) {
            Button("Cancel", role: .cancel) { controller.cancelPendingAction() }
            Button("Discard", role: .destructive) { controller.discardAndContinue() }
            Button("Save") { controller.saveOrContinue() }
        } message: {
            Text("Your current document has unsaved changes.")
        }
        .onAppear {
            controller.refreshWindowTitle(NSApp.keyWindow)
        }
        .onChange(of: controller.windowTitle) { _, newValue in
            NSApp.keyWindow?.title = newValue
        }
    }

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 0) {
            panelHeader("Sections")
            List(controller.document.sections) { section in
                Button {
                    controller.jump(to: section)
                } label: {
                    Text(section.title.isEmpty ? "(untitled)" : section.title)
                        .lineLimit(4)
                        .multilineTextAlignment(.leading)
                        .padding(.leading, CGFloat(max(section.level - 1, 0)) * 12)
                        .padding(.vertical, 4)
                }
                .buttonStyle(.plain)
            }
            .listStyle(.sidebar)
        }
        .background(Color(red: 0.16, green: 0.20, blue: 0.28))
    }

    private var editorPane: some View {
        VStack(alignment: .leading, spacing: 0) {
            panelHeader("Editor")
            NativeTextEditor(
                text: Binding(
                    get: { controller.text },
                    set: { controller.text = $0 }
                ),
                selectedRange: Binding(
                    get: { controller.selectedRange },
                    set: { controller.selectedRange = $0 }
                )
            )
        }
        .background(cardBackground)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }

    private var previewPane: some View {
        VStack(alignment: .leading, spacing: 0) {
            panelHeader("Preview")
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    if controller.document.blocks.isEmpty {
                        Text("Start writing markdown in the editor.")
                            .italic()
                            .foregroundStyle(Color(red: 0.5, green: 0.55, blue: 0.64))
                    } else {
                        ForEach(controller.document.blocks) { block in
                            PreviewBlockView(block: block)
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(20)
            }
        }
        .background(cardBackground)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }

    private var cardBackground: some View {
        RoundedRectangle(cornerRadius: 16, style: .continuous)
            .fill(Color(red: 0.16, green: 0.19, blue: 0.26))
    }

    private func panelHeader(_ title: String) -> some View {
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

private struct NativeTextEditor: NSViewRepresentable {
    @Binding var text: String
    @Binding var selectedRange: NSRange

    func makeCoordinator() -> Coordinator {
        Coordinator(text: $text, selectedRange: $selectedRange)
    }

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSScrollView()
        let textView = NSTextView()
        textView.isRichText = false
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        textView.allowsUndo = true
        textView.font = .monospacedSystemFont(ofSize: 14, weight: .regular)
        textView.backgroundColor = NSColor(calibratedRed: 0.16, green: 0.19, blue: 0.26, alpha: 1.0)
        textView.textColor = NSColor(calibratedRed: 0.84, green: 0.87, blue: 0.92, alpha: 1.0)
        textView.insertionPointColor = NSColor(calibratedRed: 0.64, green: 0.78, blue: 0.55, alpha: 1.0)
        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        textView.autoresizingMask = [.width]
        textView.textContainerInset = NSSize(width: 18, height: 18)
        textView.textContainer?.widthTracksTextView = true
        textView.delegate = context.coordinator
        textView.string = text

        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = false
        scrollView.borderType = .noBorder
        scrollView.backgroundColor = textView.backgroundColor
        scrollView.documentView = textView
        context.coordinator.textView = textView
        return scrollView
    }

    func updateNSView(_ nsView: NSScrollView, context: Context) {
        guard let textView = context.coordinator.textView else { return }
        if textView.string != text {
            textView.string = text
        }
        if textView.selectedRange() != selectedRange {
            textView.setSelectedRange(selectedRange)
            textView.scrollRangeToVisible(selectedRange)
        }
    }

    final class Coordinator: NSObject, NSTextViewDelegate {
        @Binding var text: String
        @Binding var selectedRange: NSRange
        weak var textView: NSTextView?

        init(text: Binding<String>, selectedRange: Binding<NSRange>) {
            _text = text
            _selectedRange = selectedRange
        }

        func textDidChange(_ notification: Notification) {
            guard let textView else { return }
            text = textView.string
            selectedRange = textView.selectedRange()
        }

        func textViewDidChangeSelection(_ notification: Notification) {
            guard let textView else { return }
            selectedRange = textView.selectedRange()
        }
    }
}
#endif
