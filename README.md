# LinHPSDR

### Development environment

Development and testing has been run on Ubuntu 17.10 and Ubuntu 18.04. If run on early versions there may be a problem with GTK not supporting the gtk_menu_popup_at_pointer function vfo.c. For information on MacOS support see [MacOS.md](./MacOS.md).

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

