<p align="center"><img src="https://cloud.githubusercontent.com/assets/203457/24553916/1f3f88b6-162c-11e7-990a-731b2560f15c.png" alt="Guetzli" width="64"></p>

# Introduction
This is a fork of [Google's *Guetzli* JPEG encoder](https://github.com/google/guetzli), for a university project course, UNSW's COMP4601. The aim was to accelerate the DCT component of JPEG encoding using an FPGA (specifically a Zynq-7000 SoC on a Zedboard). The code specific to hardware can be found within the files `guetzli/jpeg_data_encoder.cc` and `guetzli/hwdct.cc`.

# Building

**IMPORTANT**: To build for usage with a hardware accelerator via Xillybus, ensure the `HLS` C Macro is defined when compiling. If using Unix makefiles, this can be done by simply setting the `hls` environment variable. This fork has been developed and tested on Linux.

## On POSIX systems

1.  Get a copy of the source code, either by cloning this repository, or by
    downloading an
    [archive](https://github.com/Tobotimus/guetzli/archive/master.zip) and
    unpacking it.
2.  Install [libpng](http://www.libpng.org/pub/png/libpng.html).
    If using your operating system
    package manager, install development versions of the packages if the
    distinction exists.
    *   On Ubuntu (and Xillinux), do `apt-get install libpng-dev`.
    *   On Fedora, do `dnf install libpng-devel`. 
    *   On Arch Linux, do `pacman -S libpng`.
    *   On Alpine Linux, do `apk add libpng-dev`.
3.  Run `make` and expect the binary to be created in `bin/Release/guetzli`.

## On Windows

1.  Get a copy of the source code, either by cloning this repository, or by
    downloading an
    [archive](https://github.com/Tobotimus/guetzli/archive/master.zip) and
    unpacking it.
2.  Install [Visual Studio 2015](https://www.visualstudio.com) and
    [vcpkg](https://github.com/Microsoft/vcpkg)
3.  Install `libpng` using vcpkg: `.\vcpkg install libpng`.
4.  Cause the installed packages to be available system-wide: `.\vcpkg integrate
    install`. If you prefer not to do this, refer to [vcpkg's
    documentation](https://github.com/Microsoft/vcpkg/blob/master/docs/EXAMPLES.md#example-1-2).
5.  Open the Visual Studio project enclosed in the repository and build it.

## On macOS

To install using [Homebrew](https://brew.sh/):
1. Install [Homebrew](https://brew.sh/)
2. `brew install guetzli`

To install using the repository:
1.  Get a copy of the source code, either by cloning this repository, or by
    downloading an
    [archive](https://github.com/Tobotimus/guetzli/archive/master.zip) and
    unpacking it.
2.  Install [Homebrew](https://brew.sh/) or [MacPorts](https://www.macports.org/)
3.  Install `libpng`
    *   Using [Homebrew](https://brew.sh/): `brew install libpng`.
    *   Using [MacPorts](https://www.macports.org/): `port install libpng` (You may need to use `sudo`).
4.  Run the following command to build the binary in `bin/Release/guetzli`.
    *   If you installed using [Homebrew](https://brew.sh/) simply use `make`
    *   If you installed using [MacPorts](https://www.macports.org/) use `CFLAGS='-I/opt/local/include' LDFLAGS='-L/opt/local/lib' make`

# Using

After compiling, you will find the `guetzli` executable in the `bin/Release` or `bin/Debug` subdirectory (depending on the build configuration). Then, on POSIX systems, usage is as follows:

```bash
./guetzli original.png output.jpg
```
