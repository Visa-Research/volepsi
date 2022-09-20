#pragma once	
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "cryptoTools/Common/CLP.h"
#include "volePSI/config.h"
#ifdef VOLE_PSI_ENABLE_GMW

void generateBase_test();
void SilentTripleGen_test();
//void IknpTripleGen_test();



void Gmw_half_test(const oc::CLP& cmd);
void Gmw_basic_test(const oc::CLP& cmd);
void Gmw_inOut_test(const oc::CLP& cmd);
void Gmw_xor_test(const oc::CLP& cmd);
void Gmw_and_test(const oc::CLP& cmd);
void Gmw_na_and_test(const oc::CLP& cmd);
void Gmw_or_test(const oc::CLP& cmd);
void Gmw_xor_and_test(const oc::CLP& cmd);
void Gmw_aa_na_and_test(const oc::CLP& cmd);

void Gmw_add_test(const oc::CLP& cmd);
void Gmw_noLevelize_test(const oc::CLP& cmd);

#endif