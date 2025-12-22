#ifndef INDEX_ENGINE_HPP
#define INDEX_ENGINE_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace digestive {

/**
 * Index types
 */
enum class IndexType {
    HASH,           // Fast O(1) lookup for equality
    ORDERED,        // For range queries (std::map)
    NONE
};

/**
 * Index definition
 */
struct IndexDefinition {
    std::string name;               // Index name
    std::string table;              // Table name
    std::string column;             // Column name
    IndexType type;                 // Index type
    bool is_unique;                 // Unique constraint
    double heat;                    // Index heat (how often used)

    IndexDefinition();
};

/**
 * Index entry value
 */
struct IndexEntry {
    std::vector<uint64_t> row_ids;  // Row IDs matching this value
    double heat;                     // Heat for this index entry

    IndexEntry();
};

/**
 * Index engine for fast queries
 * Supports hash indexes (equality) and ordered indexes (range queries)
 */
class IndexEngine {
public:
    IndexEngine();
    ~IndexEngine();

    /**
     * Create an index on a table column
     * @param table Table name
     * @param column Column name
     * @param type Index type (HASH or ORDERED)
     * @param is_unique Whether values must be unique
     */
    void create_index(const std::string& table,
                     const std::string& column,
                     IndexType type = IndexType::HASH,
                     bool is_unique = false);

    /**
     * Drop an index
     */
    bool drop_index(const std::string& table, const std::string& column);

    /**
     * Insert into index
     * @param table Table name
     * @param column Column name
     * @param value Column value
     * @param row_id Row identifier
     */
    void insert_into_index(const std::string& table,
                          const std::string& column,
                          const std::string& value,
                          uint64_t row_id);

    /**
     * Remove from index
     */
    void remove_from_index(const std::string& table,
                          const std::string& column,
                          const std::string& value,
                          uint64_t row_id);

    /**
     * Query index for exact match
     * @return Row IDs matching the value
     */
    std::vector<uint64_t> query_index(const std::string& table,
                                      const std::string& column,
                                      const std::string& value);

    /**
     * Query index for range (only works with ORDERED indexes)
     * @param min_value Minimum value (inclusive)
     * @param max_value Maximum value (inclusive)
     * @return Row IDs in range
     */
    std::vector<uint64_t> query_range(const std::string& table,
                                      const std::string& column,
                                      const std::string& min_value,
                                      const std::string& max_value);

    /**
     * Check if index exists
     */
    bool has_index(const std::string& table, const std::string& column) const;

    /**
     * Get index type
     */
    std::optional<IndexType> get_index_type(const std::string& table,
                                            const std::string& column) const;

    /**
     * Decay heat for all indexes
     */
    void decay_index_heat(double decay_factor);

    /**
     * Get all indexes for a table
     */
    std::vector<std::string> get_table_indexes(const std::string& table) const;

    /**
     * Save/load index metadata
     */
    void save_indexes(const std::string& path);
    void load_indexes(const std::string& path);

private:
    // Index definitions: "table:column" -> definition
    std::map<std::string, IndexDefinition> index_defs_;

    // Hash indexes: "table:column:value" -> row_ids
    std::unordered_map<std::string, IndexEntry> hash_indexes_;

    // Ordered indexes: "table:column" -> (value -> row_ids)
    std::map<std::string, std::map<std::string, IndexEntry>> ordered_indexes_;

    // Helper methods
    std::string make_index_key(const std::string& table, const std::string& column) const;
    std::string make_hash_key(const std::string& table,
                              const std::string& column,
                              const std::string& value) const;
};

} // namespace digestive

#endif // INDEX_ENGINE_HPP
