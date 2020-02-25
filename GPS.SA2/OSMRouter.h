#pragma once

#include "misc.h"
#include "common.h"
#include "pugixml.hpp"

#include <queue>

typedef long long int osmID;
typedef long long int lli;

struct RawNode
{
	lli		id;
	double	lat;
	double	lon;
};

struct flags
{
	unsigned finish: 1;
	unsigned field_1: 1;
	unsigned field_2: 1;
	unsigned field_3: 1;
	unsigned field_4: 1;
	unsigned field_5: 1;
	unsigned field_6: 1;
	unsigned field_7: 1;
};

struct PathNode
{
	lli		id;
	double	lat;
	double	lon;
	float	x;
	float	y;
	float	z;
	double	distantion_to;
	union {
		char flagsData;
		struct flags flags;
	};
};

struct PathContainer
{
	std::vector<PathNode>	path;
	double					distantion;
	/* and more next time */
};

struct Node
{
	osmID	id;
	double	lat;
	double	lon;

	bool operator ==(const Node& r) const
	{
		return id == r.id;
	}
};

typedef std::unordered_map<lli, RawNode>								NodeCache;
typedef std::unordered_map<lli, std::vector<std::pair<lli, double>>>	RouteableNode;

class OSMRouter
{
	std::string			path;
	pugi::xml_document	doc;
	bool				map_loaded;

	std::map<std::string, float> costMap;
	
	std::unordered_map<osmID, Node>										node_cache;
	std::unordered_map<osmID, std::vector<std::pair<osmID, double>>>	new_routeable_node;

	bool loadFile();
	void parseWays();

public:
	OSMRouter(void);

	std::vector<CVector2D> dijkstra(CVector from, CVector to);

	void loadOSM(const std::string _path);
	void setNewCostMap(std::map<std::string, float> newMap);

	Node		findNearestNodeFromEarthCoordinates(double lat, double lon);
	Node		convertGameWorldPointToEarthCoordinates(CVector2D point);
	CVector2D	convertEarthCoordinatesToGameWorldPoint(Node node);
	
	~OSMRouter(void);
};
