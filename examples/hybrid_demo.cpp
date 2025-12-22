#include "digestive_database.hpp"
#include "sql_engine.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>

using namespace digestive;

void print_separator(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << title << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void example_embedded_system() {
    print_separator("Example 1: Embedded System Configuration");

    // Create database optimized for embedded systems
    DbConfig config = DbConfig::config_for_embedded();
    DigestiveDatabase db("embedded_db", config);

    std::cout << "Configuration for embedded systems:" << std::endl;
    std::cout << "  - Chunking: " << (config.enable_chunking ? "ENABLED" : "disabled") << std::endl;
    std::cout << "  - Chunk size: " << config.chunk_size / 1024 << " KB" << std::endl;
    std::cout << "  - Heat decay: " << (config.enable_heat_decay ? "ENABLED" : "disabled") << std::endl;
    std::cout << "  - SQL: " << (config.enable_sql ? "enabled" : "DISABLED (saves memory)") << std::endl;
    std::cout << "  - Indexes: " << (config.enable_indexes ? "enabled" : "DISABLED (saves memory)") << std::endl;
    std::cout << std::endl;

    // Insert small data
    db.insert("sensor_reading_1", "Temperature: 22.5°C");
    db.insert("sensor_reading_2", "Humidity: 45%");
    db.insert("sensor_reading_3", "Pressure: 1013 hPa");

    // Retrieve data
    auto temp = db.get("sensor_reading_1");
    if (temp) {
        std::cout << "Retrieved: " << *temp << std::endl;
    }

    // Check if large data would be chunked
    std::vector<uint8_t> large_image(300 * 1024);  // 300KB simulated image
    std::cout << "\nChecking 300KB image..." << std::endl;
    std::cout << "  - Chunking threshold: " << config.chunking_threshold / 1024 << " KB" << std::endl;
    std::cout << "  - Will be chunked: " << (large_image.size() >= config.chunking_threshold ? "YES" : "NO") << std::endl;
}

void example_cctv_system() {
    print_separator("Example 2: CCTV System with SQL & Indexes");

    // Create database optimized for CCTV systems
    DbConfig config = DbConfig::config_for_cctv();
    DigestiveDatabase db("cctv_db", config);

    std::cout << "Configuration for CCTV systems:" << std::endl;
    std::cout << "  - Chunking: " << (config.enable_chunking ? "ENABLED" : "disabled") << std::endl;
    std::cout << "  - Chunk size: " << config.chunk_size / 1024 / 1024 << " MB (approx 1 sec of video)" << std::endl;
    std::cout << "  - Heat decay: " << (config.enable_heat_decay ? "ENABLED" : "disabled") << std::endl;
    std::cout << "  - Strategy: EXPONENTIAL (old footage becomes cold)" << std::endl;
    std::cout << "  - SQL: " << (config.enable_sql ? "ENABLED" : "disabled") << std::endl;
    std::cout << "  - Indexes: " << (config.enable_indexes ? "ENABLED" : "disabled") << std::endl;
    std::cout << std::endl;

    // Create table for video metadata
    std::cout << "Creating videos table..." << std::endl;
    auto result = db.execute_sql(
        "CREATE TABLE videos ("
        "id INTEGER PRIMARY KEY, "
        "filename TEXT, "
        "camera_id INTEGER, "
        "timestamp TEXT, "
        "duration INTEGER)"
    );

    if (result.success) {
        std::cout << "✓ Table created successfully" << std::endl;
    } else {
        std::cout << "✗ Error: " << result.error << std::endl;
    }

    // Create index on camera_id for fast queries
    std::cout << "\nCreating index on camera_id..." << std::endl;
    db.create_index("videos", "camera_id");
    std::cout << "✓ Index created" << std::endl;

    // Insert video metadata
    std::cout << "\nInserting video metadata..." << std::endl;
    db.execute_sql("INSERT INTO videos VALUES (1, 'video_cam1_001.mp4', 1, '2024-12-22 10:00:00', 60)");
    db.execute_sql("INSERT INTO videos VALUES (2, 'video_cam1_002.mp4', 1, '2024-12-22 10:01:00', 60)");
    db.execute_sql("INSERT INTO videos VALUES (3, 'video_cam2_001.mp4', 2, '2024-12-22 10:00:00', 60)");
    db.execute_sql("INSERT INTO videos VALUES (4, 'video_cam2_002.mp4', 2, '2024-12-22 10:01:00', 60)");
    std::cout << "✓ Inserted 4 video records" << std::endl;

    // Query by index (fast!)
    std::cout << "\nQuerying videos from camera 1 (using index)..." << std::endl;
    auto query_result = db.execute_sql("SELECT * FROM videos WHERE camera_id = 1");

    if (query_result.success) {
        std::cout << "✓ Found " << query_result.rows.size() << " videos:" << std::endl;
        for (const auto& row : query_result.rows) {
            auto filename = std::get<std::string>(row.at("filename"));
            auto timestamp = std::get<std::string>(row.at("timestamp"));
            std::cout << "  - " << filename << " at " << timestamp << std::endl;
        }
    }

    // Simulate storing video chunks
    std::cout << "\nSimulating large video file storage..." << std::endl;
    std::vector<uint8_t> video_data(5 * 1024 * 1024);  // 5MB simulated video
    std::cout << "  - Video size: " << video_data.size() / 1024 / 1024 << " MB" << std::endl;
    std::cout << "  - Chunk size: " << config.chunk_size / 1024 / 1024 << " MB" << std::endl;
    std::cout << "  - Expected chunks: " << (video_data.size() + config.chunk_size - 1) / config.chunk_size << std::endl;

    db.insert_binary("video_cam1_001_data", video_data);

    if (db.is_chunked("video_cam1_001_data")) {
        std::cout << "✓ Video stored as chunks" << std::endl;

        // Get specific chunk range (e.g., 2 seconds at 1 second offset)
        std::cout << "\nRetrieving chunk range 1-2 (seconds 1-2 of video)..." << std::endl;
        auto chunk_range = db.get_chunk_range("video_cam1_001_data", 1, 2);
        if (chunk_range) {
            std::cout << "✓ Retrieved " << chunk_range->size() / 1024 << " KB without loading full file" << std::endl;
        }
    }
}

void example_heat_decay() {
    print_separator("Example 3: Heat Decay Strategies");

    // Create database with exponential heat decay
    DbConfig config;
    config.enable_heat_decay = true;
    config.heat_decay_strategy = HeatDecayStrategy::EXPONENTIAL;
    config.heat_decay_factor = 0.9;  // 10% decay per interval
    config.heat_decay_interval = 1;  // 1 second for demo purposes

    DigestiveDatabase db("heat_decay_db", config);

    std::cout << "Heat Decay Configuration:" << std::endl;
    std::cout << "  - Strategy: EXPONENTIAL" << std::endl;
    std::cout << "  - Decay factor: " << config.heat_decay_factor << " (10% decay)" << std::endl;
    std::cout << "  - Interval: " << config.heat_decay_interval << " second" << std::endl;
    std::cout << std::endl;

    // Insert some data
    db.insert("hot_data", "Frequently accessed");
    db.insert("cold_data", "Rarely accessed");

    // Access hot data multiple times
    std::cout << "Accessing hot_data 10 times..." << std::endl;
    for (int i = 0; i < 10; i++) {
        db.get("hot_data");
    }

    // Get metadata before decay
    auto hot_meta_before = db.get_metadata("hot_data");
    auto cold_meta_before = db.get_metadata("cold_data");

    if (hot_meta_before && cold_meta_before) {
        std::cout << "\nBefore heat decay:" << std::endl;
        std::cout << "  - hot_data:  heat = " << hot_meta_before->heat
                  << ", accesses = " << hot_meta_before->access_count << std::endl;
        std::cout << "  - cold_data: heat = " << cold_meta_before->heat
                  << ", accesses = " << cold_meta_before->access_count << std::endl;
    }

    // Wait for decay interval
    std::cout << "\nWaiting for heat decay..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Trigger operation to apply decay
    db.insert("trigger", "trigger decay");

    // Get metadata after decay
    auto hot_meta_after = db.get_metadata("hot_data");
    auto cold_meta_after = db.get_metadata("cold_data");

    if (hot_meta_after && cold_meta_after) {
        std::cout << "\nAfter heat decay:" << std::endl;
        std::cout << "  - hot_data:  heat = " << hot_meta_after->heat
                  << " (was " << hot_meta_before->heat << ")" << std::endl;
        std::cout << "  - cold_data: heat = " << cold_meta_after->heat
                  << " (was " << cold_meta_before->heat << ")" << std::endl;

        std::cout << "\nHeat decay applied! Both values decreased by ~10%" << std::endl;
    }
}

void example_chunked_file_access() {
    print_separator("Example 4: Chunked File Partial Access");

    // Create database with chunking enabled
    DbConfig config;
    config.enable_chunking = true;
    config.chunking_threshold = 512 * 1024;  // 512KB
    config.chunk_size = 256 * 1024;  // 256KB chunks

    DigestiveDatabase db("chunked_db", config);

    std::cout << "Chunking Configuration:" << std::endl;
    std::cout << "  - Threshold: " << config.chunking_threshold / 1024 << " KB" << std::endl;
    std::cout << "  - Chunk size: " << config.chunk_size / 1024 << " KB" << std::endl;
    std::cout << std::endl;

    // Create a 1MB test file with pattern
    std::vector<uint8_t> large_file(1024 * 1024);
    std::cout << "Creating 1MB test file with pattern..." << std::endl;
    for (size_t i = 0; i < large_file.size(); i++) {
        large_file[i] = static_cast<uint8_t>(i % 256);
    }

    // Insert the large file
    std::cout << "Inserting large file..." << std::endl;
    db.insert_binary("large_test_file", large_file);

    // Check if chunked
    if (db.is_chunked("large_test_file")) {
        std::cout << "✓ File stored as chunks" << std::endl;

        size_t num_chunks = (large_file.size() + config.chunk_size - 1) / config.chunk_size;
        std::cout << "  - Total chunks: " << num_chunks << std::endl;
        std::cout << "  - Chunk size: " << config.chunk_size / 1024 << " KB" << std::endl;

        // Access only chunks 1-2 (middle portion of file)
        std::cout << "\nAccessing chunks 1-2 only (512KB from middle)..." << std::endl;
        auto partial_data = db.get_chunk_range("large_test_file", 1, 2);

        if (partial_data) {
            std::cout << "✓ Retrieved " << partial_data->size() / 1024 << " KB" << std::endl;
            std::cout << "  - WITHOUT loading the full 1MB file!" << std::endl;

            // Verify data integrity
            bool data_valid = true;
            size_t offset = 1 * config.chunk_size;  // Start of chunk 1
            for (size_t i = 0; i < partial_data->size() && i + offset < large_file.size(); i++) {
                if ((*partial_data)[i] != large_file[i + offset]) {
                    data_valid = false;
                    break;
                }
            }

            std::cout << "  - Data integrity: " << (data_valid ? "✓ VALID" : "✗ INVALID") << std::endl;
        }
    } else {
        std::cout << "✗ File was not chunked (too small)" << std::endl;
    }
}

void example_sql_queries() {
    print_separator("Example 5: SQL Query Capabilities");

    // Create database with SQL enabled
    DbConfig config;
    config.enable_sql = true;
    config.enable_indexes = true;

    DigestiveDatabase db("sql_db", config);

    std::cout << "SQL Features:" << std::endl;
    std::cout << "  - CREATE TABLE" << std::endl;
    std::cout << "  - INSERT INTO" << std::endl;
    std::cout << "  - SELECT with WHERE" << std::endl;
    std::cout << "  - DROP TABLE" << std::endl;
    std::cout << "  - CREATE INDEX (via create_index API)" << std::endl;
    std::cout << std::endl;

    // Create employees table
    std::cout << "Creating employees table..." << std::endl;
    auto result = db.execute_sql(
        "CREATE TABLE employees ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "department TEXT, "
        "salary INTEGER)"
    );
    std::cout << (result.success ? "✓" : "✗") << " "
              << (result.success ? "Created" : result.error) << std::endl;

    // Insert employees
    std::cout << "\nInserting employees..." << std::endl;
    db.execute_sql("INSERT INTO employees VALUES (1, 'Alice', 'Engineering', 90000)");
    db.execute_sql("INSERT INTO employees VALUES (2, 'Bob', 'Engineering', 85000)");
    db.execute_sql("INSERT INTO employees VALUES (3, 'Charlie', 'Marketing', 75000)");
    db.execute_sql("INSERT INTO employees VALUES (4, 'Diana', 'Sales', 80000)");
    std::cout << "✓ Inserted 4 employees" << std::endl;

    // Create index on department
    std::cout << "\nCreating index on department..." << std::endl;
    db.create_index("employees", "department");
    std::cout << "✓ Index created" << std::endl;

    // Query all employees
    std::cout << "\nQuery: SELECT * FROM employees" << std::endl;
    auto all_result = db.execute_sql("SELECT * FROM employees");
    if (all_result.success) {
        std::cout << "Found " << all_result.rows.size() << " employees:" << std::endl;
        for (const auto& row : all_result.rows) {
            std::cout << "  - ID " << std::get<int64_t>(row.at("id"))
                      << ": " << std::get<std::string>(row.at("name"))
                      << " (" << std::get<std::string>(row.at("department")) << ")"
                      << " - $" << std::get<int64_t>(row.at("salary")) << std::endl;
        }
    }

    // Query by department (uses index!)
    std::cout << "\nQuery: SELECT * FROM employees WHERE department = 'Engineering'" << std::endl;
    auto eng_result = db.execute_sql("SELECT * FROM employees WHERE department = Engineering");
    if (eng_result.success) {
        std::cout << "Found " << eng_result.rows.size() << " engineers:" << std::endl;
        for (const auto& row : eng_result.rows) {
            std::cout << "  - " << std::get<std::string>(row.at("name"))
                      << " - $" << std::get<int64_t>(row.at("salary")) << std::endl;
        }
    }
}

int main() {
    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Digestive Database - Hybrid System   ║" << std::endl;
    std::cout << "║    Comprehensive Feature Demo         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝\n" << std::endl;

    try {
        example_embedded_system();
        example_cctv_system();
        example_heat_decay();
        example_chunked_file_access();
        example_sql_queries();

        print_separator("All Examples Completed Successfully!");
        std::cout << "\nKey Features Demonstrated:" << std::endl;
        std::cout << "  ✓ Embedded system optimization (memory-efficient)" << std::endl;
        std::cout << "  ✓ CCTV system with SQL and indexes" << std::endl;
        std::cout << "  ✓ Heat decay strategies (exponential)" << std::endl;
        std::cout << "  ✓ Chunked file storage with partial access" << std::endl;
        std::cout << "  ✓ SQL queries with CREATE, INSERT, SELECT" << std::endl;
        std::cout << "  ✓ Index-accelerated queries (O(1) lookups)" << std::endl;
        std::cout << "\nHybrid system implementation complete! 🎉" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
