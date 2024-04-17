// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <objbase.h>

namespace {
	// returns value of hex digit (0-15), or 16 for illegal char 
	unsigned char hexDigit(char ch)
	{
		if (ch > 47 && ch < 58)
			return ch - 48;
		if (ch > 96 && ch < 103)
			return ch - 87;
		if (ch > 64 && ch < 71)
			return ch - 55;
		return 16;
	}
}

namespace arras4 {
	namespace api {

void UUID::toString(std::string& out) const
{
	char one[10], two[6], three[6], four[6], five[14];

	snprintf(one, 10, "%02x%02x%02x%02x",
		mBytes[0], mBytes[1], mBytes[2], mBytes[3]);
	snprintf(two, 6, "%02x%02x",
		mBytes[4], mBytes[5]);
	snprintf(three, 6, "%02x%02x",
		mBytes[6], mBytes[7]);
	snprintf(four, 6, "%02x%02x",
		mBytes[8], mBytes[9]);
	snprintf(five, 14, "%02x%02x%02x%02x%02x%02x",
		mBytes[10], mBytes[11], mBytes[12], mBytes[13], mBytes[14], mBytes[15]);
	const std::string sep("-");
	out = one + sep + two + sep + three + sep + four + sep + five;
}

void UUID::parse(const std::string& s)
{
	if (s.empty()) {
		mBytes.fill(0);
		return;
	}

	unsigned bp = 0; // byte we're writing
	size_t sp = 0; // char we're reading
	size_t len = s.size();
	while (bp < 16 && sp < len - 1)
	{
		if (s[sp] == '-') {
			sp++;
			continue;
		}
		char c1 = hexDigit(s[sp++]);
		char c2 = hexDigit(s[sp++]);
		if (c1 < 16 && c2 < 16) {
			mBytes[bp++] = c1 * 16 + c2;
		} else {
			// illegal character, set to null
			mBytes.fill(0);
			break;
		}
	}
	
	if (bp < 16 || sp < len) mBytes.fill(0);
}

void UUID::regenerate()
{
	GUID newId;
	CoCreateGuid(&newId);

	mBytes[0] = (unsigned char)((newId.Data1 >> 24) & 0xFF);
	mBytes[1] = (unsigned char)((newId.Data1 >> 16) & 0xFF);
	mBytes[2] = (unsigned char)((newId.Data1 >> 8) & 0xFF);
	mBytes[3] = (unsigned char)((newId.Data1) & 0xff);
	mBytes[4] = (unsigned char)((newId.Data2 >> 8) & 0xFF);
	mBytes[5] = (unsigned char)((newId.Data2) & 0xff);
	mBytes[6] = (unsigned char)((newId.Data3 >> 8) & 0xFF);
	mBytes[7] = (unsigned char)((newId.Data3) & 0xFF);
	mBytes[8] = (unsigned char)newId.Data4[0];
	mBytes[9] = (unsigned char)newId.Data4[1];
	mBytes[10] = (unsigned char)newId.Data4[2];
	mBytes[11] = (unsigned char)newId.Data4[3];
	mBytes[12] = (unsigned char)newId.Data4[4];
	mBytes[13] = (unsigned char)newId.Data4[5];
	mBytes[14] = (unsigned char)newId.Data4[6];
	mBytes[15] = (unsigned char)newId.Data4[7];
	
}


}
}


