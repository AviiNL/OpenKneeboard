/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/VRConfig.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {
void from_json(const nlohmann::json& j, VRConfig& vrc) {
  vrc.x = j.at("x");
  vrc.eyeY = j.at("eyeY");
  vrc.floorY = j.at("floorY");
  vrc.z = j.at("z");
  vrc.rx = j.at("rx");
  vrc.ry = j.at("ry");
  vrc.rz = j.at("ry");
  vrc.height = j.at("height");
  vrc.zoomScale = j.at("zoomScale");
}

void to_json(nlohmann::json& j, const VRConfig& vrc) {
  j = {
    {"x", vrc.x},
    {"eyeY", vrc.eyeY},
    {"floorY", vrc.floorY},
    {"z", vrc.z},
    {"rx", vrc.rx},
    {"ry", vrc.ry},
    {"rz", vrc.rz},
    {"height", vrc.height},
    {"zoomScale", vrc.zoomScale},
  };
}
}// namespace OpenKneeboard
