// dllmain.cpp : DLL 응용 프로그램의 진입점을 정의합니다.
#include "stdafx.h"
#include "d3d9.h"
#include "d3dx9.h"
#pragma comment(lib, "d3dx9.lib")

#define ENDSCENE 42	// vTable의 42번째에 위치함

static HRESULT WINAPI h_EndScene(LPDIRECT3DDEVICE9 pDevice); 
typedef HRESULT(__stdcall* EndScene_t)(LPDIRECT3DDEVICE9);
EndScene_t org_EndScene;

HRESULT WINAPI h_EndScene(LPDIRECT3DDEVICE9 pDevice)
{ 


	//DXGameHook.DrawRect(pDevice, 40, 110, 50, 50, txtPink);
	return org_EndScene(pDevice); 
}

void *DetourFunc(BYTE *src, const BYTE *dst, const int len) 
{
	BYTE *jmp = (BYTE*)malloc(len+5);
	DWORD dwback;
	VirtualProtect(src, len, PAGE_READWRITE, &dwback);
	memcpy(jmp, src, len); jmp += len;
	jmp[0] = 0xE9;
	*(DWORD*)(jmp+1) = (DWORD)(src+len - jmp) - 5;
	src[0] = 0xE9;
	*(DWORD*)(src+1) = (DWORD)(dst - src) - 5;
	VirtualProtect(src, len, dwback, &dwback);
	return (jmp-len);
}

bool bDataCompare(const BYTE* pData, const BYTE* bMask, const char* szMask)
{
	for(;*szMask;++szMask,++pData,++bMask)
		if(*szMask=='x'&& *pData!=*bMask ) 
			return false;
	return (*szMask) == NULL;
}

DWORD FindPattern(DWORD dwAddress,DWORD dwLen,BYTE *bMask,char* szMask)
{
	for(DWORD i=0; i < dwLen; i++)
		if( bDataCompare( (BYTE*)( dwAddress+i ),bMask,szMask) )
			return(DWORD)(dwAddress+i);
	return 0;
}

int StartD3DHooks()
{
	DWORD D3DPattern,*vTable, DXBase=NULL;
	DXBase = (DWORD)LoadLibraryA("d3d9.dll");
	while(!DXBase);
	{
		D3DPattern = FindPattern(DXBase, 0x128000, 
			(PBYTE)"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx    xx    xx"); 
	}
	if(D3DPattern)
	{
		memcpy(&vTable,(void*)(D3DPattern+2),4);
		org_EndScene = (EndScene_t)DetourFunc((PBYTE)vTable[ENDSCENE],
			(PBYTE)h_EndScene,5);
	}
	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			// shared memory로 hwnd 얻기
			TCHAR str[256];
			HWND hWnd = 0;
			GetWindowText(hWnd, str, 256);
			lstrcat(str, L" (Hooked)");
			SetWindowText(hWnd, str);

			/* d3d hook */
			DisableThreadLibraryCalls(hModule);	// DLL THREAD ATTACH 이벤트 미연 방지, 디버거 검출 코드로부터 숨기.
			StartD3DHooks();
			break;
		}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

