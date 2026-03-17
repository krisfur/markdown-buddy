#if os(macOS)
import AppCore
import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var controller: MacDocumentController?
    private var pendingURLs: [URL] = []
    private weak var window: NSWindow?

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        activateApp()

        let launchURLs = CommandLine.arguments.dropFirst().map { URL(fileURLWithPath: $0) }
        if !launchURLs.isEmpty {
            handle(urls: launchURLs)
        }
    }

    func application(_ application: NSApplication, open urls: [URL]) {
        handle(urls: urls)
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        controller?.prepareForTermination() ?? .terminateNow
    }

    func attach(controller: MacDocumentController) {
        self.controller = controller
        flushPendingURLs()
    }

    func attach(window: NSWindow?) {
        guard let window, self.window !== window else { return }
        self.window = window
        window.delegate = self
        window.collectionBehavior.remove(.transient)
        window.level = .normal
        activateApp()
        DispatchQueue.main.async {
            self.activateApp()
            window.orderFrontRegardless()
            window.makeKeyAndOrderFront(nil)
        }
    }

    private func handle(urls: [URL]) {
        let fileURLs = urls.filter { $0.isFileURL }
        guard !fileURLs.isEmpty else { return }
        pendingURLs.append(contentsOf: fileURLs)
        activateApp()
        flushPendingURLs()
    }

    private func flushPendingURLs() {
        guard let controller, let url = pendingURLs.first else { return }
        pendingURLs.removeAll()
        controller.openDocument(at: url.path)
    }

    private func activateApp() {
        NSApp.activate(ignoringOtherApps: true)
        NSRunningApplication.current.activate(options: [.activateAllWindows])
    }
}

extension AppDelegate: NSWindowDelegate {
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        guard let controller else { return true }
        switch controller.prepareForTermination() {
        case .terminateNow:
            NSApp.terminate(nil)
        case .terminateLater, .terminateCancel:
            break
        @unknown default:
            break
        }
        return false
    }
}

private struct WindowAccessor: NSViewRepresentable {
    let onResolve: (NSWindow?) -> Void

    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        DispatchQueue.main.async {
            onResolve(view.window)
        }
        return view
    }

    func updateNSView(_ nsView: NSView, context: Context) {
        DispatchQueue.main.async {
            onResolve(nsView.window)
        }
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
                .background(WindowAccessor { window in
                    appDelegate.attach(window: window)
                })
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

        CommandGroup(replacing: .help) {
        }

        CommandGroup(after: .help) {
            Button("Repository ↗") { controller.openRepository() }
        }
    }
}
#endif
