#include "digestive_database.hpp"
#include <iostream>
#include <string>

using namespace digestive;

int main() {
    std::cout << "=== Digestive Database - Simple Example ===\n" << std::endl;

    // 1. Create a database with default configuration
    DbConfig config = DbConfig::default_config();
    DigestiveDatabase db("my_simple_db", config);

    std::cout << "✓ Database created: my_simple_db\n" << std::endl;

    // 2. Insert some string data (key-value pairs)
    std::cout << "Inserting data..." << std::endl;
    db.insert("username", "alice");
    db.insert("email", "alice@example.com");
    db.insert("age", "25");
    std::cout << "✓ Inserted 3 key-value pairs\n" << std::endl;

    // 3. Retrieve data
    std::cout << "Retrieving data..." << std::endl;
    auto username = db.get("username");
    auto email = db.get("email");
    auto age = db.get("age");

    if (username && email && age) {
        std::cout << "  Username: " << *username << std::endl;
        std::cout << "  Email: " << *email << std::endl;
        std::cout << "  Age: " << *age << std::endl;
    }
    std::cout << std::endl;

    // 4. Insert binary data (e.g., simulating a small file)
    std::cout << "Inserting binary data..." << std::endl;
    std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello" in hex
    db.insert_binary("binary_key", binary_data);
    std::cout << "✓ Inserted binary data (5 bytes)\n" << std::endl;

    // 5. Retrieve binary data
    auto retrieved_binary = db.get_binary("binary_key");
    if (retrieved_binary) {
        std::cout << "Retrieved binary data: ";
        for (uint8_t byte : *retrieved_binary) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << "\n" << std::endl;
    }

    // 6. Update existing data
    std::cout << "Updating data..." << std::endl;
    db.insert("age", "26");  // Update age
    auto updated_age = db.get("age");
    if (updated_age) {
        std::cout << "  Updated age: " << *updated_age << "\n" << std::endl;
    }

    // 7. Remove data
    std::cout << "Removing data..." << std::endl;
    bool removed = db.remove("email");
    if (removed) {
        std::cout << "✓ Removed 'email' key" << std::endl;
    }

    // Verify it's gone
    auto check_email = db.get("email");
    if (!check_email) {
        std::cout << "✓ Confirmed: 'email' no longer exists\n" << std::endl;
    }

    // 8. View database statistics
    std::cout << "Database Statistics:" << std::endl;
    std::cout << "-------------------" << std::endl;
    db.print_stats();

    std::cout << "\n=== Example Complete! ===" << std::endl;
    std::cout << "\nWhat happened:" << std::endl;
    std::cout << "  • Created a database called 'my_simple_db'" << std::endl;
    std::cout << "  • Stored text and binary data" << std::endl;
    std::cout << "  • Retrieved and updated values" << std::endl;
    std::cout << "  • Removed a key" << std::endl;
    std::cout << "  • All data is automatically saved to disk!" << std::endl;
    std::cout << "\nDatabase files created in: my_simple_db.db/" << std::endl;

    return 0;
}
