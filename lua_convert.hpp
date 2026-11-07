#pragma once

#include <sol/table.hpp>
#include "json.hpp"

nlohmann::json tableToJson(const sol::table& table);
sol::table jsonToTable(sol::state_view lua, const nlohmann::json& j);