# Jaguar GD Command Line Tool
## What is it?
This is a libarary and simple command line tool which allows a Windows based computer to connect to, upload and execute code on the Jaguar via the Jaguar
GameDrive cartridge.

## Usage
```
JagGD [commands]

Commands:
-r                                             Reboot
-rd                                            Reboot to debug stub
-rr                                            Reboot to currently loaded ROM
-wf file                                       Write file to memory card
-u[x[r]] file[,a:addr,s:size,o:offset,x:entry] Upload file with parameters
-e file[,size]                                 Enable eeprom file on memory card with given size (in byes, default 128)
-x addr                                        Execute from address
-xr                                            Execute via reboot
-q                                             Quiet, suppress any non-error messages
```

**Example:**
`JagGD -rd -ux myfile.abs`

Any commands can be placed on the command line in any order and will be executed in that order. Any commands other than the reset commands `-r` and `-rd` require the Jaguar to be running the stub, so placing `-rd` on the command line as the first option is required unless it has been previously issued.

Any command which includes execute (either `-ux` or `-x`) will execute when all other commands have been processed, as further instructions other than reset cannot be processed after the stub has passed control to another program.

Any addresses, sizes, offsets, etc, can be specified as decimal (default) or hexidecimal by preceeding the number with `$` or `0x`.

If a file is a recognised format, such as ABS, COFF, JAGR (Jaguar Server) or ROM, the load address and entry point will be determined from the file. Any of these can be overridden if required.

## Reboot
Reboot the Jaguar either back to the menu, debug stub, or currently loaded ROM.

## Write File
Write any file directly to the memory card. It will have the same name as the source file and can be used without the debug stub running.

## Upload
Upload any data to the given memory location. This can be internal DRAM ($2000-$1fffff) or cartridge RAM ($800000+). Multiple uploads can be issued and the size to upload, offset within the file and address to upload to can be specified. You can also specify an entry address to have the file executed from automatically after upload if you have used the `x` flag. For some formats (like ABS), the load and execute address can be taken from the file.

## EEPROM
The settings for the supported EEPROM can be specified before executing the code. This is a file on the memory card to persist the EEPROM data and the size of the EEPROM. The size is one of 128, 256/512, 1024/2048 bytes.

## Execute
Any arbitrary address can be executed from or the console can be rebooted for the standard boot process. If multiple files are uploaded it may be useful to specify execution separately. If no address is specified the currently determined entry point from the last file uploaded will be used.

## Quiet
Suppress any non-error messages from displaying, otherwise a log of commands being executed will be shown.
