#pragma once
#include <pqxx/pqxx>
#include <string>
#include <stdexcept>
#include <cstdlib>

// Reads DB config from environment variables and returns a libpqxx connection.
// Required env vars: DB_HOST, DB_USER, DB_PASS, DB_NAME
// Optional:          DB_PORT (default 5432)
inline std::string build_conn_string() {
    auto get_env = [](const char* key, const char* fallback = nullptr) -> std::string {
        const char* val = std::getenv(key);
        if (!val || std::string(val).empty()) {
            if (!fallback)
                throw std::runtime_error(std::string("Missing required env var: ") + key);
            return fallback;
        }
        return val;
    };

    // DB_SSLMODE defaults to "prefer" which works for both:
    //   - local Docker postgres (no SSL) → falls back to unencrypted
    //   - AWS RDS (SSL enabled)          → uses SSL automatically
    // Set DB_SSLMODE=require in production (docker run -e) to enforce SSL on RDS.
    return "host="      + get_env("DB_HOST")
         + " port="     + get_env("DB_PORT", "5432")
         + " user="     + get_env("DB_USER")
         + " password=" + get_env("DB_PASS")
         + " dbname="   + get_env("DB_NAME")
         + " sslmode="  + get_env("DB_SSLMODE", "prefer");
}

inline pqxx::connection make_connection() {
    return pqxx::connection(build_conn_string());
}
