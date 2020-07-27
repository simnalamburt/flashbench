Rust part of flashbench
========

```bash
cargo +nightly build

# You can set nightly as default rust toolchain in this project using rustup
rustup override set nightly
cargo build
```

### TODOs
- [ ] Handle `CONFIG_*` macros properly. Consider adopting bindgen
- [ ] Refactor into safe Rust codes, remove redundant `unsafe` blocks
- [ ] Remove redundant `as`

###### References
- https://doc.rust-lang.org/nightly/cargo/reference/unstable.html#build-std
- https://github.com/rust-lang/rust/blob/3dbade6/src/librustc_target/spec/x86_64_linux_kernel.rs
