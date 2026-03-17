#if os(macOS)
import AppCore
import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    weak var controller: MacDocumentController?

    private var pendingURLs: [URL] = []
    private weak var window: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)

        let launchURLs = CommandLine.arguments.dropFirst().map { URL(fileURLWithPath: $0) }
        if !launchURLs.isEmpty {
            handle(urls: launchURLs)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
                self.ensureMainWindowExists()
            }
        } else {
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
                self.ensureMainWindowExists()
            }
        }
    }

    func applicationShouldOpenUntitledFile(_ sender: NSApplication) -> Bool {
        true
    }

    func applicationOpenUntitledFile(_ sender: NSApplication) -> Bool {
        ensureMainWindowExists()
        return true
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        controller?.prepareForTermination() ?? .terminateNow
    }

    func application(_ application: NSApplication, open urls: [URL]) {
        handle(urls: urls)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
            self.ensureMainWindowExists()
        }
    }

    func attach(controller: MacDocumentController) {
        self.controller = controller
        flushPendingURLsIfPossible()
    }

    func attach(window: NSWindow?) {
        guard let window, self.window !== window else { return }
        self.window = window
        window.delegate = self
        flushPendingURLsIfPossible()
    }

    func sceneDidAppear() {
        activateApp()
        flushPendingURLsIfPossible()
    }

    func windowDidBecomeMain(_ notification: Notification) {
        flushPendingURLsIfPossible()
    }

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

    private func handle(urls: [URL]) {
        let fileURLs = urls.filter { $0.isFileURL }
        guard !fileURLs.isEmpty else { return }
        pendingURLs.append(contentsOf: fileURLs)
    }

    private func flushPendingURLsIfPossible() {
        guard let controller, let window, let url = pendingURLs.first else { return }
        pendingURLs.removeAll()
        controller.openDocument(at: url.path)
        activateApp()
        window.makeKeyAndOrderFront(nil)
    }

    private func ensureMainWindowExists() {
        if NSApp.windows.isEmpty {
            triggerNewWindowMenuItem()
        } else {
            activateApp()
            window?.makeKeyAndOrderFront(nil)
            flushPendingURLsIfPossible()
        }
    }

    private func triggerNewWindowMenuItem() {
        guard
            let mainMenu = NSApp.mainMenu,
            let fileMenuItem = mainMenu.items.first(where: { $0.title == "File" }),
            let fileMenu = fileMenuItem.submenu,
            let newWindowItem = fileMenu.items.first(where: { $0.title == "New Window" }),
            let action = newWindowItem.action
        else {
            return
        }

        NSApp.sendAction(action, to: newWindowItem.target, from: nil)
    }

    func activateApp() {
        NSApp.activate(ignoringOtherApps: true)
        NSRunningApplication.current.activate(options: [.activateAllWindows])
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
        WindowGroup(id: "main") {
            ContentView(controller: controller)
                .frame(minWidth: 1100, minHeight: 760)
                .background(WindowAccessor { window in
                    appDelegate.attach(window: window)
                })
                .onAppear {
                    appDelegate.attach(controller: controller)
                    appDelegate.sceneDidAppear()
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
    @Environment(\.openWindow) private var openWindow
    @ObservedObject var controller: MacDocumentController

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("New") { controller.newDocument() }
                .keyboardShortcut("n")
            Button("Open...") { controller.openDocument() }
                .keyboardShortcut("o")
        }

        CommandGroup(after: .newItem) {
            Button("New Window") { openWindow(id: "main") }
                .keyboardShortcut("N", modifiers: [.command, .shift])
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
