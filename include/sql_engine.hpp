#ifndef SQL_ENGINE_HPP
#define SQL_ENGINE_HPP

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <variant>
#include <cstdint>

namespace digestive {

// Forward declaration
class DigestiveDatabase;

/**
 * SQL data types
 */
enum class SqlType {
    INTEGER,
    REAL,
    TEXT,
    BLOB
};

/**
 * Column definition
 */
struct ColumnDef {
    std::string name;
    SqlType type;
    bool primary_key;
    bool not_null;
    bool unique;

    ColumnDef();
};

/**
 * Table schema
 */
struct TableSchema {
    std::string name;
    std::vector<ColumnDef> columns;
    std::string primary_key_column;
    uint64_t next_row_id;

    TableSchema();
};

/**
 * SQL value (can be int, double, string, or blob)
 */
using SqlValue = std::variant<int64_t, double, std::string, std::vector<uint8_t>>;

/**
 * Row data (column name -> value)
 */
using Row = std::map<std::string, SqlValue>;

/**
 * Query result set
 */
struct ResultSet {
    std::vector<std::string> columns;
    std::vector<Row> rows;
    std::string error;
    bool success;

    ResultSet();
};

/**
 * Query types
 */
enum class QueryType {
    CREATE_TABLE,
    CREATE_INDEX,
    DROP_TABLE,
    DROP_INDEX,
    INSERT,
    SELECT,
    UPDATE,
    DELETE,
    UNKNOWN
};

/**
 * WHERE clause condition
 */
struct WhereCondition {
    std::string column;
    std::string op;  // "=", "<", ">", "<=", ">=", "!=", "LIKE"
    SqlValue value;

    WhereCondition();
};

/**
 * Parsed SQL query
 */
struct ParsedQuery {
    QueryType type;
    std::string table;
    std::vector<std::string> columns;
    std::vector<SqlValue> values;
    std::vector<WhereCondition> where_conditions;
    std::string order_by_column;
    bool order_ascending;
    int limit;

    // For CREATE TABLE
    std::vector<ColumnDef> column_defs;

    // For CREATE INDEX
    std::string index_name;
    std::string index_column;

    ParsedQuery();
};

/**
 * Simple SQL engine for DigestiveDatabase
 * Supports: CREATE TABLE, INSERT, SELECT, UPDATE, DELETE, CREATE INDEX
 */
class SqlEngine {
public:
    SqlEngine(DigestiveDatabase* db);
    ~SqlEngine();

    /**
     * Execute SQL query
     * @param sql SQL query string
     * @return Result set
     */
    ResultSet execute(const std::string& sql);

    /**
     * Get table schema
     */
    std::optional<TableSchema> get_table_schema(const std::string& table) const;

    /**
     * Check if table exists
     */
    bool table_exists(const std::string& table) const;

    /**
     * Get all table names
     */
    std::vector<std::string> get_all_tables() const;

    /**
     * Save/load schemas
     */
    void save_schemas(const std::string& path);
    void load_schemas(const std::string& path);

private:
    DigestiveDatabase* db_;
    std::map<std::string, TableSchema> schemas_;

    // Parse SQL
    ParsedQuery parse_query(const std::string& sql);

    // Execute different query types
    ResultSet execute_create_table(const ParsedQuery& query);
    ResultSet execute_create_index(const ParsedQuery& query);
    ResultSet execute_insert(const ParsedQuery& query);
    ResultSet execute_select(const ParsedQuery& query);
    ResultSet execute_update(const ParsedQuery& query);
    ResultSet execute_delete(const ParsedQuery& query);
    ResultSet execute_drop_table(const ParsedQuery& query);
    ResultSet execute_drop_index(const ParsedQuery& query);

    // Helper methods
    std::string make_row_key(const std::string& table, uint64_t row_id) const;
    uint64_t extract_row_id(const std::string& key) const;
    std::vector<uint8_t> serialize_row(const Row& row) const;
    Row deserialize_row(const std::vector<uint8_t>& data, const TableSchema& schema) const;
    bool evaluate_where(const Row& row, const std::vector<WhereCondition>& conditions) const;
    std::string sql_value_to_string(const SqlValue& value) const;
    SqlValue string_to_sql_value(const std::string& str, SqlType type) const;

    // Tokenization
    std::vector<std::string> tokenize(const std::string& sql) const;
    std::string to_upper(const std::string& str) const;
    std::string trim(const std::string& str) const;
};

} // namespace digestive

#endif // SQL_ENGINE_HPP
