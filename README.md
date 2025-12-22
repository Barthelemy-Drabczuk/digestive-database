# Digestive Database - Hybrid Storage System

A high-performance, adaptive database system with **hybrid architecture** combining frequency-based tiered compression, chunked file storage, SQL queries, and adaptive indexes. Optimized for embedded systems (robots, drones, CCTV cameras) and edge computing.

## Overview

The Digestive Database automatically adjusts compression levels based on access frequency:
- **Hot data** (frequently accessed) → no or light compression for fast access
- **Cold data** (rarely accessed) → heavy compression to save space

The database uses a **5-tier progressive compression system** with **4 smart reorganization strategies** to avoid performance issues from constant reorganization.

### 🆕 Hybrid Architecture (NEW!)

The system now features a **hybrid architecture** with optional engines:

- **Chunked File Storage**: Handle 100MB+ files with per-chunk heat tracking and partial access
- **SQL Engine**: Full SQL support with CREATE, INSERT, SELECT, DELETE, WHERE clauses
- **Index Engine**: O(1) hash indexes and O(log n) ordered indexes with heat tracking
- **Heat Decay**: Time-based cooling strategies for long-term storage optimization

All features are **optional** and can be enabled via configuration flags.

## Key Features

### 🎯 Smart Reorganization (Solves the Performance Problem)

Instead of reorganizing after every transaction, choose your strategy:

1. **MANUAL**: Only reorganize when you explicitly call `db.reorganize()`
2. **EVERY_N_OPS**: Reorganize after N insert/get/remove operations
3. **PERIODIC**: Reorganize every N seconds
4. **ADAPTIVE**: Smart reorganization when access patterns change significantly (recommended)

### 🗜️ Configurable Compression Per Tier

You control the compression algorithm for each tier:

- **Tier 0** (Hottest): No compression (default) - instant access
- **Tier 1**: LZ4 Fast (default) - very fast lossless
- **Tier 2**: LZ4 High (default) - high compression lossless
- **Tier 3**: ZSTD Medium (default) - balanced lossless
- **Tier 4** (Coldest): ZSTD Max level 19 (default) - maximum lossless

**Supported algorithms:**
- `NONE` - No compression
- `LZ4_FAST` - Fast lossless (good for text, logs)
- `LZ4_HIGH` - High compression lossless
- `ZSTD_FAST` - Fast ZSTD lossless
- `ZSTD_MEDIUM` - Medium ZSTD lossless
- `ZSTD_MAX` - Maximum ZSTD lossless (level 19)
- Future: Lossy compression for images/video (JPEG, WebP, H.264)

### 📦 Large File Support

Designed for storing **images, videos, and large text files**:

```cpp
// Store a 5GB video file
db.insert_from_file("movie_2024", "/path/to/video.mp4");

// Retrieve to disk (efficient for large files)
db.get_to_file("movie_2024", "/output/video.mp4");

// Or work with binary data directly
std::vector<uint8_t> image_data = load_image("photo.jpg");
db.insert_binary("profile_pic", image_data);
auto retrieved = db.get_binary("profile_pic");
```

### 📊 Preset Configurations

Use optimized presets for different data types:

```cpp
// For images (PNG, JPEG, etc.)
DbConfig config = DbConfig::config_for_images();

// For videos (MP4, AVI, etc.)
DbConfig config = DbConfig::config_for_videos();

// For text/logs
DbConfig config = DbConfig::config_for_text();

// 🆕 For embedded systems (robots, drones)
DbConfig config = DbConfig::config_for_embedded();
// - Small chunks (256KB)
// - Time-based heat decay
// - No SQL/indexes (saves memory)

// 🆕 For CCTV systems
DbConfig config = DbConfig::config_for_cctv();
// - Large chunks (4MB = ~1 sec of video)
// - SQL support for metadata queries
// - Indexes for camera/timestamp lookups
// - Exponential heat decay
```

## Installation

### Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y liblz4-dev libzstd-dev g++ make

# Fedora/RHEL
sudo dnf install lz4-devel libzstd-devel gcc-c++ make

# macOS (Homebrew)
brew install lz4 zstd
```

### Building

```bash
make          # Build the project
make run      # Build and run examples
make clean    # Clean build files
```

## Quick Start

### Example 1: CCTV System with SQL Queries (NEW!)

```cpp
#include "digestive_database.hpp"
#include "sql_engine.hpp"
using namespace digestive;

int main() {
    // Use CCTV-optimized config with SQL and indexes
    DbConfig config = DbConfig::config_for_cctv();
    DigestiveDatabase db("cctv_db", config);

    // Create table for video metadata
    db.execute_sql(
        "CREATE TABLE videos ("
        "id INTEGER PRIMARY KEY, "
        "filename TEXT, "
        "camera_id INTEGER, "
        "timestamp TEXT)"
    );

    // Create index on camera_id for fast queries
    db.create_index("videos", "camera_id");

    // Insert video metadata
    db.execute_sql("INSERT INTO videos VALUES (1, 'vid1.mp4', 1, '2024-12-22 10:00')");
    db.execute_sql("INSERT INTO videos VALUES (2, 'vid2.mp4', 1, '2024-12-22 10:01')");

    // Store actual video files (automatically chunked if > 1MB)
    std::vector<uint8_t> video_data(5 * 1024 * 1024);  // 5MB video
    db.insert_binary("vid1_data", video_data);

    // Query videos by camera (uses index!)
    auto result = db.execute_sql("SELECT * FROM videos WHERE camera_id = 1");

    // Access specific chunk range (e.g., 2 seconds at offset 10s)
    auto chunk_data = db.get_chunk_range("vid1_data", 10, 12);

    return 0;
}
```

### Example 2: Image Storage with Auto-Reorganization

```cpp
#include "digestive_database.hpp"
using namespace digestive;

int main() {
    // Use image-optimized config
    DbConfig config = DbConfig::config_for_images();
    config.reorg_strategy = ReorgStrategy::EVERY_N_OPS;
    config.reorg_operation_threshold = 500;  // Reorganize every 500 operations

    DigestiveDatabase db("photo_library", config);

    // Store images
    db.insert_from_file("photo1", "vacation.jpg");
    db.insert_from_file("photo2", "family.jpg");

    // Access frequently viewed photos (becomes hot, uncompressed)
    for (int i = 0; i < 100; i++) {
        db.get_binary("photo1");  // Profile picture
    }

    // Automatically reorganizes after 500 operations
    // photo1 moves to Tier 0 (uncompressed) for instant access
    // photo2 stays in Tier 4 (heavily compressed) to save space

    db.print_stats();
    return 0;
}
```

### Example 2: Custom Compression Configuration

```cpp
DbConfig config;

// Customize each tier
config.tier_configs[0] = TierConfig(CompressionAlgo::NONE, false);
config.tier_configs[1] = TierConfig(CompressionAlgo::LZ4_FAST, false);
config.tier_configs[2] = TierConfig(CompressionAlgo::LZ4_HIGH, false);
config.tier_configs[3] = TierConfig(CompressionAlgo::ZSTD_MEDIUM, false);
config.tier_configs[4] = TierConfig(CompressionAlgo::ZSTD_MAX, false);

// Reorganization strategy
config.reorg_strategy = ReorgStrategy::ADAPTIVE;
config.reorg_change_threshold = 0.2;  // Reorganize when 20% change in access pattern

DigestiveDatabase db("my_db", config);
```

### Example 3: Chunked File with Partial Access (NEW!)

```cpp
DbConfig config;
config.enable_chunking = true;
config.chunk_size = 4 * 1024 * 1024;  // 4MB chunks

DigestiveDatabase db("large_files_db", config);

// Insert 100MB video file
std::vector<uint8_t> video(100 * 1024 * 1024);
db.insert_binary("movie", video);

// Access only chunks 10-20 (40MB from middle)
// WITHOUT loading the full 100MB file!
auto partial = db.get_chunk_range("movie", 10, 20);

// Check if file is chunked
if (db.is_chunked("movie")) {
    std::cout << "File stored as chunks" << std::endl;
}
```

### Example 4: Heat Decay for Long-Term Storage (NEW!)

```cpp
DbConfig config;
config.enable_heat_decay = true;
config.heat_decay_strategy = HeatDecayStrategy::EXPONENTIAL;
config.heat_decay_factor = 0.95;  // 5% decay per hour
config.heat_decay_interval = 3600;  // Every hour

DigestiveDatabase db("archive_db", config);

// Old data automatically cools down
// Hot data: TIER_0 (no compression)
// Cold data: TIER_4 (maximum compression)
```

### Example 5: Manual Control (No Auto-Reorganization)

```cpp
DbConfig config;
config.reorg_strategy = ReorgStrategy::MANUAL;  // No automatic reorganization

DigestiveDatabase db("controlled_db", config);

// Insert lots of data
for (int i = 0; i < 10000; i++) {
    db.insert("key_" + std::to_string(i), data);
}

// No performance impact during high-throughput operations

// Later, when convenient, manually reorganize
db.reorganize();  // Recompresses all data based on access patterns
```

## API Reference

### Database Creation

```cpp
DigestiveDatabase(const std::string& name, const DbConfig& config);
```

### Binary Data Operations

```cpp
// Insert binary data (images, videos, files)
void insert_binary(const std::string& key, const std::vector<uint8_t>& data);

// Insert from file (efficient for large files)
void insert_from_file(const std::string& key, const std::string& file_path);

// Get binary data
std::optional<std::vector<uint8_t>> get_binary(const std::string& key);

// Get to file (efficient for large files)
bool get_to_file(const std::string& key, const std::string& output_path);
```

### String Data Operations (Convenience)

```cpp
void insert(const std::string& key, const std::string& value);
std::optional<std::string> get(const std::string& key);
```

### Database Management

```cpp
bool remove(const std::string& key);           // Delete key-value pair
void reorganize();                             // Manually trigger reorganization
void flush();                                  // Flush pending writes to disk
```

### 🆕 Chunked File Operations (NEW!)

```cpp
// Get chunk range (only if chunking enabled)
std::optional<std::vector<uint8_t>> get_chunk_range(
    const std::string& key,
    uint32_t start_chunk,
    uint32_t end_chunk
);

// Check if file is chunked
bool is_chunked(const std::string& key) const;
```

### 🆕 SQL Operations (NEW!)

```cpp
// Execute SQL query
ResultSet execute_sql(const std::string& sql);

// Create index on table column
void create_index(const std::string& table, const std::string& column);

// Supported SQL commands:
// - CREATE TABLE table_name (col1 TYPE, col2 TYPE, ...)
// - INSERT INTO table_name VALUES (val1, val2, ...)
// - SELECT * FROM table_name [WHERE column = value]
// - DROP TABLE table_name
```

### Statistics

```cpp
DatabaseStats get_stats() const;               // Get statistics
void print_stats() const;                      // Print statistics to stdout
size_t get_size_on_disk() const;              // Get database size
std::optional<NodeMetadata> get_metadata(const std::string& key) const;
```

## Configuration Options

### DbConfig Structure

```cpp
struct DbConfig {
    // Deletion policy
    bool allow_deletion;              // Allow deletion of cold data
    size_t max_size_bytes;            // Maximum database size before cleanup

    // Compression
    bool compression_enabled;         // Enable/disable compression
    TierConfig tier_configs[5];       // Per-tier compression algorithms

    // Reorganization strategy (CRITICAL for performance!)
    ReorgStrategy reorg_strategy;     // MANUAL, EVERY_N_OPS, PERIODIC, or ADAPTIVE
    size_t reorg_operation_threshold; // For EVERY_N_OPS strategy
    size_t reorg_time_threshold;      // For PERIODIC strategy (seconds)
    double reorg_change_threshold;    // For ADAPTIVE strategy (0.0-1.0)

    // Performance tuning
    bool lazy_persistence;            // Delay writes to disk
    size_t write_buffer_size;         // Buffer size before flushing
    bool use_mmap;                    // Use memory-mapped files (for large data)

    // 🆕 HYBRID SYSTEM: Optional features
    bool enable_chunking;             // Enable chunked file storage
    size_t chunking_threshold;        // Files larger than this are chunked
    size_t chunk_size;                // Size of each chunk

    bool enable_heat_decay;           // Enable automatic heat decay
    HeatDecayStrategy heat_decay_strategy;  // NONE, EXPONENTIAL, LINEAR, TIME_BASED
    double heat_decay_factor;         // Decay factor (for EXPONENTIAL)
    double heat_decay_amount;         // Decay amount (for LINEAR)
    size_t heat_decay_interval;       // Decay interval in seconds

    bool enable_indexes;              // Enable index engine
    bool enable_sql;                  // Enable SQL engine
};
```

### Reorganization Strategies Explained

| Strategy | When to Use | Triggers When | Best For |
|----------|-------------|---------------|----------|
| **MANUAL** | You control timing | Never (call `reorganize()` manually) | Batch processing, low-latency requirements |
| **EVERY_N_OPS** | Predictable intervals | After N insert/get/remove operations | Steady workloads, image galleries |
| **PERIODIC** | Time-based | Every N seconds | Long-running services, video storage |
| **ADAPTIVE** | Smart, automatic | Access pattern changes significantly | General purpose, unpredictable workloads |

### Preset Configurations

**For Images:**
```cpp
DbConfig::config_for_images()
// - 10GB size limit
// - EVERY_N_OPS strategy (500 operations)
// - Moderate compression (images already compressed)
// - Lazy persistence for performance
```

**For Videos:**
```cpp
DbConfig::config_for_videos()
// - 100GB size limit
// - PERIODIC strategy (1 hour)
// - Light compression only (videos already compressed)
// - Memory-mapped files for large data
```

**For Text/Logs:**
```cpp
DbConfig::config_for_text()
// - No size limit
// - ADAPTIVE strategy
// - Maximum compression (text compresses very well)
// - Immediate persistence (small data)
```

**🆕 For Embedded Systems:**
```cpp
DbConfig::config_for_embedded()
// - 100MB size limit
// - Small chunks (256KB)
// - Time-based heat decay
// - No SQL/indexes (memory efficient)
// - Manual reorganization (user control)
```

**🆕 For CCTV Systems:**
```cpp
DbConfig::config_for_cctv()
// - 100GB size limit
// - Large chunks (4MB = ~1 sec HD video)
// - SQL support for metadata queries
// - Indexes for camera_id/timestamp
// - Exponential heat decay
// - Periodic reorganization (1 hour)
```

## Performance Characteristics

### Access Times

- **Tier 0 (Hot)**: O(1) - No decompression, instant
- **Tier 1 (Warm)**: O(1) + ~0.1ms decompression (LZ4 fast)
- **Tier 2 (Medium)**: O(1) + ~0.2ms decompression (LZ4 high)
- **Tier 3 (Cool)**: O(1) + ~1ms decompression (ZSTD medium)
- **Tier 4 (Cold)**: O(1) + ~5ms decompression (ZSTD max)

### Reorganization Cost

- **MANUAL**: Zero overhead during operations, O(n) when called
- **EVERY_N_OPS**: Amortized O(1), spike every N operations
- **PERIODIC**: Zero overhead during operations, O(n) every T seconds
- **ADAPTIVE**: Very low overhead, smart triggering

### Space Savings

Depends on data type and access patterns:
- **Text files**: 50-90% reduction (ZSTD max on cold data)
- **Images (JPEG/PNG)**: 10-30% reduction (already compressed)
- **Videos**: 0-10% reduction (already compressed, use light compression only)
- **Logs/JSON**: 60-95% reduction (excellent compression)

## Advanced Usage

### Custom Compression Functions

```cpp
DbConfig config;

// Custom compression function for a tier
config.tier_configs[4].compress_fn = [](const std::vector<uint8_t>& data) {
    // Your custom compression logic here
    return compressed_data;
};

config.tier_configs[4].decompress_fn = [](const std::vector<uint8_t>& data, size_t original_size) {
    // Your custom decompression logic here
    return decompressed_data;
};
```

### Monitoring Access Patterns

```cpp
// Get metadata for a specific key
auto metadata = db.get_metadata("my_key");
if (metadata.has_value()) {
    std::cout << "Access count: " << metadata->access_count << std::endl;
    std::cout << "Tier: " << static_cast<int>(metadata->tier) << std::endl;
    std::cout << "Algorithm: " << static_cast<int>(metadata->algorithm) << std::endl;
    std::cout << "Compression ratio: "
              << (double)metadata->original_size / metadata->compressed_size
              << "x" << std::endl;
}
```

## Project Structure

```
digestive-database/
├── include/
│   ├── digestive_database.hpp    # Main API and class definitions
│   ├── chunking_engine.hpp       # 🆕 Chunked file storage
│   ├── index_engine.hpp          # 🆕 Index support
│   └── sql_engine.hpp            # 🆕 SQL query engine
├── src/
│   ├── digestive_database.cpp    # Core implementation
│   ├── chunking_engine.cpp       # 🆕 Chunking implementation (430 lines)
│   ├── index_engine.cpp          # 🆕 Index implementation (480 lines)
│   ├── sql_engine.cpp            # 🆕 SQL implementation (560 lines)
│   └── main.cpp                  # Basic examples
├── examples/
│   ├── hybrid_demo.cpp           # 🆕 Comprehensive hybrid demo
│   └── Makefile                  # Example build system
├── docs/
│   ├── HYBRID_ARCHITECTURE.md    # 🆕 Architecture documentation
│   ├── IMPLEMENTATION_STATUS.md  # 🆕 Implementation status
│   └── COMPLETION_GUIDE.md       # 🆕 Integration guide
├── Makefile                       # Build system
├── README.md                      # This file
├── CHANGELOG.md                   # Version history
└── .gitignore                     # Git ignore rules
```

## Comparison with Other Solutions

| Feature | Digestive DB | Redis | MongoDB | SQLite | File System |
|---------|--------------|-------|---------|--------|-------------|
| Auto compression | ✅ Per-tier | ❌ | ❌ | ❌ | ❌ |
| Access-based optimization | ✅ | ❌ | ❌ | ❌ | ❌ |
| Large file support | ✅ Chunked | Limited | ✅ GridFS | ❌ | ✅ |
| Custom compression | ✅ | ❌ | ❌ | ❌ | ❌ |
| Embedded (no server) | ✅ | ❌ | ❌ | ✅ | ✅ |
| Smart reorganization | ✅ 4 strategies | ❌ | Limited | ❌ | ❌ |
| SQL queries | ✅ Basic | ❌ | ✅ | ✅ Full | ❌ |
| Indexes | ✅ Hash/Ordered | ✅ | ✅ | ✅ | ❌ |
| Heat decay | ✅ 4 strategies | ❌ | ❌ | ❌ | ❌ |
| Partial file access | ✅ Chunks | ❌ | ✅ GridFS | ❌ | Limited |

## FAQ

**Q: Will reorganization slow down my inserts?**
A: Use `ReorgStrategy::MANUAL` for zero overhead, or `ADAPTIVE` for smart triggering only when needed.

**Q: How do I store a 50GB video file?**
A: Use `insert_from_file()` and `get_to_file()` for efficient handling of large files without loading everything into memory.

**Q: Can I use lossy compression for images?**
A: The framework supports it (see `allow_lossy` flag), but lossy algorithms (JPEG, WebP) will be added in future versions. Currently only lossless compression is implemented.

**Q: What happens when the database exceeds the size limit?**
A: If `allow_deletion = true`, the coldest 10% of items are automatically deleted. If `false`, you'll get a warning but no data is lost.

**Q: Which reorganization strategy should I use?**
A: Start with `ADAPTIVE` (default) for most use cases. Use `MANUAL` for maximum control and zero overhead.

## License

This project is provided as-is for educational and personal use.

## Contributing

Contributions welcome! Please:
1. Update CHANGELOG.md with your changes
2. Add tests for new features
3. Follow the existing code style

## Acknowledgments

- LZ4 compression library by Yann Collet
- Zstandard compression library by Facebook/Meta
- Inspired by modern database storage engines and hierarchical caching systems
