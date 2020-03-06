// swift-tools-version:5.0
// The swift-tools-version declares the minimum version of Swift required to build this package.
import PackageDescription

let package = Package(
    name: "cmark_gfm",
    products: [
        .library(
            name: "cmark_gfm",
            targets: ["cmark_gfm"]
        ),
    ],
    targets: [
        .target(
            name: "cmark_gfm",
            path: "./",
            exclude: ["./src/main.c"],
            sources: [
                "./src",
                "./extensions"
            ],
            publicHeadersPath: "./include/",
            cSettings: [
                .headerSearchPath("./include"),
                .headerSearchPath("./src"),
                .headerSearchPath("./extensions")
            ]
        )
    ]
)
