#pragma once
// � 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <fstream>
#include <string>
#include <vector>
#include <assert.h>
#include "Defines.h"
#include "cryptoTools/Common/CLP.h"

namespace volePSI
{


	std::ifstream::pos_type filesize(std::ifstream& file);

	bool hasSuffix(std::string const& value, std::string const& ending);

	bool isHexBlock(const std::string& buff);
	block hexToBlock(const std::string& buff);

	enum class FileType
	{
		Bin,
		Csv,
		Unspecified
	};

	enum class Role {
		Sender = 0,
		Receiver = 1,
		Invalid
	};

	std::vector<block> readSet(const std::string& path, FileType ft, bool debug);

	void writeOutput(std::string outPath, FileType ft, const std::vector<u64>& intersection, bool indexOnly, std::string inPath);



	void padSmallSet(std::vector<block>& set, u64& theirSize, const oc::CLP& cmd);

	void doFilePSI(const oc::CLP& cmd);


	inline
		bool exist(std::string path)
	{
		std::fstream stream;
		stream.open(path.c_str(), std::ios::in);
		return stream.is_open();
	}

}

