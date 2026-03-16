#if os(macOS)
import AppCore
import AppKit
import SwiftUI

struct ContentView: View {
    @ObservedObject var controller: MacDocumentController

    var body: some View {
        NavigationSplitView {
            SidebarView(controller: controller)
                .navigationSplitViewColumnWidth(min: 220, ideal: 240, max: 280)
        } detail: {
            HSplitView {
                EditorView(controller: controller)
                PreviewView(document: controller.document)
            }
            .padding(12)
            .background(
                LinearGradient(colors: [Color(red: 0.10, green: 0.14, blue: 0.20), Color(red: 0.07, green: 0.10, blue: 0.14)], startPoint: .top, endPoint: .bottom)
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
}
#endif
