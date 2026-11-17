FROM ghcr.io/wiiu-env/devkitppc:latest
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO
RUN dkp-pacman -Sy --noconfirm wiiu-acp 2>/dev/null || true
