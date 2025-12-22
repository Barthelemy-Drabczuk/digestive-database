# Changelog

All notable changes to the Digestive Database project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0] - 2025-12-22

### Changed
- **BREAKING**: Complete rewrite from Rust to C++
- Migrated entire codebase to C++17 for better maintainability
- Replaced Rust-specific dependencies with C++ equivalents:
  - Sled database → Custom file-based storage
  - Postcard serialization → Binary file I/O
  - Heapless vectors → STL containers

### Added
- Professional C++ class-based architecture
- Comprehensive API documentation in header file
- Makefile build system with helpful targets
- README.md with full documentation and usage examples
- Support for standard C++ features:
  - `std::optional` for safe value retrieval
  - `std::map` for efficient key-value storage
  - `std::filesystem` for cross-platform file operations
- Installation instructions for multiple platforms (Ubuntu, Fedora, macOS)
- Detailed code comments and documentation

### Removed
- All Rust files and dependencies:
  - `src/main.rs`
  - `Cargo.toml`
  - `Cargo.lock`
  - `target/` directory
- Rust-specific toolchain requirements

### Fixed
- Simplified dependency management (only LZ4 and ZSTD needed)
- Improved error messages and user feedback
- More intuitive API without Rust's lifetime complexity

## [1.0.0] - 2025-12-22

### Added
- Initial Rust implementation
- Self-organizing frequency-based tree structure
- 5-tier progressive compression system:
  - Tier 0: No compression (>30% access frequency)
  - Tier 1: Light LZ4 (15-30% frequency)
  - Tier 2: Standard LZ4 (5-15% frequency)
  - Tier 3: Medium ZSTD (1-5% frequency)
  - Tier 4: Heavy ZSTD level 19 (<1% frequency)
- Configurable deletion policy:
  - Optional automatic deletion of coldest 10% when size limit exceeded
  - Warning system when deletion is disabled
- Access frequency tracking:
  - Per-item access counters
  - Last access timestamps
  - Total accesses tracking
- Automatic tier reorganization based on access patterns
- Database statistics reporting:
  - Item distribution across tiers
  - Total compressed size
  - Access patterns
- Persistent storage with metadata preservation
- Database size monitoring and limits

### Features (Preserved in v2.0.0)
- **Frequency-based compression**: Hot data uncompressed, cold data heavily compressed
- **Self-organizing tree**: Items automatically move between tiers
- **ACID-like properties**: Data persisted on every transaction
- **Configurable behavior**: User controls deletion policy and size limits
- **Performance optimization**: No compression overhead for frequently accessed data

## [0.1.0] - 2025-12-22 (Pre-release)

### Added
- Basic BTreeSet-based database (prototype)
- Simple file serialization using postcard
- Initial proof of concept with Rust

### Issues
- Manual serialization/deserialization required
- No compression support
- Memory inefficient (entire database in RAM)
- Data loss on crashes during write
- No access frequency tracking

---

## Migration Guide: Rust → C++

### For Users

**Old (Rust):**
```rust
let config = DbConfig {
    allow_deletion: true,
    max_size_bytes: 1024 * 1024,
    compression_enabled: true,
};
let mut db = DigestiveDatabase::new("test_db".to_string(), config)?;
db.insert("key", "value")?;
let value: String = db.get::<&str, String>("key")?;
```

**New (C++):**
```cpp
DbConfig config;
config.allow_deletion = true;
config.max_size_bytes = 1024 * 1024;
config.compression_enabled = true;

DigestiveDatabase db("test_db", config);
db.insert("key", "value");
auto value = db.get("key");
```

### Compatibility Notes

- Database files created in Rust version are **NOT** compatible with C++ version
- Migration requires re-importing data through the API
- All core features and behavior preserved
- Performance characteristics similar or better

### Build System Changes

**Old (Rust):**
```bash
cargo build
cargo run
```

**New (C++):**
```bash
make
make run
```

---

## Future Roadmap

### Planned Features
- [ ] Thread-safe operations with mutex protection
- [ ] Batch insert/update operations
- [ ] Custom compression algorithm plugins
- [ ] Range queries and iteration support
- [ ] Transaction support with rollback
- [ ] Compression ratio statistics per tier
- [ ] Automatic reorganization triggers
- [ ] Key prefix search
- [ ] Binary value support (not just strings)
- [ ] Memory-mapped file option for large databases
- [ ] Replication and backup utilities

### Under Consideration
- [ ] Network protocol for client-server mode
- [ ] Multiple database instances in one process
- [ ] Custom allocators for performance tuning
- [ ] Asynchronous I/O support
- [ ] Encryption at rest
- [ ] Python/JavaScript bindings

---

## Version History

- **v2.0.0** (Current) - C++ implementation
- **v1.0.0** - Full-featured Rust implementation
- **v0.1.0** - Initial prototype

## Contributing

When contributing, please:
1. Update this CHANGELOG.md with your changes
2. Follow the existing format
3. Add entries under `[Unreleased]` section
4. Include migration notes for breaking changes

## Acknowledgments

- LZ4 compression library by Yann Collet
- Zstandard compression library by Facebook/Meta
- Inspired by modern database storage engines and cache hierarchies
