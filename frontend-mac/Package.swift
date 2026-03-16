// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "MarkdownBuddyMac",
    platforms: [
        .macOS(.v14),
    ],
    products: [
        .library(name: "AppCore", targets: ["AppCore"]),
        .executable(name: "MarkdownBuddyAbiProbe", targets: ["MarkdownBuddyAbiProbe"]),
        .executable(name: "MarkdownBuddyMac", targets: ["MarkdownBuddyMac"]),
    ],
    targets: [
        .target(
            name: "CMarkdownBuddy",
            path: "Sources/CMarkdownBuddy",
            publicHeadersPath: "include"
        ),
        .target(
            name: "AppCore",
            dependencies: ["CMarkdownBuddy"],
            path: "Sources/AppCore",
            linkerSettings: [
                .linkedLibrary("markdown_buddy"),
                .unsafeFlags(["-L", "../dist"]),
            ]
        ),
        .executableTarget(
            name: "MarkdownBuddyAbiProbe",
            dependencies: ["AppCore"],
            path: "Sources/MarkdownBuddyAbiProbe"
        ),
        .executableTarget(
            name: "MarkdownBuddyMac",
            dependencies: ["AppCore"],
            path: "Sources/MarkdownBuddyMac"
        ),
    ]
)
