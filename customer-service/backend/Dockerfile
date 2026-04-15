# Multi-stage Docker build for customer_service microservice
# Stage 1: Builder — compiles C++ application with dependencies
# Stage 2: Runtime — minimal production image with only runtime dependencies

# ── Build Arguments ───────────────────────────────────────────────────────────
# These can be overridden at build time: docker build --build-arg UBUNTU_VERSION=20.04 ...
ARG UBUNTU_VERSION=22.04
ARG CMAKE_BUILD_TYPE=Release

# ── Stage 1: Build ────────────────────────────────────────────────────────────
FROM ubuntu:${UBUNTU_VERSION} AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libpqxx-dev \
    libssl-dev \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

# Set build directory as working directory
WORKDIR /build

# Copy build configuration and source code
COPY CMakeLists.txt .
COPY src/ src/

# Configure cmake build
# -B: specify build directory
# -DCMAKE_BUILD_TYPE: optimization level (Release, Debug, etc.)
RUN cmake -B build -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} && \
    cmake --build build --parallel "$(nproc)"

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
# Minimal production image with only runtime dependencies
FROM ubuntu:${UBUNTU_VERSION} AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libpqxx-6.4 \
    libssl3 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security (adheres to principle of least privilege)
# UID 1001 avoids conflicts with system users
RUN useradd -m -u 1001 appuser
# Run as non-root user
USER appuser

# Set application working directory
WORKDIR /app

# Copy compiled application from builder stage
# Only the binary is needed — build artifacts are left behind in builder
COPY --from=builder /build/build/customer_service .

# ── Environment Variables ───────────────────────────────────
# HTTP server port
ENV PORT=8080
# PostgreSQL connection parameters (override these at runtime)
ENV DB_HOST=""
ENV DB_PORT=5432
ENV DB_USER=""
ENV DB_PASS=""
ENV DB_NAME="customerdb"

# Expose HTTP port
EXPOSE 8080

# ── Health Check ──────────────────────────────────────────
# Docker container health probe
# Check GET /health endpoint every 30s, timeout after 5s, start after 15s
HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
    CMD curl -sf http://localhost:8080/health || exit 1

# ── Graceful Shutdown ──────────────────────────────────────
# Use SIGTERM for graceful shutdown (Docker default)
STOPSIGNAL SIGTERM

# ── Image Metadata ─────────────────────────────────────────
# Docker image labels for documentation and tooling
LABEL name="customer-service"
LABEL description="Customer Service API microservice built with Crow and PostgreSQL"
LABEL version="1.0.0"
LABEL maintainer="GozhanProjects"
LABEL org.opencontainers.image.source="https://github.com/GozhanProjects/customer-service"

# Start the application
CMD ["./customer_service"]
