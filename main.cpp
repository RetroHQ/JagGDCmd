#pragma once

#include "JagGDUSBDevice.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Convert string from decimal or hex to binary
//
////////////////////////////////////////////////////////////////////////////////

DWORD StringToNumber(const char* opt)
{
	int base = 10;
	if (*opt == '$')
	{
		opt++;
		base = 16;
	}
	else if (opt[0] == '0' && opt[1] == 'x')
	{
		opt += 2;
		base = 16;
	}
	DWORD v = (DWORD)strtol(opt, NULL, base);
	return v;
}

////////////////////////////////////////////////////////////////////////////////
//
// Main commandline processing
//
////////////////////////////////////////////////////////////////////////////////

int __cdecl _tmain(int argc, const char* argv[], const char* envp[])
{
	bool bQuiet = false;
	CJagGDCmd cJagGD;
	if (argc == 1)
	{
		printf(	"%s [commands]\n\n"\
				"-r          Reboot\n"\
				"-rd         Reboot to debug stub\n"\
				"-rr         Reboot and keep current ROM\n"\
				"-wf file    Write file to memory card\n\n"\
				"From stub mode (all ROM, RAM > $2000) --\n"\
				"-u[x[r]] file[,a:addr,s:size,o:offset,x:entry]\n"\
				"            Upload to address with size and file offset and optionally eXecute directly or via Reboot.\n"\
				"            Prefix numbers with $ or 0x for hex, otherwise decimal assumed.\n"\
				"-e file[,size]\n"\
				"            Enable eeprom file on memory card with given size (in byes, default 128)\n"\
				"-x addr     Execute from address\n"\
				"-xr         Execute via reboot\n"\
				"-q          Quiet, use first to suppress all non-error messages\n\n",
				argv[0]);
	}
	else
	{
		if (cJagGD.Connect() == ERROR_SUCCESS)
		{
			bool bExecute = false;
			bool bReboot = false;
			DWORD entry = 0;
			const char **arg = argv+1; // 0 is exe path
			while (--argc)
			{
			//-- Shhhh

				if (_stricmp(*arg, "-q") == 0)
				{
					bQuiet = true;
				}
				
			//-- Reset
				
				if (_stricmp(*arg, "-r") == 0)
				{
					cJagGD.Reset(EResetType_Menu);
					if (!bQuiet) printf("Reboot (Menu)\n");
				}
				
			//-- Reset to debug stub
				
				else if (_stricmp(*arg, "-rd") == 0)
				{
					cJagGD.Reset(EResetType_Debug);
					if (!bQuiet) printf("Reboot (Debug Console)\n");
					Sleep(1500);
				}
				
			//-- Reset to current ROM
				
				else if (_stricmp(*arg, "-rr") == 0)
				{
					cJagGD.Reset(EResetType_ROM);
					if (!bQuiet) printf("Reboot (ROM)\n");
				}

			//-- Enable EEPROM with given parameters
				
				else if (_stricmp(*arg, "-e") == 0)
				{
					if (argc)
					{
						argc--;
						arg++;

						static const char* aszSizes[] =
						{
							"128",
							"256/512",
							"1024/2048"
						};

					//-- Pull optional load paramters from string

						unsigned int nEEPROMSize = 0;
						char szFilename[256];
						strcpy(szFilename, *arg);
						char* opt = strchr(szFilename, ',');
						if (opt)
						{
							*opt++ = 0;
							nEEPROMSize = atoi(opt);
							switch (nEEPROMSize)
							{
							default:
								if (!bQuiet) printf("Warning: Unrecognised EEPROM size (%d), ignoring.\n", nEEPROMSize);
							case 128:
								nEEPROMSize = 0;
								break;
							case 256:
							case 512:
								nEEPROMSize = 1;
								break;
							case 1024:
							case 2048:
								nEEPROMSize = 2;
								break;
							}
						}

						if (!bQuiet) printf("Setting EEPROM file: '%s', %s bytes... ", szFilename, aszSizes[nEEPROMSize]);
						if (cJagGD.EnableEEPROM(szFilename, nEEPROMSize) == ERROR_SUCCESS)
						{
							if (!bQuiet) printf("OK!\n");
						}
						else
						{
							if (bQuiet) printf("EEPROM SETTING ");
							printf("FAILED!\n");
						}
					}
				}

			//-- Write file to memory card
				
				else if (stricmp(*arg, "-wf") == 0)
				{
					if (argc)
					{
						arg++;
						argc--;
						if (!bQuiet) printf("WRITE FILE (%s)... ", *arg);

						if (cJagGD.WriteFile(*arg) == ERROR_SUCCESS)
						{
							if (!bQuiet) printf("OK!\n");
						}
						else
						{
							if (bQuiet) printf("WRITE FILE ");
							printf("FAILED!\n");
						}
					}
				}

			//-- Upload / upload and execute
				
				else if (_stricmp(*arg, "-u") == 0 || _stricmp(*arg, "-ux") == 0 || _stricmp(*arg, "-uxr") == 0)
				{
					if (argc)
					{
						bExecute = _strnicmp(*arg, "-ux", 3) == 0;
						bReboot = _stricmp(*arg, "-uxr") == 0;
						argc--;
						arg++;

					//-- Pull optional load paramters from string

						DWORD addr = 0, size = 0, offset = 0;

						char szFilename[256];
						strcpy(szFilename, *arg);

						char *opt = szFilename;
						while (opt)
						{
							opt = strchr(opt, ',');
							if (opt)
							{
								*opt++ = 0;
								char t = *opt++|0x20;
								if (*opt++ == ':')
								{
									DWORD v = StringToNumber(opt);
									
									// load address
									if (t == 'a')
									{
										addr = v;
									}
									
									// size to load
									else if (t == 's')
									{
										size = v;
									}
									
									// offset within file
									else if (t == 'o')
									{
										offset = v;
									}
									
									// entry address
									else if (t == 'x')
									{
										entry = v;
									}
								}
							}
						}

					//-- Check header to see if it has useful info in it

						char szFormat[16] = "BINARY";
						FILE *f = fopen(szFilename, "rb");
						if (f)
						{
							unsigned char aHeader[2048];
							fread(aHeader, 1, 2048, f);
							fclose(f);

						//-- ROM
							
							if (aHeader[0x400] == 0x04 &&
								aHeader[0x401] == 0x04 &&
								aHeader[0x402] == 0x04 &&
								aHeader[0x403] == 0x04)
							{
								if (!addr)
								{
									addr = 0x800000;
								}
								if (!entry)
								{
									entry = 0x802000;
								}

								strcpy(szFormat, "ROM");
							}

						//-- Jaguar server

							else if (	aHeader[0x1c]=='J' &&
										aHeader[0x1d]=='A' &&
										aHeader[0x1e]=='G' &&
										aHeader[0x1f]=='R')
							{
								if (!addr)
								{
									addr =	aHeader[0x22] << 24 |
											aHeader[0x23] << 16 |
											aHeader[0x22] << 8 |
											aHeader[0x21] << 0;
								}

								if (!entry)
								{
									entry =	aHeader[0x2a] << 24 |
											aHeader[0x2b] << 16 |
											aHeader[0x2c] << 8 |
											aHeader[0x2d] << 0;
								}

								if (!offset)
								{
									offset = 0x2e;
								}

								strcpy(szFormat, "JAGR");
							}
						
						//-- COFF
						
							else if (	aHeader[0x00] == 0x01 &&
										aHeader[0x01] == 0x50 &&
										aHeader[0x02] == 0x00 &&
										aHeader[0x03] == 0x03 &&
										aHeader[0x16] == 0x01 &&
										aHeader[0x17] == 0x07)
							{
								if (!addr)
								{
									addr =	aHeader[0x28] << 24 |
											aHeader[0x29] << 16 |
											aHeader[0x2a] << 8 |
											aHeader[0x2b] << 0;
								}

								if (!entry)
								{
									entry =	aHeader[0x24] << 24 |
											aHeader[0x25] << 16 |
											aHeader[0x26] << 8 |
											aHeader[0x27] << 0;
								}

								if (!offset)
								{
									offset = 0xa8;
								}

								strcpy(szFormat, "COFF");
							}

						//-- ABS header

							else if (	aHeader[0x00] == 0x60 &&
										aHeader[0x01] == 0x1b)
							{
								if (!addr)
								{
									addr =	aHeader[0x16] << 24 |
											aHeader[0x17] << 16 |
											aHeader[0x18] << 8 |
											aHeader[0x19] << 0;
								}

								if (!entry)
								{
									entry =	aHeader[0x16] << 24 |
											aHeader[0x17] << 16 |
											aHeader[0x18] << 8 |
											aHeader[0x19] << 0;
								}

								if (!offset)
								{
									offset = 0x24;
								}

								strcpy(szFormat, "ABS");
							}
						}

					//-- Dont overwrite the stub if no address has been given, or found

						if (!addr)
						{
							printf("Please specify address for upload of %s.\n", szFilename);
						}
						else
						{
							if (!bQuiet)
							{
								printf("UPLOADING %s (%s) TO $%x", szFilename, szFormat, addr);
								if (offset)
								{
									printf(" OFFSET $%x", offset);
								}
								if (size)
								{
									printf(" SIZE $%x", size);
								}
								if (entry)
								{
									printf(" ENTRY $%x", entry);
								}
								if (bExecute)
								{
									printf(" EXECUTE");
								}
								printf("... ");
							}
						
							if (cJagGD.UploadFile(szFilename, offset, addr, size, entry, false) == ERROR_SUCCESS)
							{
								if (!bQuiet) printf("OK!\n");
							}
							else
							{
								if (bQuiet) printf("UPLOAD ");
								printf("FAILED!\n");
							}
						}
					}
				}
				else if (_stricmp(*arg, "-x") == 0)
				{
					bExecute = true;
					if (argc)
					{
						argc--;
						arg++;

						entry = StringToNumber(*arg);
					}
				}
				else if (_stricmp(*arg, "-xr") == 0)
				{
					bExecute = true;
					bReboot = true;
				}
				arg++;
			}

			// and finally execute if required
			if (bExecute)
			{
				if (!bQuiet)
				{
					if (bReboot) printf("REBOOTING... ");
					else printf("EXECUTING $%x... ", entry);
				}
				if (cJagGD.Execute(bReboot ? EXEC_REBOOT : entry) == ERROR_SUCCESS)
				{
					if (!bQuiet) printf("OK!\n");
				}
				else
				{
					if (bQuiet) printf("EXECUTE ");
					printf("FAILED!\n");
				}
			}
		}
		else
		{
			printf("Jaguar GameDrive not connected.\n");
		}
	}

	return 0;
}
