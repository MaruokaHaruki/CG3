#pragma once
#include "structure/Vector3.h"
#include <cmath>
#include "structure/Matrix3x3.h"

//内積
float Dot(const Vector3& v1, const Vector3& v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

// ベクトルの大きさ（長さ）を計算する関数
float Magnitude(const Vector3& v) {
	return std::sqrt(Dot(v, v));
}

// ベクトルを正規化
Vector3 Normalize(const Vector3& v) {
	float mag = Magnitude(v);
	if (mag != 0.0f) {
		return { v.x / mag, v.y / mag, v.z / mag };
	}
	// ゼロベクトルの場合はそのまま返す
	return v;
}