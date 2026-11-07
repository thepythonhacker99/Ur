#include <filesystem>
#include <map>
#include <queue>
#include <utility>

#include "json.hpp"
#include "lua_convert.hpp"
#include "raylib-cpp.hpp"
#include "sol/sol.hpp"

struct GameConfiguration {
    int rows = -1;
    int columns = -1;
    int pebble = -1;

    int p1_start_x = -1;
    int p1_start_y = -1;
    int p2_start_x = -1;
    int p2_start_y = -1;

    int p1_end_x = -1;
    int p1_end_y = -1;
    int p2_end_x = -1;
    int p2_end_y = -1;

    std::vector<std::vector<char>> schematic, circuit_p1, circuit_p2;

    void loadSchematic(const std::string &s) {
        schematic = loadBoard(s);
    }

    void loadCircuitP1(const std::string &s) {
        circuit_p1 = loadBoard(s);
    }

    void loadCircuitP2(const std::string &s) {
        circuit_p2 = loadBoard(s);
    }

    [[nodiscard]] std::optional<std::string> isValid() const {
        if (rows <= 0) return "rows <= 0";
        if (columns <= 0) return "columns <= 0";
        if (pebble <= 0) return "pebble <= 0";

        if (!isBoardValid(schematic, rows, columns))
            return "Schematic does not match the specified row and column sizes";

        if (!isBoardValid(circuit_p1, rows, columns))
            return "circuit_p1 does not match the specified row and column sizes";

        if (!isBoardValid(circuit_p2, rows, columns))
            return "circuit_p2 does not match the specified row and column sizes";

        if (!isCoordinateInBoard(p1_start_x, p1_start_y, rows, columns))
            return "p1_start is not valid";

        if (!isCoordinateInBoard(p2_start_x, p2_start_y, rows, columns))
            return "p2_start is not valid";

        if (!isCoordinateInBoard(p1_end_x, p1_end_y, rows, columns))
            return "p1_end is not valid";

        if (!isCoordinateInBoard(p2_end_x, p2_end_y, rows, columns))
            return "p2_end is not valid";

        return std::nullopt;
    }

private:
    static std::vector<std::vector<char>> loadBoard(const std::string &s) {
        std::vector<std::vector<char>> board;

        int iStart = 0;
        while (isspace(s[iStart])) iStart++;

        int iEnd = s.size() - 1;
        while (isspace(s[iEnd])) iEnd--;

        for (int i = iStart; i <= iEnd; i++) {
            char c = s[i];

            if (c == '\n') board.emplace_back();
            if (isspace(c)) continue;

            if (board.empty()) board.emplace_back();
            board.back().push_back(c);
        }

        return std::move(board);
    }

    static bool isBoardValid(const std::vector<std::vector<char>> &board, int rows, int cols) {
        return board.size() == rows
               && std::all_of(board.begin(), board.end(),
                              [cols](const auto &row) {
                                  return row.size() == cols;
                              }
               );
    }

    static bool isCoordinateInBoard(int x, int y, int rows, int cols) {
        return x >= 0 && y >= 0 && x < cols && y < rows;
    }
};


using TileState = std::pair<int, int>;

class GameState {
public:
    GameState(GameConfiguration conf, bool myTurn) : m_configuration(std::move(conf)) {
        m_pebbles.resize(m_configuration.rows, std::vector<TileState>(m_configuration.columns, {0, 0}));
        m_pebbles[m_configuration.p1_start_y][m_configuration.p1_start_x].first = m_configuration.pebble;
        m_pebbles[m_configuration.p2_start_y][m_configuration.p2_start_x].second = m_configuration.pebble;
    }

    TileState& getTileState(int x, int y) {
        return m_pebbles[y][x];
    }

    int getP1Start() {
        return getTileState(m_configuration.p1_start_x, m_configuration.p1_start_y).first;
    }

    int getP2Start() {
        return getTileState(m_configuration.p2_start_x, m_configuration.p2_start_y).second;
    }

    int getP1End() {
        return getTileState(m_configuration.p1_end_x, m_configuration.p1_end_y).first;
    }

    int getP2End() {
        return getTileState(m_configuration.p2_end_x, m_configuration.p2_end_y).second;
    }

private:
    const GameConfiguration m_configuration;

    std::vector<std::vector<TileState>> m_pebbles;
};

class EventManager {
public:
    EventManager(sol::state* lua, std::mutex* luaMutex, GameState* gameState) : m_lua(lua), m_luaMutex(luaMutex), m_gameState(gameState) {

    }

    void sendEvent(const std::string& eventType, const sol::table& data, bool broadcast) {
        std::lock_guard eventQueueGuard(m_eventQueueMutex);

        bool isProcessing = m_processing;
        if (!isProcessing) m_luaMutex->lock();

        nlohmann::json json = tableToJson(data);
        json["type"] = eventType;

        if (broadcast) {
            // send it through the socket with json.dump()
        }

        m_eventQueue.push(std::move(json));

        if (!isProcessing) m_luaMutex->unlock();
    }

    // Must be called in main thread
    void processEvents() {
        m_eventQueueMutex.lock();
        std::queue<nlohmann::json> tempQueue = std::move(m_eventQueue);
        m_eventQueue = std::queue<nlohmann::json>();
        m_eventQueueMutex.unlock();

        std::lock_guard luaGuard(*m_luaMutex);
        m_processing = true;

        while (!tempQueue.empty()) {
            auto& e = tempQueue.front();

            bool hasEventType = e.contains("type");
            if (!hasEventType) {
                std::cout << "event with no event type!\n";
                return;
            }

            std::string eventType = e["type"];

            sol::object callbackObject = m_lua->operator[](eventType + "Event");
            if (callbackObject.is<sol::protected_function>()) {
                std::cout << "Calling callback for " << eventType << "\n";
                sol::protected_function eventFunction = callbackObject;

                sol::table dataTable = jsonToTable(*m_lua, e);
                if (dataTable["invalid"].get<bool>() == true) {
                    std::cout << "Got invalid json as callback data\n";
                } else {
                    auto result = eventFunction(*m_gameState, dataTable, *this);
                    if (!result.valid()) {
                        sol::error err = result;
                        std::cout << "Error in callback (" << eventType << "): " << err.what() << "\n";
                    }
                }
            } else {
                std::cout << "Event with type " << eventType << " has no lua callback!\n";
            }

            tempQueue.pop();
        }

        m_processing = false;
    }

private:
    sol::state* m_lua;
    std::mutex* m_luaMutex;

    std::atomic<bool> m_processing = false;
    std::mutex m_eventQueueMutex;
    std::queue<nlohmann::json> m_eventQueue;

    GameState* m_gameState;
};

struct TextureManager {
    void assignTileTexture(const std::string &id, const std::string &path) {
        if (id.size() != 1) {
            std::cout << "Error in assignTileTexture: `id.size() != 1`\n";
            return;
        }

        using namespace std::filesystem;
        if (!exists(path) || !is_regular_file(path)) {
            std::cout << "Error in assignTileTexture: " << path << ", file does not exists or is a folder\n";
            return;
        }

        texture_paths[id[0]] = path;
    }

    void loadTextures() {
        textures.clear();

        using namespace std::filesystem;
        for (auto &[id, pathString]: texture_paths) {
            textures[id] = LoadTexture(pathString.c_str());
        }
    }

    std::map<char, std::string> texture_paths;
    std::map<char, Texture2D> textures;
};

int main() {
    std::mutex luaMutex;
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    {
        sol::usertype GameConfiguration_table = lua.new_usertype<GameConfiguration>("GameConfiguration");
        GameConfiguration_table["rows"] = &GameConfiguration::rows;
        GameConfiguration_table["columns"] = &GameConfiguration::columns;
        GameConfiguration_table["pebble"] = &GameConfiguration::pebble;
        GameConfiguration_table["loadSchematic"] = &GameConfiguration::loadSchematic;
        GameConfiguration_table["loadCircuitP1"] = &GameConfiguration::loadCircuitP1;
        GameConfiguration_table["loadCircuitP2"] = &GameConfiguration::loadCircuitP2;
        GameConfiguration_table["p1_start_x"] = &GameConfiguration::p1_start_x;
        GameConfiguration_table["p1_start_y"] = &GameConfiguration::p1_start_y;
        GameConfiguration_table["p1_end_x"] = &GameConfiguration::p1_end_x;
        GameConfiguration_table["p1_end_y"] = &GameConfiguration::p1_end_y;
        GameConfiguration_table["p2_start_x"] = &GameConfiguration::p2_start_x;
        GameConfiguration_table["p2_start_y"] = &GameConfiguration::p2_start_y;
        GameConfiguration_table["p2_end_x"] = &GameConfiguration::p2_end_x;
        GameConfiguration_table["p2_end_y"] = &GameConfiguration::p2_end_y;

        sol::usertype TextureManager_table = lua.new_usertype<TextureManager>("TextureManager");
        TextureManager_table["assignTileTexture"] = &TextureManager::assignTileTexture;

        sol::usertype EventManager_table = lua.new_usertype<EventManager>("EventManager");
        EventManager_table["send"] = &EventManager::sendEvent;

        sol::usertype GameState_table = lua.new_usertype<GameState>("GameState");
        GameState_table["getTileState"] = &GameState::getTileState;
    }

    {
        auto result = lua.safe_script_file("./scripts/init.lua");
        if (!result.valid()) {
            std::cout << "Failed to load init.lua: " << static_cast<sol::error>(result).what() << "\n";
            return 0;
        }
    }

    sol::protected_function lua_initGameConfiguration = lua["initGameConfiguration"];
    sol::protected_function lua_assignTextures = lua["assignTextures"];

    GameConfiguration game_configuration;
    {
        auto result = lua_initGameConfiguration(game_configuration);
        if (!result.valid()) {
            std::cout << "lua_initGameConfiguration failed\n";
            sol::error err = result;
            std::cout << "Error: " << err.what() << "\n";
            return 0;
        }
        std::cout << "lua_initGameConfiguration success!\n";

        auto validation = game_configuration.isValid();
        if (validation.has_value()) {
            std::cout << "game_configuration.isValid() = " << validation.value() << "\n";
            return 0;
        }
    }

    TextureManager texture_manager;
    {
        auto result = lua_assignTextures(texture_manager);
        if (!result.valid()) {
            std::cout << "lua_assignTextures failed\n";
            sol::error err = result;
            std::cout << "Error: " << err.what() << "\n";
            return 0;
        }
        std::cout << "lua_assignTextures success!\n";
    }

    GameState gameState(game_configuration, true);
    EventManager eventManager(&lua, &luaMutex, &gameState);

    raylib::Window window(1280, 720, "Ur", FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);

    std::cout << "Loading textures\n";
    texture_manager.loadTextures();

    while (!raylib::Window::ShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        eventManager.processEvents();

        for (int i = 0; i < game_configuration.rows; i++) {
            for (int j = 0; j < game_configuration.columns; j++) {
                char id = game_configuration.schematic[i][j];
                auto &tex = texture_manager.textures[id];

                Rectangle source = {
                    .x = 0, .y = 0, .width = static_cast<float>(tex.width), .height = static_cast<float>(tex.height)
                };

                float texSize = 100;

                Rectangle dest = {
                    .x = j * texSize, .y = i * texSize,
                    .width = texSize, .height = texSize
                };

                auto c = WHITE;
                if (i == game_configuration.p1_start_y && j == game_configuration.p1_start_x) c = RED;
                if (i == game_configuration.p2_start_y && j == game_configuration.p2_start_x) c = RED;
                if (i == game_configuration.p1_end_y && j == game_configuration.p1_end_x) c = BLUE;
                if (i == game_configuration.p2_end_y && j == game_configuration.p2_end_x) c = BLUE;

                DrawTexturePro(tex, source, dest, {.x = 0, .y = 0}, 0, c);
            }
        }

        if (IsKeyPressed(KEY_SPACE)) {
            eventManager.sendEvent(
                "random",
                lua.create_table(),
                false
            );
        }

        EndDrawing();
    }
}
