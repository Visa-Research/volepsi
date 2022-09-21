

#include "volePSI/Paxos.h"

int main()
{
	
	volePSI::Paxos<volePSI::u32> pp;

	// call some functions which require linking.
	if(oc::sysRandomSeed() == oc::ZeroBlock)
		pp.setInput({});

	return 0;
}