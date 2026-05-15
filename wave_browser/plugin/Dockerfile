FROM ghcr.io/wiiu-env/devkitppc:latest

ENV DEVKITPRO=/opt/devkitpro
ENV PATH=$PATH:/opt/devkitpro/tools/bin:/opt/devkitpro/devkitPPC/bin

# Build wpsstrip from source (it's a host tool, not in the artifacts image)
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
        git cmake build-essential ca-certificates \
    && git clone --depth=1 https://github.com/wiiu-env/WiiUPluginSystem /tmp/wups-src \
    && cmake -S /tmp/wups-src/tools/wpsstrip -B /tmp/wpsstrip-build \
    && cmake --build /tmp/wpsstrip-build \
    && cp /tmp/wpsstrip-build/wpsstrip /opt/devkitpro/tools/bin/wpsstrip \
    && chmod +x /opt/devkitpro/tools/bin/wpsstrip \
    && rm -rf /tmp/wups-src /tmp/wpsstrip-build \
    && apt-get purge -y git cmake build-essential \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*

# Copy WUPS headers, specs and libs from the official image
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO
