#pragma once
#include"structure/Matrix4x4.h"
#include"4x4Calc.h"
#include <cmath>

/// <summary>
/// 正射影行列
/// </summary>
/// <param name="left"></param>
/// <param name="top"></param>
/// <param name="right"></param>
/// <param name="bottom"></param>
/// <param name="nearClip"></param>
/// <param name="farClip"></param>
/// <returns></returns>
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	// 単位行列で初期化
	Matrix4x4 result = IdentityMatrix();

	// 正射影平面の範囲から正射影行列を構築する
	result.m[0][0] = 2.0f / ( right - left );
	result.m[1][1] = 2.0f / ( top - bottom );
	result.m[2][2] = -2.0f / ( farClip - nearClip );
	result.m[3][0] = -( right + left ) / ( right - left );
	result.m[3][1] = -( top + bottom ) / ( top - bottom );
	result.m[3][2] = -( farClip + nearClip ) / ( farClip - nearClip );

	return result;

}

/// <summary>
/// 透視投影行列
/// </summary>
/// <param name="fovY"></param>
/// <param name="aspectRatio"></param>
/// <param name="nearClip"></param>
/// <param name="farClip"></param>
/// <returns></returns>
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	// 単位行列で初期化
	Matrix4x4 result = IdentityMatrix();

	// tanの逆説
	float cot = tanf(fovY / 2.0f);

	// 射影変換行列の要素を計算する
	result.m[0][0] = 1.0f / ( aspectRatio * cot );
	result.m[1][1] = 1.0f / cot;
	result.m[2][2] = farClip / ( farClip - nearClip );
	result.m[2][3] = 1.0f;
	result.m[3][2] = -nearClip * farClip / ( farClip - nearClip );
	result.m[3][3] = 0.0f;

	return result;
}

/// <summary>
/// ビューポート行列
/// </summary>
/// <param name="left"></param>
/// <param name="top"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="minDepth"></param>
/// <param name="maxDepth"></param>
/// <returns></returns>
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	// 単位行列で初期化
	Matrix4x4 result = IdentityMatrix();

	// ビューポート変換行列の要素を計算する
	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[3][0] = left + width / 2.0f;
	result.m[3][1] = top + height / 2.0f;
	result.m[3][2] = minDepth;

	return result;
}
#pragma endregion

