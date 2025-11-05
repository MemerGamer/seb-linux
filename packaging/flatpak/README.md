# Flatpak Build Instructions

This directory contains the Flatpak manifest for building seb-linux as a sandboxed application.

## Prerequisites

Install Flatpak and the KDE Platform SDK:

```bash
# On Arch Linux
sudo pacman -S flatpak flatpak-builder

# On Ubuntu/Debian
sudo apt install flatpak flatpak-builder

# Install KDE Platform runtime and SDK
flatpak install org.kde.Platform//6.6 org.kde.Sdk//6.6
```

## Building

From the repository root:

```bash
flatpak-builder --repo=repo build-dir packaging/flatpak/io.seb.linux.yaml
```

Or to build and install directly:

```bash
flatpak-builder --install --user build-dir packaging/flatpak/io.seb.linux.yaml
```

## Running

After building and installing:

```bash
flatpak run io.seb.linux --config /path/to/config.json
```

Note: The config file path must be accessible from within the sandbox. You may need to use `--filesystem=home` or mount the config file.

## Permissions

The manifest configures minimal permissions:

- Network access (required for web browsing)
- Wayland and X11 sockets (for display)
- XDG cache directory (for WebEngine cache)
- DBus access for idle inhibition
- No host filesystem access (except cache)

## Troubleshooting

If you need to access files outside the sandbox, you can temporarily add:

```yaml
- --filesystem=home
```

to the `finish-args` section, but this reduces security.
