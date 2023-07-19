# PyDigiham

This project implements Python bindings for the [`libdigiham` library of software defined radio components](https://github.com/jketterl/digiham).

It is primarily used for demodulation of digital radio signals in the OpenWebRX project, however since the digiham components are generic building blocks for decoding digial data, the classes provided by this project may become useful in other Python SDR applications as well.

# Installation

# Installation

The OpenWebRX project is hosting pydigiham packages in their repositories. Please click the respective link for [Debian](https://www.openwebrx.de/download/debian.php) or [Ubuntu](https://www.openwebrx.de/download/ubuntu.php). Due to naming conventions, the repository package is called `python3-digiham`.

# Compiling from source

Please install `digiham` and its dependencies before compiling this project.

Please also install the python development files (`libpython3-dev` on most Debian-based distributions).

```
sudo ./setup.py install
```

# TODO

Add an example folder with some generic Python code using this library, e.g. DMR and Pocsag demodulators.