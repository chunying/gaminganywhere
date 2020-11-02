GamingAnywhere
==============

GamingAnywhere: An Open Cloud Gaming System

# Overview

GamingAnywhere is an open-source clouding gaming platform. In addition to its
openness, we design GamingAnywhere for high extensibility, portability, and
reconfigurability. GamingAnywhere currently supports Windows and Linux, and
can be ported to other OS's including OS X and Android.

# Documents

* Official web site: http://gaminganywhere.org/

* Quick start guide: http://gaminganywhere.org/doc/quick_start.html

* Configuration file guide: http://gaminganywhere.org/doc/config.html

* FAQ: http://gaminganywhere.org/faq.html

# Quick Notes

* Recommended development platforms Ubuntu Linux x86_64.

* Required packages on Linux OS (both runtime and development files):
```libx11```, ```libxext```, ```libxtst```, ```libfreetype6```,
```libgl1-mesa```, ```libglu1-mesa```, ```libpulse```,
```libasound2```, ```lib32z1```

* Sample command to install required packages on Ubuntu Linux:
  ```
  apt-get  install patch make cmake g++ pkg-config \
		libx11-dev libxext-dev libxtst-dev libfreetype6-dev \
		libgl1-mesa-dev libglu1-mesa-dev \
		libpulse-dev libasound2-dev lib32z1
  ```
