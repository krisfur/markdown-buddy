#if os(macOS)
import AppCore
import AppKit
import SwiftUI

struct EditorView: View {
    @ObservedObject var controller: MacDocumentController

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header("Editor")
            NativeTextEditor(text: Binding(
                get: { controller.text },
                set: { controller.text = $0 }
            ), selectedRange: Binding(
                get: { controller.selectedRange },
                set: { controller.selectedRange = $0 }
            ))
        }
        .background(cardBackground)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }

    private var cardBackground: some View {
        RoundedRectangle(cornerRadius: 16, style: .continuous)
            .fill(Color(red: 0.16, green: 0.19, blue: 0.26))
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

struct NativeTextEditor: NSViewRepresentable {
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
