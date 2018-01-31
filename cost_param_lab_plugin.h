#pragma once
#include "plugins/route_server_plugin.h"

class CostParamLabPlugin
: public RouteServerPlugin
, public RouteServerRequestProtocol
, public RouteServerComputationProtocol

{
public:
	// derived from RouteServerRequestProtocol
	virtual void beforeRouting(RouteEngine* routeEngine);
	virtual void onParsingRequest(json_t* root);
	virtual void onWritingResponse(json_t* root, const routeContainer::routeResult::CRouteResult* result);
	// derived from RouteServerComputationProtocol
	virtual size_t modifyOutNodesAndCosts(DSegmentId fromSeg, DSegmentId* outSegs, RoutingCost* outCosts, size_t outNum, BOOL isReverseSearch, void *userdata);

	// register / unregister
	static RouteServerPlugin* createPlugin();
	static void releasePlugin(RouteServerPlugin* plugin);

	// derived from RouteServerPlugin
	virtual void reset();
private:
	CostParamLabPlugin();
	~CostParamLabPlugin();
private:
	class CPrivate;
	CPrivate* mp; 
};
