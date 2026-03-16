#if os(macOS)
import AppCore
import AppKit
import Foundation
import UniformTypeIdentifiers

@MainActor
final class MacDocumentController: ObservableObject {
    @Published private(set) var core = DocumentController()
    @Published var showingUnsavedAlert = false
    @Published var pendingAction: PendingAction = .none

    enum PendingAction {
        case none
        case newDocument
        case openDocument
        case closeWindow
    }

    var windowTitle: String {
        core.displayName + (core.isDirty ? " *" : "") + " - Markdown Buddy"
    }

    var text: String {
        get { core.text }
        set {
            objectWillChange.send()
            core.setText(newValue)
        }
    }

    var document: MarkdownDocument { core.document }
    var selectedRange: NSRange {
        get { core.selectedRange }
        set {
            objectWillChange.send()
            core.selectedRange = newValue
        }
    }

    func refreshWindowTitle(_ window: NSWindow?) {
        window?.title = windowTitle
    }

    func newDocument() {
        if core.isDirty {
            pendingAction = .newDocument
            showingUnsavedAlert = true
            return
        }
        objectWillChange.send()
        core.newDocument()
    }

    func openDocument() {
        if core.isDirty {
            pendingAction = .openDocument
            showingUnsavedAlert = true
            return
        }
        presentOpenPanel()
    }

    func quit() {
        if core.isDirty {
            pendingAction = .closeWindow
            showingUnsavedAlert = true
            return
        }
        NSApp.keyWindow?.performClose(nil)
    }

    func performPendingAction() {
        let action = pendingAction
        pendingAction = .none
        switch action {
        case .none:
            break
        case .newDocument:
            objectWillChange.send()
            core.newDocument()
        case .openDocument:
            presentOpenPanel()
        case .closeWindow:
            NSApp.keyWindow?.performClose(nil)
        }
    }

    func cancelPendingAction() {
        pendingAction = .none
        showingUnsavedAlert = false
    }

    func saveOrContinue() {
        do {
            if core.currentPath == nil {
                try saveAs()
            } else {
                objectWillChange.send()
                try core.save()
            }
            showingUnsavedAlert = false
            performPendingAction()
        } catch {
            NSApp.presentError(error)
        }
    }

    func discardAndContinue() {
        showingUnsavedAlert = false
        performPendingAction()
    }

    func save() {
        do {
            if core.currentPath == nil {
                try saveAs()
            } else {
                objectWillChange.send()
                try core.save()
            }
        } catch {
            NSApp.presentError(error)
        }
    }

    func saveAs() throws {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.markdown, .plainText]
        panel.nameFieldStringValue = core.currentPath.map { URL(fileURLWithPath: $0).lastPathComponent } ?? "untitled.md"
        if panel.runModal() == .OK, let path = panel.url?.path {
            objectWillChange.send()
            try core.save(as: path)
        }
    }

    func presentOpenPanel() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.markdown, .plainText]
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let path = panel.url?.path {
            do {
                objectWillChange.send()
                try core.load(from: path)
            } catch {
                NSApp.presentError(error)
            }
        }
    }

    func apply(_ command: MarkdownEditCommand) {
        do {
            objectWillChange.send()
            try core.applyEditCommand(command)
        } catch {
            NSApp.presentError(error)
        }
    }

    func jump(to section: MarkdownSection) {
        objectWillChange.send()
        core.jumpToSection(section)
    }

    func showAbout() {
        NSApp.orderFrontStandardAboutPanel(
            options: [
                .applicationName: "Markdown Buddy",
                .applicationVersion: "0.1",
                .credits: NSAttributedString(string: "Backend: Odin C ABI\nFrontend: SwiftUI on macOS"),
                .applicationIcon: NSImage(systemSymbolName: "doc.text.image", accessibilityDescription: nil) as Any,
            ]
        )
    }

    func openRepository() {
        NSWorkspace.shared.open(URL(string: "https://github.com/krisfur/markdown-buddy")!)
    }
}
#endif
