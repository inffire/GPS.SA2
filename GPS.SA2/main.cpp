#include "plugin.h"
#include "RenderWare.h"
#include "common.h"
#include "CMenuManager.h"
#include "CRadar.h"
#include "CWorld.h"
#include "RenderWare.h"
#include "CFont.h"

#include "OSMRouter.h"

#define MAX_TARGET_DISTANCE 10.0f

using namespace plugin;

void xml_load_settings(char* filename);
void xml_save_settings(char* filename);

typedef std::map<std::string, float> mCostMap;

struct Settings
{
	uint8_t		GPS_LINE_R = 255;
	uint8_t		GPS_LINE_G = 244;
	uint8_t		GPS_LINE_B = 244;
	uint8_t		GPS_LINE_A = 255;
	float		GPS_LINE_WIDTH = 8.0f;

	std::map<std::string, float> costMap =
	{
		{ "motorway", 1.01f },
		{ "tertiary", 1.24f },
		{ "residential", 1.81f },
		{ "track", 3.74f },
		{ "path", 3.74f },
		{ "_reverse", 220.0f },
		{ "trunk", 1.5f },
		{ "primary", 2.0f },
		{ "secondary", 4.0f },
		{ "minor", 125.0f },
		{ "service", 30.0f },
		{ "unclassified", 15.0f }
	};
} stDefaultSettings, stSettings;

OSMRouter osmRouter;

class GPS {
public:
	HANDLE hThread;

	GPS()
	{
		xml_load_settings("GPS.SA.xml");
		osmRouter.loadOSM("sanandreas.osm");
		osmRouter.setNewCostMap(stSettings.costMap);

		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GPS::init, (LPVOID)this, 0, (LPDWORD)NULL);
	}

	static LPVOID WINAPI init(LPVOID *lpParam)
	{
		GPS* sender = (GPS*)lpParam;

		while (!FindPlayerVehicle(0, false))
		{
			Sleep(778);
		}

		sender->HookEvents();
		sender->hThread = NULL;
		return NULL;
	}

	static void HookEvents()
	{
		static bool gpsShown;
		static float gpsDistance;

		Events::gameProcessEvent += []() {
			if (FrontEndMenuManager.m_nTargetBlipIndex
				&& CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)].m_nCounter == HIWORD(FrontEndMenuManager.m_nTargetBlipIndex)
				&& CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)].m_nBlipDisplayFlag
				&& FindPlayerPed()
				&& DistanceBetweenPoints(CVector2D(FindPlayerCoors(0)),
					CVector2D(CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)].m_vPosition)) < MAX_TARGET_DISTANCE)
			{
				CRadar::ClearBlip(FrontEndMenuManager.m_nTargetBlipIndex);
				FrontEndMenuManager.m_nTargetBlipIndex = 0;
			}
		};

		Events::drawRadarOverlayEvent += []()
		{
			gpsShown = false;
			CPed *playa = FindPlayerPed(0);
			if (playa
				&& playa->m_pVehicle
				&& playa->m_nPedFlags.bInVehicle
				&& playa->m_pVehicle->m_nVehicleSubClass != VEHICLE_PLANE
				&& playa->m_pVehicle->m_nVehicleSubClass != VEHICLE_HELI
				&& playa->m_pVehicle->m_nVehicleSubClass != VEHICLE_BMX
				&& FrontEndMenuManager.m_nTargetBlipIndex
				&& CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)].m_nCounter == HIWORD(FrontEndMenuManager.m_nTargetBlipIndex)
				&& CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)].m_nBlipDisplayFlag)
			{
				CVector destPosn = CRadar::ms_RadarTrace[LOWORD(FrontEndMenuManager.m_nTargetBlipIndex)].m_vPosition;
				destPosn.z = CWorld::FindGroundZForCoord(destPosn.x, destPosn.y);

				CVector origin = FindPlayerCoors(0);
				std::vector<CVector2D> nodePoints = osmRouter.dijkstra(origin, destPosn);

				if (nodePoints.size() > 1)
				{
					float distance = 0;
					for (size_t i = 1; i < nodePoints.size(); i++)
					{
						float dist = DistanceBetweenPoints(nodePoints[i - 1], nodePoints[i]);
						distance += dist;
					}
					gpsDistance = distance;
				}

				if (nodePoints.size() > 0)
				{
					CVector nodePosn;
					for (size_t i = 0; i < nodePoints.size(); i++)
					{
						nodePosn = CVector(nodePoints[i].x, nodePoints[i].y, 0.0f);
						CVector2D tmpPoint;
						CRadar::TransformRealWorldPointToRadarSpace(tmpPoint, nodePosn);
						if (!FrontEndMenuManager.drawRadarOrMap)
							CRadar::TransformRadarPointToScreenSpace(nodePoints[i], tmpPoint);
						else
						{
							CRadar::LimitRadarPoint(tmpPoint);
							CRadar::TransformRadarPointToScreenSpace(nodePoints[i], tmpPoint);
							nodePoints[i].x *= static_cast<float>(RsGlobal.maximumWidth) / 640.0f;
							nodePoints[i].y *= static_cast<float>(RsGlobal.maximumHeight) / 448.0f;
							CRadar::LimitToMap(&nodePoints[i].x, &nodePoints[i].y);
						}
					}

					if (!FrontEndMenuManager.drawRadarOrMap
						&& reinterpret_cast<D3DCAPS9 const*>(RwD3D9GetCaps())->RasterCaps & D3DPRASTERCAPS_SCISSORTEST)
					{
						RECT rect;
						CVector2D posn;
						CRadar::TransformRadarPointToScreenSpace(posn, CVector2D(-1.0f, -1.0f));
						rect.left = posn.x + 2.0f; rect.bottom = posn.y - 2.0f;
						CRadar::TransformRadarPointToScreenSpace(posn, CVector2D(1.0f, 1.0f));
						rect.right = posn.x - 2.0f; rect.top = posn.y + 2.0f;
						GetD3DDevice()->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
						GetD3DDevice()->SetScissorRect(&rect);
					}

					RwRenderStateSet(rwRENDERSTATETEXTURERASTER, NULL);

					float line_width;
					if (!FrontEndMenuManager.drawRadarOrMap)
					{
						line_width = stSettings.GPS_LINE_WIDTH / 2.0f;
					}
					else
					{
						float mp = FrontEndMenuManager.m_fMapZoom - 140.0f;
						if (mp < 140.0f)
							mp = 140.0f;
						else if (mp > 960.0f)
							mp = 960.0f;
						mp = mp / 960.0f + 0.5f;
						line_width = mp * stSettings.GPS_LINE_WIDTH / 2.0f;
					}

					DrawCircle(nodePoints[0], line_width, RWRGBALONG(stSettings.GPS_LINE_R, stSettings.GPS_LINE_G, stSettings.GPS_LINE_B, stSettings.GPS_LINE_A));
					for (size_t i = 0; i < (nodePoints.size() - 1); i++)
					{
						DrawCircle(nodePoints[i + 1], line_width, RWRGBALONG(stSettings.GPS_LINE_R, stSettings.GPS_LINE_G, stSettings.GPS_LINE_B, stSettings.GPS_LINE_A));

						CVector2D rect_vertex[4];
						CVector2D node_dir(nodePoints[i + 1] - nodePoints[i]);
						node_dir /= node_dir.Magnitude();
						float angle = atan2f(node_dir.y, node_dir.x);

						CVector2D shift_vertex[2];

						shift_vertex[0].x = cosf(angle - 1.5707963f) * line_width;
						shift_vertex[0].y = sinf(angle - 1.5707963f) * line_width;
						shift_vertex[1].x = cosf(angle + 1.5707963f) * line_width;
						shift_vertex[1].y = sinf(angle + 1.5707963f) * line_width;

						rect_vertex[0] = CVector2D(nodePoints[i].x + shift_vertex[0].x, nodePoints[i].y + shift_vertex[0].y);
						rect_vertex[1] = CVector2D(nodePoints[i + 1].x + shift_vertex[0].x, nodePoints[i + 1].y + shift_vertex[0].y);
						rect_vertex[2] = CVector2D(nodePoints[i].x + shift_vertex[1].x, nodePoints[i].y + shift_vertex[1].y);
						rect_vertex[3] = CVector2D(nodePoints[i + 1].x + shift_vertex[1].x, nodePoints[i + 1].y + shift_vertex[1].y);

						RwIm2DVertex tmp_vertex[4];
						Setup2dVertex(tmp_vertex[0], rect_vertex[0].x, rect_vertex[0].y);
						Setup2dVertex(tmp_vertex[1], rect_vertex[1].x, rect_vertex[1].y);
						Setup2dVertex(tmp_vertex[2], rect_vertex[2].x, rect_vertex[2].y);
						Setup2dVertex(tmp_vertex[3], rect_vertex[3].x, rect_vertex[3].y);
						RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, tmp_vertex, 4);
					}
					if (!FrontEndMenuManager.drawRadarOrMap
						&& reinterpret_cast<D3DCAPS9 const*>(RwD3D9GetCaps())->RasterCaps & D3DPRASTERCAPS_SCISSORTEST)
					{
						GetD3DDevice()->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
					}
					gpsShown = true;
				}
			}
		};

		Events::drawHudEvent += [] {
			if (gpsShown) {
				CFont::SetOrientation(ALIGN_CENTER);
				CFont::SetColor(CRGBA(200, 200, 200, 255));
				CFont::SetBackground(false, false);
				CFont::SetWrapx(500.0f);
				CFont::SetScale(0.4f * static_cast<float>(RsGlobal.maximumWidth) / 640.0f,
					0.8f * static_cast<float>(RsGlobal.maximumHeight) / 448.0f);
				CFont::SetFontStyle(FONT_SUBTITLES);
				CFont::SetProportional(true);
				CFont::SetDropShadowPosition(1);
				CFont::SetDropColor(CRGBA(0, 0, 0, 255));
				CVector2D radarBottom;
				CRadar::TransformRadarPointToScreenSpace(radarBottom, CVector2D(0.0f, -1.0f));
				char text[16];
				if (gpsDistance > 1000.0f)
					sprintf(text, "%.2fkm", gpsDistance / 1000.0f);
				else
					sprintf(text, "%dm", static_cast<int>(gpsDistance));
				CFont::PrintString(radarBottom.x, radarBottom.y + 8.0f * static_cast<float>(RsGlobal.maximumHeight) / 448.0f, text);
			}
		};
	}

	static void Setup2dVertex(RwIm2DVertex &vertex, float x, float y) {
		vertex.x = x;
		vertex.y = y;
		vertex.u = vertex.v = 0.0f;
		vertex.z = CSprite2d::NearScreenZ + 0.0001f;
		vertex.rhw = CSprite2d::RecipNearClip;
		vertex.emissiveColor = RWRGBALONG(stSettings.GPS_LINE_R, stSettings.GPS_LINE_G, stSettings.GPS_LINE_B, stSettings.GPS_LINE_A);
	}

	static void Setup2dVertexB(RwIm2DVertex &vertex, float x, float y, RwUInt32 color) {
		vertex.x = x;
		vertex.y = y;
		vertex.u = vertex.v = 0.0f;
		vertex.z = CSprite2d::NearScreenZ + 0.0001f;
		vertex.rhw = CSprite2d::RecipNearClip;
		vertex.emissiveColor = color;
	}

	static void DrawCircle(CVector2D center, float radius, RwUInt32 color) {
		const int triangles = 16;
		RwIm2DVertex circle_vertex[triangles + 2];

		Setup2dVertexB(circle_vertex[0], center.x, center.y, color);
		float angle_step = 3.1415f * 2.0f / triangles;
		for (int i(0); i < triangles + 1; ++i)
		{
			float angle = angle_step * i;
			Setup2dVertexB(circle_vertex[i + 1], center.x + cosf(angle)*radius, center.y + sinf(angle)*radius, color);
		}
		RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, circle_vertex, triangles + 2);
	}
} gps;

void xml_load_settings(char* filename)
{
	pugi::xml_document doc;
	if (doc.load_file(filename))
	{
		pugi::xml_node lineColor = doc.child("Color");
		stSettings.GPS_LINE_R = lineColor.attribute("r").as_uint(stDefaultSettings.GPS_LINE_R);
		stSettings.GPS_LINE_G = lineColor.attribute("g").as_uint(stDefaultSettings.GPS_LINE_G);
		stSettings.GPS_LINE_B = lineColor.attribute("b").as_uint(stDefaultSettings.GPS_LINE_B);
		stSettings.GPS_LINE_A = lineColor.attribute("a").as_uint(stDefaultSettings.GPS_LINE_A);
		stSettings.GPS_LINE_WIDTH = lineColor.attribute("width").as_float(stDefaultSettings.GPS_LINE_WIDTH);

		pugi::xml_node costs = doc.child("Costs");
		stSettings.costMap["motorway"] = costs.attribute("motorway").as_float(stDefaultSettings.costMap["motorway"]);
		stSettings.costMap["tertiary"] = costs.attribute("tertiary").as_float(stDefaultSettings.costMap["tertiary"]);
		stSettings.costMap["residential"] = costs.attribute("residential").as_float(stDefaultSettings.costMap["residential"]);
		stSettings.costMap["track"] = costs.attribute("track").as_float(stDefaultSettings.costMap["track"]);
		stSettings.costMap["path"] = costs.attribute("path").as_float(stDefaultSettings.costMap["path"]);
		stSettings.costMap["_reverse"] = costs.attribute("_reverse").as_float(stDefaultSettings.costMap["_reverse"]);
		stSettings.costMap["trunk"] = costs.attribute("trunk").as_float(stDefaultSettings.costMap["trunk"]);
		stSettings.costMap["primary"] = costs.attribute("primary").as_float(stDefaultSettings.costMap["primary"]);
		stSettings.costMap["secondary"] = costs.attribute("secondary").as_float(stDefaultSettings.costMap["secondary"]);
		stSettings.costMap["minor"] = costs.attribute("minor").as_float(stDefaultSettings.costMap["minor"]);
		stSettings.costMap["service"] = costs.attribute("service").as_float(stDefaultSettings.costMap["service"]);
		stSettings.costMap["unclassified"] = costs.attribute("unclassified").as_float(stDefaultSettings.costMap["unclassified"]);
	}
	else
	{
		stSettings = stDefaultSettings;
	}
}

void xml_save_settings(char* filename)
{
	pugi::xml_document doc;
	pugi::xml_node lineColor = doc.append_child("Color");
	lineColor.append_attribute("r") = stSettings.GPS_LINE_R;
	lineColor.append_attribute("g") = stSettings.GPS_LINE_G;
	lineColor.append_attribute("b") = stSettings.GPS_LINE_B;
	lineColor.append_attribute("a") = stSettings.GPS_LINE_A;
	lineColor.append_attribute("width") = stSettings.GPS_LINE_WIDTH;

	pugi::xml_node costs = doc.append_child("Costs");
	costs.append_attribute("motorway") = stSettings.costMap["motorway"];
	costs.append_attribute("tertiary") = stSettings.costMap["tertiary"];
	costs.append_attribute("residential") = stSettings.costMap["residential"];
	costs.append_attribute("track") = stSettings.costMap["track"];
	costs.append_attribute("path") = stSettings.costMap["path"];
	costs.append_attribute("_reverse") = stSettings.costMap["_reverse"];
	costs.append_attribute("trunk") = stSettings.costMap["trunk"];
	costs.append_attribute("primary") = stSettings.costMap["primary"];
	costs.append_attribute("secondary") = stSettings.costMap["secondary"];
	costs.append_attribute("minor") = stSettings.costMap["minor"];
	costs.append_attribute("service") = stSettings.costMap["service"];
	costs.append_attribute("unclassified") = stSettings.costMap["unclassified"];

	doc.save_file(filename);
}
