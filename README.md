Introduction
============

This is a spectrum sensing application for the VESNA platform. It presents
a unified interface to different radio modules through a serial interface. It
also serves as an example use of hardware drivers using the low-level spectrum
sensing API.

All spectrum sensing devices supported by this application work by tuning
to a radiofrequency channel and either estimating the power level at the
antenna interface or sampling the baseband data signal. The application
supports both continuously sensing one channel of performing a frequency sweep
over a range of channels.

The range of frequencies that can be tuned to is determined by a hardware
configuration. Each device may support one or more configurations.  A
configuration is characterized by the frequency of the bottom-most channel,
channel spacing, channel bandwidth, number of channels and the time it takes
the device to perform a measurement for a single channel.

Currently the following hardware models are supported (selectable via the
MODEL environment variable when invoking make):

- sne-eshter

  VESNA SNC + SNE-ESHTER hardware configuration

  wide- and narrow-band energy detection in the UHF band using
  NXPTDA18219HN tuner. Also supports sampling the baseband signal
  directly.

  (requires proprietary libtda18219 library)

- sne-crewtv

  VESNA SNC + SNE-CREWTV hardware configuration

  wide-band energy detection in the UHF band using NXP TDA18219HN tuner
  (also requires proprietary libtda18219 VESNA library)

- sne-ismtv-uhf

  VESNA SNC + SNE-ISMTV-UHF hardware configuration

  wide-band energy detection in the UHF band using NXP TDA18219HN tuner
  (also requires proprietary libtda18219 VESNA library)

- sne-ismtv-868

  VESNA SNC + SNE-ISMTV-868 hardware configuration

  energy detection in the 868 MHz ISM band using Texas Instruments
  CC1101.

- snr-trx-868

  VESNA SNC + SNR-TRX-868 hardware configuration

  energy detection in the 868 MHz ISM band using Texas Instruments
  CC1101.

- sne-ismtv-2400

  VESNA SNC + SNE-ISMTV-2400 hardware configuration

  energy detection in the 2.4 GHz ISM band using Texas Instruments
  CC2500.

- snr-trx-2400

  VESNA SNC + SNR-TRX-2400 hardware configuration

  energy detection in the 2.4 GHz ISM band using Texas Instruments
  CC2500.

- null

  VESNA SNC hardware configuration

  dummy software-only device, returning noise and zero measurements

For more info on VESNA, see http://sensorlab.ijs.si/hardware.html


Compilation
===========

To configure the build, use the ./configure script. First argument is the name
of the hardware model you are using. Second argument is the USART used for
communication with the host.

Typically, USART1 is used when the VESNA is connected directly via RS-232 to a
PC. USART3 is used when the sensor is accessed remotely through SNR-ETH
(DigiConnect ME) TCP/IP module.

    $ cd firmware
    $ ./configure sne-eshter usart1

To compile and upload firmware to a VESNA using OpenOCD and Olimex ARM-USB-OCD:

    $ make spectrum-sensor.u

To make a binary image for loading with the VESNA bootloader, use the following
make command:

    $ LDSCRIPT=vesna_app.ld make spectrum-sensor.bin

To make a hex file for uploading to the OpenBLT bootloader, use the following
make command:

    $ LDSCRIPT=vesna_openblt.ld make spectrum-sensor.srec


Testing
-------

To run unit tests, make sure that Unity framework is installed in a directory
along side vesna-spectrum-sensor:

    $ ls
    Unity  vesna-spectrum-sensor

Then run "make test":

    $ cd vesna-spectrum-sensor/firmware
    $ make test


Python bindings
---------------

To install Python bindings:

    $ cd vesna-spectrum-sensor/python
    $ python setup.py


Remote programming with the OpenBLT bootloader
----------------------------------------------

To compile and upload the OpenBLT bootloader to the node using OpenOCD and
Olimex ARM-USB-OCD. Note that you need the source of the OpenBLT bootloader
v1.00.01 (available from http://feaser.com/openblt/doku.php - see Makefile for
the required path to OpenBLT source tree)

    $ cd bootloader
    $ make upload

Once the bootloader has been uploaded to the node, application can be
reprogrammed remotely over TCP/IP:

    $ cd firmware
    $ LDSCRIPT=vesna_openblt.ld make spectrum-sensor.srec

    $ openblt-tcp-boot -dXXX.XXX.XXX.XXX -p2101 spectrum-sensor.srec

(see https://github.com/avian2/openblt-tcp-boot for the openblt-tcp-boot
application)


Usage
=====

Connect VESNA to a serial terminal using 576000 baud, 8 data bits, 1 stop
bit, no parity. On the other hand, if you are accessing the sensor remotely
over TCP/IP network, connect a telnet session to the VESNA node.

You can then use the terminal to interact with the application in a
command-line fashion (conclude each command with a new line)

Type "help" to print a list of available commands with brief descriptions.

A typical session consists of:

1. "list" command to get the list of hardware configurations available

2. "select" command to select a configuration and setup the spectrum sweep
   parameters.

3. "sweep-on" command to start the sweep.

4. "sweep-off" command to stop the sweep.


The python/ directory includes Python classes that abstract this interface.
Please refer to the README in that directory for details.


License
=======

Copyright (C) 2014 SensorLab, Jozef Stefan Institute
http://sensorlab.ijs.si

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Authors:	Tomaz Solc, <tomaz.solc@ijs.si>
		Zoltan Padrah
		Ales Verbic


(OpenBLT bootloader code is covered by a different license - please see files
in the bootloader/ for details)
