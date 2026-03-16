#if os(macOS)
import AppCore
import SwiftUI

@main
struct MarkdownBuddyApp: App {
    @StateObject private var controller = MacDocumentController()

    var body: some Scene {
        WindowGroup {
            ContentView(controller: controller)
                .frame(minWidth: 1100, minHeight: 760)
        }
        .commands {
            MarkdownBuddyCommands(controller: controller)
        }

        Settings {
            EmptyView()
        }
    }
}
#endif
