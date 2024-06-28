#pragma once
#include <Windows.h>
#include <string>

///--------------------------------------------------
//Wstring変換
///--------------------------------------------------
#pragma region Wstring変換

//Wstring -> string
std::wstring ConvertString(const std::string& str);

//string -> Wstring
std::string ConvertString(const std::wstring& str);

#pragma endregion