#include <windows.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define ID_WINDOW_KEY_EDITCTL	50
#define ID_WINDOW_GENERATE_BTN	51

#define WINDOW_WIDTH			500
#define WINDOW_HEIGHT			175

unsigned char base58[] = 
{
	'1', '2', '3', '4', '5', '6', '7', '8', '9', 'A',
	'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L',
	'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'm', 'n', 'o', 'p', 'q', 'r',
	's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

int is_valid_minikey(char *minikey)
{
	char *tmpbuf = (char *)malloc(sizeof(char) * (strlen(minikey) + 2));
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int ret;
	EVP_MD_CTX ctx;
	
	strcpy(tmpbuf, minikey);
	strcat(tmpbuf, "?");
	
	EVP_DigestInit(&ctx, EVP_sha256());
	EVP_DigestUpdate(&ctx, tmpbuf, strlen(tmpbuf));
	EVP_DigestFinal(&ctx, hash, &ret);
	
	free(tmpbuf);
	
	// If hash is 0x00, return 1, else 0
	if(!hash[0]) return(1);
	else return(0);
}

// Candidate must be 32 or more chars
DWORD WINAPI GenerateThreadProc(char *Candidate)
{
	int i;
	unsigned char tmp;
		
	for(;;)
	{
		for(i = 0; i < 30; i++)
		{
			do
			{
				RAND_bytes(&tmp, 1);
				tmp /= (255 / (57 + 1));
			} while(tmp >= 58);
			
			Candidate[i] = base58[tmp];
		}
		
		Candidate[i] = 0x00;
		
		if(is_valid_minikey(Candidate)) break;
	}
	
	return(0);
}

typedef struct _Info
{
	HANDLE MyWaitObj, hGenerateThread;
	HWND hKeyEditCtl;
	char *Minikey;
} Info;

#if defined __GNUC__
VOID CALLBACK GenerateCompleteCallback(PVOID CallbackInfo, BOOLEAN TimerOrWaitFired __attribute__ ((unused)))
#else
VOID CALLBACK GenerateCompleteCallback(PVOID CallbackInfo, BOOLEAN TimerOrWaitFired)
#endif
{
	Info *CInfo = (Info *)CallbackInfo;
	
	UnregisterWait(CInfo->MyWaitObj);
	CloseHandle(CInfo->hGenerateThread);
	SetWindowText(CInfo->hKeyEditCtl, CInfo->Minikey);
	
	CloseHandle(CInfo->hGenerateThread);
	free(CInfo->Minikey);
	return;
}

LRESULT WINAPI MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hKeyEditCtl, hGenerateBtn;
	
	switch(msg)
	{
		case WM_CREATE:
		{
			RECT MainWindowRect;
			
			GetClientRect(hwnd, &MainWindowRect);
			
			hKeyEditCtl = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER | ES_READONLY, (int)(MainWindowRect.right * .1),
				(int)(MainWindowRect.bottom * .3), (int)(MainWindowRect.right * .8), 20, hwnd,
				(HMENU)ID_WINDOW_KEY_EDITCTL, GetModuleHandle(NULL), NULL);
				
			hGenerateBtn = CreateWindow(TEXT("BUTTON"), TEXT("Generate"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				(int)(MainWindowRect.right * .47), (int)(MainWindowRect.bottom * .6), 55, 30, hwnd,
				(HMENU)ID_WINDOW_GENERATE_BTN, GetModuleHandle(NULL), NULL);
			
			SendMessage(hKeyEditCtl, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
			SendMessage(hGenerateBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
				
			return(FALSE);
		}
		case WM_COMMAND:
		{
			if((HIWORD(wParam) == BN_CLICKED) && (LOWORD(wParam) == ID_WINDOW_GENERATE_BTN))
			{
				char *Minikey;
				Info *CallbackInfo;
				HANDLE hGenerateThread;
				
				CallbackInfo = (Info *)malloc(sizeof(Info));
				Minikey = (char *)malloc(sizeof(char) * 32);
				hGenerateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GenerateThreadProc, Minikey, 0, NULL);
				
				CallbackInfo->Minikey = Minikey;
				CallbackInfo->hGenerateThread = hGenerateThread;
				CallbackInfo->hKeyEditCtl = GetDlgItem(hwnd, ID_WINDOW_KEY_EDITCTL);
				RegisterWaitForSingleObject(&CallbackInfo->MyWaitObj, hGenerateThread, GenerateCompleteCallback, CallbackInfo, INFINITE, WT_EXECUTEONLYONCE);
				return(FALSE);
			}
			else
			{
				return(DefWindowProc(hwnd, msg, wParam, lParam));
			}
		}
		case WM_CLOSE:
		{
			DestroyWindow(hwnd);
			break;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
		default:
			return(DefWindowProc(hwnd, msg, wParam, lParam));
	}
	return(0);
}

#ifdef __GNUC__
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance __attribute__ ((unused)), LPSTR lpCmdLine __attribute__ ((unused)), INT nCmdShow)
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
#endif
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG msg;
	
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = TEXT("Window");
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	
	RegisterClassEx(&wc);
	
	hwnd = CreateWindow(TEXT("Window"), TEXT("Minikey Generator"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);
	
	SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	
	// If GetMessage() fails it returns -1,
	// so use > 0 for the check.
	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if(!IsDialogMessage(hwnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	return((int)msg.wParam);
}