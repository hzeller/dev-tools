load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "symbol-finder",
    srcs = ["symbol-finder.cc"],
    deps = [
        "@llvm-project//clang:ast",
        "@llvm-project//clang:basic",
        "@llvm-project//clang:frontend",
        "@llvm-project//clang:tooling",
        "@llvm-project//llvm:Support",
    ],
)
