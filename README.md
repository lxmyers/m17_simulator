M17 Protocol 'C' Implementation
==========

M17 Specification - https://github.com/M17-Project/M17_spec

Disclaimer - This was done as a way for me to learn 'C'. I have no idea what I'm doing, no doubt this project is filled with errors and bad programming practices.


This is a "almost" Protocol Implementation of M17. This project has been built using code from other great projects including codec2, libfec, liquid-dsp, avr-libc and Tiny-AES. 

It demonstrates the use of codec2, Convolutional Encoding, Golay Encoding, Puncturing, and Interleaving. It also includes CRC and basic AES Encryption. Data is printed in the Terminal.

## Issues ##

Unfortunately I was not able to correctly implement the specified puncturing schemes. While this project does return punctured code, it is not compatible. Hopefully this can be resolved at a later date.

No Scrambler has been implemented at this time.

Different Sync Bursts used for Link Setup Frame and Sub Frames.

## Dependencies ##

All the relevant dependencies have been stripped down and included in the project.

### Building ###

cmake .

make

