#include "chunking_engine.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <chrono>

namespace fs = std::filesystem;

namespace digestive {

// ==================== ChunkMetadata ====================

ChunkMetadata::ChunkMetadata()
    : chunk_id(0)
    , heat(0.0)
    , compressed_size(0)
    , original_size(0)
    , file_offset(0)
    , tier(4)  // Start cold
    , last_access(0) {
}

// ==================== ChunkedFileMetadata ====================

ChunkedFileMetadata::ChunkedFileMetadata()
    : total_size(0)
    , chunk_size(0)
    , num_chunks(0) {
}

// ==================== ChunkingEngine ====================

ChunkingEngine::ChunkingEngine(const std::string& storage_path, size_t default_chunk_size)
    : storage_path_(storage_path)
    , chunks_dir_(storage_path + "/chunks")
    , default_chunk_size_(default_chunk_size) {

    // Create chunks directory if it doesn't exist
    if (!fs::exists(chunks_dir_)) {
        fs::create_directories(chunks_dir_);
    }

    load_metadata();
}

ChunkingEngine::~ChunkingEngine() {
    save_metadata();
}

void ChunkingEngine::insert_chunked(
    const std::string& key,
    const std::vector<uint8_t>& data,
    std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, uint8_t)> compress_fn) {

    ChunkedFileMetadata file_meta;
    file_meta.key = key;
    file_meta.total_size = data.size();
    file_meta.chunk_size = default_chunk_size_;
    file_meta.num_chunks = (data.size() + default_chunk_size_ - 1) / default_chunk_size_;

    // Create directory for this file's chunks
    std::string file_chunk_dir = chunks_dir_ + "/" + key;
    if (!fs::exists(file_chunk_dir)) {
        fs::create_directories(file_chunk_dir);
    }

    // Split into chunks and compress
    for (uint32_t i = 0; i < file_meta.num_chunks; i++) {
        size_t offset = i * default_chunk_size_;
        size_t chunk_data_size = std::min(default_chunk_size_, data.size() - offset);

        // Extract chunk data
        std::vector<uint8_t> chunk_data(
            data.begin() + offset,
            data.begin() + offset + chunk_data_size
        );

        // Compress chunk (start with coldest tier = 4)
        std::vector<uint8_t> compressed = compress_fn(chunk_data, 4);

        // Create chunk metadata
        ChunkMetadata chunk_meta;
        chunk_meta.chunk_id = i;
        chunk_meta.heat = 0.1;  // Start cold
        chunk_meta.original_size = chunk_data_size;
        chunk_meta.compressed_size = compressed.size();
        chunk_meta.tier = 4;
        chunk_meta.last_access = 0;

        // Write chunk to disk
        std::string chunk_path = get_chunk_path(key, i);
        std::ofstream chunk_file(chunk_path, std::ios::binary);
        if (chunk_file) {
            chunk_file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
            chunk_meta.file_offset = 0;  // Each chunk in separate file
        } else {
            std::cerr << "Failed to write chunk: " << chunk_path << std::endl;
        }

        file_meta.chunks[i] = chunk_meta;
    }

    file_metadata_[key] = file_meta;
    save_metadata();
}

std::optional<std::vector<uint8_t>> ChunkingEngine::get_chunk_range(
    const std::string& key,
    uint32_t start_chunk,
    uint32_t end_chunk,
    std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, uint8_t, size_t)> decompress_fn) {

    auto it = file_metadata_.find(key);
    if (it == file_metadata_.end()) {
        return std::nullopt;
    }

    ChunkedFileMetadata& file_meta = it->second;

    // Validate range
    if (start_chunk >= file_meta.num_chunks || end_chunk >= file_meta.num_chunks) {
        std::cerr << "Chunk range out of bounds" << std::endl;
        return std::nullopt;
    }

    std::vector<uint8_t> result;

    // Read and decompress each chunk in range
    for (uint32_t i = start_chunk; i <= end_chunk; i++) {
        auto chunk_it = file_meta.chunks.find(i);
        if (chunk_it == file_meta.chunks.end()) {
            std::cerr << "Chunk " << i << " not found" << std::endl;
            continue;
        }

        ChunkMetadata& chunk_meta = chunk_it->second;

        // Read compressed chunk from disk
        std::string chunk_path = get_chunk_path(key, i);
        std::ifstream chunk_file(chunk_path, std::ios::binary);
        if (!chunk_file) {
            std::cerr << "Failed to read chunk: " << chunk_path << std::endl;
            continue;
        }

        std::vector<uint8_t> compressed(chunk_meta.compressed_size);
        chunk_file.read(reinterpret_cast<char*>(compressed.data()), chunk_meta.compressed_size);

        // Decompress
        std::vector<uint8_t> decompressed = decompress_fn(
            compressed,
            chunk_meta.tier,
            chunk_meta.original_size
        );

        // Append to result
        result.insert(result.end(), decompressed.begin(), decompressed.end());

        // Update heat
        chunk_meta.heat = std::min(1.0, chunk_meta.heat + 0.1);
        chunk_meta.last_access = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    return result;
}

std::optional<std::vector<uint8_t>> ChunkingEngine::get_full_file(
    const std::string& key,
    std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, uint8_t, size_t)> decompress_fn) {

    auto it = file_metadata_.find(key);
    if (it == file_metadata_.end()) {
        return std::nullopt;
    }

    return get_chunk_range(key, 0, it->second.num_chunks - 1, decompress_fn);
}

void ChunkingEngine::update_chunk_heat(const std::string& key, uint32_t chunk_id, double heat_increment) {
    auto it = file_metadata_.find(key);
    if (it == file_metadata_.end()) {
        return;
    }

    auto chunk_it = it->second.chunks.find(chunk_id);
    if (chunk_it == it->second.chunks.end()) {
        return;
    }

    chunk_it->second.heat = std::min(1.0, chunk_it->second.heat + heat_increment);
}

void ChunkingEngine::decay_all_chunks(double decay_factor) {
    for (auto& [key, file_meta] : file_metadata_) {
        for (auto& [chunk_id, chunk_meta] : file_meta.chunks) {
            chunk_meta.heat *= decay_factor;

            // Update tier based on new heat
            uint8_t new_tier = calculate_tier_from_heat(chunk_meta.heat);
            if (new_tier != chunk_meta.tier) {
                // TODO: Recompress chunk with new tier
                chunk_meta.tier = new_tier;
            }
        }
    }
}

std::optional<ChunkedFileMetadata> ChunkingEngine::get_metadata(const std::string& key) const {
    auto it = file_metadata_.find(key);
    if (it == file_metadata_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ChunkingEngine::remove_chunked(const std::string& key) {
    auto it = file_metadata_.find(key);
    if (it == file_metadata_.end()) {
        return false;
    }

    // Delete chunk files
    std::string file_chunk_dir = chunks_dir_ + "/" + key;
    if (fs::exists(file_chunk_dir)) {
        fs::remove_all(file_chunk_dir);
    }

    file_metadata_.erase(it);
    save_metadata();

    return true;
}

void ChunkingEngine::save_metadata() {
    std::string meta_path = chunks_dir_ + "/chunk_metadata.db";
    std::ofstream file(meta_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to save chunk metadata" << std::endl;
        return;
    }

    // Write number of files
    uint32_t num_files = file_metadata_.size();
    file.write(reinterpret_cast<const char*>(&num_files), sizeof(num_files));

    for (const auto& [key, file_meta] : file_metadata_) {
        // Write key
        uint32_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), key_len);

        // Write file metadata
        file.write(reinterpret_cast<const char*>(&file_meta.total_size), sizeof(file_meta.total_size));
        file.write(reinterpret_cast<const char*>(&file_meta.chunk_size), sizeof(file_meta.chunk_size));
        file.write(reinterpret_cast<const char*>(&file_meta.num_chunks), sizeof(file_meta.num_chunks));

        // Write chunks
        uint32_t num_chunks = file_meta.chunks.size();
        file.write(reinterpret_cast<const char*>(&num_chunks), sizeof(num_chunks));

        for (const auto& [chunk_id, chunk_meta] : file_meta.chunks) {
            file.write(reinterpret_cast<const char*>(&chunk_meta.chunk_id), sizeof(chunk_meta.chunk_id));
            file.write(reinterpret_cast<const char*>(&chunk_meta.heat), sizeof(chunk_meta.heat));
            file.write(reinterpret_cast<const char*>(&chunk_meta.compressed_size), sizeof(chunk_meta.compressed_size));
            file.write(reinterpret_cast<const char*>(&chunk_meta.original_size), sizeof(chunk_meta.original_size));
            file.write(reinterpret_cast<const char*>(&chunk_meta.file_offset), sizeof(chunk_meta.file_offset));
            file.write(reinterpret_cast<const char*>(&chunk_meta.tier), sizeof(chunk_meta.tier));
            file.write(reinterpret_cast<const char*>(&chunk_meta.last_access), sizeof(chunk_meta.last_access));
        }
    }
}

void ChunkingEngine::load_metadata() {
    std::string meta_path = chunks_dir_ + "/chunk_metadata.db";
    if (!fs::exists(meta_path)) {
        return;
    }

    std::ifstream file(meta_path, std::ios::binary);
    if (!file) {
        return;
    }

    // Read number of files
    uint32_t num_files;
    file.read(reinterpret_cast<char*>(&num_files), sizeof(num_files));

    for (uint32_t f = 0; f < num_files && file; f++) {
        // Read key
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        ChunkedFileMetadata file_meta;
        file_meta.key = key;

        // Read file metadata
        file.read(reinterpret_cast<char*>(&file_meta.total_size), sizeof(file_meta.total_size));
        file.read(reinterpret_cast<char*>(&file_meta.chunk_size), sizeof(file_meta.chunk_size));
        file.read(reinterpret_cast<char*>(&file_meta.num_chunks), sizeof(file_meta.num_chunks));

        // Read chunks
        uint32_t num_chunks;
        file.read(reinterpret_cast<char*>(&num_chunks), sizeof(num_chunks));

        for (uint32_t c = 0; c < num_chunks && file; c++) {
            ChunkMetadata chunk_meta;
            file.read(reinterpret_cast<char*>(&chunk_meta.chunk_id), sizeof(chunk_meta.chunk_id));
            file.read(reinterpret_cast<char*>(&chunk_meta.heat), sizeof(chunk_meta.heat));
            file.read(reinterpret_cast<char*>(&chunk_meta.compressed_size), sizeof(chunk_meta.compressed_size));
            file.read(reinterpret_cast<char*>(&chunk_meta.original_size), sizeof(chunk_meta.original_size));
            file.read(reinterpret_cast<char*>(&chunk_meta.file_offset), sizeof(chunk_meta.file_offset));
            file.read(reinterpret_cast<char*>(&chunk_meta.tier), sizeof(chunk_meta.tier));
            file.read(reinterpret_cast<char*>(&chunk_meta.last_access), sizeof(chunk_meta.last_access));

            file_meta.chunks[chunk_meta.chunk_id] = chunk_meta;
        }

        file_metadata_[key] = file_meta;
    }
}

size_t ChunkingEngine::get_storage_size() const {
    size_t total = 0;

    if (fs::exists(chunks_dir_)) {
        for (const auto& entry : fs::recursive_directory_iterator(chunks_dir_)) {
            if (fs::is_regular_file(entry)) {
                total += fs::file_size(entry);
            }
        }
    }

    return total;
}

std::string ChunkingEngine::get_chunk_path(const std::string& key, uint32_t chunk_id) const {
    return chunks_dir_ + "/" + key + "/chunk_" +
           std::string(3 - std::to_string(chunk_id).length(), '0') + std::to_string(chunk_id) + ".bin";
}

uint8_t ChunkingEngine::calculate_tier_from_heat(double heat) const {
    if (heat > 0.7) return 0;      // Hot: uncompressed
    if (heat > 0.4) return 1;      // Warm: light compression
    if (heat > 0.2) return 2;      // Medium
    if (heat > 0.1) return 3;      // Cool
    return 4;                       // Cold: heavy compression
}

} // namespace digestive
