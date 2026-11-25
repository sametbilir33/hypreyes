# hypreyes

A cursor-tracking eye renderer for Hyprland, implemented using the Hyprland Plugin API, GTK3 and Cairo.

## Overview

hypreyes consists of two independent processes that communicate through a Unix domain datagram socket.

The Hyprland plugin captures pointer movement events and forwards cursor coordinates to the GTK application. The GTK application uses these coordinates to calculate pupil positions and render the eyes.

## Architecture

The project is divided into two components:

### Hyprland Plugin

Source: `hypreyes-plugin.cpp`

Responsibilities:

- Subscribe to Hyprland's mouse movement event.
- Retrieve pointer coordinates from `Vector2D`.
- Serialize coordinates into an `x,y` representation.
- Forward coordinates through a Unix domain datagram socket.
- Handle plugin initialization and cleanup.

The plugin uses a non-blocking `AF_UNIX` socket with `SOCK_DGRAM`.

Socket path:

```text
/tmp/hypreyes-<UID>.sock
```

### GTK Application

Source: `hypreyes.c`

Responsibilities:

- Create and bind the Unix domain socket.
- Receive pointer coordinate datagrams.
- Parse and validate incoming coordinates.
- Schedule GTK redraw operations when new coordinates arrive.
- Render the eyes using Cairo.

The rendering implementation uses elliptical eye geometry and constrains pupil movement within the eye boundaries.

## Dependencies

- Hyprland
- Hyprland Plugin API headers
- GTK 3
- Cairo
- GLib
- GCC / G++

## Build

### GTK Application

The application source uses C++ standard library functionality despite its `.c` extension. Compile it using G++:

```bash
g++ -x c++ hypreyes.c -o hypreyes $(pkg-config --cflags --libs gtk+-3.0 cairo) -lm
```

### Hyprland Plugin

Compile the plugin as a shared library using the build configuration and headers corresponding to the installed Hyprland version.

The resulting shared object must be compatible with the running Hyprland instance.

## Execution

1. Start the GTK application:

```bash
./hypreyes
```

2. Load the compiled plugin into Hyprland using the plugin manager or the supported plugin-loading mechanism.

3. Move the pointer to update the rendered pupil positions.

Both processes must be running for cursor tracking to work.

## IPC Protocol

Communication uses Unix domain datagram sockets.

| Property | Value |
|---|---|
| Address family | `AF_UNIX` |
| Socket type | `SOCK_DGRAM` |
| Communication | Local IPC |
| Payload format | ASCII |
| Coordinate format | `x,y` |
| Coordinate precision | 3 decimal places |
| Socket location | `/tmp/hypreyes-<UID>.sock` |

Example payload:

```text
1024.500,768.250
```

Each datagram represents a pointer position. The GTK application processes available datagrams and redraws the interface when updated coordinates are received.

## Rendering

Rendering is implemented using Cairo through GTK3's drawing area.

Each eye consists of:

- An elliptical sclera.
- A circular pupil.
- A dark outline.

Pupil positioning is calculated using normalized cursor displacement relative to the monitor center.

The pupil coordinates are constrained by the available inner ellipse dimensions to prevent the pupils from exceeding the eye boundaries.

The application requests an RGBA visual when supported by the display server and compositor, allowing transparent window rendering.

## Compatibility

- Linux
- Hyprland
- Wayland
- GTK 3

The plugin must be compiled against a compatible Hyprland API version. ABI compatibility is checked during plugin initialization.

## Limitations

- Cursor tracking depends on the Hyprland plugin being loaded.
- The GTK application must remain active to receive and render cursor updates.
- Monitor geometry is currently obtained through GTK's primary monitor interface.
- The socket path is shared through `/tmp` and uses the current user's UID.
- Socket communication is unidirectional, from the Hyprland plugin to the GTK application.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
