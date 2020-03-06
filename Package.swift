// swift-tools-version:5.0
// The swift-tools-version declares the minimum version of Swift required to build this package.
import PackageDescription

let package = Package(
  name: "cmark_gfm",
  products: [
    .library(
      name: "cmark_gfm",
      targets: ["cmark_gfm"]),
  ],
  targets: [
    .target(
      name: "cmark_gfm",
      path: "./",
      // Exclude the main file so cmark is built as a library.
      exclude: ["./src/main.c"],
	  sources: ["./src/", "./extensions/"],
	  cSettings: [
		  .headerSearchPath("./src"),
		  .headerSearchPath("./extensions")
	  ]
    )
  ]
)