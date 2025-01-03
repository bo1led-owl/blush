# blush

A tiny shell written in C

## Building
For debug mode:

    zig build

For release mode:

    zig build -Doptimize=OPT

Where `OPT` is one of Zig's optimization modes: `ReleaseSafe`, `ReleaseSmall` or `ReleaseFast` 
(also `Debug`, but plain `zig build` is more concise)

To enable sanitizers, pass `-Dsanitize` (this is not guaranteed to work)
To enable link-time optimizations, pass `-Dlto`
