FROM ghcr.io/wiiu-env/devkitppc:latest

# Install wut-tools which provides wpsstrip (needed to produce .wps from .elf)
RUN dkp-pacman -Sy --noconfirm wut-tools

# Copy WUPS headers, specs and libs from the official wiiupluginsystem image
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO
