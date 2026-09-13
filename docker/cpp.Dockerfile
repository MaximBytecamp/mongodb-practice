FROM debian:bookworm

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      g++ make cmake pkg-config curl ca-certificates libssl-dev \
 && rm -rf /var/lib/apt/lists/*

# Драйвер C++ в пакетах Debian старый (3.x), поэтому собираем из исходников
# ту же пару версий, на которой проверены решения: сначала драйвер C, потом C++.
ARG C_DRIVER=2.5.3
ARG CXX_DRIVER=4.5.3

RUN curl -fsSL "https://github.com/mongodb/mongo-c-driver/releases/download/${C_DRIVER}/mongo-c-driver-${C_DRIVER}.tar.gz" \
      | tar -xz --no-same-owner -C /tmp \
 && cmake -S /tmp/mongo-c-driver-${C_DRIVER} -B /tmp/build-c \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DENABLE_TESTS=OFF -DENABLE_EXAMPLES=OFF -DBUILD_TESTING=OFF \
 && cmake --build /tmp/build-c -j"$(nproc)" \
 && cmake --install /tmp/build-c \
 && rm -rf /tmp/mongo-c-driver-${C_DRIVER} /tmp/build-c

RUN curl -fsSL "https://github.com/mongodb/mongo-cxx-driver/releases/download/r${CXX_DRIVER}/mongo-cxx-driver-r${CXX_DRIVER}.tar.gz" \
      | tar -xz --no-same-owner -C /tmp \
 && cmake -S /tmp/mongo-cxx-driver-r${CXX_DRIVER} -B /tmp/build-cxx \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_PREFIX_PATH=/usr/local \
      -DBUILD_TESTING=OFF \
 && cmake --build /tmp/build-cxx -j"$(nproc)" \
 && cmake --install /tmp/build-cxx \
 && ldconfig \
 && rm -rf /tmp/mongo-cxx-driver-r${CXX_DRIVER} /tmp/build-cxx

ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig

COPY docker/run.sh /usr/local/bin/run
RUN sed -i 's/\r$//' /usr/local/bin/run && chmod +x /usr/local/bin/run
