#include "stdafx.h"
#include "cost_param_lab_plugin.h"
#include "cost_logbuf.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/ostreamwrapper.h"

#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>

#define COSTLAB_SINGLE_TRAFFICLIGHT_PENALTY (1000)
// wangxl code on 20170407
class CCostLabLogger : public CCostLogBuffer
{
	public:
		size_t printf(const char* fmt, ...)
		{
			static char buf[BUFSIZ];
			va_list ap;
			va_start(ap, fmt);
			const char* ts = timestamp();
			strncpy(buf, ts, BUFSIZ);
			size_t len = strlen(ts);
			len += vsnprintf(buf + len, BUFSIZ - len, fmt, ap);
			va_end(ap);

			if(len > 0)
			{
				CCostLogBuffer::write(buf, len);
			}
			return len;
		}

		static CCostLabLogger* instance()
		{
			if(!m_singleton)
			{
				m_singleton = new CCostLabLogger();
			}
			
			return m_singleton;
		}
		void close()
		{
			CCostLogBuffer::close();
		}

		void destroy()
		{
			if(m_singleton)
			{
				delete m_singleton;
				m_singleton = NULL;
			}
		}
	private: // helper
		const char* timestamp()
		{
			static char buf[BUFSIZ];
			time_t t = time(NULL);		
			struct tm* tm= localtime(&t);
			sprintf(buf, "%04d%02d%02d %02d:%02d:%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
			return buf;
		}

		void flush()
		{
			CCostLogBuffer::flush();
		}
	private: // compulsive to singleton instance
		CCostLabLogger() : CCostLogBuffer()
		{
		}

		~CCostLabLogger()
		{
			close();
		}
	private: // data member
		static CCostLabLogger* m_singleton;

	private: // copy prevention
		CCostLabLogger(CCostLabLogger&);
		CCostLabLogger& operator=(const CCostLabLogger&);
};

CCostLabLogger* CCostLabLogger::m_singleton = NULL;
const size_t g_stackBufPtSize = 384;
Point g_stackBufPt[g_stackBufPtSize];

const bool g_isTrafficLightLabCostOnly = true;

#define LANECLS_MAX (6)
#define COSTTIME_FACTOR (100)
#define IS_BETWEEN(_begin_, _end_, _n_) (((_n_) >= (_begin_)) && ((_n_) < (_end_)))

/* speed unit km/h */
static size_t g_costFactorSpeedTableByRoadPriority[RoutingRule_max][SegmentPriority_MAXNUM] =
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocPth	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* Recom */	{100,	80,	70,	65,	55,	40,	35,	20,	10,	0,	0},
/* Short */	{60,	60,	60,	60,	60,	60,	60,	60,	60,	0,	0},
/* Hiway */	{150,	80,	70,	65,	55,	40,	35,	20,	10,	0,	0},
/* econo */	{40,	80,	70,	65,	55,	40,	35,	20,	10,	0,	0},
/* pedes */	{60,	60,	60,	60,	60,	60,	60,	60,	60,	60,	60},
/* User */	{60,	60,	60,	60,	60,	60,	60,	60,	60,	60,	60}
};

/* lane factor for recommended rule, unit 0.01 */
static size_t g_costFactorLaneClassTableRule_Recommend[LANECLS_MAX][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocPth	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* 0.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 3.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

/* lane factor for shortest rule, unit 0.01 */
static size_t g_costFactorLaneClassTableRule_Shortest[LANECLS_MAX][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocPth	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* 0.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 3.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

/* factor unit 0.01 */
/* lane factor for recommended rule, unit 0.01 */
static size_t g_costFactorLaneClassTableRule_Highway[LANECLS_MAX][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocPth	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* 0.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 3.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

/* lane factor for shortest rule, unit 0.01 */
static size_t g_costFactorLaneClassTableRule_Economic[LANECLS_MAX][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocPth	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* 0.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 1.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 2.5 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* 3.0 */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

static size_t g_costFactorRoadUsageTable_Recommend[SegmentUsage_MAXNUM][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocalP	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* rotory */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* unknown */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* divided */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* hiBridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* Junction */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* exitrance */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* PrkArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* SrvArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* bridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* iRtTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* UTurn   */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* LeftTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* RightTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

static size_t g_costFactorRoadUsageTable_Shortest[SegmentUsage_MAXNUM][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocalP	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* rotory */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* unknown */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* divided */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* hiBridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* Junction */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* exitrance */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* PrkArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* SrvArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* bridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* iRtTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* UTurn   */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* LeftTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* RightTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

static size_t g_costFactorRoadUsageTable_PreferHighway[SegmentUsage_MAXNUM][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocalP	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* rotory */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* unknown */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* divided */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* hiBridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* Junction */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* exitrance */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* PrkArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* SrvArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* bridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* iRtTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* UTurn   */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* LeftTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* RightTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};

static size_t g_costFactorRoadUsageTable_Economic[SegmentUsage_MAXNUM][SegmentPriority_MAXNUM] = 
{
	/*	HiWay	ExpWay	Arter	LocMaj	Local	LocalP	LocMin	PoiConn	Other	bicyc	Pedes	 */

/* rotory */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* unknown */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* divided */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* hiBridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* Junction */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* exitrance */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* PrkArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* SrvArea */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* bridge */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* iRtTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* UTurn   */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* LeftTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* RightTurn */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
/* deprecate */	{100,	100,	100,	100,	100,	100,	100,	100,	100,	100,	0},
};


enum {
	ETurnType_unknown = 0,
       	ETurnType_forward,
	ETurnType_forwardleft, 
	ETurnType_forwardright, 
	ETurnType_left, 
	ETurnType_right, 
	ETurnType_backwordleft, 
	ETurnType_backwardright, 
	ETurnType_Uturn
};

static int g_costTurnPenalty[] ={

	3600 * COSTTIME_FACTOR, // ETurnType_unknown
	1  * COSTTIME_FACTOR,	// ETurnType_forward
	15 * COSTTIME_FACTOR,	// ETurnType_forwardleft
	5  * COSTTIME_FACTOR,	// ETurnType_backwardright
	30 * COSTTIME_FACTOR,	// ETurnType_left
	10 * COSTTIME_FACTOR,	// ETurnType_right
	45 * COSTTIME_FACTOR,	// ETurnType_forwardleft
	20 * COSTTIME_FACTOR,	// ETurnType_backwardright
	//60 * COSTTIME_FACTOR,	// ETurnType_Uturn
	600 * COSTTIME_FACTOR,	// ETurnType_Uturn
}; 



class CostParamLabPlugin::CPrivate
{
	RoutingRule m_routeRule;

public: // lifecycle
	CPrivate() : m_routeRule(RoutingRule_recommended)
	{
	}

	~CPrivate()
	{
	}

public: // helper
	void setRoutingRule(RoutingRule rule) 
	{
		m_routeRule = rule;
		switch(m_routeRule)
		{
			case RoutingRule_recommended:
				CCostLabLogger::instance()->printf("setRoutingRule() RoutingRule_recommended\n");
				break;
			case RoutingRule_shortest:
				CCostLabLogger::instance()->printf("setRoutingRule() RoutingRule_shortest\n");
				break;
			case RoutingRule_preferHighway:
				CCostLabLogger::instance()->printf("setRoutingRule() RoutingRule_preferHighway\n");
				break;
			case RoutingRule_economic:
				CCostLabLogger::instance()->printf("setRoutingRule() RoutingRule_economic\n");
				break;
			default:
				CCostLabLogger::instance()->printf("setRoutingRule() !NOT supported\n");
		}
	}

	RoutingRule getRoutingRule() {return m_routeRule;}

	struct tagLABFROMCOSTINFO
	{
		DSegmentId mDSegId;
		BOOL mAttrOK;
		DSegmentAttributes mAttr;
		BOOL mIsSuper;
		BOOL mIsToll;
		BOOL mIsRevSrch;
		size_t mOutNum;
	}m_LabFromCostInfo;

	void SetLabFromDSegCostInfo(DSegmentId fromSeg, size_t outnum, BOOL isReverseSearch)
	{
		m_LabFromCostInfo.mDSegId = fromSeg;	
		if(fromSeg != INVALID_DSEGMENT_ID)
		{
			m_LabFromCostInfo.mAttrOK = DSegment_getAttributes(fromSeg, &m_LabFromCostInfo.mAttr, SegmentAttributesFlag_AllButName);
			m_LabFromCostInfo.mIsSuper = DSegment_isSuperLink(fromSeg);

			if(RoutingRule_economic == getRoutingRule())
			{
				TollStation dummy;
				m_LabFromCostInfo.mIsToll = DSegment_getTollStationOnStart(fromSeg, &dummy);
			}

			m_LabFromCostInfo.mIsRevSrch = isReverseSearch;
			m_LabFromCostInfo.mOutNum = outnum;
			if(m_LabFromCostInfo.mAttrOK)
			{
				json_addFromInfo(m_LabFromCostInfo.mAttr, isReverseSearch, m_LabFromCostInfo.mIsSuper, fromSeg, outnum);
			}
		}

		return;
	}

	struct tagLABOUTCOSTINFO
	{
		DSegmentId mDSegId;
		BOOL mAttrOK;
		BOOL mIsSuper;
		DSegmentAttributes mAttr;
		RoutingCost mDefCost;
	} m_LabOutDSegCostInfo;

	void SetLabOutCostInfo(DSegmentId dsid, RoutingCost defcost)
	{
		m_LabOutDSegCostInfo.mDSegId = dsid;
		if(m_LabOutDSegCostInfo.mDSegId != INVALID_DSEGMENT_ID)
		{
			m_LabOutDSegCostInfo.mDefCost = defcost;

			DSegmentId outSeg = m_LabOutDSegCostInfo.mDSegId;
			m_LabOutDSegCostInfo.mAttrOK = DSegment_getAttributes(outSeg, &m_LabOutDSegCostInfo.mAttr, SegmentAttributesFlag_AllButName);
			m_LabOutDSegCostInfo.mIsSuper = DSegment_isSuperLink(outSeg);

			if(RoutingRule_economic ==  getRoutingRule() && FALSE)
			{
#if 0
				TollStation dummy;
				BOOL bOutToll = DSegment_getTollStationOnStart(outSegs[i], &dummy);
				if(!bFromToll && bOutToll)
				{
					crossNodeCost += 10000000;
				}
#endif
			}
		}
	}

	RoutingCost calcCurOutDSegBasCustomCost()
	{
		return m_LabOutDSegCostInfo.mAttrOK ? calcDSegBasicCostByAttribute(m_LabOutDSegCostInfo.mAttr) : INFINITE_ROUTING_COST;
	}

	RoutingCost calcCurOutDSegBasCost()
	{
		BOOL bUseDefCost = g_isTrafficLightLabCostOnly ? TRUE : FALSE;

		return bUseDefCost ? m_LabOutDSegCostInfo.mDefCost : calcCurOutDSegBasCustomCost();
	}

	RoutingCost calcDSegBasicCostByAttribute(DSegmentAttributes& attr)
	{
		size_t RuleIndex = SIZE_T_MAX;
		if(RoutingRule_recommended == m_routeRule)
		{
			RuleIndex = 0;
		}
		else if(RoutingRule_shortest == m_routeRule)
		{
			RuleIndex = 1;
		}
		else if(RoutingRule_preferHighway == m_routeRule)
		{
			RuleIndex = 2;
		}
		else if(RoutingRule_economic == m_routeRule)
		{
			RuleIndex = 3;
		}

		uint32 factorSpeed = 1;
		if(IS_BETWEEN(0, RoutingRule_max, m_routeRule))
		{
			factorSpeed = g_costFactorSpeedTableByRoadPriority[RuleIndex][attr.priority];
		}

		size_t laneClsIndex = 0; ;
		if(attr.forwardLaneNum < 0.5)
		{
			laneClsIndex = 0;
		}
		else if(attr.forwardLaneNum < 1)
		{
			laneClsIndex = 1;
		}
		else if(attr.forwardLaneNum < 1.5)
		{
			laneClsIndex = 2;
		}
		else if(attr.forwardLaneNum < 2)
		{
			laneClsIndex = 3;
		}
		else if(attr.forwardLaneNum < 2.5)
		{
			laneClsIndex = 4;
		}
		else if(attr.forwardLaneNum < 10)
		{
			laneClsIndex = 5;
		}
		else
		{
			laneClsIndex = LANECLS_MAX;
		}

		uint32 factorLane = 100;
		if(IS_BETWEEN(0, LANECLS_MAX, laneClsIndex))
		{
			if(RoutingRule_recommended == m_routeRule)
			{
				factorLane = g_costFactorLaneClassTableRule_Recommend[laneClsIndex][attr.priority];
			}
			else if(RoutingRule_shortest == m_routeRule)
			{
				factorLane = g_costFactorLaneClassTableRule_Shortest[laneClsIndex][attr.priority];
			}
			else if(RoutingRule_preferHighway == m_routeRule)
			{
				factorLane = g_costFactorLaneClassTableRule_Highway[laneClsIndex][attr.priority];
			}
			else if(RoutingRule_economic == m_routeRule)
			{
				factorLane = g_costFactorLaneClassTableRule_Economic[laneClsIndex][attr.priority];
			}
		}
		uint32 factorUsage = 100;
		if(IS_BETWEEN(0, SegmentUsage_MAXNUM, attr.usage))
		{
			if(RoutingRule_recommended == m_routeRule)
			{
				factorUsage = g_costFactorRoadUsageTable_Recommend[attr.usage][attr.priority];
			}
			else if(RoutingRule_shortest == m_routeRule)
			{
				factorUsage = g_costFactorRoadUsageTable_Shortest[attr.usage][attr.priority];
			}
			else if(RoutingRule_preferHighway == m_routeRule)
			{
				factorUsage = g_costFactorRoadUsageTable_PreferHighway[attr.usage][attr.priority];
			}
			else if(RoutingRule_economic == m_routeRule)
			{
				factorUsage = g_costFactorRoadUsageTable_Economic[attr.usage][attr.priority];
				if(!attr.tollFree)
				{
					factorUsage = factorUsage * 250 / 100; 
				}
			}

		}

		RoutingCost segCost = INFINITE_ROUTING_COST;
		if(0 != factorSpeed)
		{
			segCost = attr.length * factorLane * factorUsage * 36 / factorSpeed / 100;
		}
		else
		{
			CCostLabLogger::instance()->printf("ERORR : divide ZERO speed factor! link priority %d, link usage %d\n", attr.priority, attr.usage);	
		}

		return segCost;
	}

	size_t getSuperLinkTrafficLightNum(DSegmentId SuperDSegId)
	{
		size_t lightnum = 0;	
		size_t num = DSegment_getMemberNum(SuperDSegId);
		BOOL* aryLight = new BOOL[num];
		BOOL light_ok = DSegment_getSuperLinkTrafficLightArray(SuperDSegId, aryLight, num);
		if(light_ok)
		{
			for(size_t lightIdx = 0; lightIdx < num; ++lightIdx)
			{
				if(aryLight[lightIdx])
				{
					lightnum++;
				}
			}	
		}
		delete[] aryLight;
		return lightnum;
	}


	void PrintTurnType()
	{
		//const char* strTurnType[] = {"ETurnType_unknown", "ETurnType_forward", "ETurnType_forwardleft", "ETurnType_forwardright", "ETurnType_left", "ETurnType_right", "ETurnType_forwardleft", "ETurnType_backwardright", "ETurnType_Uturn" };

			//CCostLabLogger::instance()->printf("%s turn type %s(sav angle %d, cale angle %d) node cost %d \n", isReverseSearch ? "backward search" : "forward search", strTurnType[turnType], savAngle, turnAngle, nodeCost);
	}

	bool IsValidCurFromDSegAttr() const
	{
		return INVALID_DSEGMENT_ID != m_LabFromCostInfo.mDSegId && m_LabFromCostInfo.mAttrOK;
	}

	bool IsValidCurOutDSegAttr() const
	{
		return INVALID_DSEGMENT_ID != m_LabOutDSegCostInfo.mDSegId && m_LabOutDSegCostInfo.mAttrOK;
	}

	bool IsValidCurIntersectCostInfo() const
	{
		return IsValidCurFromDSegAttr() && IsValidCurOutDSegAttr();
	}

	RoutingCost calcCurOutDSegIntersectCustomCost()
	{
		return  IsValidCurIntersectCostInfo()
			? calcOutDSegIntersectCustomCost(m_LabFromCostInfo.mAttr, m_LabOutDSegCostInfo.mAttr, m_LabFromCostInfo.mIsRevSrch)
			: INFINITE_ROUTING_COST;
	}

	RoutingCost calcCurOutDSegIntersectCost()
	{
		BOOL bUseDefCost = g_isTrafficLightLabCostOnly ? TRUE : FALSE;

		return bUseDefCost ? 0 : calcCurOutDSegIntersectCustomCost();
	}

	RoutingCost calcOutDSegIntersectCustomCost(DSegmentAttributes& inAttr, DSegmentAttributes& outAttr, BOOL isReverseSearch)
	{
		int turnType = ETurnType_unknown;
		RoutingCost nodeCost = INFINITE_ROUTING_COST;
		if(IS_BETWEEN(0, 360, inAttr.endHeadingAngle) && IS_BETWEEN(0, 360, outAttr.startHeadingAngle))
		{
			int startAngle = outAttr.startHeadingAngle > 180 ? outAttr.startHeadingAngle - 180 : outAttr.startHeadingAngle + 180;
			int turnAngle = (!isReverseSearch) ? startAngle - inAttr.endHeadingAngle : inAttr.endHeadingAngle - startAngle;
			//int savAngle = turnAngle;

			if(turnAngle < 0)
			{
				turnAngle += 360;
			}

			if(turnAngle < 15 || turnAngle > 360 - 15)
			{
				turnType = ETurnType_forward;
			}
			else if(turnAngle < 45 || turnAngle > 360 - 45)
			{
				turnType = turnAngle < 180 ? ETurnType_forwardleft : ETurnType_forwardright;
			}
			else if(turnAngle < 135 || turnAngle > 360 - 135)
			{
				turnType = turnAngle < 180 ? ETurnType_left : ETurnType_right;
			}
			else if(turnAngle < 165 || turnAngle > 360 - 165)
			{
				turnType = turnAngle < 180 ? ETurnType_backwordleft : ETurnType_backwardright;
			}
			else 
			{
				turnType = ETurnType_Uturn;
			}

			nodeCost = g_costTurnPenalty[turnType];

			if((SegmentUsage_exitrance == inAttr.usage && SegmentUsage_exitrance != outAttr.usage)
					|| (SegmentUsage_exitrance == outAttr.usage && SegmentUsage_exitrance != inAttr.usage))
			{
				nodeCost += g_costTurnPenalty[ETurnType_backwordleft];
			}

			if((inAttr.sideRoad &&  !outAttr.sideRoad)
					|| (outAttr.sideRoad && ! inAttr.sideRoad))
			{
				nodeCost += g_costTurnPenalty[ETurnType_backwordleft];
			}

		}

		return nodeCost;
	}

	RoutingCost calcCurTrafficLightPenaltyCost()
	{
		return IsValidCurOutDSegAttr() ? calcTrafficLightPenaltyCost(m_LabOutDSegCostInfo.mIsSuper, m_LabOutDSegCostInfo.mDSegId, m_LabOutDSegCostInfo.mAttr) : 0;
	}

	RoutingCost calcTrafficLightPenaltyCost(bool isSuper, DSegmentId outDSegId, DSegmentAttributes& rOutAttr)
	{
		RoutingCost lightCostPenalty = 0;
		size_t light_num = isSuper ? getSuperLinkTrafficLightNum(outDSegId) : 0;
		if(rOutAttr.endTrafficLight)
		{
			light_num++;
		}

		if(light_num > 0)
		{
			if(g_isTrafficLightLabCostOnly)
			{
				lightCostPenalty = light_num * COSTLAB_SINGLE_TRAFFICLIGHT_PENALTY;
			}
			else
			{
				DSegmentAttributes trafficLightLinkAttr = rOutAttr;
				trafficLightLinkAttr.priority = SegmentPriority_poiConnector;
				trafficLightLinkAttr.length = 25 * light_num; 

				lightCostPenalty = calcDSegBasicCostByAttribute(trafficLightLinkAttr);
			}
		}
		
		return lightCostPenalty;
	}

	struct FromInfo
	{
		BOOL isReverseSearch;
		BOOL isSuper;
		DSegmentId dsid;
		NavInfoLinkId nil_id;
		DSegmentAttributes attr;
		Point firstShpPt;
		Point lastShpPt;
		size_t outnum;
	};

	struct OutInfo
	{
		BOOL isSuper;
		DSegmentId dsid;
		NavInfoLinkId nil_id;
		RoutingCost mCost;
	};

	struct RouteInfo
	{
		size_t mLinkNum;
		size_t mStartIdx;
		RoutingCost mCost;
		size_t mTrafficLightNum;
	};

	struct RouteLink
	{
		DSegmentId dsid;
		Point firstShpPt;
		Point lastShpPt;
		size_t mShpPtNum;
		size_t mShpPtVecStart;
	};

	bool json_dump(const char* json_path)
	{
		std::ofstream ofs;
		if(json_path){
			ofs.open(json_path);
			if(ofs.is_open()){
				rapidjson::OStreamWrapper osw(ofs);
				rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);

				writer.StartObject(); {
					size_t outidx = 0;
					writer.Key("ary_branch");
					writer.StartArray();
					for(size_t i = 0; i < m_vecFromInfo.size(); ++i){
						writer.StartObject();{
							writer.Key("is_revsrch");
							writer.Bool(!!m_vecFromInfo[i].isReverseSearch);
							writer.Key("from_isSuper");
							writer.Bool(!!m_vecFromInfo[i].isSuper);
							writer.Key("from_dsid");
							writer.Uint64(m_vecFromInfo[i].dsid);
							writer.Key("from_nilid");
							writer.Uint(m_vecFromInfo[i].nil_id);
							writer.Key("outnum");
							writer.Int(m_vecFromInfo[i].outnum);
							writer.Key("from_length");
							writer.Int(m_vecFromInfo[i].attr.length);
							writer.Key("from_firstShpPtX");
							writer.Uint(m_vecFromInfo[i].firstShpPt.x);
							writer.Key("from_firstShpPtY");
							writer.Uint(m_vecFromInfo[i].firstShpPt.y);
							writer.Key("from_lastShpPtX");
							writer.Uint(m_vecFromInfo[i].lastShpPt.x);
							writer.Key("from_lastShpPtY");
							writer.Uint(m_vecFromInfo[i].lastShpPt.y);

							writer.Key("outary");
							writer.StartArray();
							for(size_t j = 0; j < m_vecFromInfo[i].outnum; ++j){
								OutInfo& rOutInfo = m_vecOutInfo[outidx++];
								writer.StartObject();{
									writer.Key("is_super");
									writer.Bool(!!rOutInfo.isSuper);
									writer.Key("out_dsid");
									writer.Uint64(rOutInfo.dsid);
									writer.Key("out_nilid");
									writer.Uint(rOutInfo.nil_id);
									writer.Key("out_cost");
									writer.Uint(rOutInfo.mCost);
								}writer.EndObject();
							}
							writer.EndArray();

						} writer.EndObject();
					}
					writer.EndArray();

					writer.Key("ary_route");
					writer.StartArray();
					for(size_t i = 0; i < m_vecRouteInfo.size(); ++i)
					{
						size_t basIdx = m_vecRouteInfo[i].mStartIdx;
						size_t lnknum = m_vecRouteInfo[i].mLinkNum;
						writer.StartObject();

						writer.Key("cost");
						writer.Uint(m_vecRouteInfo[i].mCost);
						writer.Key("lightnum");
						writer.Uint(m_vecRouteInfo[i].mTrafficLightNum);

						writer.Key("linkary");
						writer.StartArray();
						for(size_t j = 0; j < lnknum; ++j)
						{
							writer.StartObject();
							{
								const RouteLink& rRutLnk = m_vecRouteLink[basIdx + j];
								writer.Key("dsid");
								writer.Uint(rRutLnk.dsid);
								writer.Key("firstShpPtX");
								writer.Int(rRutLnk.firstShpPt.x);
								writer.Key("firstShpPtY");
								writer.Int(rRutLnk.firstShpPt.y);
								writer.Key("lastShpPtX");
								writer.Int(rRutLnk.lastShpPt.x);
								writer.Key("lastShpPtY");
								writer.Int(rRutLnk.lastShpPt.y);
								writer.Key("shppt_ary");
								writer.StartArray();
								for(size_t k = 0; k < rRutLnk.mShpPtNum; k++)
								{
									writer.StartObject();
									{
										writer.Key("shppt_x");
										writer.Int(m_vecRouteLinkShpPt[rRutLnk.mShpPtVecStart + k].x);
										writer.Key("shppt_y");
										writer.Int(m_vecRouteLinkShpPt[rRutLnk.mShpPtVecStart + k].y);
									}
									writer.EndObject();
								}
								writer.EndArray();
							}
							writer.EndObject();
						}
						writer.EndArray();

						writer.EndObject();
					}
					writer.EndArray();
				} writer.EndObject();
			}

		}

		return ofs.is_open();
	}
	
	void json_reset()
	{
		m_vecFromInfo.clear();
		m_vecOutInfo.clear();
		m_vecRouteInfo.clear();
		m_vecRouteLink.clear();
		m_vecRouteLinkShpPt.clear();
	}

	void json_addFromInfo(DSegmentAttributes& attr, BOOL isReverseSearch, BOOL isSuper, DSegmentId dsid, size_t outnum)
	{
		NavInfoLinkId nil_id = isSuper ? invalidNavInfoLinkId : DSegment_getNavInfoLinkId(dsid);	

		Point first, last;
		DSegment_getFirstShapePoint(dsid, &first);
		DSegment_getLastShapePoint(dsid, &last);

		FromInfo fi = {isReverseSearch, isSuper, dsid, nil_id, attr, first, last, outnum};

		m_vecFromInfo.push_back(fi);
	}

	void json_addOutInfo(bool isSuper, DSegmentId dsid, RoutingCost cost)
	{
		NavInfoLinkId nil_id = isSuper ? invalidNavInfoLinkId : DSegment_getNavInfoLinkId(dsid);	

		OutInfo oi = {isSuper, dsid, nil_id, cost};

		m_vecOutInfo.push_back(oi);
	}
	
	void json_addOutInfo(RoutingCost cost)
	{
		json_addOutInfo(m_LabOutDSegCostInfo.mIsSuper, m_LabOutDSegCostInfo.mDSegId, cost);
	}

	void json_addRouteInfo(size_t linknum, RoutingCost cost, size_t lightNum)
	{
		RouteInfo ri = {linknum, m_vecRouteLink.size(), cost, lightNum};
		m_vecRouteInfo.push_back(ri);
	}
	void json_addRouteLink(RouteResult* result, size_t segIdx, DSegmentId dsid)
	{
		RouteLink link = {INVALID_DSEGMENT_ID};
		link.dsid = dsid;
		// first, last shape points
		DSegment_getFirstShapePoint(dsid, &link.firstShpPt);
		DSegment_getLastShapePoint(dsid, &link.lastShpPt);
		// coarse shape point
		link.mShpPtVecStart = m_vecRouteLinkShpPt.size();
		link.mShpPtNum  = RouteResult_getSegmentFinePoints(result, segIdx, g_stackBufPt, g_stackBufPtSize);
		//link.mShpPtNum  = RouteResult_getSegmentCoarsePoints(result, segIdx, g_stackBufPt, g_stackBufPtSize);
		const Point* ary = g_stackBufPt;
		for(size_t i = 0; i < link.mShpPtNum; i++)
		{
			m_vecRouteLinkShpPt.push_back(ary[i]);
		}
		m_vecRouteLink.push_back(link);
	}
private:
	std::vector<FromInfo> m_vecFromInfo;
	std::vector<OutInfo> m_vecOutInfo;
	std::vector<RouteInfo> m_vecRouteInfo;
	std::vector<RouteLink> m_vecRouteLink;
	std::vector<Point> m_vecRouteLinkShpPt;
};

CostParamLabPlugin::CostParamLabPlugin()
{
	m_requestProtocol = this;
	m_computationProtocol = this;
	mp = new CPrivate;
}

CostParamLabPlugin::~CostParamLabPlugin()
{
	delete mp;
}

// Export functions for register this plugin
// this function MUST be contained in the plugin lib
RouteServerPlugin* CostParamLabPlugin::createPlugin()
{
	log4rs::CLog4RS::getInstance().LOG_INFO("create plugin CostParamLabPlugin\n");
	return new CostParamLabPlugin();
}

void CostParamLabPlugin::releasePlugin(RouteServerPlugin* plugin)
{
	delete plugin;
}

// derived from RouteServerPlugin
void CostParamLabPlugin::reset()
{
	//m_selectedCostModel = NewCostModel_none;
}

// derived from RouteServerRequestProtocol
void CostParamLabPlugin::onParsingRequest(json_t* root)
{
	log4rs::CLog4RS::getInstance().LOG_INFO(">>>>> onParsingRequest()\n");
	UNUSED_VAR(root);
}

void CostParamLabPlugin::beforeRouting(RouteEngine* routeEngine)
{
	//cq_assert(routeEngine);
	mp->setRoutingRule(RouteEngine_getRule(routeEngine));
	mp->json_reset();
}


void CostParamLabPlugin::onWritingResponse(json_t* root, const routeContainer::routeResult::CRouteResult* result)
{
	UNUSED_VAR(root);
	UNUSED_VAR(result);
	if(result)
	{
		for(size_t i = 0; i < routeContainer::routeResult::ROUTE_MAX_NUM_RESULT; i++)
		{
			RouteResult* pRt = const_cast<RouteResult*>(result->getRouteResultByIdx(i));
			if(pRt)
			{
				mp->json_addRouteInfo(pRt->m_segmentNum, pRt->m_cost, pRt->m_trafficLightNumber);
				for(size_t j = 0; j < pRt->m_segmentNum; j++)
				{
					mp->json_addRouteLink(pRt, j, pRt->m_segments[j]);
				}
			}
		}
	}
	mp->json_dump(CCostLabLogger::instance()->makePathWithExt("json"));
	
	CCostLabLogger::instance()->printf("<<<<< onWritingResponse()\n");
	log4rs::CLog4RS::getInstance().LOG_INFO("<<<<< onWritingResponse()\n");
	CCostLabLogger::instance()->close();
}

size_t CostParamLabPlugin::modifyOutNodesAndCosts(DSegmentId fromSeg, DSegmentId* outSegs, RoutingCost* outCosts, size_t outNum, BOOL isReverseSearch, void *userdata)
{
	if(RoutingRule_shortest != mp->getRoutingRule())
	{
		mp->SetLabFromDSegCostInfo(fromSeg, isReverseSearch, outNum);
		RoutingCost basLinkCost, crossNodeCost, lightCostPenalty;
		for (size_t i = 0; i < outNum; i++)
		{
			mp->SetLabOutCostInfo(outSegs[i], outCosts[i]);
			basLinkCost = mp->calcCurOutDSegBasCost();
			crossNodeCost = mp->calcCurOutDSegIntersectCost();
			lightCostPenalty = mp->calcCurTrafficLightPenaltyCost();

			if(INFINITE_ROUTING_COST != basLinkCost 
			&& INFINITE_ROUTING_COST != crossNodeCost 
			&& INFINITE_ROUTING_COST != lightCostPenalty)
			{
				outCosts[i] = crossNodeCost + basLinkCost + lightCostPenalty;
			}
			else
			{
				outCosts[i] = INFINITE_ROUTING_COST;
			}

			mp->json_addOutInfo(outCosts[i]);
		}
	}

	return outNum;
}
