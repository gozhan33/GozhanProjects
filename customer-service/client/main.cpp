// Customer Service CLI — uses cpp-httplib (header-only) + nlohmann/json
//
// Build (from the client/ directory):
//   git clone --depth 1 https://github.com/yhirose/cpp-httplib
//   git clone --depth 1 https://github.com/nlohmann/json
//   g++ -std=c++17 -O2 -I cpp-httplib -I json/include main.cpp \
//       -o customer_cli -lssl -lcrypto
//
// Usage:
//   export API_URL=http://<EC2_PUBLIC_IP>:8080
//   ./customer_cli list
//   ./customer_cli create "Alice Smith" alice@example.com
//   ./customer_cli get 1
//   ./customer_cli update 1 --name "Alice Jones"
//   ./customer_cli update 1 --email newemail@example.com
//   ./customer_cli delete 1

// HTTP client library for making REST requests
#include "httplib.h"
// JSON parsing and serialization library
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstdlib>

// Alias for JSON operations
using json = nlohmann::json;

// API client configuration and factory for managing connection details
struct ApiClient {
    std::string host;
    int port = 8080;  // Default port for the backend service

    // Constructor: parses API_URL environment variable or defaults to localhost:8080
    ApiClient() {
        const char* url_env = std::getenv("API_URL");
        std::string base = url_env ? url_env : "http://localhost:8080";
        // Remove trailing slash if present
        if (!base.empty() && base.back() == '/') base.pop_back();

        // Remove http:// or https:// scheme prefix
        for (auto prefix : {"https://", "http://"}) {
            if (base.rfind(prefix, 0) == 0) {
                base = base.substr(strlen(prefix));
                break;
            }
        }
        // Extract host and port from the remaining string
        auto colon = base.rfind(':');
        if (colon != std::string::npos) {
            // Port is specified in the URL
            host = base.substr(0, colon);
            port = std::stoi(base.substr(colon + 1));
        } else {
            // No port specified, use host as-is with default port
            host = base;
        }
    }

    // Factory method: creates a configured HTTP client with timeouts
    httplib::Client make() const {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);      // 5 second connection timeout
        cli.set_read_timeout(10);           // 10 second read timeout
        return cli;
    }
};

// Helper function: prints a customer record in a formatted table
static void print_customer(const json& c) {
    std::cout << "  id         : " << c["id"]         << "\n"
              << "  name       : " << c["name"]        << "\n"
              << "  email      : " << c["email"]       << "\n"
              << "  created_at : " << c["created_at"]  << "\n";
}

// Helper function: prints API error messages from response body
static void print_error(const json& body, int status) {
    // Extract error message from response or use generic message
    std::string msg = body.contains("error")
        ? body["error"].get<std::string>() : "unknown error";
    std::cerr << "Error " << status << ": " << msg << "\n";
}

// Helper function: displays usage instructions for the CLI
static void usage() {
    std::cout <<
        "Usage:\n"
        "  customer_cli list\n"
        "  customer_cli create <name> <email>\n"
        "  customer_cli get <id>\n"
        "  customer_cli update <id> [--name <name>] [--email <email>]\n"
        "  customer_cli delete <id>\n"
        "\n"
        "Set API_URL env var (default: http://localhost:8080)\n";
}

// Command: list all customers from the API
static int cmd_list(ApiClient& api) {
    auto cli = api.make();
    // Send GET request to /customers endpoint
    auto res = cli.Get("/customers");
    // Check if connection was successful
    if (!res) { std::cerr << "Connection failed — is the service running?\n"; return 1; }
    // Parse JSON response body
    auto body = json::parse(res->body);
    // Check for HTTP errors
    if (res->status != 200) { print_error(body, res->status); return 1; }
    // Handle empty result
    if (body.empty()) { std::cout << "No customers.\n"; return 0; }
    // Display all customers
    std::cout << "Customers (" << body.size() << "):\n";
    for (const auto& c : body) {
        std::cout << "─────────────────────\n";
        print_customer(c);
    }
    return 0;
}

// Command: create a new customer with the given name and email
static int cmd_create(ApiClient& api, const std::string& name, const std::string& email) {
    auto cli = api.make();
    // Build JSON payload with customer data
    json payload = {{"name", name}, {"email", email}};
    // Send POST request to create customer
    auto res = cli.Post("/customers", payload.dump(), "application/json");
    if (!res) { std::cerr << "Connection failed\n"; return 1; }
    // Parse response body
    auto body = json::parse(res->body);
    if (res->status != 200) { print_error(body, res->status); return 1; }
    // Display the created customer with generated ID and timestamp
    std::cout << "Created:\n";
    print_customer(body);
    return 0;
}

// Command: retrieve a single customer by ID
static int cmd_get(ApiClient& api, int id) {
    auto cli = api.make();
    // Send GET request to /customers/{id} endpoint
    auto res = cli.Get("/customers/" + std::to_string(id));
    if (!res) { std::cerr << "Connection failed\n"; return 1; }
    // Parse response body
    auto body = json::parse(res->body);
    if (res->status != 200) { print_error(body, res->status); return 1; }
    print_customer(body);
    return 0;
}

// Command: update a customer's name and/or email
static int cmd_update(ApiClient& api, int id,
                      const std::string& name, const std::string& email) {
    auto cli = api.make();
    // Build JSON payload with only fields that were provided
    json payload;
    if (!name.empty())  payload["name"]  = name;
    if (!email.empty()) payload["email"] = email;
    // Send PUT request to update customer
    auto res = cli.Put("/customers/" + std::to_string(id),
                       payload.dump(), "application/json");
    if (!res) { std::cerr << "Connection failed\n"; return 1; }
    // Parse response body
    auto body = json::parse(res->body);
    if (res->status != 200) { print_error(body, res->status); return 1; }
    std::cout << "Updated:\n";
    print_customer(body);
    return 0;
}

// Command: delete a customer by ID
static int cmd_delete(ApiClient& api, int id) {
    auto cli = api.make();
    // Send DELETE request to /customers/{id} endpoint
    auto res = cli.Delete("/customers/" + std::to_string(id));
    if (!res) { std::cerr << "Connection failed\n"; return 1; }
    // Parse response body
    auto body = json::parse(res->body);
    if (res->status != 200) { print_error(body, res->status); return 1; }
    std::cout << "Deleted customer id=" << id << "\n";
    return 0;
}

// Main entry point: parses command-line arguments and dispatches to appropriate handler
int main(int argc, char* argv[]) {
    // Require at least one argument (the command name)
    if (argc < 2) { usage(); return 1; }
    // Initialize API client (parses API_URL environment variable)
    ApiClient api;
    std::string cmd = argv[1];

    // Dispatch to appropriate command handler
    if (cmd == "list") {
        return cmd_list(api);
    } else if (cmd == "create") {
        // Create requires name and email arguments
        if (argc < 4) { std::cerr << "Usage: customer_cli create <name> <email>\n"; return 1; }
        return cmd_create(api, argv[2], argv[3]);
    } else if (cmd == "get") {
        // Get requires customer ID
        if (argc < 3) { std::cerr << "Usage: customer_cli get <id>\n"; return 1; }
        return cmd_get(api, std::stoi(argv[2]));
    } else if (cmd == "update") {
        // Update requires at least ID, optionally --name and/or --email
        if (argc < 4) {
            std::cerr << "Usage: customer_cli update <id> [--name <n>] [--email <e>]\n";
            return 1;
        }
        int id = std::stoi(argv[2]);
        std::string name, email;
        // Parse optional --name and --email flags
        for (int i = 3; i < argc - 1; ++i) {
            std::string flag = argv[i];
            if (flag == "--name")  name  = argv[i + 1];
            if (flag == "--email") email = argv[i + 1];
        }
        // Require at least one field to update
        if (name.empty() && email.empty()) {
            std::cerr << "Provide --name and/or --email\n"; return 1;
        }
        return cmd_update(api, id, name, email);
    } else if (cmd == "delete") {
        // Delete requires customer ID
        if (argc < 3) { std::cerr << "Usage: customer_cli delete <id>\n"; return 1; }
        return cmd_delete(api, std::stoi(argv[2]));
    } else {
        // Unrecognized command
        std::cerr << "Unknown command: " << cmd << "\n\n";
        usage();
        return 1;
    }
}
