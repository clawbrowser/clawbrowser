# Docker Runtime for ClawBrowser

This repo does not build Chromium inside Docker.

Instead, the Docker runtime packages an existing **Linux** browser artifact and
runs it in a **headed** mode on a virtual monitor provided by `Xvfb`.

That gives you the same practical model as headed Playwright in containers:

- browser is **not** started with `--headless`
- an in-container X server provides a virtual display
- you can still expose CDP and drive the browser from outside the container

## What You Need First

Prepare one of:

- a Linux runtime archive such as `clawbrowser-prod-linux-arm64.tar.gz`
- a Linux build output directory that contains at least:

- `chrome`
- `chrome_crashpad_handler`
- `icudtl.dat`
- `locales/`
- `resources.pak`

For example:

- `/path/to/chromium/src/out/Default`
- `/path/to/chromium/src/out/CBFast`

macOS `.app` bundles cannot run inside this Linux container.

## Build the Image

If `docker/artifacts/clawbrowser-prod-linux-arm64.tar.gz` is present in the
repo on an Apple Silicon Mac, the helper script will pick it automatically:

```bash
bash scripts/build_docker_image.sh --image-tag clawbrowser:headed
```

You can also pass the archive explicitly:

```bash
bash scripts/build_docker_image.sh \
  --build-source ./docker/artifacts/clawbrowser-prod-linux-arm64.tar.gz \
  --image-tag clawbrowser:headed
```

Or use a raw build output directory:

```bash
bash scripts/build_docker_image.sh \
  --build-source /path/to/chromium/src/out/Default \
  --image-tag clawbrowser:headed
```

You can also pass the Chromium `src` root and the script will try to find
`out/CBFast`, `out/Default`, or `out/Release` automatically:

```bash
bash scripts/build_docker_image.sh \
  --build-source /path/to/chromium/src \
  --image-tag clawbrowser:headed
```

Internally the script:

- auto-detects the archive or build directory
- extracts archives into a temporary staging directory
- copies the full runtime into a temporary Docker build context
- builds [docker/clawbrowser-runtime.Dockerfile](/Users/lrdoflnlss/GolandProjects/ai_agents/clawbrowser_core/docker/clawbrowser-runtime.Dockerfile:1)

Keeping the browser artifact as a `.tar.gz` is preferred over checking in an
unpacked directory because it is smaller on disk and the helper script already
expands it into a temporary staging area during the build.

## Run the Browser

Basic launch:

```bash
docker run --rm -it clawbrowser:headed
```

Expose CDP on port `9222`:

```bash
docker run --rm -it \
  -p 9222:9222 \
  clawbrowser:headed \
  --remote-debugging-address=0.0.0.0 \
  --remote-debugging-port=9222
```

Mount a persistent config directory:

```bash
docker run --rm -it \
  -v "$PWD/claw-config:/home/clawbrowser/.config/clawbrowser" \
  clawbrowser:headed
```

## Runtime Details

The container entrypoint:

- starts `Xvfb` on `DISPLAY=:99`
- launches the browser under `dbus-run-session`
- keeps the browser in headed mode unless you explicitly pass `--headless`
- defaults to `--no-sandbox` because that is the reliable Docker path here
- forwards CDP from the container IP to the browser's internal loopback socket
  when you ask for `--remote-debugging-address=0.0.0.0`

Default environment variables:

- `DISPLAY=:99`
- `XVFB_WHD=1920x1080x24`
- `CLAWBROWSER_WINDOW_SIZE=1920,1080`
- `CLAWBROWSER_BROWSER_BINARY=/opt/clawbrowser/chrome`
- `CLAWBROWSER_NO_SANDBOX=1`

You can override them at runtime:

```bash
docker run --rm -it \
  -e XVFB_WHD=1440x900x24 \
  -e CLAWBROWSER_WINDOW_SIZE=1440,900 \
  clawbrowser:headed
```

## Notes

- `--disable-dev-shm-usage` is included in the default launch args because many
  Docker environments have a very small `/dev/shm`.
- If you need a live remote desktop view, add VNC on top of the same `Xvfb`
  display; the current setup only provides the virtual monitor itself.
