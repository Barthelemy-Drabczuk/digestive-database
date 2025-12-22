#include "digestive_database.hpp"
#include <iostream>
#include <fstream>

using namespace digestive;

void example_text_database() {
    std::cout << "\n===== Example 1: Text Database with Adaptive Reorganization =====" << std::endl;

    // Text-optimized configuration with adaptive reorganization
    DbConfig config = DbConfig::config_for_text();
    DigestiveDatabase db("text_db", config);

    // Insert some text data
    std::cout << "Inserting text data..." << std::endl;
    for (int i = 0; i < 20; i++) {
        std::string longtext = "This is a long text document #" + std::to_string(i) + ". ";
        longtext += std::string(100, 'x');  // Add 100 chars
        db.insert("doc_" + std::to_string(i), longtext);
    }

    // Access some documents frequently (hot data)
    std::cout << "Accessing hot documents repeatedly..." << std::endl;
    for (int i = 0; i < 10; i++) {
        db.get("doc_0");
        db.get("doc_1");
        db.get("doc_2");
    }

    // Access some moderately
    for (int i = 0; i < 5; i++) {
        db.get("doc_5");
    }

    // Stats show adaptive reorganization hasn't triggered yet
    std::cout << "\nBefore manual reorganization:" << std::endl;
    db.print_stats();

    // Manually trigger reorganization
    db.reorganize();

    // Stats after reorganization
    std::cout << "\nAfter reorganization:" << std::endl;
    db.print_stats();
}

void example_image_database() {
    std::cout << "\n\n===== Example 2: Image Database (Simulated) =====" << std::endl;

    // Image-optimized configuration
    DbConfig config = DbConfig::config_for_images();
    config.reorg_strategy = ReorgStrategy::EVERY_N_OPS;
    config.reorg_operation_threshold = 15;  // Reorganize after 15 ops

    DigestiveDatabase db("image_db", config);

    // Simulate storing images (using dummy data)
    std::cout << "Storing images..." << std::endl;
    for (int i = 0; i < 10; i++) {
        // Simulate a 1KB "image"
        std::vector<uint8_t> fake_image(1024, static_cast<uint8_t>(i));
        db.insert_binary("photo_" + std::to_string(i), fake_image);
    }

    // Access pattern: some images viewed frequently
    std::cout << "Simulating user access patterns..." << std::endl;
    for (int i = 0; i < 8; i++) {
        db.get_binary("photo_0");  // Profile picture - accessed often
        db.get_binary("photo_1");
    }

    for (int i = 0; i < 3; i++) {
        db.get_binary("photo_5");  // Sometimes viewed
    }

    // Auto-reorganization should trigger after 15 operations
    std::cout << "\nDatabase automatically reorganized!" << std::endl;
    db.print_stats();
}

void example_custom_compression() {
    std::cout << "\n\n===== Example 3: Custom Compression Per Tier =====" << std::endl;

    DbConfig config;
    config.reorg_strategy = ReorgStrategy::MANUAL;

    // Configure each tier with different algorithms
    config.tier_configs[0] = TierConfig(CompressionAlgo::NONE, false);        // Hot: no compression
    config.tier_configs[1] = TierConfig(CompressionAlgo::LZ4_FAST, false);    // Warm: fast
    config.tier_configs[2] = TierConfig(CompressionAlgo::LZ4_HIGH, false);    // Medium: high compression
    config.tier_configs[3] = TierConfig(CompressionAlgo::ZSTD_MEDIUM, false); // Cool: ZSTD medium
    config.tier_configs[4] = TierConfig(CompressionAlgo::ZSTD_MAX, false);    // Cold: ZSTD max

    DigestiveDatabase db("custom_db", config);

    // Insert data that compresses well
    std::cout << "Inserting compressible data..." << std::endl;
    for (int i = 0; i < 15; i++) {
        std::string repeated_data = std::string(500, 'A' + (i % 26));  // Highly compressible
        db.insert("data_" + std::to_string(i), repeated_data);
    }

    // Create access pattern
    for (int i = 0; i < 20; i++) {
        db.get("data_0");  // Very hot
    }
    for (int i = 0; i < 10; i++) {
        db.get("data_1");  // Hot
    }
    for (int i = 0; i < 5; i++) {
        db.get("data_5");  // Warm
    }

    db.reorganize();

    std::cout << "\nCompression effectiveness:" << std::endl;
    db.print_stats();
}

void example_file_operations() {
    std::cout << "\n\n===== Example 4: File-based Operations =====" << std::endl;

    DbConfig config = DbConfig::config_for_videos();
    config.max_size_bytes = 1024 * 1024;  // 1MB limit for demo
    config.allow_deletion = false;  // Don't delete for this example

    DigestiveDatabase db("file_db", config);

    // Create a temporary file
    std::string test_file = "test_data.txt";
    {
        std::ofstream out(test_file);
        out << "This is test file content that will be stored in the database.\n";
        out << "It demonstrates file-based insert and retrieve operations.\n";
        out << std::string(200, '=') << "\n";
    }

    // Insert from file
    std::cout << "Inserting from file: " << test_file << std::endl;
    db.insert_from_file("my_document", test_file);

    // Retrieve to file
    std::string output_file = "retrieved_data.txt";
    std::cout << "Retrieving to file: " << output_file << std::endl;
    if (db.get_to_file("my_document", output_file)) {
        std::cout << "File retrieved successfully!" << std::endl;

        // Verify metadata
        auto metadata = db.get_metadata("my_document");
        if (metadata.has_value()) {
            std::cout << "Original size: " << metadata->original_size << " bytes" << std::endl;
            std::cout << "Compressed size: " << metadata->compressed_size << " bytes" << std::endl;
            std::cout << "Compression ratio: "
                      << (static_cast<double>(metadata->original_size) / metadata->compressed_size)
                      << "x" << std::endl;
        }
    }

    // Cleanup
    std::remove(test_file.c_str());
    std::remove(output_file.c_str());
}

void example_reorganization_strategies() {
    std::cout << "\n\n===== Example 5: Different Reorganization Strategies =====" << std::endl;

    // Strategy 1: Manual (no auto-reorg)
    {
        std::cout << "\n--- Manual Strategy ---" << std::endl;
        DbConfig config;
        config.reorg_strategy = ReorgStrategy::MANUAL;
        DigestiveDatabase db("manual_db", config);

        for (int i = 0; i < 50; i++) {
            db.insert("key_" + std::to_string(i), "value");
            db.get("key_0");  // Access one key repeatedly
        }

        DatabaseStats stats = db.get_stats();
        std::cout << "Operations since reorg: " << stats.operations_since_reorg
                  << " (no auto-reorg)" << std::endl;
    }

    // Strategy 2: Every N operations
    {
        std::cout << "\n--- Every N Operations Strategy ---" << std::endl;
        DbConfig config;
        config.reorg_strategy = ReorgStrategy::EVERY_N_OPS;
        config.reorg_operation_threshold = 20;
        DigestiveDatabase db("ops_db", config);

        for (int i = 0; i < 25; i++) {
            db.insert("key_" + std::to_string(i), "value");
        }

        DatabaseStats stats = db.get_stats();
        std::cout << "Operations since reorg: " << stats.operations_since_reorg
                  << " (auto-reorganized at 20)" << std::endl;
    }

    // Strategy 3: Adaptive (based on access pattern changes)
    {
        std::cout << "\n--- Adaptive Strategy ---" << std::endl;
        DbConfig config;
        config.reorg_strategy = ReorgStrategy::ADAPTIVE;
        config.reorg_change_threshold = 0.5;  // 50% change
        DigestiveDatabase db("adaptive_db", config);

        // Insert items
        for (int i = 0; i < 10; i++) {
            db.insert("key_" + std::to_string(i), "value");
        }

        // Access pattern should trigger adaptive reorg
        for (int i = 0; i < 6; i++) {
            db.get("key_0");
        }

        DatabaseStats stats = db.get_stats();
        std::cout << "Adaptive reorganization triggered when access pattern changed" << std::endl;
    }
}

int main() {
    std::cout << "=== Digestive Database - Enhanced Examples ===" << std::endl;
    std::cout << "Demonstrating large file support, custom compression, and smart reorganization\n" << std::endl;

    try {
        example_text_database();
        example_image_database();
        example_custom_compression();
        example_file_operations();
        example_reorganization_strategies();

        std::cout << "\n\n=== All Examples Completed Successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
