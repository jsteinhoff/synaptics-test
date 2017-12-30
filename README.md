# synaptics-test: Read data from Synaptics USB Hardware

You need libusb and its header files (which are in an extra libusb-dev package
on some disributions) to compile this program. Then just run make in the source
directory. You can get a list of options with `./synaptics-test --help`. Without
the `--abs` option, the device will emulate an USB mouse, which is not very
interesting (I am not sure if the TouchScreen can emulate an USB mouse, if it can
not do so the `--abs` option is not needed).

## Example 1

If some button is not working on your TouchPad, you can find out which bit
corresponds to this button with:
```
	./synaptics-test --abs --decode
```
Then press the buttons and look what happens to the B field. The output is
more clearly with the additional option `--no-newline`.

## Example 2

If you have a composite TouchPad/TouchStyk (e.g. USB UltraNav Keyboards), you can
get the data packets of the TouchStyk with the option `--interface 1`:
```
	./synaptics-test --interface 1 --abs --decode
```
The TouchPad would be `--interface 0`. 
