// Customer Service API — Crow HTTP microservice with PostgreSQL backend
// Uses pqxx 6.x (Ubuntu 22.04 compatible) for database operations
// Provides REST endpoints for customer CRUD operations

#include "crow.h"              // Crow C++ web framework
#include "db.h"                // Database connection helper
#include <pqxx/pqxx>           // PostgreSQL C++ client library
#include <nlohmann/json.hpp>   // JSON parsing and serialization
#include <string>
#include <vector>
#include <iostream>

// Type alias for JSON operations
using json = nlohmann::json;

// ── Response Helpers ──────────────────────────────────────────────────────────

// Helper: creates a successful JSON response with 200 OK status
static crow::response json_ok(const json& body) {
    crow::response res(200, body.dump());
    res.set_header("Content-Type", "application/json");
    return res;
}

// Helper: creates an error JSON response with specified HTTP status code
static crow::response json_err(int code, const std::string& msg) {
    crow::response res(code, json{{"error", msg}}.dump());
    res.set_header("Content-Type", "application/json");
    return res;
}

// Helper: validates email format (basic check: must contain @ and have text before and after)
static bool valid_email(const std::string& e) {
    auto at = e.find('@');
    // Check that @ exists at position > 0 and before the last 2 characters
    return at != std::string::npos && at > 0 && at < e.size() - 2;
}

// Helper: converts a PostgreSQL row to JSON object for API responses
static json row_to_json(const pqxx::row& row) {
    return {
        {"id",         row["id"].as<int>()},
        {"name",       row["name"].c_str()},
        {"email",      row["email"].c_str()},
        {"created_at", row["created_at"].c_str()}
    };
}

// ── Main Application ──────────────────────────────────────────────────────────

int main() {
    // Verify database is reachable at startup — fail fast if not
    try {
        auto conn = make_connection();
        std::cout << "[startup] DB connection OK\n";
    } catch (const std::exception& e) {
        std::cerr << "[startup] DB connection FAILED: " << e.what() << "\n";
        return 1;
    }

    // Initialize Crow application
    crow::SimpleApp app;

    // ── Route: GET /health ────────────────────────────────────────────────────
    // Health check endpoint — returns 200 OK with status message
    CROW_ROUTE(app, "/health")([] {
        return json_ok({{"status", "ok"}});
    });

    // ── Route: POST /customers ────────────────────────────────────────────────
    // Create a new customer
    // Request body: {"name":"Alice","email":"alice@example.com"}
    // Returns: 200 with created customer (including id and created_at)
    //          400 for validation errors
    //          409 if email already exists
    CROW_ROUTE(app, "/customers").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        // Parse JSON request body
        json body;
        try { body = json::parse(req.body); }
        catch (...) { return json_err(400, "Invalid JSON body"); }

        // Validate required fields
        if (!body.contains("name") || !body.contains("email"))
            return json_err(400, "Fields 'name' and 'email' are required");

        std::string name  = body["name"].get<std::string>();
        std::string email = body["email"].get<std::string>();

        // Validate field values
        if (name.empty())        return json_err(400, "'name' must not be empty");
        if (!valid_email(email)) return json_err(400, "'email' is invalid");

        // Insert customer into database
        try {
            auto conn = make_connection();
            pqxx::work txn(conn);
            // Use parameterized query to prevent SQL injection
            auto result = txn.exec_params(
                "INSERT INTO customers (name, email) "
                "VALUES ($1, $2) "
                "RETURNING id, name, email, created_at::text",
                name, email
            );
            txn.commit();
            return json_ok(row_to_json(result[0]));
        } catch (const pqxx::unique_violation&) {
            // Email uniqueness constraint violated
            return json_err(409, "A customer with that email already exists");
        } catch (const std::exception& e) {
            std::cerr << "[POST /customers] " << e.what() << "\n";
            return json_err(500, "Internal server error");
        }
    });

    // ── Route: GET /customers ─────────────────────────────────────────────────
    // Retrieve all customers, ordered by ID
    // Returns: 200 with array of customer objects
    CROW_ROUTE(app, "/customers").methods(crow::HTTPMethod::GET)
    ([] {
        try {
            auto conn = make_connection();
            pqxx::work txn(conn);
            // Fetch all customers from database
            auto result = txn.exec(
                "SELECT id, name, email, created_at::text "
                "FROM customers ORDER BY id"
            );
            txn.commit();

            // Convert rows to JSON array
            json arr = json::array();
            for (const auto& row : result) arr.push_back(row_to_json(row));
            return json_ok(arr);
        } catch (const std::exception& e) {
            std::cerr << "[GET /customers] " << e.what() << "\n";
            return json_err(500, "Internal server error");
        }
    });

    // ── Route: GET /customers/:id ─────────────────────────────────────────────
    // Retrieve a single customer by ID
    // Returns: 200 with customer object
    //          400 if id is invalid
    //          404 if customer not found
    CROW_ROUTE(app, "/customers/<int>").methods(crow::HTTPMethod::GET)
    ([](int id) {
        // Validate ID
        if (id <= 0) return json_err(400, "id must be a positive integer");
        try {
            auto conn = make_connection();
            pqxx::work txn(conn);
            // Fetch customer by ID
            auto result = txn.exec_params(
                "SELECT id, name, email, created_at::text "
                "FROM customers WHERE id = $1",
                id
            );
            txn.commit();
            if (result.empty()) return json_err(404, "Customer not found");
            return json_ok(row_to_json(result[0]));
        } catch (const std::exception& e) {
            std::cerr << "[GET /customers/" << id << "] " << e.what() << "\n";
            return json_err(500, "Internal server error");
        }
    });

    // ── Route: PUT /customers/:id ─────────────────────────────────────────────
    // Update a customer's name and/or email (partial updates supported)
    // Request body: {"name":"..."} and/or {"email":"..."}
    // Returns: 200 with updated customer
    //          400 for validation errors
    //          404 if customer not found
    //          409 if email already exists
    //
    // Note: pqxx 6.x (Ubuntu 22.04) lacks pqxx::params, so we use exec_params
    // with explicit arguments, handling each field combination separately.
    CROW_ROUTE(app, "/customers/<int>").methods(crow::HTTPMethod::PUT)
    ([](const crow::request& req, int id) {
        // Validate ID
        if (id <= 0) return json_err(400, "id must be a positive integer");

        // Parse JSON request body
        json body;
        try { body = json::parse(req.body); }
        catch (...) { return json_err(400, "Invalid JSON body"); }

        // Check which fields are provided for update
        bool has_name  = body.contains("name");
        bool has_email = body.contains("email");
        if (!has_name && !has_email)
            return json_err(400, "Provide at least one of 'name' or 'email'");

        // Validate provided fields
        std::string name, email;
        if (has_name) {
            name = body["name"].get<std::string>();
            if (name.empty()) return json_err(400, "'name' must not be empty");
        }
        if (has_email) {
            email = body["email"].get<std::string>();
            if (!valid_email(email)) return json_err(400, "'email' is invalid");
        }

        // Update customer in database
        try {
            auto conn = make_connection();
            pqxx::work txn(conn);
            pqxx::result result;

            // Handle the three possible field combinations (name, email, or both)
            // pqxx 6.x requires explicit overload selection
            if (has_name && has_email) {
                result = txn.exec_params(
                    "UPDATE customers SET name=$1, email=$2 "
                    "WHERE id=$3 "
                    "RETURNING id, name, email, created_at::text",
                    name, email, id
                );
            } else if (has_name) {
                result = txn.exec_params(
                    "UPDATE customers SET name=$1 "
                    "WHERE id=$2 "
                    "RETURNING id, name, email, created_at::text",
                    name, id
                );
            } else {
                // Update email only
                result = txn.exec_params(
                    "UPDATE customers SET email=$1 "
                    "WHERE id=$2 "
                    "RETURNING id, name, email, created_at::text",
                    email, id
                );
            }
            txn.commit();

            if (result.empty()) return json_err(404, "Customer not found");
            return json_ok(row_to_json(result[0]));
        } catch (const pqxx::unique_violation&) {
            // Email uniqueness constraint violated
            return json_err(409, "A customer with that email already exists");
        } catch (const std::exception& e) {
            std::cerr << "[PUT /customers/" << id << "] " << e.what() << "\n";
            return json_err(500, "Internal server error");
        }
    });

    // ── Route: DELETE /customers/:id ──────────────────────────────────────────
    // Delete a customer by ID
    // Returns: 200 with deleted_id confirmation
    //          400 if id is invalid
    //          404 if customer not found
    CROW_ROUTE(app, "/customers/<int>").methods(crow::HTTPMethod::DELETE)
    ([](int id) {
        // Validate ID
        if (id <= 0) return json_err(400, "id must be a positive integer");
        try {
            auto conn = make_connection();
            pqxx::work txn(conn);
            // Delete customer from database
            auto result = txn.exec_params(
                "DELETE FROM customers WHERE id=$1 RETURNING id",
                id
            );
            txn.commit();
            if (result.empty()) return json_err(404, "Customer not found");
            return json_ok({{"deleted_id", id}});
        } catch (const std::exception& e) {
            std::cerr << "[DELETE /customers/" << id << "] " << e.what() << "\n";
            return json_err(500, "Internal server error");
        }
    });

    // Start the HTTP server
    // Read port from PORT environment variable or use default 8080
    const char* port_env = std::getenv("PORT");
    uint16_t port = port_env ? static_cast<uint16_t>(std::stoi(port_env)) : 8080;
    
    std::cout << "[startup] Listening on :" << port << "\n";
    // Enable multithreaded request handling and start listening
    app.port(port).multithreaded().run();
}
