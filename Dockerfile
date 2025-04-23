# Stage 1: Builder
FROM manjarolinux/base:latest AS builder

LABEL maintainer="Nikita Selin <farvard34@gmail.com>"

# Set environment variable to avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install build dependencies
RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        base-devel \
        cmake \
        qt6-base \
        qt6-tools \
        git # If you need to clone source code \
        kcoreaddons ki18n kxmlgui ktextwidgets kconfigwidgets kwidgetsaddons kio kiconthemes at-spi2-core # Add AT-SPI library \

WORKDIR /app
COPY CMakeLists.txt ./
COPY src ./src


# Configure and build the application
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/app/install
RUN cmake --build build --target install # Use install target defined in CMakeLists.txt


