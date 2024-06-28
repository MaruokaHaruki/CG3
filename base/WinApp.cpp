#include "WinApp.h"
/// ===imgui=== //
#include "externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

///=====================================================/// 
///ウィンドウプロシーシャ
///=====================================================///
LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//ImGuiに伝える
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
	//メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}
	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

///=====================================================/// 
///ウィンドウの生成
///=====================================================///
void WinApp::CreateGameWindow(const wchar_t* title, int32_t clientWidth, int32_t clientHeight) {
	///-------------------------------------------/// 
	//COMの初期化
	///-------------------------------------------///
	CoInitializeEx(0, COINIT_MULTITHREADED);

	///-------------------------------------------/// 
	//ウィンドウクラス生成
	///-------------------------------------------///
	//ウィンドウプロシーシャ
	wc_.lpfnWndProc = WindowProc;
	//ウィンドウクラス
	wc_.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	//ウィンドウクラスの登録
	RegisterClass(&wc_);

	///-------------------------------------------/// 
	//ウィンドウ生成
	///-------------------------------------------///

	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	wrc_ = { 0,0,clientWidth,clientHeight };
	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc_, WS_OVERLAPPEDWINDOW, false);
	//ウィンドウ作成
	hwnd_ = CreateWindow(wc_.lpszClassName,
		title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc_.right - wrc_.left,
		wrc_.bottom - wrc_.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr);

	ShowWindow(hwnd_, SW_SHOW);
}

///=====================================================/// 
///ウィンドウの終了
///=====================================================///
void WinApp::CloseWindow() {
	// COM 終了
	CoUninitialize();
}

