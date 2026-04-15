-- Run once to initialize the database.
-- Via SSH tunnel: psql -h localhost -p 5433 -U csadmin -d customerdb -f schema.sql

CREATE TABLE IF NOT EXISTS customers (
    id         SERIAL       PRIMARY KEY,
    name       VARCHAR(255) NOT NULL,
    email      VARCHAR(255) NOT NULL UNIQUE,
    created_at TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_customers_email ON customers(email);
