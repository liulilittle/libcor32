#include "lsp.h"

#include <stdio.h> 
#include <Winsock2.h> 
#include <Windows.h> 
#include <Ws2spi.h> 
#include <tchar.h> 
#include <iostream>
#include <Sporder.h>      // ������WSCWriteProviderOrder���� 
#include <Iphlpapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Rpcrt4.lib")  // ʵ����UuidCreate

static GUID ProviderGuid[] = {
    GUID({ 0x70b2b755, 0xa09d, 0x4b5d,{ 0xba, 0xda, 0xdb, 0x70, 0xbb, 0x1a, 0xbb, 0x21 } }),
    GUID({ 0x51361ede, 0xe7c4, 0x4598,{ 0xa1, 0x77, 0xf4, 0xc5, 0xe9, 0x1, 0x7c, 0x25 } }),
};

static LPWSAPROTOCOL_INFOW GetProvider(GUID& providerGuid, LPINT lpnTotalProtocols)
{
    DWORD dwSize = 0;
    int nError;
    LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

    // ȡ����Ҫ�ĳ��� 
    if (::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR)
    {
        if (nError != WSAENOBUFS)
        {
            return NULL;
        }
    }

    pProtoInfo = (LPWSAPROTOCOL_INFOW)::GlobalAlloc(GPTR, dwSize);
    *lpnTotalProtocols = ::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError);
    return pProtoInfo;
}

static void FreeProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
    ::GlobalFree(pProtoInfo);
}

static BOOL UninstallProvider(GUID& providerGuid)
{
    LPWSAPROTOCOL_INFOW pProtoInfo;
    int nProtocols;
    DWORD dwLayeredCatalogId;

    // ����Guidȡ�÷ֲ�Э���Ŀ¼ID�� 
    pProtoInfo = GetProvider(providerGuid, &nProtocols);
    int nError, i;
    for (i = 0; i < nProtocols; i++)
    {
        if (memcmp(&providerGuid, &pProtoInfo[i].ProviderId, sizeof(providerGuid)) == 0)
        {
            dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
            break;
        }
    }

    if (i < nProtocols)
    {
        // �Ƴ�Э���� 
        for (int i = 0; i < nProtocols; i++)
        {
            if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) &&
                (pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId))
            {
                ::WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError);
            }
        }
        // �Ƴ��ֲ�Э�� 
        ::WSCDeinstallProvider(&providerGuid, &nError);
    }
    FreeProvider(pProtoInfo);
    return TRUE;
}

static BOOL InstallProvider(GUID& providerGuid, WCHAR* pwszPathName)
{
    WCHAR wszLSPName[] = L"PaperAirplane";
    LPWSAPROTOCOL_INFOW pProtoInfo;
    int nProtocols;
    WSAPROTOCOL_INFOW OriginalProtocolInfo[3];
    DWORD            dwOrigCatalogId[3];
    int nArrayCount = 0;

    DWORD dwLayeredCatalogId;       // ���Ƿֲ�Э���Ŀ¼ID�� 

    int nError;

    // �ҵ����ǵ��²�Э�飬����Ϣ���������� 
    // ö�����з�������ṩ�� 
    pProtoInfo = GetProvider(providerGuid, &nProtocols);
    BOOL bFindUdp = FALSE;
    BOOL bFindTcp = FALSE;
    for (int i = 0; i < nProtocols; i++)
    {
        if (pProtoInfo[i].iAddressFamily == AF_INET)
        {
            if (!bFindUdp && pProtoInfo[i].iProtocol == IPPROTO_UDP)
            {
                memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                OriginalProtocolInfo[nArrayCount].dwServiceFlags1 =
                    OriginalProtocolInfo[nArrayCount].dwServiceFlags1 & (~XP1_IFS_HANDLES);

                dwOrigCatalogId[nArrayCount++] = pProtoInfo[i].dwCatalogEntryId;

                bFindUdp = TRUE;
            }

            if (!bFindTcp && pProtoInfo[i].iProtocol == IPPROTO_TCP)
            {
                memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                OriginalProtocolInfo[nArrayCount].dwServiceFlags1 =
                    OriginalProtocolInfo[nArrayCount].dwServiceFlags1 & (~XP1_IFS_HANDLES);

                dwOrigCatalogId[nArrayCount++] = pProtoInfo[i].dwCatalogEntryId;

                bFindTcp = TRUE;
            }
        }
    }

    // ��װ���ǵķֲ�Э�飬��ȡһ��dwLayeredCatalogId 
    // �����һ���²�Э��Ľṹ���ƹ������� 
    WSAPROTOCOL_INFOW LayeredProtocolInfo;
    memcpy(&LayeredProtocolInfo, &OriginalProtocolInfo[0], sizeof(WSAPROTOCOL_INFOW));

    // �޸�Э�����ƣ����ͣ�����PFL_HIDDEN��־ 
    wcscpy(LayeredProtocolInfo.szProtocol, wszLSPName);
    LayeredProtocolInfo.ProtocolChain.ChainLen = LAYERED_PROTOCOL; // 0; 
    LayeredProtocolInfo.dwProviderFlags |= PFL_HIDDEN;

    __try
    {
        // ��װ 
        if (::WSCInstallProvider(&providerGuid,
            pwszPathName, &LayeredProtocolInfo, 1, &nError) == SOCKET_ERROR)
        {
            return FALSE;
        }

        // ����ö��Э�飬��ȡ�ֲ�Э���Ŀ¼ID�� 
        FreeProvider(pProtoInfo);
        pProtoInfo = GetProvider(providerGuid, &nProtocols);
        for (int i = 0; i < nProtocols; i++)
        {
            if (memcmp(&pProtoInfo[i].ProviderId, &providerGuid, sizeof(providerGuid)) == 0)
            {
                dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
                break;
            }
        }

        // ��װЭ���� 
        // �޸�Э�����ƣ����� 
        WCHAR wszChainName[WSAPROTOCOL_LEN + 1];
        for (int i = 0; i < nArrayCount; i++)
        {
            if (OriginalProtocolInfo[i].iProtocol == IPPROTO_TCP)
            {
                swprintf(wszChainName, L"%ws %ws", wszLSPName, L"Tcpip [TCP/IP]");
            }
            else if (OriginalProtocolInfo[i].iProtocol == IPPROTO_UDP)
            {
                swprintf(wszChainName, L"%ws %ws", wszLSPName, L"Tcpip [UDP/IP]");
            }
            wcscpy(OriginalProtocolInfo[i].szProtocol, wszChainName);
            if (OriginalProtocolInfo[i].ProtocolChain.ChainLen == 1)
            {
                OriginalProtocolInfo[i].ProtocolChain.ChainEntries[1] = dwOrigCatalogId[i];
            }
            else
            {
                for (int j = OriginalProtocolInfo[i].ProtocolChain.ChainLen; j > 0; j--)
                {
                    OriginalProtocolInfo[i].ProtocolChain.ChainEntries[j]
                        = OriginalProtocolInfo[i].ProtocolChain.ChainEntries[j - 1];
                }
            }
            OriginalProtocolInfo[i].ProtocolChain.ChainLen++;
            OriginalProtocolInfo[i].ProtocolChain.ChainEntries[0] = dwLayeredCatalogId;
        }

        // ��ȡһ��Guid����װ֮ 
        GUID ProviderChainGuid;
        if (::UuidCreate(&ProviderChainGuid) == RPC_S_OK)
        {
            if (::WSCInstallProvider(&ProviderChainGuid,
                pwszPathName, OriginalProtocolInfo, nArrayCount, &nError) == SOCKET_ERROR)
            {
                return FALSE;
            }
        }
        else
        {
            return FALSE;
        }

        // ��������WinsockĿ¼�������ǵ�Э������ǰ 
        // ����ö�ٰ�װ��Э�� 
        FreeProvider(pProtoInfo);
        pProtoInfo = GetProvider(providerGuid, &nProtocols);

        DWORD dwIds[1000];
        int nIndex = 0;
        // ������ǵ�Э���� 
        for (int i = 0; i < nProtocols; i++)
        {
            if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) &&
                (pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId))
            {
                dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
            }
        }
        // �������Э�� 
        for (int i = 0; i < nProtocols; i++)
        {
            if ((pProtoInfo[i].ProtocolChain.ChainLen <= 1) ||
                (pProtoInfo[i].ProtocolChain.ChainEntries[0] != dwLayeredCatalogId))
            {
                dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
            }
        }
        // ��������WinsockĿ¼ 
        if ((nError = ::WSCWriteProviderOrder(dwIds, nIndex)) != ERROR_SUCCESS)
        {
            return FALSE;
        }

        INT nCategories = 0;
        size_t nCategoriesSize = 4;
        if (WSCGetProviderInfo(&providerGuid, ProviderInfoLspCategories, (PBYTE)&nCategories, &nCategoriesSize, 0, &nError) != ERROR_SUCCESS)
        {
            return FALSE;
        }

        nCategories |= LSP_SYSTEM
            | LSP_INSPECTOR
            | LSP_REDIRECTOR
            | LSP_PROXY
            | LSP_FIREWALL
            | LSP_INBOUND_MODIFY
            | LSP_OUTBOUND_MODIFY
            | LSP_CRYPTO_COMPRESS
            | LSP_LOCAL_CACHE;

        if (WSCSetProviderInfo(&providerGuid, ProviderInfoLspCategories, (PBYTE)&nCategories, nCategoriesSize, 0, &nError) != ERROR_SUCCESS)
        {
            return FALSE;
        }
        return TRUE;
    }
    __finally
    {
        FreeProvider(pProtoInfo);
    }
}

BOOL InstallProvider(WCHAR* pwszPathName, BOOL b32)
{
    if (IsInstallProvider(b32))
    {
        return TRUE;
    }
    if (NULL == pwszPathName)
    {
        pwszPathName = (WCHAR*)L"%SystemRoot%\\System32\\PaperAirplane.dll";
    }
    if (b32)
    {
        return InstallProvider(ProviderGuid[0], pwszPathName);
    }
    else
    {
        return InstallProvider(ProviderGuid[1], pwszPathName);
    }
}

BOOL UninstallProvider(BOOL b32)
{
    if (!IsInstallProvider(b32))
    {
        return FALSE;
    }
    if (b32)
    {
        return UninstallProvider(ProviderGuid[0]);
    }
    else
    {
        return UninstallProvider(ProviderGuid[1]);
    }
}

BOOL IsInstallProvider(BOOL b32)
{
    INT nCategories = 0;
    INT nError = 0;
    size_t nCategoriesSize = 4;
    if (b32)
    {
        int rc = WSCGetProviderInfo(&ProviderGuid[0],
            ProviderInfoLspCategories, (PBYTE)&nCategories, &nCategoriesSize, 0, &nError);
        if (rc != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    else
    {
        int rc = WSCGetProviderInfo(&ProviderGuid[1],
            ProviderInfoLspCategories, (PBYTE)&nCategories, &nCategoriesSize, 0, &nError);
        if (rc != ERROR_SUCCESS)
        {
            return FALSE;
        }
    }
    return nError == ERROR_SUCCESS ? TRUE : FALSE;
}