# seb-linux

Safe Exam Browser alternative for Linux, built with C++ and Qt6.

## Features

- Secure web browser shell with domain-based navigation control
- Request interception and header injection
- Locked-down browser features (disabled context menu, printing, downloads)
- X11 shortcut suppression
- Idle/screensaver inhibition
- Configurable via JSON policy files

## Prerequisites

### Arch Linux

```bash
sudo pacman -S qt6-base qt6-webengine cmake ninja gcc
```

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install qt6-base-dev qt6-webengine-dev cmake ninja-build build-essential
```

### Required Qt6 Components

- Qt6::Core
- Qt6::Gui
- Qt6::Widgets
- Qt6::WebEngineWidgets
- Qt6::DBus (for idle inhibition)

## Building

1. Clone the repository:

```bash
git clone <repository-url>
cd seb-linux
```

2. Configure the build:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Alternatively, for a release build:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

3. Build the project:

```bash
cmake --build build
```

The executable will be located at `build/src/app/seb-linux`.

## Running

Run the application with a configuration file:

```bash
./build/src/app/seb-linux --config examples/mvp.json
```

### Command-line Options

- `--config` or `-c`: Path to JSON configuration file (required)
- `--help` or `-h`: Display help message
- `--version` or `-v`: Display version information

## Configuration

Configuration is done via JSON files. See `examples/mvp.json` for a complete example.

### Configuration Schema

#### Required Fields

- **`startUrl`** (string, required): HTTPS URL to load on startup. Must be a valid HTTPS URL.

#### Optional Fields

- **`allowedDomains`** (array of strings, optional): List of allowed domains. Navigation to domains not in this list will be blocked. Supports subdomain matching (e.g., `cdn.example.com` matches `example.com`).

- **`userAgentSuffix`** (string, optional): Suffix to append to the browser's user agent string.

- **`clientVersion`** (string, optional): Client version string. Defaults to `"0.1.0"` if not specified. Used in `X-SafeExamBrowser-ClientVersion` header.

- **`clientType`** (string, optional): Client type string. Defaults to `"SEB-Linux"` if not specified. Used in `X-SafeExamBrowser-ClientType` header.

- **`sendConfigKey`** (boolean, optional): Whether to send the `X-SafeExamBrowser-ConfigKey` header. Defaults to `true`.

### Example Configuration

```json
{
    "startUrl": "https://example.com/exam",
    "allowedDomains": [
        "example.com",
        "cdn.example.com",
        "api.example.com"
    ],
    "userAgentSuffix": "SEB-Linux/1.0",
    "clientVersion": "0.1.0",
    "clientType": "SEB-Linux",
    "sendConfigKey": true
}
```

### Configuration Validation

The application validates the configuration file on startup:

- **Missing `startUrl`**: Error message and exit with code 1
- **Invalid URL format**: Error message and exit with code 1
- **Non-HTTPS scheme**: Error message and exit with code 1
- **Invalid field types**: Error message and exit with code 1

All errors are printed to stderr with clear error messages.

## Known Limitations

### Wayland Support

**Wayland support is limited** in the current implementation. The following features have known limitations on Wayland:

1. **Shortcut Suppression**: Global keyboard shortcut suppression (Ctrl+L, Ctrl+T, Ctrl+N, Ctrl+W, Ctrl+Shift+I, F11) is currently **only implemented for X11**. On Wayland, these shortcuts may still function in the browser.

2. **Keyboard Grabbing**: The X11-specific `XGrabKey` API does not work on Wayland. Wayland's security model prevents applications from intercepting global keyboard shortcuts that are not explicitly granted by the compositor.

3. **Idle Inhibition**: Idle/screensaver inhibition works on Wayland via D-Bus (org.gnome.SessionManager, org.freedesktop.ScreenSaver), but may not be as effective as on X11 depending on the compositor.

### Recommendations

- **For X11**: Full functionality is available. Recommended for production deployments.
- **For Wayland**: Consider using X11 session or wait for kiosk mode implementation for production use.

## Roadmap

### Short-term (MVP)

- [x] Basic WebEngine shell with domain blocking
- [x] Request header injection
- [x] Browser feature lockdown
- [x] X11 shortcut suppression
- [x] Idle/screensaver inhibition
- [x] Configuration file support

### Medium-term

- [ ] **SEB Compatibility**: Full compatibility with SEB/Moodle expectations
  - Request hash calculation and validation
  - Config key encryption/decryption
  - Full header set matching SEB specification

- [ ] **Kiosk Mode**: Dedicated kiosk session for maximum security
  - Minimal Wayland compositor integration (cage, wayfire)
  - Full keyboard and display control
  - Session isolation
  - Automatic startup configuration

- [ ] **Enhanced Security**
  - Process isolation
  - Network filtering
  - Clipboard blocking
  - Screen capture prevention

### Long-term

- [ ] **Packaging**: Distribution packages
  - Arch Linux (AUR)
  - Debian/Ubuntu (.deb)
  - Fedora/RHEL (.rpm)
  - AppImage for universal distribution

- [ ] **Advanced Features**
  - Multi-monitor support
  - Exam timer integration
  - Logging and audit trails
  - Remote configuration management

- [ ] **Testing and Quality Assurance**
  - Automated test suite
  - Integration with Moodle/SEB servers
  - Security audit
  - Performance benchmarking

## Development

### Project Structure

```text
seb-linux/
├── src/
│   ├── app/          # Main application entry point
│   ├── core/         # Core functionality (config, policy)
│   └── web/          # WebEngine integration
├── include/          # Public headers (future)
├── tests/            # Test suite (future)
├── examples/         # Example configurations
├── docs/            # Documentation (future)
└── packaging/       # Packaging scripts (future)
```

### Building for Development

For development with debug symbols:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Testing Configuration

A test utility is available to validate configuration files:

```bash
./build/src/core/test_config
```

This will load `examples/mvp.json` and display the parsed configuration.

## License

[To be determined]

## Contributing

Contributions are welcome! Please ensure that:

1. Code follows the existing style
2. New features include appropriate error handling
3. Configuration changes are backward-compatible where possible
4. Documentation is updated accordingly
