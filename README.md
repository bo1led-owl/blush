# blush

A tiny shell written in C

## Building
For debug mode:

    zig build

To enable sanitizers, pass `-Dsanitize` (this is not guaranteed to work)

For release mode:

    zig build --release=MODE

Where `mode` is one of: `fast`, `small` or `safe`. To enable link-time optimizations, pass `-Dlto`
