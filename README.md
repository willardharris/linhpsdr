# LinHPSDR

### Whats new in Reimagined!

Added RingBuffer for IQ samples, this greatly improves audio quality.
Optimized Panadapter and Waterfall for better performance.
Moved WDSP (DSP Processing) to a seperate thread to improve performance.
Added NR2 Trained and new noise menu.
Fixed TX OC
Added Fequency Calibration
Various other tweaks and fixes!

6/2/2025 -  Added TX compressor.  Set to disable all DSP functions in DIGI Mode

Please note: This is very beta, and is constantly updating!  Use SOUNDIO for audio, its non blocking and better for slower machines!

Mods make this run pretty good on a RPI 4!  But a x86 pc running debian would give best results!

Thanks,

W4WHL



### Prerequisites for building

```
  sudo apt-get install libfftw3-dev
  sudo apt-get install libpulse-dev
  sudo apt-get install libsoundio-dev
  sudo apt-get install libasound2-dev
  sudo apt-get install libgtk-3-dev
  sudo apt-get install libsoapysdr-dev
```

### Prerequisites for installing the Debian Package

```
  sudo apt-get install libfftw3-3
  sudo apt-get install libpulse
  sudo apt-get install libsoundio
  sudo apt-get install libasound2
  sudo apt-get install libsoapysdr
```



### To download, compile and install linHPSDR from here

```
  git clone https://github.com/willardharris/linhpsdr.git
  cd linhpsdr/wdsp
  make clean
  make
  sudo make install
  cd ..
  make clean
  make
  sudo make install
  
```

