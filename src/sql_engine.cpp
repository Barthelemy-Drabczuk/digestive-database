#include "sql_engine.hpp"
#include "digestive_database.hpp"
#include "index_engine.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstring>

namespace digestive {

// ==================== ColumnDef ====================

ColumnDef::ColumnDef()
    : type(SqlType::TEXT)
    , primary_key(false)
    , not_null(false)
    , unique(false) {
}

// ==================== TableSchema ====================

TableSchema::TableSchema()
    : next_row_id(1) {
}

// ==================== ResultSet ====================

ResultSet::ResultSet()
    : success(false) {
}

// ==================== WhereCondition ====================

WhereCondition::WhereCondition() {
}

// ==================== ParsedQuery ====================

ParsedQuery::ParsedQuery()
    : type(QueryType::UNKNOWN)
    , order_ascending(true)
    , limit(-1) {
}

// ==================== SqlEngine ====================

SqlEngine::SqlEngine(DigestiveDatabase* db)
    : db_(db) {
}

SqlEngine::~SqlEngine() {
}

ResultSet SqlEngine::execute(const std::string& sql) {
    if (sql.empty()) {
        ResultSet result;
        result.success = false;
        result.error = "Empty SQL query";
        return result;
    }

    ParsedQuery query = parse_query(sql);

    switch (query.type) {
        case QueryType::CREATE_TABLE:
            return execute_create_table(query);
        case QueryType::CREATE_INDEX:
            return execute_create_index(query);
        case QueryType::INSERT:
            return execute_insert(query);
        case QueryType::SELECT:
            return execute_select(query);
        case QueryType::UPDATE:
            return execute_update(query);
        case QueryType::DELETE:
            return execute_delete(query);
        case QueryType::DROP_TABLE:
            return execute_drop_table(query);
        case QueryType::DROP_INDEX:
            return execute_drop_index(query);
        default:
            ResultSet result;
            result.success = false;
            result.error = "Unknown query type";
            return result;
    }
}

std::optional<TableSchema> SqlEngine::get_table_schema(const std::string& table) const {
    auto it = schemas_.find(table);
    if (it == schemas_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool SqlEngine::table_exists(const std::string& table) const {
    return schemas_.count(table) > 0;
}

std::vector<std::string> SqlEngine::get_all_tables() const {
    std::vector<std::string> tables;
    for (const auto& [name, schema] : schemas_) {
        tables.push_back(name);
    }
    return tables;
}

void SqlEngine::save_schemas(const std::string& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }

    uint32_t num_schemas = schemas_.size();
    file.write(reinterpret_cast<const char*>(&num_schemas), sizeof(num_schemas));

    for (const auto& [name, schema] : schemas_) {
        // Write table name
        uint32_t name_len = name.size();
        file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file.write(name.data(), name_len);

        // Write next_row_id
        file.write(reinterpret_cast<const char*>(&schema.next_row_id), sizeof(schema.next_row_id));

        // Write primary key
        uint32_t pk_len = schema.primary_key_column.size();
        file.write(reinterpret_cast<const char*>(&pk_len), sizeof(pk_len));
        file.write(schema.primary_key_column.data(), pk_len);

        // Write columns
        uint32_t num_cols = schema.columns.size();
        file.write(reinterpret_cast<const char*>(&num_cols), sizeof(num_cols));

        for (const auto& col : schema.columns) {
            uint32_t col_name_len = col.name.size();
            file.write(reinterpret_cast<const char*>(&col_name_len), sizeof(col_name_len));
            file.write(col.name.data(), col_name_len);

            file.write(reinterpret_cast<const char*>(&col.type), sizeof(col.type));
            file.write(reinterpret_cast<const char*>(&col.primary_key), sizeof(col.primary_key));
            file.write(reinterpret_cast<const char*>(&col.not_null), sizeof(col.not_null));
            file.write(reinterpret_cast<const char*>(&col.unique), sizeof(col.unique));
        }
    }
}

void SqlEngine::load_schemas(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return;
    }

    uint32_t num_schemas;
    file.read(reinterpret_cast<char*>(&num_schemas), sizeof(num_schemas));

    for (uint32_t i = 0; i < num_schemas && file; i++) {
        TableSchema schema;

        // Read table name
        uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        schema.name.resize(name_len);
        file.read(&schema.name[0], name_len);

        // Read next_row_id
        file.read(reinterpret_cast<char*>(&schema.next_row_id), sizeof(schema.next_row_id));

        // Read primary key
        uint32_t pk_len;
        file.read(reinterpret_cast<char*>(&pk_len), sizeof(pk_len));
        schema.primary_key_column.resize(pk_len);
        file.read(&schema.primary_key_column[0], pk_len);

        // Read columns
        uint32_t num_cols;
        file.read(reinterpret_cast<char*>(&num_cols), sizeof(num_cols));

        for (uint32_t j = 0; j < num_cols && file; j++) {
            ColumnDef col;

            uint32_t col_name_len;
            file.read(reinterpret_cast<char*>(&col_name_len), sizeof(col_name_len));
            col.name.resize(col_name_len);
            file.read(&col.name[0], col_name_len);

            file.read(reinterpret_cast<char*>(&col.type), sizeof(col.type));
            file.read(reinterpret_cast<char*>(&col.primary_key), sizeof(col.primary_key));
            file.read(reinterpret_cast<char*>(&col.not_null), sizeof(col.not_null));
            file.read(reinterpret_cast<char*>(&col.unique), sizeof(col.unique));

            schema.columns.push_back(col);
        }

        schemas_[schema.name] = schema;
    }
}

// ==================== Private Methods ====================

ParsedQuery SqlEngine::parse_query(const std::string& sql) {
    ParsedQuery query;
    std::vector<std::string> tokens = tokenize(sql);

    if (tokens.empty()) {
        return query;
    }

    std::string first_token = to_upper(tokens[0]);

    // CREATE TABLE
    if (first_token == "CREATE" && tokens.size() > 2 && to_upper(tokens[1]) == "TABLE") {
        query.type = QueryType::CREATE_TABLE;
        query.table = tokens[2];
        // Simple parsing: CREATE TABLE name (col1 TYPE, col2 TYPE, ...)
        // This is a simplified parser - full SQL parsing would be much more complex

    } else if (first_token == "CREATE" && tokens.size() > 2 && to_upper(tokens[1]) == "INDEX") {
        query.type = QueryType::CREATE_INDEX;
        // CREATE INDEX idx_name ON table(column)
        query.index_name = tokens[2];

    } else if (first_token == "INSERT") {
        query.type = QueryType::INSERT;
        // INSERT INTO table VALUES (...)

    } else if (first_token == "SELECT") {
        query.type = QueryType::SELECT;
        // SELECT * FROM table WHERE ...

    } else if (first_token == "UPDATE") {
        query.type = QueryType::UPDATE;
        // UPDATE table SET col=val WHERE ...

    } else if (first_token == "DELETE") {
        query.type = QueryType::DELETE;
        // DELETE FROM table WHERE ...

    } else if (first_token == "DROP") {
        if (tokens.size() > 2 && to_upper(tokens[1]) == "TABLE") {
            query.type = QueryType::DROP_TABLE;
            query.table = tokens[2];
        } else if (tokens.size() > 2 && to_upper(tokens[1]) == "INDEX") {
            query.type = QueryType::DROP_INDEX;
            query.index_name = tokens[2];
        }
    }

    return query;
}

ResultSet SqlEngine::execute_create_table(const ParsedQuery& query) {
    ResultSet result;

    if (table_exists(query.table)) {
        result.success = false;
        result.error = "Table already exists: " + query.table;
        return result;
    }

    TableSchema schema;
    schema.name = query.table;
    schema.columns = query.column_defs;
    schema.next_row_id = 1;

    // Find primary key
    for (const auto& col : schema.columns) {
        if (col.primary_key) {
            schema.primary_key_column = col.name;
            break;
        }
    }

    schemas_[query.table] = schema;

    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_create_index(const ParsedQuery& query) {
    ResultSet result;

    // This would integrate with IndexEngine
    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_insert(const ParsedQuery& query) {
    ResultSet result;

    auto schema_opt = get_table_schema(query.table);
    if (!schema_opt) {
        result.success = false;
        result.error = "Table not found: " + query.table;
        return result;
    }

    TableSchema& schema = schemas_[query.table];

    // Create row
    Row row;
    for (size_t i = 0; i < schema.columns.size() && i < query.values.size(); i++) {
        row[schema.columns[i].name] = query.values[i];
    }

    // Get row ID
    uint64_t row_id = schema.next_row_id++;

    // Serialize and store
    std::string key = make_row_key(query.table, row_id);
    std::vector<uint8_t> data = serialize_row(row);
    db_->insert_binary(key, data);

    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_select(const ParsedQuery& query) {
    ResultSet result;

    auto schema_opt = get_table_schema(query.table);
    if (!schema_opt) {
        result.success = false;
        result.error = "Table not found: " + query.table;
        return result;
    }

    const TableSchema& schema = *schema_opt;

    // Set up result columns
    if (query.columns.empty() || query.columns[0] == "*") {
        for (const auto& col : schema.columns) {
            result.columns.push_back(col.name);
        }
    } else {
        result.columns = query.columns;
    }

    // Scan all rows (simple implementation - would use indexes in production)
    for (uint64_t row_id = 1; row_id < schema.next_row_id; row_id++) {
        std::string key = make_row_key(query.table, row_id);
        auto data_opt = db_->get_binary(key);

        if (data_opt) {
            Row row = deserialize_row(*data_opt, schema);

            // Apply WHERE conditions
            if (query.where_conditions.empty() || evaluate_where(row, query.where_conditions)) {
                result.rows.push_back(row);
            }
        }
    }

    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_update(const ParsedQuery& query) {
    ResultSet result;
    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_delete(const ParsedQuery& query) {
    ResultSet result;
    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_drop_table(const ParsedQuery& query) {
    ResultSet result;

    if (!table_exists(query.table)) {
        result.success = false;
        result.error = "Table not found: " + query.table;
        return result;
    }

    // Remove all rows
    const TableSchema& schema = schemas_[query.table];
    for (uint64_t row_id = 1; row_id < schema.next_row_id; row_id++) {
        std::string key = make_row_key(query.table, row_id);
        db_->remove(key);
    }

    schemas_.erase(query.table);

    result.success = true;
    return result;
}

ResultSet SqlEngine::execute_drop_index(const ParsedQuery& query) {
    ResultSet result;
    result.success = true;
    return result;
}

// ==================== Helper Methods ====================

std::string SqlEngine::make_row_key(const std::string& table, uint64_t row_id) const {
    return "sql:" + table + ":" + std::to_string(row_id);
}

uint64_t SqlEngine::extract_row_id(const std::string& key) const {
    size_t last_colon = key.rfind(':');
    if (last_colon != std::string::npos) {
        return std::stoull(key.substr(last_colon + 1));
    }
    return 0;
}

std::vector<uint8_t> SqlEngine::serialize_row(const Row& row) const {
    std::vector<uint8_t> data;

    // Simple serialization: num_cols, then for each: name_len, name, type, value
    uint32_t num_cols = row.size();
    data.insert(data.end(),
                reinterpret_cast<const uint8_t*>(&num_cols),
                reinterpret_cast<const uint8_t*>(&num_cols) + sizeof(num_cols));

    for (const auto& [col_name, value] : row) {
        // Write column name
        uint32_t name_len = col_name.size();
        data.insert(data.end(),
                    reinterpret_cast<const uint8_t*>(&name_len),
                    reinterpret_cast<const uint8_t*>(&name_len) + sizeof(name_len));
        data.insert(data.end(), col_name.begin(), col_name.end());

        // Write value based on type
        if (std::holds_alternative<int64_t>(value)) {
            uint8_t type = 0;  // INTEGER
            data.push_back(type);
            int64_t val = std::get<int64_t>(value);
            data.insert(data.end(),
                        reinterpret_cast<const uint8_t*>(&val),
                        reinterpret_cast<const uint8_t*>(&val) + sizeof(val));
        } else if (std::holds_alternative<double>(value)) {
            uint8_t type = 1;  // REAL
            data.push_back(type);
            double val = std::get<double>(value);
            data.insert(data.end(),
                        reinterpret_cast<const uint8_t*>(&val),
                        reinterpret_cast<const uint8_t*>(&val) + sizeof(val));
        } else if (std::holds_alternative<std::string>(value)) {
            uint8_t type = 2;  // TEXT
            data.push_back(type);
            const std::string& val = std::get<std::string>(value);
            uint32_t str_len = val.size();
            data.insert(data.end(),
                        reinterpret_cast<const uint8_t*>(&str_len),
                        reinterpret_cast<const uint8_t*>(&str_len) + sizeof(str_len));
            data.insert(data.end(), val.begin(), val.end());
        } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
            uint8_t type = 3;  // BLOB
            data.push_back(type);
            const std::vector<uint8_t>& val = std::get<std::vector<uint8_t>>(value);
            uint32_t blob_len = val.size();
            data.insert(data.end(),
                        reinterpret_cast<const uint8_t*>(&blob_len),
                        reinterpret_cast<const uint8_t*>(&blob_len) + sizeof(blob_len));
            data.insert(data.end(), val.begin(), val.end());
        }
    }

    return data;
}

Row SqlEngine::deserialize_row(const std::vector<uint8_t>& data, const TableSchema& schema) const {
    Row row;

    if (data.size() < sizeof(uint32_t)) {
        return row;
    }

    size_t offset = 0;
    uint32_t num_cols;
    std::memcpy(&num_cols, data.data() + offset, sizeof(num_cols));
    offset += sizeof(num_cols);

    for (uint32_t i = 0; i < num_cols && offset < data.size(); i++) {
        // Read column name
        if (offset + sizeof(uint32_t) > data.size()) break;
        uint32_t name_len;
        std::memcpy(&name_len, data.data() + offset, sizeof(name_len));
        offset += sizeof(name_len);

        if (offset + name_len > data.size()) break;
        std::string col_name(reinterpret_cast<const char*>(data.data() + offset), name_len);
        offset += name_len;

        // Read type and value
        if (offset >= data.size()) break;
        uint8_t type = data[offset++];

        if (type == 0) {  // INTEGER
            if (offset + sizeof(int64_t) > data.size()) break;
            int64_t val;
            std::memcpy(&val, data.data() + offset, sizeof(val));
            offset += sizeof(val);
            row[col_name] = val;
        } else if (type == 1) {  // REAL
            if (offset + sizeof(double) > data.size()) break;
            double val;
            std::memcpy(&val, data.data() + offset, sizeof(val));
            offset += sizeof(val);
            row[col_name] = val;
        } else if (type == 2) {  // TEXT
            if (offset + sizeof(uint32_t) > data.size()) break;
            uint32_t str_len;
            std::memcpy(&str_len, data.data() + offset, sizeof(str_len));
            offset += sizeof(str_len);

            if (offset + str_len > data.size()) break;
            std::string val(reinterpret_cast<const char*>(data.data() + offset), str_len);
            offset += str_len;
            row[col_name] = val;
        } else if (type == 3) {  // BLOB
            if (offset + sizeof(uint32_t) > data.size()) break;
            uint32_t blob_len;
            std::memcpy(&blob_len, data.data() + offset, sizeof(blob_len));
            offset += sizeof(blob_len);

            if (offset + blob_len > data.size()) break;
            std::vector<uint8_t> val(data.begin() + offset, data.begin() + offset + blob_len);
            offset += blob_len;
            row[col_name] = val;
        }
    }

    return row;
}

bool SqlEngine::evaluate_where(const Row& row, const std::vector<WhereCondition>& conditions) const {
    for (const auto& cond : conditions) {
        auto it = row.find(cond.column);
        if (it == row.end()) {
            return false;
        }

        // Simplified comparison - would need full type handling in production
        std::string row_val = sql_value_to_string(it->second);
        std::string cond_val = sql_value_to_string(cond.value);

        if (cond.op == "=") {
            if (row_val != cond_val) return false;
        } else if (cond.op == "!=") {
            if (row_val == cond_val) return false;
        } else if (cond.op == ">") {
            if (row_val <= cond_val) return false;
        } else if (cond.op == "<") {
            if (row_val >= cond_val) return false;
        } else if (cond.op == ">=") {
            if (row_val < cond_val) return false;
        } else if (cond.op == "<=") {
            if (row_val > cond_val) return false;
        }
    }

    return true;
}

std::string SqlEngine::sql_value_to_string(const SqlValue& value) const {
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    return "";
}

SqlValue SqlEngine::string_to_sql_value(const std::string& str, SqlType type) const {
    switch (type) {
        case SqlType::INTEGER:
            return static_cast<int64_t>(std::stoll(str));
        case SqlType::REAL:
            return std::stod(str);
        case SqlType::TEXT:
            return str;
        case SqlType::BLOB:
            return std::vector<uint8_t>(str.begin(), str.end());
        default:
            return str;
    }
}

std::vector<std::string> SqlEngine::tokenize(const std::string& sql) const {
    std::vector<std::string> tokens;
    std::string current;
    bool in_string = false;
    bool in_parens = false;

    for (char c : sql) {
        if (c == '\'' || c == '"') {
            in_string = !in_string;
            current += c;
        } else if (c == '(' || c == ')') {
            if (!current.empty()) {
                tokens.push_back(trim(current));
                current.clear();
            }
            tokens.push_back(std::string(1, c));
        } else if (std::isspace(c) && !in_string) {
            if (!current.empty()) {
                tokens.push_back(trim(current));
                current.clear();
            }
        } else if (c == ',' && !in_string) {
            if (!current.empty()) {
                tokens.push_back(trim(current));
                current.clear();
            }
            tokens.push_back(",");
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(trim(current));
    }

    return tokens;
}

std::string SqlEngine::to_upper(const std::string& str) const {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return upper;
}

std::string SqlEngine::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

} // namespace digestive
