#if os(macOS)
import AppCore
import AppKit
import SwiftUI

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

        CommandGroup(after: .appTermination) {
            Button("Quit") { controller.quit() }
                .keyboardShortcut("q")
        }

        CommandMenu("Edit") {
            Button("Bold") { controller.apply(.bold) }
                .keyboardShortcut("b")
            Button("Italic") { controller.apply(.italic) }
                .keyboardShortcut("i")
        }

        CommandMenu("Help") {
            Button("About") { controller.showAbout() }
            Button("Repository ↗") { controller.openRepository() }
        }
    }
}
#endif
