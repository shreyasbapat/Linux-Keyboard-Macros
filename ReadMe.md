# KeyMac


Steps to install **KeyMac** on your machine.

Prerequisites -
1. Linux Kernel (tested on 5.0).
2. Sudo access to kernel.
3. Python3 with Tkinter (Only for GUI, utility would run without GUI as well)

To install KeyMac,
unzip the folder and follow the given commands -

 ```
 make
 sudo insmod keymac.ko
 ```

After this you can start recording macros by pressing `ctrl + alt + shift`
Anything pressed after this would be recorded.
To stop recording macros again press `ctrl + alt + shift`

To run the macro press `ctrl + shift + i` where ` 0 <= i <= 9`.

Open the GUI by running -

```
    $ python3 gui.py
```

The module would be removed by default on rebooting.

To load module on boot

1. Edit `/etc/modules` file to add our module name i.e KeyMac.
2. Copy *KeyMac.ko* to `/lib/modules/$(uname -r)/kernel/drivers/input/keyboard/`
3. To check if the module loaded on reboot run `lsmod | grep KeyMac`
