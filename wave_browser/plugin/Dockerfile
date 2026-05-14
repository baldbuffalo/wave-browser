FROM ghcr.io/wiiu-env/devkitppc:latest

ENV DEVKITPRO=/opt/devkitpro
ENV PATH=$PATH:/opt/devkitpro/tools/bin:/opt/devkitpro/devkitPPC/bin

# wut-tools provides elf2rpl etc — already present but reinstall to be safe
RUN dkp-pacman -Sy --noconfirm wut-tools

# Copy WUPS headers, specs, libs and tools from the official wiiupluginsystem image
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO

# wpsstrip lands somewhere in the WUPS artifacts — find it and symlink to tools/bin
RUN find /opt/devkitpro -name "wpsstrip" 2>/dev/null && \
    WPSSTRIP=$(find /opt/devkitpro -name "wpsstrip" 2>/dev/null | head -1) && \
    if [ -n "$WPSSTRIP" ] && [ "$WPSSTRIP" != "/opt/devkitpro/tools/bin/wpsstrip" ]; then \
        ln -sf "$WPSSTRIP" /opt/devkitpro/tools/bin/wpsstrip && \
        echo "Symlinked wpsstrip from $WPSSTRIP"; \
    elif [ -z "$WPSSTRIP" ]; then \
        echo "ERROR: wpsstrip not found anywhere!" && exit 1; \
    else \
        echo "wpsstrip already at correct path"; \
    fi
