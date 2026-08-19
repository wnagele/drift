# Runs the firmware end-to-end tests headless in Espressif's QEMU fork
# (ESP32-C3 / RISC-V), see entrypoint.sh.
# Upstream QEMU has no esp32c3 machine, so we use the prebuilt release from
# https://github.com/espressif/qemu
FROM debian:bookworm-slim

ARG QEMU_VERSION=esp_develop_9.2.2_20260417
ARG QEMU_TAG=esp-develop-9.2.2-20260417
ARG TARGETARCH

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils python3 python3-pip gdb-multiarch \
        libglib2.0-0 libpixman-1-0 libslirp0 libsdl2-2.0-0 libgcrypt20 \
    && pip3 install --no-cache-dir --break-system-packages \
        "esptool~=4.11" "esp-idf-nvs-partition-gen" \
    && apt-get purge -y python3-pip && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*

RUN case "${TARGETARCH}" in \
        arm64) QEMU_ARCH=aarch64-linux-gnu ;; \
        amd64) QEMU_ARCH=x86_64-linux-gnu ;; \
        *) echo "unsupported arch ${TARGETARCH}" >&2; exit 1 ;; \
    esac \
    && curl -fsSL -o /tmp/qemu.tar.xz \
        "https://github.com/espressif/qemu/releases/download/${QEMU_TAG}/qemu-riscv32-softmmu-${QEMU_VERSION}-${QEMU_ARCH}.tar.xz" \
    && tar -xJf /tmp/qemu.tar.xz -C /opt \
    && rm /tmp/qemu.tar.xz \
    && ln -s /opt/qemu/bin/qemu-system-riscv32 /usr/local/bin/qemu-system-riscv32

COPY entrypoint.sh /usr/local/bin/run-e2e
RUN chmod +x /usr/local/bin/run-e2e

WORKDIR /work
ENTRYPOINT ["/usr/local/bin/run-e2e"]
