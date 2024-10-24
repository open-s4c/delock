# Delegation lock

An implicit delegation lock based on MCS lock.

This is a demo implementation and should not be used in production.

## Build instructions

Configure and make

```
cmake -S. -Bbuild
make -C build
```

You'll need `cmake`, `make` and `gcc` or `clang`. Also you'll internet
connection because `cmake` will download `libvsync` and `tilt`. (TODO:
add links).

## Running

There is a simple program built in `build/src/delock_test`.

To preload delock, use the shared library that is generated in the build
process:


```
LD_PRELOAD=build/src/libdelock.so <some-program>
```

Here is an example:


```
LD_PRELOAD=build/src/libdelock.so build/src/delock_test_ld
```
