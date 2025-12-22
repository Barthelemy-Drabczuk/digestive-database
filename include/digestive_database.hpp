#ifndef DIGESTIVE_DATABASE_HPP
#define DIGESTIVE_DATABASE_HPP

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>
#include <functional>

namespace digestive {

/**
 * Compression tiers based on access frequency
 * Tier 0 = Hottest (no compression)
 * Tier 4 = Coldest (maximum compression)
 */
enum class CompressionTier {
    TIER_0 = 0,  // Root - No compression (>30% of accesses)
    TIER_1 = 1,  // Very light compression (15-30%)
    TIER_2 = 2,  // Light compression (5-15%)
    TIER_3 = 3,  // Medium compression (1-5%)
    TIER_4 = 4   // Heavy compression (<1%)
};

/**
 * Compression algorithm types
 */
enum class CompressionAlgo {
    NONE,           // No compression
    LZ4_FAST,       // Fast lossless (good for text, logs)
    LZ4_HIGH,       // High compression lossless
    ZSTD_FAST,      // Fast ZSTD lossless
    ZSTD_MEDIUM,    // Medium ZSTD lossless
    ZSTD_MAX,       // Maximum ZSTD lossless (level 19)
    // Future lossy options for media files:
    // JPEG_QUALITY_X,  // Lossy JPEG for images
    // WEBP_QUALITY_X,  // Lossy WebP for images
    // H264_QUALITY_X,  // Lossy H.264 for video
};

/**
 * Compression function signature
 * Takes input data and returns compressed data
 */
using CompressionFunc = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

/**
 * Decompression function signature
 * Takes compressed data, original size, and returns decompressed data
 */
using DecompressionFunc = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, size_t)>;

/**
 * Configuration for a compression tier
 */
struct TierConfig {
    CompressionAlgo algorithm;      // Compression algorithm to use
    CompressionFunc compress_fn;    // Custom compression function (optional)
    DecompressionFunc decompress_fn; // Custom decompression function (optional)
    bool allow_lossy;               // Allow lossy compression (for images/video)

    TierConfig();
    TierConfig(CompressionAlgo algo, bool lossy = false);
};

/**
 * Metadata for each key-value pair in the database
 */
struct NodeMetadata {
    uint64_t access_count;       // Number of times accessed
    uint64_t last_access;        // Timestamp of last access
    CompressionTier tier;        // Current compression tier
    CompressionAlgo algorithm;   // Algorithm used for compression
    size_t original_size;        // Size before compression
    size_t compressed_size;      // Size after compression

    NodeMetadata();
};

/**
 * Reorganization strategy
 */
enum class ReorgStrategy {
    MANUAL,         // Only reorganize when explicitly called
    EVERY_N_OPS,    // Reorganize after N operations
    PERIODIC,       // Reorganize every N seconds
    ADAPTIVE        // Smart: reorganize when access pattern changes significantly
};

/**
 * Database configuration
 */
struct DbConfig {
    // Deletion policy
    bool allow_deletion;         // Allow deletion of cold data when needed
    size_t max_size_bytes;       // Maximum database size before cleanup

    // Compression settings
    bool compression_enabled;    // Enable/disable compression
    TierConfig tier_configs[5];  // Configuration for each tier (0-4)

    // Reorganization strategy
    ReorgStrategy reorg_strategy;     // When to trigger reorganization
    size_t reorg_operation_threshold; // For EVERY_N_OPS: operations before reorg
    size_t reorg_time_threshold;      // For PERIODIC: seconds between reorg
    double reorg_change_threshold;    // For ADAPTIVE: % access pattern change

    // Performance tuning
    bool lazy_persistence;       // Delay writes to disk for performance
    size_t write_buffer_size;    // Buffer size before flushing to disk
    bool use_mmap;               // Use memory-mapped files for large data

    DbConfig();
    static DbConfig default_config();
    static DbConfig config_for_images();    // Preset for image storage
    static DbConfig config_for_videos();    // Preset for video storage
    static DbConfig config_for_text();      // Preset for text/logs
};

/**
 * Database statistics
 */
struct DatabaseStats {
    size_t tier0_count;
    size_t tier1_count;
    size_t tier2_count;
    size_t tier3_count;
    size_t tier4_count;
    size_t total_size;
    size_t original_total_size;
    uint64_t total_accesses;
    double compression_ratio;
    size_t operations_since_reorg;

    DatabaseStats();
};

/**
 * Self-organizing frequency-based database with tiered compression
 *
 * Designed for storing large files (images, videos, large text):
 * - Frequently accessed data (hot) -> no or light compression
 * - Rarely accessed data (cold) -> heavy compression (or lossy for media)
 * - Smart reorganization based on configurable strategy
 *
 * Supports both lossless and lossy compression algorithms per tier.
 */
class DigestiveDatabase {
public:
    /**
     * Create or open a database
     * @param name Database name (will create/open name.db directory)
     * @param config Database configuration
     */
    DigestiveDatabase(const std::string& name, const DbConfig& config = DbConfig::default_config());

    ~DigestiveDatabase();

    // ==================== Binary Data API ====================

    /**
     * Insert binary data (images, videos, any file)
     * @param key Unique identifier
     * @param data Binary data
     */
    void insert_binary(const std::string& key, const std::vector<uint8_t>& data);

    /**
     * Insert from file (efficient for large files)
     * @param key Unique identifier
     * @param file_path Path to file
     */
    void insert_from_file(const std::string& key, const std::string& file_path);

    /**
     * Get binary data
     * @return Binary data if found, nullopt otherwise
     */
    std::optional<std::vector<uint8_t>> get_binary(const std::string& key);

    /**
     * Get data and save to file (efficient for large files)
     * @param key Unique identifier
     * @param output_path Where to save the file
     * @return true if successful, false if key not found
     */
    bool get_to_file(const std::string& key, const std::string& output_path);

    // ==================== String Data API (convenience) ====================

    /**
     * Insert string data (text, JSON, XML, etc.)
     */
    void insert(const std::string& key, const std::string& value);

    /**
     * Get string data
     */
    std::optional<std::string> get(const std::string& key);

    // ==================== Database Management ====================

    /**
     * Delete a key-value pair
     * @return true if deleted, false if not found
     */
    bool remove(const std::string& key);

    /**
     * Manually trigger reorganization
     * Recompresses items into appropriate tiers based on access patterns
     */
    void reorganize();

    /**
     * Flush pending writes to disk
     */
    void flush();

    // ==================== Statistics & Monitoring ====================

    /**
     * Get current database statistics
     */
    DatabaseStats get_stats() const;

    /**
     * Print statistics to stdout
     */
    void print_stats() const;

    /**
     * Get current database size on disk
     */
    size_t get_size_on_disk() const;

    /**
     * Get metadata for a specific key (for debugging)
     */
    std::optional<NodeMetadata> get_metadata(const std::string& key) const;

private:
    std::string db_path_;
    DbConfig config_;
    uint64_t total_accesses_;
    size_t operations_since_reorg_;
    uint64_t last_reorg_time_;

    // In-memory cache of data and metadata
    std::map<std::string, std::vector<uint8_t>> data_store_;
    std::map<std::string, NodeMetadata> metadata_store_;

    // Write buffer for lazy persistence
    std::map<std::string, std::vector<uint8_t>> write_buffer_;
    size_t write_buffer_current_size_;

    // Helper methods
    void load_from_disk();
    void save_to_disk();
    void save_metadata();
    void load_metadata();

    // Compression/decompression
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data, CompressionTier tier);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, CompressionAlgo algo, size_t original_size);

    std::vector<uint8_t> compress_with_algo(const std::vector<uint8_t>& data, CompressionAlgo algo);
    std::vector<uint8_t> decompress_with_algo(const std::vector<uint8_t>& data, CompressionAlgo algo, size_t original_size);

    // Tier management
    CompressionTier calculate_tier(uint64_t access_count) const;
    void check_size_limit();
    void delete_coldest_data();

    // Reorganization management
    void check_reorganization_trigger();
    bool should_reorganize() const;
    double calculate_access_pattern_change() const;

    // Utilities
    uint64_t current_timestamp() const;
    void after_operation();  // Called after each insert/get/remove
};

} // namespace digestive

#endif // DIGESTIVE_DATABASE_HPP
