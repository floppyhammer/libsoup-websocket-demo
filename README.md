# libsoup WebSocket Demo

## Get Dependencies (Linux)

```sh
sudo apt install pkg-config libglib2.0-dev libjson-glib-dev
```

Plus some version of libsoup.

- If you can use libsoup3:
    - `libsoup-3.0-dev`
- Otherwise, use libsoup 2:
    - `libsoup2.4-dev`
    - In this case, you must also pass `-DUSE_LIBSOUP2=ON` to CMake.

Best to only have one of the two libsoup dev packages installed at a time.
