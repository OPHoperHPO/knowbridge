# --- File: ../Dockerfile ---
# Stage 1: Builder
FROM archlinux:base-devel AS builder

LABEL maintainer="Nikita Selin <farvard34@gmail.com>"

# Update package lists and install build dependencies
# Removed irrelevant ENV DEBIAN_FRONTEND
# Corrected package names for KF6 and added missing dependencies
RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        base-devel \
        cmake \
        extra-cmake-modules \
        qt6-base \
        qt6-tools \
        kcoreaddons \
        kglobalaccel \
        ki18n \
        kxmlgui \
        knotifications \
        at-spi2-core \
        atk \
        glib2 \
        # git # Optional: Only needed if CMake downloads things via git
    && \
    # Clean up package cache
    pacman -Scc --noconfirm && \
    rm -rf /var/cache/pacman/pkg/* /tmp/*

WORKDIR /app
COPY CMakeLists.txt ./
COPY assets ./assets
COPY src ./src


# Configure and build the application
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/app/install
RUN cmake --build build --target install # Use install target defined in CMakeLists.txt