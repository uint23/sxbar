# sxbar

The simple, yet powerful, status bar for Xorg.

## Install

```
sudo make clean install
```

This will build and copy the binary to `/usr/local/bin`, and copy the default config to `/usr/local/share/sxbarc`.

## Configuration

Copy the default config file to `$HOME/.config/sxbarc`.

Default configuration:

```
bottom_bar          : true
height              : 19
vertical_padding    : 0
horizontal_padding  : 0
text_padding        : 0
border              : false
border_width        : 0
background_colour   : #000000
foreground_colour   : #7abccd
border_colour       : #005577
font                : fixed
enabled_modules     : clock,date,volume,cpu
```

### Modules

Enabled modules are controlling what status bar modules are present and in what order. The available modules are: `clock`, `date`, `battery`, `volume`, `cpu`.
