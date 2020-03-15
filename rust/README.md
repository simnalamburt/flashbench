Rust part of flashbench
========

```bash
# Use nightly as default toolchain in this directory
rustup override set nightly

# xbuild is alias of `build -Z build-std=core`
cargo xbuild
```

### TODOs
- [ ] Handle `CONFIG_*` macros properly. Consider adopting bindgen
- [ ] Refactor into safe Rust codes

###### References
- https://doc.rust-lang.org/nightly/cargo/reference/unstable.html#build-std
- https://github.com/rust-lang/rust/blob/3dbade6/src/librustc_target/spec/x86_64_linux_kernel.rs
