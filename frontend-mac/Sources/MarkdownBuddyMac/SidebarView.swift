#if os(macOS)
import AppCore
import SwiftUI

struct SidebarView: View {
    @ObservedObject var controller: MacDocumentController

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header("Sections")
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
#endif
