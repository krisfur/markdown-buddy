#if os(macOS)
import AppCore
import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var controller: MacDocumentController?
    private var pendingURLs: [URL] = []

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)

        for window in NSApp.windows {
            window.collectionBehavior.remove(.transient)
            window.level = .normal
            window.makeKeyAndOrderFront(nil)
        }

        let launchURLs = CommandLine.arguments.dropFirst().map { URL(fileURLWithPath: $0) }
        if !launchURLs.isEmpty {
            handle(urls: launchURLs)
        }
    }

    func application(_ application: NSApplication, open urls: [URL]) {
        handle(urls: urls)
    }

    func attach(controller: MacDocumentController) {
        self.controller = controller
        flushPendingURLs()
    }

    private func handle(urls: [URL]) {
        let fileURLs = urls.filter { $0.isFileURL }
        guard !fileURLs.isEmpty else { return }
        pendingURLs.append(contentsOf: fileURLs)
        flushPendingURLs()
    }

    private func flushPendingURLs() {
        guard let controller, let url = pendingURLs.first else { return }
        pendingURLs.removeAll()
        controller.openDocument(at: url.path)
    }
}

@main
struct MarkdownBuddyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var controller = MacDocumentController()

    var body: some Scene {
        WindowGroup {
            ContentView(controller: controller)
                .frame(minWidth: 1100, minHeight: 760)
                .onAppear {
                    appDelegate.attach(controller: controller)
                }
        }
        .commands {
            MarkdownBuddyCommands(controller: controller)
        }

        Settings {
            EmptyView()
        }
    }
}

struct MarkdownBuddyCommands: Commands {
    @ObservedObject var controller: MacDocumentController

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("New") { controller.newDocument() }
                .keyboardShortcut("n")
            Button("Open...") { controller.openDocument() }
                .keyboardShortcut("o")
        }

        CommandGroup(after: .saveItem) {
            Button("Save") { controller.save() }
                .keyboardShortcut("s")
            Button("Save As...") { try? controller.saveAs() }
                .keyboardShortcut("S", modifiers: [.command, .shift])
        }

        CommandGroup(replacing: .appInfo) {
            Button("About Markdown Buddy") { controller.showAbout() }
        }

        CommandMenu("Edit") {
            Button("Bold") { controller.apply(.bold) }
                .keyboardShortcut("b")
            Button("Italic") { controller.apply(.italic) }
                .keyboardShortcut("i")
        }

        CommandGroup(after: .help) {
            Button("Repository ↗") { controller.openRepository() }
        }
    }
}
#endif
