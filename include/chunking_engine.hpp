#ifndef CHUNKING_ENGINE_HPP
#define CHUNKING_ENGINE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <functional>

namespace digestive {

/**
 * Chunk metadata for large files
 */
struct ChunkMetadata {
    uint32_t chunk_id;
    double heat;                    // Individual heat per chunk
    size_t compressed_size;
    size_t original_size;
    uint64_t file_offset;           // Offset in chunks storage file
    uint8_t tier;                   // Compression tier for this chunk
    uint64_t last_access;

    ChunkMetadata();
};

/**
 * Large file metadata
 */
struct ChunkedFileMetadata {
    std::string key;
    size_t total_size;              // Original file size
    size_t chunk_size;              // Size per chunk
    uint32_t num_chunks;
    std::map<uint32_t, ChunkMetadata> chunks;  // chunk_id -> metadata

    ChunkedFileMetadata();
};

/**
 * Chunking engine for large file support
 * Splits large files into chunks, each with independent heat tracking
 */
class ChunkingEngine {
public:
    ChunkingEngine(const std::string& storage_path, size_t default_chunk_size = 4 * 1024 * 1024);
    ~ChunkingEngine();

    /**
     * Insert large file as chunks
     * @param key File identifier
     * @param data File data
     * @param compress_fn Compression function per chunk
     */
    void insert_chunked(const std::string& key,
                       const std::vector<uint8_t>& data,
                       std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, uint8_t)> compress_fn);

    /**
     * Get specific chunk range from file
     * @param key File identifier
     * @param start_chunk First chunk to retrieve
     * @param end_chunk Last chunk to retrieve (inclusive)
     * @param decompress_fn Decompression function
     * @return Concatenated chunk data
     */
    std::optional<std::vector<uint8_t>> get_chunk_range(
        const std::string& key,
        uint32_t start_chunk,
        uint32_t end_chunk,
        std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, uint8_t, size_t)> decompress_fn);

    /**
     * Get entire file (all chunks)
     */
    std::optional<std::vector<uint8_t>> get_full_file(
        const std::string& key,
        std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, uint8_t, size_t)> decompress_fn);

    /**
     * Update heat for accessed chunks
     */
    void update_chunk_heat(const std::string& key, uint32_t chunk_id, double heat_increment);

    /**
     * Apply heat decay to all chunks
     */
    void decay_all_chunks(double decay_factor);

    /**
     * Get metadata for a chunked file
     */
    std::optional<ChunkedFileMetadata> get_metadata(const std::string& key) const;

    /**
     * Remove chunked file
     */
    bool remove_chunked(const std::string& key);

    /**
     * Save/load chunk metadata
     */
    void save_metadata();
    void load_metadata();

    /**
     * Get total size of chunks storage
     */
    size_t get_storage_size() const;

private:
    std::string storage_path_;
    std::string chunks_dir_;
    size_t default_chunk_size_;

    // Metadata: file key -> chunk metadata
    std::map<std::string, ChunkedFileMetadata> file_metadata_;

    // Helper methods
    std::string get_chunk_path(const std::string& key, uint32_t chunk_id) const;
    uint8_t calculate_tier_from_heat(double heat) const;
};

} // namespace digestive

#endif // CHUNKING_ENGINE_HPP
