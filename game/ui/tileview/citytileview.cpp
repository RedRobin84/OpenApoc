#include "game/ui/tileview/citytileview.h"
#include "framework/data.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/image.h"
#include "framework/keycodes.h"
#include "framework/renderer.h"
#include "framework/trace.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/vehicle_type.h"
#include "game/state/tileview/tileobject_vehicle.h"

namespace OpenApoc
{
CityTileView::CityTileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
                           TileViewMode initialMode, GameState &gameState)
    : TileView(map, isoTileSize, stratTileSize, initialMode), state(gameState)
{
	selectedTileImageBack = fw().data->loadImage("city/selected-citytile-back.png");
	selectedTileImageFront = fw().data->loadImage("city/selected-citytile-front.png");
	selectedTileImageOffset = {32, 16};
	pal = fw().data->loadPalette("xcom3/ufodata/pal_01.dat");
	alertImage = fw().data->loadImage("city/building-circle.png");

	selectionBracketsFriendly.push_back(fw().data->loadImage("city/vehicle-brackets-f0.png"));
	selectionBracketsFriendly.push_back(fw().data->loadImage("city/vehicle-brackets-f1.png"));
	selectionBracketsFriendly.push_back(fw().data->loadImage("city/vehicle-brackets-f2.png"));
	selectionBracketsFriendly.push_back(fw().data->loadImage("city/vehicle-brackets-f3.png"));
	selectionBracketsHostile.push_back(fw().data->loadImage("city/vehicle-brackets-h0.png"));
	selectionBracketsHostile.push_back(fw().data->loadImage("city/vehicle-brackets-h1.png"));
	selectionBracketsHostile.push_back(fw().data->loadImage("city/vehicle-brackets-h2.png"));
	selectionBracketsHostile.push_back(fw().data->loadImage("city/vehicle-brackets-h3.png"));
};

CityTileView::~CityTileView() = default;

void CityTileView::eventOccurred(Event *e)
{
	if (e->type() == EVENT_KEY_DOWN)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_1:
				pal = fw().data->loadPalette("xcom3/ufodata/pal_01.dat");
				return;
			case SDLK_2:
				pal = fw().data->loadPalette("xcom3/ufodata/pal_02.dat");
				return;
			case SDLK_3:
				pal = fw().data->loadPalette("xcom3/ufodata/pal_03.dat");
				return;
			case SDLK_F10:
			{
				DEBUG_SHOW_ALIEN_CREW = !DEBUG_SHOW_ALIEN_CREW;
				LogWarning("Debug Alien display set to %s", DEBUG_SHOW_ALIEN_CREW);
			}
				return;
			case SDLK_F6:
			{
				LogWarning("Writing voxel view to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(
				    this->map.dumpVoxelView({imageOffset, imageOffset + dpySize}, *this, 11.0f));
				fw().data->writeImage("tileviewvoxels.png", img);
			}
				return;
			case SDLK_F7:
			{
				LogWarning("Writing voxel view (fast) to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(this->map.dumpVoxelView(
				    {imageOffset, imageOffset + dpySize}, *this, 11.0f, true));
				fw().data->writeImage("tileviewvoxels.png", img);
			}
				return;
			case SDLK_F8:
			{
				LogWarning("Writing voxel view to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(this->map.dumpVoxelView(
				    {imageOffset, imageOffset + dpySize}, *this, 11.0f, false, true));
				fw().data->writeImage("tileviewvoxels.png", img);
			}
				return;
			case SDLK_F9:
			{
				LogWarning("Writing voxel view (fast) to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(this->map.dumpVoxelView(
				    {imageOffset, imageOffset + dpySize}, *this, 11.0f, true, true));
				fw().data->writeImage("tileviewvoxels.png", img);
			}
				return;
		}
	}
	TileView::eventOccurred(e);
}

void CityTileView::render()
{
	TRACE_FN;
	Renderer &r = *fw().renderer;
	r.clear();
	r.setPalette(this->pal);

	// screenOffset.x/screenOffset.y is the 'amount added to the tile coords' - so we want
	// the inverse to tell which tiles are at the screen bounds
	auto topLeft = offsetScreenToTileCoords(Vec2<int>{-isoTileSize.x, -isoTileSize.y}, 0);
	auto topRight = offsetScreenToTileCoords(Vec2<int>{dpySize.x, -isoTileSize.y}, 0);
	auto bottomLeft = offsetScreenToTileCoords(Vec2<int>{-isoTileSize.x, dpySize.y}, map.size.z);
	auto bottomRight = offsetScreenToTileCoords(Vec2<int>{dpySize.x, dpySize.y}, map.size.z);

	int minX = std::max(0, topLeft.x);
	int maxX = std::min(map.size.x, bottomRight.x);

	int minY = std::max(0, topRight.y);
	int maxY = std::min(map.size.y, bottomLeft.y);

	switch (this->viewMode)
	{
		case TileViewMode::Isometric:
		{
			// List of vehicles that require drawing of brackets
			std::set<sp<Vehicle>> vehiclesToDrawBracketsFriendly;
			std::set<sp<Vehicle>> vehiclesToDrawBracketsHostile;

			auto selectedVehicleLocked = selectedVehicle.lock();

			// Go through every selected vehicle and add target to list of bracket draws
			if (selectedVehicleLocked)
			{
				for (auto &m : selectedVehicleLocked->missions)
				{
					if (m->type == VehicleMission::MissionType::AttackVehicle)
					{
						vehiclesToDrawBracketsHostile.insert(m->targetVehicle);
					}
				}
			}

			for (int z = 0; z < maxZDraw; z++)
			{
				for (unsigned int layer = 0; layer < map.getLayerCount(); layer++)
				{
					for (int y = minY; y < maxY; y++)
					{
						for (int x = minX; x < maxX; x++)
						{
							auto tile = map.getTile(x, y, z);
							auto object_count = tile->drawnObjects[layer].size();
							for (size_t obj_id = 0; obj_id < object_count; obj_id++)
							{
								auto &obj = tile->drawnObjects[layer][obj_id];
								Vec2<float> pos = tileToOffsetScreenCoords(obj->getCenter());
								obj->draw(r, *this, pos, this->viewMode);

								switch (obj->getType())
								{
									case TileObject::Type::Vehicle:
									{
										auto v = std::static_pointer_cast<TileObjectVehicle>(obj)
										             ->getVehicle();

										if (selectedVehicleLocked)
										{
											if (v == selectedVehicleLocked)
											{
												vehiclesToDrawBracketsFriendly.insert(v);
											}
										}
									}
									break;
								}
							}
#ifdef PATHFINDING_DEBUG
							if (tile->pathfindingDebugFlag && viewMode == TileViewMode::Isometric)
								r.draw(selectedTileImageFront,
								       tileToOffsetScreenCoords(Vec3<int>{x, y, z}) -
								           selectedTileImageOffset);
#endif
						}
					}
				}
			}

			// Draw brackets
			for (auto &obj : vehiclesToDrawBracketsFriendly)
			{
				Vec3<float> size = obj->type->size.at(obj->type->getVoxelMapFacing(obj->facing));
				size /= 2;
				Vec2<float> pTop = tileToOffsetScreenCoords(obj->getPosition() +
				                                            Vec3<float>{-size.x, -size.y, size.z});
				Vec2<float> pLeft =
				    tileToOffsetScreenCoords(obj->getPosition() + Vec3<float>{-size.x, +size.y, 0});
				Vec2<float> pRight =
				    tileToOffsetScreenCoords(obj->getPosition() + Vec3<float>{size.x, -size.y, 0});
				Vec2<float> pBottom = tileToOffsetScreenCoords(
				    obj->getPosition() + Vec3<float>{size.x, size.y, -size.z});

				r.draw(selectionBracketsFriendly[0], {pLeft.x - 2.0f, pTop.y - 2.0f});
				r.draw(selectionBracketsFriendly[1], {pLeft.x - 2.0f, pBottom.y - 2.0f});
				r.draw(selectionBracketsFriendly[2], {pRight.x - 2.0f, pTop.y - 2.0f});
				r.draw(selectionBracketsFriendly[3], {pRight.x - 2.0f, pBottom.y - 2.0f});
			}
			for (auto &obj : vehiclesToDrawBracketsHostile)
			{
				Vec3<float> size = obj->type->size.at(obj->type->getVoxelMapFacing(obj->facing));
				size /= 2;
				Vec2<float> pTop = tileToOffsetScreenCoords(obj->getPosition() +
				                                            Vec3<float>{-size.x, -size.y, size.z});
				Vec2<float> pLeft =
				    tileToOffsetScreenCoords(obj->getPosition() + Vec3<float>{-size.x, +size.y, 0});
				Vec2<float> pRight =
				    tileToOffsetScreenCoords(obj->getPosition() + Vec3<float>{size.x, -size.y, 0});
				Vec2<float> pBottom = tileToOffsetScreenCoords(
				    obj->getPosition() + Vec3<float>{size.x, size.y, -size.z});

				r.draw(selectionBracketsHostile[0], {pLeft.x - 2.0f, pTop.y - 2.0f});
				r.draw(selectionBracketsHostile[1], {pLeft.x - 2.0f, pBottom.y - 2.0f});
				r.draw(selectionBracketsHostile[2], {pRight.x - 2.0f, pTop.y - 2.0f});
				r.draw(selectionBracketsHostile[3], {pRight.x - 2.0f, pBottom.y - 2.0f});
			}
		}
		break;
		case TileViewMode::Strategy:
		{
			for (int z = 0; z < maxZDraw; z++)
			{
				for (unsigned int layer = 0; layer < map.getLayerCount(); layer++)
				{
					for (int y = minY; y < maxY; y++)
					{
						for (int x = minX; x < maxX; x++)
						{
							auto tile = map.getTile(x, y, z);
							auto object_count = tile->drawnObjects[layer].size();
							for (size_t obj_id = 0; obj_id < object_count; obj_id++)
							{
								auto &obj = tile->drawnObjects[layer][obj_id];
								Vec2<float> pos = tileToOffsetScreenCoords(obj->getCenter());
								obj->draw(r, *this, pos, this->viewMode);
							}
#ifdef PATHFINDING_DEBUG
							if (tile->pathfindingDebugFlag && viewMode == TileViewMode::Isometric)
								r.draw(selectedTileImageFront,
								       tileToOffsetScreenCoords(Vec3<int>{x, y, z}) -
								           selectedTileImageOffset);
#endif
						}
					}
				}
			}
			renderStrategyOverlay(r);

			// Detection
			for (auto &b : state.current_city->buildings)
			{
				if (!b.second->detected)
				{
					continue;
				}
				float initialRadius = std::max(alertImage->size.x, alertImage->size.y);
				// Eventually scale to 1/2 the size, but start with some bonus time of full size,
				// so that it doesn't become distorted immediately, that's why we add extra 0.05
				float radius = std::min(initialRadius,
				                        initialRadius * (float)b.second->ticksDetectionTimeOut /
				                                (float)TICKS_DETECTION_TIMEOUT / 2.0f +
				                            0.55f);
				Vec2<float> pos = tileToOffsetScreenCoords(
				    Vec3<int>{(b.second->bounds.p0.x + b.second->bounds.p1.x) / 2,
				              (b.second->bounds.p0.y + b.second->bounds.p1.y) / 2, 2});
				pos -= Vec2<float>{radius / 2.0f, radius / 2.0f};

				if (radius == initialRadius)
				{
					r.draw(alertImage, pos);
				}
				else
				{
					r.drawScaled(alertImage, pos, {radius, radius});
				}
			}

			// Alien debug display
			if (DEBUG_SHOW_ALIEN_CREW)
			{
				for (auto &b : state.current_city->buildings)
				{
					Vec2<float> pos = tileToOffsetScreenCoords(
					    Vec3<int>{b.second->bounds.p0.x, b.second->bounds.p0.y, 2});
					for (auto &a : b.second->current_crew)
					{
						for (int i = 0; i < a.second; i++)
						{
							auto icon = a.first->portraits.at(*a.first->possible_genders.begin())
							                .at(0)
							                .icon;
							r.draw(icon, pos);
							pos.x += icon->size.x / 2;
						}
					}
				}
			}
		}
		break;
	}
}
}
