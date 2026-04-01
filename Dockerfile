# Stage 1: Build
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during apt-get (e.g. tzdata)
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    libsqlite3-dev \
    libpq-dev \
    qt6-base-dev \
    libqt6sql6-psql \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon from source
RUN git clone https://github.com/drogonframework/drogon.git /tmp/drogon \
    && cd /tmp/drogon \
    && git submodule update --init \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc) install \
    && rm -rf /tmp/drogon

# Set up project build
WORKDIR /app
COPY . .

# Build our License Server
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:22.04

# Avoid interactive prompts during apt-get (e.g. tzdata)
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libqt6core6 \
    libqt6sql6 \
    libqt6sql6-psql \
    libjsoncpp25 \
    libuuid1 \
    libssl3 \
    postgresql-client \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from builder
COPY --from=builder /app/build/LicenseServer /app/LicenseServer

# Copy essential files
COPY --from=builder /app/Bin /app/Bin
COPY --from=builder /app/public /app/public
COPY --from=builder /app/migration.json /app/migration.json
COPY --from=builder /app/config.json /app/config.json
COPY --from=builder /app/scripts/backup.sh /scripts/backup.sh

# Ensure the backup script is executable
RUN chmod +x /scripts/backup.sh

# Expose server port
EXPOSE 8080

# Set environment variable to find the LicenseLib in Bin/
ENV LD_LIBRARY_PATH=/app/Bin:$LD_LIBRARY_PATH

# Run the server
CMD ["/app/LicenseServer"]
