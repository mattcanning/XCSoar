/* Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2015 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/
#include <iostream>
#include <fstream>
#include "Printing.hpp"
#define DO_PRINT
#include "TestUtil.hpp"
#include "Route/TerrainRoute.hpp"
#include "Terrain/RasterMap.hpp"
#include "Terrain/Loader.hpp"
#include "OS/ConvertPathName.hpp"
#include "Compatibility/path.h"
#include "GlideSolvers/GlideSettings.hpp"
#include "GlideSolvers/GlidePolar.hpp"
#include "Geo/SpeedVector.hpp"
#include "Geo/GeoVector.hpp"
#include "Operation/Operation.hpp"
#include "OS/FileUtil.hpp"

#include <zzip/zzip.h>

#include <string.h>

static void
test_troute(const RasterMap& map, fixed mwind, fixed mc, RoughAltitude ceiling)
{
  GlideSettings settings;
  settings.SetDefaults();
  GlidePolar polar(mc);
  SpeedVector wind(Angle::Degrees(0), mwind);
  TerrainRoute route;
  route.UpdatePolar(settings, polar, polar, wind);
  route.SetTerrain(&map);

  GeoPoint origin(map.GetMapCenter());

  fixed pd = map.PixelDistance(origin, 1);
  printf("# pixel size %g\n", (double)pd);

  bool retval= true;

  {
    Directory::Create(_T("output/results"));
    std::ofstream fout ("output/results/terrain.txt");
    unsigned nx = 100;
    unsigned ny = 100;
    for (unsigned i=0; i< nx; ++i) {
      for (unsigned j=0; j< ny; ++j) {
        fixed fx = (fixed)i / (nx - 1) * 2 - fixed(1);
        fixed fy = (fixed)j / (ny - 1) * 2 - fixed(1);
        GeoPoint x(origin.longitude + Angle::Degrees(fixed(0.6) * fx),
                   origin.latitude + Angle::Degrees(fixed(0.4) * fy));
        short h = map.GetInterpolatedHeight(x);
        fout << x.longitude.Degrees() << " " << x.latitude.Degrees() << " " << h << "\n";
      }
      fout << "\n";
    }
    fout << "\n";
  }

  RoutePlannerConfig config;
  config.mode = RoutePlannerConfig::Mode::BOTH;

  unsigned i=0;
  for (fixed ang = fixed(0); ang < fixed_two_pi; ang += fixed(M_PI / 8)) {
    GeoPoint dest = GeoVector(fixed(40000.0), Angle::Radians(ang)).EndPoint(origin);

    short hdest = map.GetHeight(dest)+100;

    retval = route.Solve(AGeoPoint(origin,
                                   RoughAltitude(map.GetHeight(origin) + 100)),
                         AGeoPoint(dest,
                                   RoughAltitude(positive(mc)
                                                 ? hdest
                                                 : std::max(hdest, (short)3200))),
                         config, ceiling);
    char buffer[80];
    sprintf(buffer,"terrain route solve, dir=%g, wind=%g, mc=%g ceiling=%d",
            (double)ang, (double)mwind, (double)mc, (int)ceiling);
    ok(retval, buffer, 0);
    PrintHelper::print_route(route);
    i++;
  }

  // polar.SetMC(fixed(0));
  // route.UpdatePolar(polar, wind);
}

int main(int argc, char** argv) {
  static const char hc_path[] = "tmp/map.xcm";
  const char *map_path;
  if ((argc<2) || !strlen(argv[0])) {
    map_path = hc_path;
  } else {
    map_path = argv[0];
  }

  ZZIP_DIR *dir = zzip_dir_open(map_path, nullptr);
  if (dir == nullptr) {
    fprintf(stderr, "Failed to open %s\n", map_path);
    return EXIT_FAILURE;
  }

  RasterMap map;

  NullOperationEnvironment operation;
  if (!LoadTerrainOverview(dir, map.GetTileCache(),
                           operation)) {
    fprintf(stderr, "failed to load map\n");
    zzip_dir_close(dir);
    return EXIT_FAILURE;
  }

  SharedMutex mutex;
  do {
    UpdateTerrainTiles(dir, map.GetTileCache(), mutex,
                       map.GetProjection(),
                       map.GetMapCenter(), fixed(100000));
  } while (map.IsDirty());
  zzip_dir_close(dir);

  plan_tests(16*3);
  test_troute(map, fixed(0), fixed(0.1), RoughAltitude(10000));
  test_troute(map, fixed(0), fixed(0), RoughAltitude(10000));
  test_troute(map, fixed(5.0), fixed(1), RoughAltitude(10000));

  return exit_status();
}

