#pragma once

#include <string>
#include <windows.h>

using FString = std::string;
using FWString = std::wstring;
using FWideString = std::wstring;

using FStringView = std::string_view;
using FWStringView = std::wstring_view;

inline FString WideToUTF8(const FWideString& InWideStr)
{
	if (InWideStr.empty()) return FString();

	int size_needed = ::WideCharToMultiByte(
		CP_UTF8,                            // UTF-8 코드 페이지
		0,                                  // 플래그
		InWideStr.c_str(),                  // 입력 wstring
		static_cast<int>(InWideStr.size()), // 입력 문자 수
		nullptr,                            // 출력 버퍼 (nullptr이면 크기만 계산)
		0,                                  // 출력 버퍼 크기
		nullptr,                            // 기본 문자
		nullptr                             // 기본 문자 사용 여부
	);

	if (size_needed <= 0)
	{
		return FString(); // 변환 실패
	}

	// 실제 변환 수행
	FString result(size_needed, 0);
	::WideCharToMultiByte(
		CP_UTF8,
		0,
		InWideStr.c_str(),
		static_cast<int>(InWideStr.size()),
		&result[0],                         // 출력 버퍼
		size_needed,
		nullptr,
		nullptr
	);

	return result;
}

// string을 wstring으로 변환하는 함수
inline FWideString UTF8ToWide(const FString& str)
{
	if (str.empty()) return L"";

	// 필요한 버퍼 크기 계산
	int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);

	std::wstring wstrTo(size_needed, 0);
	// 실제 변환 수행
	MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);

	return wstrTo;
}
