#include <cstdint>
#include <string>
#include <windows.h>


///=====================================================/// 
///ウィンドウズアプリケーション
///=====================================================///
#pragma once
class WinApp {
private:
	/// ===クライアントの領域サイズ=== ///
	static constexpr  int32_t kWindowWidth_ = 1280;//横幅
	static constexpr  int32_t kWindowHeight_ = 720;//縦幅

public:

	/// <summary>
	/// ウィンドウプロシージャ
	/// </summary>
	/// <param name="hwnd"></param>
	/// <param name="msg"></param>
	/// <param name="wparam"></param>
	/// <param name="lparam"></param>
	/// <returns></returns>
	static LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	/// <summary>
	/// ウィンドウの生成
	/// </summary>
	/// <param name="title">ウィンドウタイトル</param>
	/// <param name="clientWidth">ウィンドウ横幅</param>
	/// <param name="clientHeight">ウィンドウ縦幅</param>
	void CreateGameWindow(const wchar_t* title = L"DirectXGame", int32_t clientWidth = kWindowWidth_, int32_t clientHeight = kWindowHeight_);

	/// <summary>
	/// ウィンドウの終了
	/// </summary>
	void CloseWindow();

	// ゲッター
	static constexpr int32_t GetWindowWidth() { return kWindowWidth_; }
	static constexpr int32_t GetWindowHeight() { return kWindowHeight_; }
	WNDCLASS GetWindowClass() const { return wc_; }
	RECT GetWindowRect() const { return wrc_; }
	HWND GetWindowHandle() const { return hwnd_; }

	// セッター
	void SetWindowClass(const WNDCLASS& wc) { wc_ = wc; }
	void SetWindowRect(const RECT& wrc) { wrc_ = wrc; }
	void SetWindowHandle(HWND hwnd) { hwnd_ = hwnd; }

private:
	/// ===ウィンドウクラス=== ///
	WNDCLASS wc_{};

	/// ===WRC=== ///
	RECT wrc_;

	/// ===ウィンドウハンドル=== ///
	HWND hwnd_;

};

