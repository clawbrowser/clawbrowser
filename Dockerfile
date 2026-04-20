FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    dbus-x11 \
    fonts-dejavu-core \
    fonts-liberation \
    fonts-noto-color-emoji \
    libasound2 \
    libatk-bridge2.0-0 \
    libatk1.0-0 \
    libatspi2.0-0 \
    libcairo2 \
    libcups2 \
    libdav1d6 \
    libdbus-1-3 \
    libdouble-conversion3 \
    libdrm2 \
    libegl1 \
    libflac12 \
    libfontconfig1 \
    libgbm1 \
    libgl1 \
    libglib2.0-0 \
    libgtk-3-0 \
    libharfbuzz-subset0 \
    libjpeg62-turbo \
    libminizip1 \
    libnspr4 \
    libnss3 \
    libopenh264-7 \
    libopenjp2-7 \
    libopus0 \
    libpci3 \
    libpango-1.0-0 \
    libpangocairo-1.0-0 \
    libpulse0 \
    libu2f-udev \
    libvulkan1 \
    libwayland-client0 \
    libx11-6 \
    libx11-xcb1 \
    libxcb1 \
    libxcomposite1 \
    libxcursor1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxi6 \
    libxinerama1 \
    libxkbcommon0 \
    libxnvctrl0 \
    libxrandr2 \
    libxrender1 \
    libxshmfence1 \
    libxss1 \
    libxtst6 \
    socat \
    tini \
    xauth \
    xdg-utils \
    xvfb \
  && rm -rf /var/lib/apt/lists/*

RUN groupadd --system clawbrowser \
  && useradd --system --create-home --gid clawbrowser clawbrowser \
  && mkdir -p /opt/clawbrowser /tmp/.X11-unix /tmp/clawbrowser-runtime \
  && chmod 1777 /tmp/.X11-unix \
  && chown -R clawbrowser:clawbrowser /opt/clawbrowser /tmp/clawbrowser-runtime

COPY docker-entrypoint.sh /usr/local/bin/clawbrowser-docker-entrypoint
RUN chmod +x /usr/local/bin/clawbrowser-docker-entrypoint

COPY clawbrowser-dist/ /opt/clawbrowser/

ENV DISPLAY=:99 \
    XVFB_WHD=1920x1080x24 \
    CLAWBROWSER_WINDOW_SIZE=1920,1080 \
    XDG_RUNTIME_DIR=/tmp/clawbrowser-runtime \
    CLAWBROWSER_BROWSER_BINARY=/opt/clawbrowser/clawbrowser.real \
    CLAWBROWSER_NO_SANDBOX=1

WORKDIR /home/clawbrowser
USER clawbrowser

EXPOSE 9222

ENTRYPOINT ["tini", "--", "/usr/local/bin/clawbrowser-docker-entrypoint"]
CMD ["/opt/clawbrowser/clawbrowser.real", "about:blank"]
