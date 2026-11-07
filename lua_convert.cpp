#include "lua_convert.hpp"

#include <sol/state_view.hpp>

#include "json.hpp"

nlohmann::json tableToJson(const sol::table &table) {
    nlohmann::json json = nlohmann::json::object();

    table.for_each([&](const sol::object &key, const sol::object &value) {
        if (key.is<std::string>()) {
            std::string keyStr = key.as<std::string>();

            if (value.is<int>()) {
                json[keyStr] = value.as<int>();
            } else if (value.is<double>()) {
                json[keyStr] = value.as<double>();
            } else if (value.is<std::string>()) {
                json[keyStr] = value.as<std::string>();
            } else if (value.is<bool>()) {
                json[keyStr] = value.as<bool>();
            } else if (value.is<sol::table>()) {
                json[keyStr] = tableToJson(value.as<sol::table>());
            }
        }
    });

    return json;
}

sol::table jsonToTable(sol::state_view lua, const nlohmann::json& j) {
    sol::table table = lua.create_table();

    if (!j.is_object()) {
        table["invalid"] = true;
        return table;
    }

    for (auto& [key, value] : j.items()) {
        if (value.is_number_integer()) {
            table[key] = value.get<int>();
        } else if (value.is_number_float()) {
            table[key] = value.get<double>();
        } else if (value.is_string()) {
            table[key] = value.get<std::string>();
        } else if (value.is_boolean()) {
            table[key] = value.get<bool>();
        } else if (value.is_object()) {
            table[key] = jsonToTable(lua, value); // Recursive call for nested objects
        }
    }

    table["invalid"] = false;

    return table;
}
