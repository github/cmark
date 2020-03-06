// swift-tools-version:5.0
// The swift-tools-version declares the minimum version of Swift required to build this package.
import PackageDescription
import Foundation


extension URL {
    func relativePath(from base: URL) -> String? {
        let destComponents = self.standardized.pathComponents
        let baseComponents = base.standardized.pathComponents

        var i = 0
        while i < destComponents.count && i < baseComponents.count
            && destComponents[i] == baseComponents[i] {
                i += 1
        }

        var relComponents = Array(repeating: "..", count: baseComponents.count - i)
        relComponents.append(contentsOf: destComponents[i...])
        return relComponents.joined(separator: "/")
    }
}

let packagePath = String(#file.dropLast(14))
let workingDir = ProcessInfo.processInfo.environment["PWD"]!
let isDependency = !workingDir.hasPrefix(packagePath)

var headerPathPrefix = ""
if isDependency {
    print("Building cmark_gfm as a dependency")

    let workingURL = URL(fileURLWithPath: workingDir, isDirectory: true)
    let packageURL = URL(fileURLWithPath: packagePath, isDirectory: true)

    headerPathPrefix = packageURL.relativePath(from: workingURL)!
    print(headerPathPrefix)
}

let headerPaths: [CSetting] = ["include", "src", "extensions"].map { path in
    CSetting.headerSearchPath(path)
}


let package = Package(
    name: "libcmark",
    products: [
        .library(
            name: "cmark_gfm",
            targets: ["cmark_gfm"]
        ),
        .library(
            name: "libcmark",
            targets: ["cmark_gfm"]
        ),
    ],
    targets: [
        .target(
            name: "cmark_gfm",
            path: "./",
            exclude: ["src/main.c"],
            sources: [
                "src",
                "extensions"
            ],
            publicHeadersPath: "./include/"
            , cSettings: headerPaths
        )
    ]
)
