#include "index_engine.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace digestive {

// ==================== IndexDefinition ====================

IndexDefinition::IndexDefinition()
    : type(IndexType::NONE)
    , is_unique(false)
    , heat(0.0) {
}

// ==================== IndexEntry ====================

IndexEntry::IndexEntry()
    : heat(0.0) {
}

// ==================== IndexEngine ====================

IndexEngine::IndexEngine() {
}

IndexEngine::~IndexEngine() {
}

void IndexEngine::create_index(const std::string& table,
                               const std::string& column,
                               IndexType type,
                               bool is_unique) {
    std::string index_key = make_index_key(table, column);

    // Check if index already exists
    if (index_defs_.count(index_key)) {
        std::cerr << "Index already exists: " << index_key << std::endl;
        return;
    }

    IndexDefinition def;
    def.name = index_key;
    def.table = table;
    def.column = column;
    def.type = type;
    def.is_unique = is_unique;
    def.heat = 0.5;  // Start with medium heat

    index_defs_[index_key] = def;

    // Initialize index structure
    if (type == IndexType::ORDERED) {
        ordered_indexes_[index_key] = std::map<std::string, IndexEntry>();
    }
}

bool IndexEngine::drop_index(const std::string& table, const std::string& column) {
    std::string index_key = make_index_key(table, column);

    if (!index_defs_.count(index_key)) {
        return false;
    }

    IndexType type = index_defs_[index_key].type;

    // Remove index data
    if (type == IndexType::HASH) {
        // Remove all hash entries for this index
        auto it = hash_indexes_.begin();
        while (it != hash_indexes_.end()) {
            if (it->first.find(index_key + ":") == 0) {
                it = hash_indexes_.erase(it);
            } else {
                ++it;
            }
        }
    } else if (type == IndexType::ORDERED) {
        ordered_indexes_.erase(index_key);
    }

    index_defs_.erase(index_key);
    return true;
}

void IndexEngine::insert_into_index(const std::string& table,
                                    const std::string& column,
                                    const std::string& value,
                                    uint64_t row_id) {
    std::string index_key = make_index_key(table, column);

    auto it = index_defs_.find(index_key);
    if (it == index_defs_.end()) {
        return;  // Index doesn't exist
    }

    IndexDefinition& def = it->second;

    if (def.type == IndexType::HASH) {
        std::string hash_key = make_hash_key(table, column, value);
        IndexEntry& entry = hash_indexes_[hash_key];

        // Check unique constraint
        if (def.is_unique && !entry.row_ids.empty()) {
            std::cerr << "Unique constraint violation for index: " << index_key << std::endl;
            return;
        }

        entry.row_ids.push_back(row_id);
        entry.heat = 0.5;

    } else if (def.type == IndexType::ORDERED) {
        IndexEntry& entry = ordered_indexes_[index_key][value];

        if (def.is_unique && !entry.row_ids.empty()) {
            std::cerr << "Unique constraint violation for index: " << index_key << std::endl;
            return;
        }

        entry.row_ids.push_back(row_id);
        entry.heat = 0.5;
    }
}

void IndexEngine::remove_from_index(const std::string& table,
                                    const std::string& column,
                                    const std::string& value,
                                    uint64_t row_id) {
    std::string index_key = make_index_key(table, column);

    auto it = index_defs_.find(index_key);
    if (it == index_defs_.end()) {
        return;
    }

    IndexDefinition& def = it->second;

    if (def.type == IndexType::HASH) {
        std::string hash_key = make_hash_key(table, column, value);
        auto hash_it = hash_indexes_.find(hash_key);
        if (hash_it != hash_indexes_.end()) {
            auto& row_ids = hash_it->second.row_ids;
            row_ids.erase(std::remove(row_ids.begin(), row_ids.end(), row_id), row_ids.end());

            if (row_ids.empty()) {
                hash_indexes_.erase(hash_it);
            }
        }

    } else if (def.type == IndexType::ORDERED) {
        auto ordered_it = ordered_indexes_.find(index_key);
        if (ordered_it != ordered_indexes_.end()) {
            auto value_it = ordered_it->second.find(value);
            if (value_it != ordered_it->second.end()) {
                auto& row_ids = value_it->second.row_ids;
                row_ids.erase(std::remove(row_ids.begin(), row_ids.end(), row_id), row_ids.end());

                if (row_ids.empty()) {
                    ordered_it->second.erase(value_it);
                }
            }
        }
    }
}

std::vector<uint64_t> IndexEngine::query_index(const std::string& table,
                                                const std::string& column,
                                                const std::string& value) {
    std::string index_key = make_index_key(table, column);

    auto it = index_defs_.find(index_key);
    if (it == index_defs_.end()) {
        return {};
    }

    IndexDefinition& def = it->second;
    def.heat = std::min(1.0, def.heat + 0.1);  // Heat up with use

    if (def.type == IndexType::HASH) {
        std::string hash_key = make_hash_key(table, column, value);
        auto hash_it = hash_indexes_.find(hash_key);
        if (hash_it != hash_indexes_.end()) {
            hash_it->second.heat = std::min(1.0, hash_it->second.heat + 0.1);
            return hash_it->second.row_ids;
        }

    } else if (def.type == IndexType::ORDERED) {
        auto ordered_it = ordered_indexes_.find(index_key);
        if (ordered_it != ordered_indexes_.end()) {
            auto value_it = ordered_it->second.find(value);
            if (value_it != ordered_it->second.end()) {
                value_it->second.heat = std::min(1.0, value_it->second.heat + 0.1);
                return value_it->second.row_ids;
            }
        }
    }

    return {};
}

std::vector<uint64_t> IndexEngine::query_range(const std::string& table,
                                                const std::string& column,
                                                const std::string& min_value,
                                                const std::string& max_value) {
    std::string index_key = make_index_key(table, column);

    auto it = index_defs_.find(index_key);
    if (it == index_defs_.end()) {
        return {};
    }

    IndexDefinition& def = it->second;

    if (def.type != IndexType::ORDERED) {
        std::cerr << "Range query requires ORDERED index" << std::endl;
        return {};
    }

    def.heat = std::min(1.0, def.heat + 0.1);

    std::vector<uint64_t> results;

    auto ordered_it = ordered_indexes_.find(index_key);
    if (ordered_it != ordered_indexes_.end()) {
        auto& index_map = ordered_it->second;

        // Find range [min_value, max_value]
        auto lower = index_map.lower_bound(min_value);
        auto upper = index_map.upper_bound(max_value);

        for (auto it = lower; it != upper; ++it) {
            it->second.heat = std::min(1.0, it->second.heat + 0.05);
            results.insert(results.end(),
                          it->second.row_ids.begin(),
                          it->second.row_ids.end());
        }
    }

    return results;
}

bool IndexEngine::has_index(const std::string& table, const std::string& column) const {
    std::string index_key = make_index_key(table, column);
    return index_defs_.count(index_key) > 0;
}

std::optional<IndexType> IndexEngine::get_index_type(const std::string& table,
                                                      const std::string& column) const {
    std::string index_key = make_index_key(table, column);
    auto it = index_defs_.find(index_key);
    if (it == index_defs_.end()) {
        return std::nullopt;
    }
    return it->second.type;
}

void IndexEngine::decay_index_heat(double decay_factor) {
    // Decay index definition heat
    for (auto& [key, def] : index_defs_) {
        def.heat *= decay_factor;
    }

    // Decay hash index entry heat
    for (auto& [key, entry] : hash_indexes_) {
        entry.heat *= decay_factor;
    }

    // Decay ordered index entry heat
    for (auto& [index_key, index_map] : ordered_indexes_) {
        for (auto& [value, entry] : index_map) {
            entry.heat *= decay_factor;
        }
    }
}

std::vector<std::string> IndexEngine::get_table_indexes(const std::string& table) const {
    std::vector<std::string> indexes;

    for (const auto& [key, def] : index_defs_) {
        if (def.table == table) {
            indexes.push_back(def.column);
        }
    }

    return indexes;
}

void IndexEngine::save_indexes(const std::string& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to save indexes" << std::endl;
        return;
    }

    // Save index definitions
    uint32_t num_defs = index_defs_.size();
    file.write(reinterpret_cast<const char*>(&num_defs), sizeof(num_defs));

    for (const auto& [key, def] : index_defs_) {
        // Write index key
        uint32_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), key_len);

        // Write definition
        uint32_t table_len = def.table.size();
        file.write(reinterpret_cast<const char*>(&table_len), sizeof(table_len));
        file.write(def.table.data(), table_len);

        uint32_t column_len = def.column.size();
        file.write(reinterpret_cast<const char*>(&column_len), sizeof(column_len));
        file.write(def.column.data(), column_len);

        file.write(reinterpret_cast<const char*>(&def.type), sizeof(def.type));
        file.write(reinterpret_cast<const char*>(&def.is_unique), sizeof(def.is_unique));
        file.write(reinterpret_cast<const char*>(&def.heat), sizeof(def.heat));
    }

    // Save hash indexes
    uint32_t num_hash = hash_indexes_.size();
    file.write(reinterpret_cast<const char*>(&num_hash), sizeof(num_hash));

    for (const auto& [key, entry] : hash_indexes_) {
        uint32_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), key_len);

        file.write(reinterpret_cast<const char*>(&entry.heat), sizeof(entry.heat));

        uint32_t num_rows = entry.row_ids.size();
        file.write(reinterpret_cast<const char*>(&num_rows), sizeof(num_rows));
        file.write(reinterpret_cast<const char*>(entry.row_ids.data()), num_rows * sizeof(uint64_t));
    }

    // Save ordered indexes
    uint32_t num_ordered = ordered_indexes_.size();
    file.write(reinterpret_cast<const char*>(&num_ordered), sizeof(num_ordered));

    for (const auto& [index_key, index_map] : ordered_indexes_) {
        uint32_t key_len = index_key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(index_key.data(), key_len);

        uint32_t num_entries = index_map.size();
        file.write(reinterpret_cast<const char*>(&num_entries), sizeof(num_entries));

        for (const auto& [value, entry] : index_map) {
            uint32_t value_len = value.size();
            file.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
            file.write(value.data(), value_len);

            file.write(reinterpret_cast<const char*>(&entry.heat), sizeof(entry.heat));

            uint32_t num_rows = entry.row_ids.size();
            file.write(reinterpret_cast<const char*>(&num_rows), sizeof(num_rows));
            file.write(reinterpret_cast<const char*>(entry.row_ids.data()), num_rows * sizeof(uint64_t));
        }
    }
}

void IndexEngine::load_indexes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return;
    }

    // Load index definitions
    uint32_t num_defs;
    file.read(reinterpret_cast<char*>(&num_defs), sizeof(num_defs));

    for (uint32_t i = 0; i < num_defs && file; i++) {
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        IndexDefinition def;

        uint32_t table_len;
        file.read(reinterpret_cast<char*>(&table_len), sizeof(table_len));
        def.table.resize(table_len);
        file.read(&def.table[0], table_len);

        uint32_t column_len;
        file.read(reinterpret_cast<char*>(&column_len), sizeof(column_len));
        def.column.resize(column_len);
        file.read(&def.column[0], column_len);

        file.read(reinterpret_cast<char*>(&def.type), sizeof(def.type));
        file.read(reinterpret_cast<char*>(&def.is_unique), sizeof(def.is_unique));
        file.read(reinterpret_cast<char*>(&def.heat), sizeof(def.heat));

        def.name = key;
        index_defs_[key] = def;
    }

    // Load hash indexes
    uint32_t num_hash;
    file.read(reinterpret_cast<char*>(&num_hash), sizeof(num_hash));

    for (uint32_t i = 0; i < num_hash && file; i++) {
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        IndexEntry entry;
        file.read(reinterpret_cast<char*>(&entry.heat), sizeof(entry.heat));

        uint32_t num_rows;
        file.read(reinterpret_cast<char*>(&num_rows), sizeof(num_rows));
        entry.row_ids.resize(num_rows);
        file.read(reinterpret_cast<char*>(entry.row_ids.data()), num_rows * sizeof(uint64_t));

        hash_indexes_[key] = entry;
    }

    // Load ordered indexes
    uint32_t num_ordered;
    file.read(reinterpret_cast<char*>(&num_ordered), sizeof(num_ordered));

    for (uint32_t i = 0; i < num_ordered && file; i++) {
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        std::string index_key(key_len, '\0');
        file.read(&index_key[0], key_len);

        uint32_t num_entries;
        file.read(reinterpret_cast<char*>(&num_entries), sizeof(num_entries));

        std::map<std::string, IndexEntry> index_map;

        for (uint32_t j = 0; j < num_entries && file; j++) {
            uint32_t value_len;
            file.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
            std::string value(value_len, '\0');
            file.read(&value[0], value_len);

            IndexEntry entry;
            file.read(reinterpret_cast<char*>(&entry.heat), sizeof(entry.heat));

            uint32_t num_rows;
            file.read(reinterpret_cast<char*>(&num_rows), sizeof(num_rows));
            entry.row_ids.resize(num_rows);
            file.read(reinterpret_cast<char*>(entry.row_ids.data()), num_rows * sizeof(uint64_t));

            index_map[value] = entry;
        }

        ordered_indexes_[index_key] = index_map;
    }
}

std::string IndexEngine::make_index_key(const std::string& table, const std::string& column) const {
    return table + ":" + column;
}

std::string IndexEngine::make_hash_key(const std::string& table,
                                       const std::string& column,
                                       const std::string& value) const {
    return table + ":" + column + ":" + value;
}

} // namespace digestive
