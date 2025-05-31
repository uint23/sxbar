# sxbar
The simple, yet powerful, status bar for Xorg.

## Configuration
sxbar can be configured using a configuration file located at `~/.config/sxbarrc`. If this file doesn't exist, sxbar will use the default values from `config.h`.

To get started with configuration:

1. Copy the example config file:
   ```bash
   mkdir -p ~/.config
   cp sxbarrc.example ~/.config/sxbarrc
   ```

2. Edit the config file to your liking:
   ```
   # Colors (hex format)
   background_color: #56002e
   foreground_color: #ffffff
   border_color: #005577
   ```

The configuration uses a simple `key: value` format. Lines starting with `#` are treated as comments.
