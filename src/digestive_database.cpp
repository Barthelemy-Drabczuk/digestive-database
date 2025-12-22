#include "digestive_database.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <cmath>
#include <lz4.h>
#include <lz4hc.h>
#include <zstd.h>

namespace fs = std::filesystem;

namespace digestive {

// ==================== TierConfig ====================

TierConfig::TierConfig()
    : algorithm(CompressionAlgo::NONE)
    , allow_lossy(false) {
}

TierConfig::TierConfig(CompressionAlgo algo, bool lossy)
    : algorithm(algo)
    , allow_lossy(lossy) {
}

// ==================== NodeMetadata ====================

NodeMetadata::NodeMetadata()
    : access_count(0)
    , last_access(0)
    , tier(CompressionTier::TIER_4)
    , algorithm(CompressionAlgo::ZSTD_MAX)
    , original_size(0)
    , compressed_size(0) {
}

// ==================== DbConfig ====================

DbConfig::DbConfig()
    : allow_deletion(false)
    , max_size_bytes(SIZE_MAX)
    , compression_enabled(true)
    , reorg_strategy(ReorgStrategy::ADAPTIVE)
    , reorg_operation_threshold(100)
    , reorg_time_threshold(300)  // 5 minutes
    , reorg_change_threshold(0.2)  // 20% change
    , lazy_persistence(false)
    , write_buffer_size(10 * 1024 * 1024)  // 10MB
    , use_mmap(false) {

    // Default tier configurations (lossless only)
    tier_configs[0] = TierConfig(CompressionAlgo::NONE, false);
    tier_configs[1] = TierConfig(CompressionAlgo::LZ4_FAST, false);
    tier_configs[2] = TierConfig(CompressionAlgo::LZ4_HIGH, false);
    tier_configs[3] = TierConfig(CompressionAlgo::ZSTD_MEDIUM, false);
    tier_configs[4] = TierConfig(CompressionAlgo::ZSTD_MAX, false);
}

DbConfig DbConfig::default_config() {
    return DbConfig();
}

DbConfig DbConfig::config_for_images() {
    DbConfig config;
    config.allow_deletion = true;
    config.max_size_bytes = 10ULL * 1024 * 1024 * 1024;  // 10GB
    config.reorg_strategy = ReorgStrategy::EVERY_N_OPS;
    config.reorg_operation_threshold = 500;
    config.lazy_persistence = true;

    // Images benefit from moderate compression on cold data
    config.tier_configs[0] = TierConfig(CompressionAlgo::NONE, false);
    config.tier_configs[1] = TierConfig(CompressionAlgo::NONE, false);  // Hot images uncompressed
    config.tier_configs[2] = TierConfig(CompressionAlgo::LZ4_FAST, false);
    config.tier_configs[3] = TierConfig(CompressionAlgo::ZSTD_FAST, false);
    config.tier_configs[4] = TierConfig(CompressionAlgo::ZSTD_MEDIUM, false);  // Not max - already compressed

    return config;
}

DbConfig DbConfig::config_for_videos() {
    DbConfig config;
    config.allow_deletion = true;
    config.max_size_bytes = 100ULL * 1024 * 1024 * 1024;  // 100GB
    config.reorg_strategy = ReorgStrategy::PERIODIC;
    config.reorg_time_threshold = 3600;  // 1 hour
    config.lazy_persistence = true;
    config.use_mmap = true;  // Better for large files

    // Videos are already compressed, don't recompress much
    config.tier_configs[0] = TierConfig(CompressionAlgo::NONE, false);
    config.tier_configs[1] = TierConfig(CompressionAlgo::NONE, false);
    config.tier_configs[2] = TierConfig(CompressionAlgo::NONE, false);
    config.tier_configs[3] = TierConfig(CompressionAlgo::LZ4_FAST, false);  // Light only
    config.tier_configs[4] = TierConfig(CompressionAlgo::LZ4_FAST, false);  // Don't use ZSTD on video

    return config;
}

DbConfig DbConfig::config_for_text() {
    DbConfig config;
    config.allow_deletion = false;
    config.max_size_bytes = SIZE_MAX;
    config.reorg_strategy = ReorgStrategy::ADAPTIVE;
    config.lazy_persistence = false;  // Text is small, persist immediately

    // Text compresses very well
    config.tier_configs[0] = TierConfig(CompressionAlgo::NONE, false);
    config.tier_configs[1] = TierConfig(CompressionAlgo::LZ4_FAST, false);
    config.tier_configs[2] = TierConfig(CompressionAlgo::LZ4_HIGH, false);
    config.tier_configs[3] = TierConfig(CompressionAlgo::ZSTD_MEDIUM, false);
    config.tier_configs[4] = TierConfig(CompressionAlgo::ZSTD_MAX, false);  // Maximum compression for cold text

    return config;
}

// ==================== DatabaseStats ====================

DatabaseStats::DatabaseStats()
    : tier0_count(0)
    , tier1_count(0)
    , tier2_count(0)
    , tier3_count(0)
    , tier4_count(0)
    , total_size(0)
    , original_total_size(0)
    , total_accesses(0)
    , compression_ratio(1.0)
    , operations_since_reorg(0) {
}

// ==================== DigestiveDatabase ====================

DigestiveDatabase::DigestiveDatabase(const std::string& name, const DbConfig& config)
    : db_path_(name + ".db")
    , config_(config)
    , total_accesses_(0)
    , operations_since_reorg_(0)
    , last_reorg_time_(current_timestamp())
    , write_buffer_current_size_(0) {

    // Create database directory if it doesn't exist
    if (!fs::exists(db_path_)) {
        fs::create_directory(db_path_);
    }

    // Warn if deletion is disabled but size limit is set
    if (!config_.allow_deletion && config_.max_size_bytes != SIZE_MAX) {
        std::cout << "⚠️  WARNING: Deletion is disabled but size limit is set. "
                  << "Database may exceed size limit!" << std::endl;
    }

    // Load existing data
    load_from_disk();
    load_metadata();
}

DigestiveDatabase::~DigestiveDatabase() {
    flush();  // Flush any pending writes
    save_to_disk();
    save_metadata();
}

// ==================== Binary Data API ====================

void DigestiveDatabase::insert_binary(const std::string& key, const std::vector<uint8_t>& data) {
    // Start with coldest tier for new data
    CompressionTier tier = CompressionTier::TIER_4;
    CompressionAlgo algo = config_.tier_configs[static_cast<int>(tier)].algorithm;

    // Compress if enabled
    std::vector<uint8_t> compressed = config_.compression_enabled
        ? compress(data, tier)
        : data;

    // Create metadata
    NodeMetadata metadata;
    metadata.access_count = 0;
    metadata.last_access = current_timestamp();
    metadata.tier = tier;
    metadata.algorithm = algo;
    metadata.original_size = data.size();
    metadata.compressed_size = compressed.size();

    // Store
    if (config_.lazy_persistence) {
        write_buffer_[key] = compressed;
        write_buffer_current_size_ += compressed.size();

        if (write_buffer_current_size_ >= config_.write_buffer_size) {
            flush();
        }
    } else {
        data_store_[key] = compressed;
    }

    metadata_store_[key] = metadata;

    // Check size limit
    check_size_limit();

    after_operation();
}

void DigestiveDatabase::insert_from_file(const std::string& key, const std::string& file_path) {
    // Read file into memory
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file: " << file_path << std::endl;
        return;
    }

    insert_binary(key, buffer);
}

std::optional<std::vector<uint8_t>> DigestiveDatabase::get_binary(const std::string& key) {
    // Check write buffer first
    auto buffer_it = write_buffer_.find(key);
    if (buffer_it != write_buffer_.end()) {
        flush();  // Flush to data store
    }

    // Check if key exists
    auto data_it = data_store_.find(key);
    if (data_it == data_store_.end()) {
        return std::nullopt;
    }

    // Update metadata
    auto meta_it = metadata_store_.find(key);
    if (meta_it != metadata_store_.end()) {
        meta_it->second.access_count++;
        meta_it->second.last_access = current_timestamp();
        total_accesses_++;
    }

    // Get compressed data
    const auto& compressed = data_it->second;
    const auto& metadata = meta_it->second;

    // Decompress if needed
    std::vector<uint8_t> decompressed = config_.compression_enabled
        ? decompress(compressed, metadata.algorithm, metadata.original_size)
        : compressed;

    after_operation();
    return decompressed;
}

bool DigestiveDatabase::get_to_file(const std::string& key, const std::string& output_path) {
    auto data = get_binary(key);
    if (!data.has_value()) {
        return false;
    }

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data->data()), data->size());
    return true;
}

// ==================== String Data API ====================

void DigestiveDatabase::insert(const std::string& key, const std::string& value) {
    std::vector<uint8_t> data(value.begin(), value.end());
    insert_binary(key, data);
}

std::optional<std::string> DigestiveDatabase::get(const std::string& key) {
    auto data = get_binary(key);
    if (!data.has_value()) {
        return std::nullopt;
    }
    return std::string(data->begin(), data->end());
}

// ==================== Database Management ====================

bool DigestiveDatabase::remove(const std::string& key) {
    bool found = data_store_.erase(key) > 0;
    metadata_store_.erase(key);
    write_buffer_.erase(key);

    after_operation();
    return found;
}

void DigestiveDatabase::reorganize() {
    std::cout << "Starting reorganization..." << std::endl;
    int recompression_count = 0;

    // Save current access pattern for future comparison
    std::map<std::string, uint64_t> old_access_counts;
    for (const auto& [key, metadata] : metadata_store_) {
        old_access_counts[key] = metadata.access_count;
    }

    for (auto& [key, metadata] : metadata_store_) {
        // Calculate new tier based on access frequency
        CompressionTier new_tier = calculate_tier(metadata.access_count);
        CompressionAlgo new_algo = config_.tier_configs[static_cast<int>(new_tier)].algorithm;

        // If tier changed, recompress
        if (new_tier != metadata.tier || new_algo != metadata.algorithm) {
            auto data_it = data_store_.find(key);
            if (data_it != data_store_.end()) {
                // Decompress with old algorithm
                std::vector<uint8_t> decompressed = config_.compression_enabled
                    ? decompress(data_it->second, metadata.algorithm, metadata.original_size)
                    : data_it->second;

                // Compress with new tier
                metadata.tier = new_tier;
                metadata.algorithm = new_algo;

                std::vector<uint8_t> recompressed = config_.compression_enabled
                    ? compress_with_algo(decompressed, new_algo)
                    : decompressed;

                // Update data and metadata
                data_it->second = recompressed;
                metadata.compressed_size = recompressed.size();

                recompression_count++;
            }
        }
    }

    // Save changes to disk
    save_to_disk();
    save_metadata();

    operations_since_reorg_ = 0;
    last_reorg_time_ = current_timestamp();

    std::cout << "Reorganization complete. Recompressed " << recompression_count << " items." << std::endl;
}

void DigestiveDatabase::flush() {
    if (write_buffer_.empty()) {
        return;
    }

    // Move all buffered data to main store
    for (const auto& [key, data] : write_buffer_) {
        data_store_[key] = data;
    }

    write_buffer_.clear();
    write_buffer_current_size_ = 0;
}

// ==================== Statistics & Monitoring ====================

DatabaseStats DigestiveDatabase::get_stats() const {
    DatabaseStats stats;
    stats.total_accesses = total_accesses_;
    stats.operations_since_reorg = operations_since_reorg_;

    for (const auto& [key, metadata] : metadata_store_) {
        stats.total_size += metadata.compressed_size;
        stats.original_total_size += metadata.original_size;

        switch (metadata.tier) {
            case CompressionTier::TIER_0: stats.tier0_count++; break;
            case CompressionTier::TIER_1: stats.tier1_count++; break;
            case CompressionTier::TIER_2: stats.tier2_count++; break;
            case CompressionTier::TIER_3: stats.tier3_count++; break;
            case CompressionTier::TIER_4: stats.tier4_count++; break;
        }
    }

    if (stats.original_total_size > 0) {
        stats.compression_ratio = static_cast<double>(stats.original_total_size) /
                                  static_cast<double>(stats.total_size);
    }

    return stats;
}

void DigestiveDatabase::print_stats() const {
    DatabaseStats stats = get_stats();

    std::cout << "===== Database Statistics =====" << std::endl;
    std::cout << "Total accesses: " << stats.total_accesses << std::endl;
    std::cout << "Operations since last reorg: " << stats.operations_since_reorg << std::endl;
    std::cout << "Total items: " << (stats.tier0_count + stats.tier1_count +
                                     stats.tier2_count + stats.tier3_count +
                                     stats.tier4_count) << std::endl;
    std::cout << std::endl;
    std::cout << "Tier 0 (hot, no compression): " << stats.tier0_count << std::endl;
    std::cout << "Tier 1 (warm, light): " << stats.tier1_count << std::endl;
    std::cout << "Tier 2 (medium): " << stats.tier2_count << std::endl;
    std::cout << "Tier 3 (cool): " << stats.tier3_count << std::endl;
    std::cout << "Tier 4 (cold, heavy): " << stats.tier4_count << std::endl;
    std::cout << std::endl;
    std::cout << "Original size: " << stats.original_total_size << " bytes" << std::endl;
    std::cout << "Compressed size: " << stats.total_size << " bytes" << std::endl;
    std::cout << "Compression ratio: " << stats.compression_ratio << "x" << std::endl;
    std::cout << "Space saved: " << (stats.original_total_size - stats.total_size) << " bytes ("
              << (100.0 * (1.0 - 1.0/stats.compression_ratio)) << "%)" << std::endl;
}

size_t DigestiveDatabase::get_size_on_disk() const {
    size_t total = 0;
    for (const auto& entry : fs::directory_iterator(db_path_)) {
        if (fs::is_regular_file(entry)) {
            total += fs::file_size(entry);
        }
    }
    return total;
}

std::optional<NodeMetadata> DigestiveDatabase::get_metadata(const std::string& key) const {
    auto it = metadata_store_.find(key);
    if (it == metadata_store_.end()) {
        return std::nullopt;
    }
    return it->second;
}

// ==================== Private Methods ====================

void DigestiveDatabase::load_from_disk() {
    std::string data_file = db_path_ + "/data.db";
    if (!fs::exists(data_file)) {
        return;
    }

    std::ifstream file(data_file, std::ios::binary);
    if (!file) return;

    while (file) {
        // Read key length
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (!file) break;

        // Read key
        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        // Read value length
        uint32_t value_len;
        file.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));

        // Read value
        std::vector<uint8_t> value(value_len);
        file.read(reinterpret_cast<char*>(value.data()), value_len);

        data_store_[key] = value;
    }
}

void DigestiveDatabase::save_to_disk() {
    std::string data_file = db_path_ + "/data.db";
    std::ofstream file(data_file, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to save data to disk" << std::endl;
        return;
    }

    for (const auto& [key, value] : data_store_) {
        // Write key length and key
        uint32_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), key_len);

        // Write value length and value
        uint32_t value_len = value.size();
        file.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        file.write(reinterpret_cast<const char*>(value.data()), value_len);
    }
}

void DigestiveDatabase::save_metadata() {
    std::string meta_file = db_path_ + "/metadata.db";
    std::ofstream file(meta_file, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to save metadata to disk" << std::endl;
        return;
    }

    // Write global stats
    file.write(reinterpret_cast<const char*>(&total_accesses_), sizeof(total_accesses_));
    file.write(reinterpret_cast<const char*>(&operations_since_reorg_), sizeof(operations_since_reorg_));
    file.write(reinterpret_cast<const char*>(&last_reorg_time_), sizeof(last_reorg_time_));

    // Write number of entries
    uint32_t count = metadata_store_.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [key, metadata] : metadata_store_) {
        // Write key
        uint32_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), key_len);

        // Write metadata
        file.write(reinterpret_cast<const char*>(&metadata.access_count), sizeof(metadata.access_count));
        file.write(reinterpret_cast<const char*>(&metadata.last_access), sizeof(metadata.last_access));
        file.write(reinterpret_cast<const char*>(&metadata.tier), sizeof(metadata.tier));
        file.write(reinterpret_cast<const char*>(&metadata.algorithm), sizeof(metadata.algorithm));
        file.write(reinterpret_cast<const char*>(&metadata.original_size), sizeof(metadata.original_size));
        file.write(reinterpret_cast<const char*>(&metadata.compressed_size), sizeof(metadata.compressed_size));
    }
}

void DigestiveDatabase::load_metadata() {
    std::string meta_file = db_path_ + "/metadata.db";
    if (!fs::exists(meta_file)) {
        return;
    }

    std::ifstream file(meta_file, std::ios::binary);
    if (!file) return;

    // Read global stats
    file.read(reinterpret_cast<char*>(&total_accesses_), sizeof(total_accesses_));
    file.read(reinterpret_cast<char*>(&operations_since_reorg_), sizeof(operations_since_reorg_));
    file.read(reinterpret_cast<char*>(&last_reorg_time_), sizeof(last_reorg_time_));

    // Read number of entries
    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count && file; i++) {
        // Read key
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        // Read metadata
        NodeMetadata metadata;
        file.read(reinterpret_cast<char*>(&metadata.access_count), sizeof(metadata.access_count));
        file.read(reinterpret_cast<char*>(&metadata.last_access), sizeof(metadata.last_access));
        file.read(reinterpret_cast<char*>(&metadata.tier), sizeof(metadata.tier));
        file.read(reinterpret_cast<char*>(&metadata.algorithm), sizeof(metadata.algorithm));
        file.read(reinterpret_cast<char*>(&metadata.original_size), sizeof(metadata.original_size));
        file.read(reinterpret_cast<char*>(&metadata.compressed_size), sizeof(metadata.compressed_size));

        metadata_store_[key] = metadata;
    }
}

std::vector<uint8_t> DigestiveDatabase::compress(const std::vector<uint8_t>& data, CompressionTier tier) {
    int tier_idx = static_cast<int>(tier);
    const TierConfig& tier_config = config_.tier_configs[tier_idx];

    // Use custom function if provided
    if (tier_config.compress_fn) {
        return tier_config.compress_fn(data);
    }

    // Otherwise use built-in algorithm
    return compress_with_algo(data, tier_config.algorithm);
}

std::vector<uint8_t> DigestiveDatabase::decompress(const std::vector<uint8_t>& data, CompressionAlgo algo, size_t original_size) {
    return decompress_with_algo(data, algo, original_size);
}

std::vector<uint8_t> DigestiveDatabase::compress_with_algo(const std::vector<uint8_t>& data, CompressionAlgo algo) {
    switch (algo) {
        case CompressionAlgo::NONE:
            return data;

        case CompressionAlgo::LZ4_FAST: {
            int max_dst_size = LZ4_compressBound(data.size());
            std::vector<uint8_t> compressed(max_dst_size);

            int compressed_size = LZ4_compress_default(
                reinterpret_cast<const char*>(data.data()),
                reinterpret_cast<char*>(compressed.data()),
                data.size(),
                max_dst_size
            );

            if (compressed_size <= 0) {
                std::cerr << "LZ4 compression failed" << std::endl;
                return data;
            }

            compressed.resize(compressed_size);
            return compressed;
        }

        case CompressionAlgo::LZ4_HIGH: {
            int max_dst_size = LZ4_compressBound(data.size());
            std::vector<uint8_t> compressed(max_dst_size);

            int compressed_size = LZ4_compress_HC(
                reinterpret_cast<const char*>(data.data()),
                reinterpret_cast<char*>(compressed.data()),
                data.size(),
                max_dst_size,
                LZ4HC_CLEVEL_MAX
            );

            if (compressed_size <= 0) {
                std::cerr << "LZ4 HC compression failed" << std::endl;
                return data;
            }

            compressed.resize(compressed_size);
            return compressed;
        }

        case CompressionAlgo::ZSTD_FAST: {
            size_t max_dst_size = ZSTD_compressBound(data.size());
            std::vector<uint8_t> compressed(max_dst_size);

            size_t compressed_size = ZSTD_compress(
                compressed.data(),
                max_dst_size,
                data.data(),
                data.size(),
                3  // Fast level
            );

            if (ZSTD_isError(compressed_size)) {
                std::cerr << "ZSTD compression failed: " << ZSTD_getErrorName(compressed_size) << std::endl;
                return data;
            }

            compressed.resize(compressed_size);
            return compressed;
        }

        case CompressionAlgo::ZSTD_MEDIUM: {
            size_t max_dst_size = ZSTD_compressBound(data.size());
            std::vector<uint8_t> compressed(max_dst_size);

            size_t compressed_size = ZSTD_compress(
                compressed.data(),
                max_dst_size,
                data.data(),
                data.size(),
                10  // Medium level
            );

            if (ZSTD_isError(compressed_size)) {
                std::cerr << "ZSTD compression failed: " << ZSTD_getErrorName(compressed_size) << std::endl;
                return data;
            }

            compressed.resize(compressed_size);
            return compressed;
        }

        case CompressionAlgo::ZSTD_MAX: {
            size_t max_dst_size = ZSTD_compressBound(data.size());
            std::vector<uint8_t> compressed(max_dst_size);

            size_t compressed_size = ZSTD_compress(
                compressed.data(),
                max_dst_size,
                data.data(),
                data.size(),
                19  // Maximum level
            );

            if (ZSTD_isError(compressed_size)) {
                std::cerr << "ZSTD compression failed: " << ZSTD_getErrorName(compressed_size) << std::endl;
                return data;
            }

            compressed.resize(compressed_size);
            return compressed;
        }
    }

    return data;
}

std::vector<uint8_t> DigestiveDatabase::decompress_with_algo(const std::vector<uint8_t>& data, CompressionAlgo algo, size_t original_size) {
    switch (algo) {
        case CompressionAlgo::NONE:
            return data;

        case CompressionAlgo::LZ4_FAST:
        case CompressionAlgo::LZ4_HIGH: {
            std::vector<uint8_t> decompressed(original_size);

            int result = LZ4_decompress_safe(
                reinterpret_cast<const char*>(data.data()),
                reinterpret_cast<char*>(decompressed.data()),
                data.size(),
                original_size
            );

            if (result < 0) {
                std::cerr << "LZ4 decompression failed" << std::endl;
                return data;
            }

            return decompressed;
        }

        case CompressionAlgo::ZSTD_FAST:
        case CompressionAlgo::ZSTD_MEDIUM:
        case CompressionAlgo::ZSTD_MAX: {
            std::vector<uint8_t> decompressed(original_size);

            size_t result = ZSTD_decompress(
                decompressed.data(),
                original_size,
                data.data(),
                data.size()
            );

            if (ZSTD_isError(result)) {
                std::cerr << "ZSTD decompression failed: " << ZSTD_getErrorName(result) << std::endl;
                return data;
            }

            return decompressed;
        }
    }

    return data;
}

CompressionTier DigestiveDatabase::calculate_tier(uint64_t access_count) const {
    if (total_accesses_ == 0) {
        return CompressionTier::TIER_4;
    }

    double frequency_ratio = static_cast<double>(access_count) / static_cast<double>(total_accesses_);

    if (frequency_ratio > 0.3) {
        return CompressionTier::TIER_0;
    } else if (frequency_ratio > 0.15) {
        return CompressionTier::TIER_1;
    } else if (frequency_ratio > 0.05) {
        return CompressionTier::TIER_2;
    } else if (frequency_ratio > 0.01) {
        return CompressionTier::TIER_3;
    } else {
        return CompressionTier::TIER_4;
    }
}

void DigestiveDatabase::check_size_limit() {
    size_t current_size = get_size_on_disk();

    if (current_size > config_.max_size_bytes) {
        if (config_.allow_deletion) {
            std::cout << "⚠️  Size limit exceeded. Deleting coldest data..." << std::endl;
            delete_coldest_data();
        } else {
            std::cout << "⚠️  WARNING: Database size (" << current_size
                      << " bytes) exceeds limit (" << config_.max_size_bytes << " bytes)!" << std::endl;
        }
    }
}

void DigestiveDatabase::delete_coldest_data() {
    // Collect all items with their access counts
    std::vector<std::pair<std::string, uint64_t>> items;
    for (const auto& [key, metadata] : metadata_store_) {
        items.push_back({key, metadata.access_count});
    }

    // Sort by access count (ascending - coldest first)
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Delete bottom 10% coldest items
    size_t delete_count = std::max<size_t>(items.size() / 10, 1);
    int deleted = 0;

    for (size_t i = 0; i < delete_count && i < items.size(); i++) {
        const std::string& key = items[i].first;
        data_store_.erase(key);
        metadata_store_.erase(key);
        write_buffer_.erase(key);
        deleted++;
    }

    std::cout << "Deleted " << deleted << " coldest items." << std::endl;

    // Save changes
    save_to_disk();
    save_metadata();
}

void DigestiveDatabase::check_reorganization_trigger() {
    if (!should_reorganize()) {
        return;
    }

    std::cout << "Auto-triggering reorganization..." << std::endl;
    reorganize();
}

bool DigestiveDatabase::should_reorganize() const {
    switch (config_.reorg_strategy) {
        case ReorgStrategy::MANUAL:
            return false;

        case ReorgStrategy::EVERY_N_OPS:
            return operations_since_reorg_ >= config_.reorg_operation_threshold;

        case ReorgStrategy::PERIODIC: {
            uint64_t now = current_timestamp();
            return (now - last_reorg_time_) >= config_.reorg_time_threshold;
        }

        case ReorgStrategy::ADAPTIVE: {
            // Reorganize if access pattern changed significantly
            double change = calculate_access_pattern_change();
            return change >= config_.reorg_change_threshold;
        }
    }

    return false;
}

double DigestiveDatabase::calculate_access_pattern_change() const {
    // Calculate how much the access pattern has changed
    // This is a simple heuristic: ratio of operations to total items
    if (metadata_store_.empty()) {
        return 0.0;
    }

    return static_cast<double>(operations_since_reorg_) /
           static_cast<double>(metadata_store_.size());
}

uint64_t DigestiveDatabase::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

void DigestiveDatabase::after_operation() {
    operations_since_reorg_++;
    check_reorganization_trigger();
}

} // namespace digestive
