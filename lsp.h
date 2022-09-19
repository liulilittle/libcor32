#pragma once

#include <stdio.h>
#include <Winsock2.h> 

BOOL IsInstallProvider(BOOL b32);
BOOL UninstallProvider(BOOL b32);
BOOL InstallProvider(WCHAR* pwszPathName, BOOL b32);