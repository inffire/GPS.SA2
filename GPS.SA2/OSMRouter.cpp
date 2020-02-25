#include "OSMRouter.h"

OSMRouter::OSMRouter(void)
{

}

void OSMRouter::loadOSM(const std::string _path)
{
	if (_path.size() > 0)
		this->path = _path;

	this->map_loaded = this->loadFile();
};

bool OSMRouter::loadFile()
{
	pugi::xml_parse_result result = doc.load_file(this->path.c_str());
	return result.status == 0;
}

void OSMRouter::parseWays()
{
	if(!this->map_loaded)
		return;

	// save all nodes
	int way_count = 0, nd_count = 0, edge_count = 0;
	for(pugi::xml_node node = this->doc.child("osm").child("node"); node; node = node.next_sibling("node"))
	{
		osmID node_id = node.attribute("id").as_llong();
		
		Node nd;
		nd.id = node_id;
		nd.lat = node.attribute("lat").as_double();
		nd.lon = node.attribute("lon").as_double();
		this->node_cache[node_id] = nd;
	}

	// parse all ways
	for(pugi::xml_node way = this->doc.child("osm").child("way"); way; way = way.next_sibling("way"))
	{
		bool	oneway = false;
		float	cost_multipler = 0;

		// explore osm way tags
		std::string k, v;
		for(pugi::xml_node tag = way.child("tag"); tag; tag = tag.next_sibling("tag"))
		{
			k = tag.attribute("k").as_string();
			v = tag.attribute("v").as_string();

			if(k == "highway" && cost_multipler == 0)
				cost_multipler = this->costMap[v];

			if(k == "oneway" && v == "yes")
				oneway = true;
		}

		if(cost_multipler == 0)
			continue;

		for(pugi::xml_node nd = way.child("nd"); nd; nd = nd.next_sibling("nd"))
		{
			Node stayinNodeInfo = this->node_cache[nd.attribute("ref").as_int()];
			nd_count++;

			// bind weight for forward direction
			if(pugi::xml_node next_nd = nd.next_sibling("nd"))
			{
				edge_count++;
				Node nextNodeInfo = this->node_cache[next_nd.attribute("ref").as_int()];
				double distance = distanceEarth(stayinNodeInfo.lat, stayinNodeInfo.lon, nextNodeInfo.lat, nextNodeInfo.lon);
				this->new_routeable_node[stayinNodeInfo.id].push_back(std::make_pair(nextNodeInfo.id, distance * cost_multipler));
			}

			// bind weight for backward direction
			if(pugi::xml_node prev_nd = nd.previous_sibling("nd"))
			{
				edge_count++;
				Node prevNodeInfo = this->node_cache[prev_nd.attribute("ref").as_int()];
				double distance = distanceEarth(stayinNodeInfo.lat, stayinNodeInfo.lon, prevNodeInfo.lat, prevNodeInfo.lon);

				if(oneway)
				{
					float new_cost = this->costMap["_reverse"];
					this->new_routeable_node[stayinNodeInfo.id].push_back(std::make_pair(prevNodeInfo.id, distance * new_cost));
				} else
				{
					this->new_routeable_node[stayinNodeInfo.id].push_back(std::make_pair(prevNodeInfo.id, distance * cost_multipler));
				}
			}
		}
		way_count++;
	}
}

std::vector<CVector2D> OSMRouter::dijkstra(CVector from, CVector to)
{
	Node earthPosOrig = convertGameWorldPointToEarthCoordinates(CVector2D(from.x, from.y));
	Node begin = findNearestNodeFromEarthCoordinates(earthPosOrig.lat, earthPosOrig.lon);
	Node earthPosDest = convertGameWorldPointToEarthCoordinates(CVector2D(to.x, to.y));
	Node end = findNearestNodeFromEarthCoordinates(earthPosDest.lat, earthPosDest.lon);


	/*
		Memcache na minimalkax
	*/
	static Node lastBegin, lastDest;
	static std::vector<CVector2D> lastResult;

	if (begin == lastBegin && end == lastDest)
		return lastResult;

	lastBegin = begin;
	lastDest = end;
	// ---

	osmID start = begin.id;
	osmID finish = end.id;

	std::priority_queue<std::pair<double, osmID>, std::vector<std::pair<double, osmID>>, std::greater<std::pair<double, osmID>>> fq;
	fq.push(std::make_pair(0.0f, start));

	std::unordered_map<osmID, double> fdist;
	fdist[start] = 0.0f;

	std::unordered_map<osmID, osmID> fcame_from;
	fcame_from[start] = start;

	bool route_found = false;

	while(!fq.empty())
	{
		osmID stay_in_node_id = fq.top().second;
        fq.pop();

		if(stay_in_node_id == finish)
		{
			route_found = true;
			break;
		}

		std::unordered_map<osmID, double>::const_iterator it_d = fdist.find(stay_in_node_id);
		double stored_stay_in_node_weight = it_d->second;

		std::vector<std::pair<osmID, double>>::const_iterator nd = this->new_routeable_node[stay_in_node_id].begin();
		for(; nd != this->new_routeable_node[stay_in_node_id].end(); ++nd)
		{
			osmID neighbor_node_id = nd->first;
			double weight_to_neighbor_node = nd->second;

			double stored_neighbor_node_dist = 80000.0f;
			std::unordered_map<osmID, double>::const_iterator it = fdist.find(neighbor_node_id);
			if(it != fdist.end())
				stored_neighbor_node_dist = it->second;

			double total_distance = stored_stay_in_node_weight + weight_to_neighbor_node;

			if(stored_neighbor_node_dist > total_distance)
			{
				fdist[neighbor_node_id] = total_distance;
				fq.push(std::make_pair(total_distance, neighbor_node_id));
				fcame_from[neighbor_node_id] = stay_in_node_id;
			}
		}
	}

	std::vector<CVector2D> result;
	if(!route_found)
	{
		return result;
	}

	result.push_back(convertEarthCoordinatesToGameWorldPoint(node_cache[finish]));

	osmID came_to = fcame_from[finish];
	while(came_to != start)
	{
		result.push_back(convertEarthCoordinatesToGameWorldPoint(node_cache[came_to]));
		came_to = fcame_from[came_to];
	}

	result.push_back(convertEarthCoordinatesToGameWorldPoint(node_cache[start]));
	result.push_back(CVector2D(from.x, from.y));
	std::reverse(result.begin(), result.end());

	lastResult = result;
	return result;
}

void OSMRouter::setNewCostMap(std::map<std::string, float> newMap)
{
	this->costMap = newMap;

	if (this->map_loaded)
		this->parseWays();
}

Node OSMRouter::findNearestNodeFromEarthCoordinates(double lat, double lon)
{
	double distance = 0, max_distance = 987654.32f;
	Node result;
	std::unordered_map<osmID, std::vector<std::pair<osmID, double>>>::const_iterator i = new_routeable_node.begin();
	for(; i != new_routeable_node.end(); ++i)
	{
		osmID node_id = i->first;
		double node_lat = node_cache[node_id].lat;
		double node_lon = node_cache[node_id].lon;
		distance = distanceEarth(lat, lon, node_lat, node_lon);
		if(distance < max_distance)
		{
			result = node_cache[node_id];
			max_distance = distance;
		}
	}
	return result;
}

Node OSMRouter::convertGameWorldPointToEarthCoordinates(CVector2D point)
{
	double scale = (1 / 69027.4 * 0.709 * 2.495 * 0.3519);
	Node result;
	result.id = 0;
	result.lon = point.x * scale;
	result.lat = point.y * scale;
	return result;
}

CVector2D OSMRouter::convertEarthCoordinatesToGameWorldPoint(Node node)
{
	double scale = 1 / (1 / 69027.4 * 0.709 * 2.495 * 0.3519);
	CVector2D result(node.lon*scale, node.lat*scale);
	return result;
}

OSMRouter::~OSMRouter(void)
{

}
