#ifndef _FILTERCHAINGROUP_H_
#define _FILTERCHAINGROUP_H_

#include <vector>
#include <set>
#include "multiimage.h"
#include "procparams.h"
#include "filterchain.h"

namespace rtengine {
using namespace procparams;
class FilterChainGroup {

	std::vector<FilterChain*> filterChains;
	Buffer<int>* buffer;
	MultiImage* workerImage;
	ProcParams* procParams;
	ImageSource* imgSource;
	bool multiThread;

	void updateBuffer (int w, int h);

public:

	FilterChainGroup (ImageSource* imgSource, ProcParams* pparams, bool multiThread = true);
	~FilterChainGroup ();

	void process (const std::set<ProcEvent>& events);

	void addNewFilterChain (ImProcListener* listener);
	void removeFilterChain (ImProcListener* listener);
	void update 		   (ImProcListener* listener);

};

}

#endif