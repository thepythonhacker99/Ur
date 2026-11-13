#include <atomic>
#include <filesystem>
#include <map>
#include <queue>
#include <utility>

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

    char getDirectionP1(int x, int y) const {
        if (isCoordinateInBoard(x, y)) return circuit_p1[y][x];
        return '.';
    }

    char getDirectionP2(int x, int y) const {
        if (isCoordinateInBoard(x, y)) return circuit_p2[y][x];
        return '.';
    }

    char getTileType(int x, int y) const {
        if (isCoordinateInBoard(x, y)) return schematic[y][x];
        return '-';
    }

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

        if (!isCoordinateInBoard(p1_start_x, p1_start_y))
            return "p1_start is not valid";

        if (!isCoordinateInBoard(p2_start_x, p2_start_y))
            return "p2_start is not valid";

        if (!isCoordinateInBoard(p1_end_x, p1_end_y))
            return "p1_end is not valid";

        if (!isCoordinateInBoard(p2_end_x, p2_end_y))
            return "p2_end is not valid";

        return std::nullopt;
    }

    bool isCoordinateInBoard(int x, int y) const {
        return x >= 0 && y >= 0 && x < columns && y < rows;
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
};


using TileState = std::pair<int, int>;

class GameState {
public:
    GameState(GameConfiguration conf) : m_configuration(std::move(conf)) {
        m_pebbles.resize(m_configuration.rows, std::vector<TileState>(m_configuration.columns, {0, 0}));
        m_pebbles[m_configuration.p1_start_y][m_configuration.p1_start_x].first = m_configuration.pebble;
        m_pebbles[m_configuration.p2_start_y][m_configuration.p2_start_x].second = m_configuration.pebble;
    }

    TileState& getTileState(int x, int y) {
        return m_pebbles[y][x];
    }

    const GameConfiguration& getConfiguration() {
        return m_configuration;
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

    void movePebbleP1(int x1, int y1, int x2, int y2) {
        if (!m_configuration.isCoordinateInBoard(x1, y1) || !m_configuration.isCoordinateInBoard(x2, y2)) return;
        auto& pebbles = getTileState(x1, y1).first;
        if (pebbles > 0) {
            pebbles--;
            getTileState(x2, y2).first++;
        }
    }

    void movePebbleP2(int x1, int y1, int x2, int y2) {
        if (!m_configuration.isCoordinateInBoard(x1, y1) || !m_configuration.isCoordinateInBoard(x2, y2)) return;
        auto& pebbles = getTileState(x1, y1).second;
        if (pebbles > 0) {
            pebbles--;
            getTileState(x2, y2).second++;
        }
    }

private:
    const GameConfiguration m_configuration;

    std::vector<std::vector<TileState>> m_pebbles;
};

class EventManager {
public:
    EventManager(sol::state* lua, std::mutex* luaMutex, GameState* gameState) : m_lua(lua), m_luaMutex(luaMutex), m_gameState(gameState) {

    }

    void sendEvent(const std::string& eventType, sol::table data, bool broadcast) {
        std::lock_guard eventQueueGuard(m_eventQueueMutex);

        bool isProcessing = m_processing;
        if (!isProcessing) m_luaMutex->lock();

        data["type"] = eventType;

        if (broadcast) {
            // send it through the socket with json.dump()
        }

        m_eventQueue.push(data);
        if (!isProcessing) m_luaMutex->unlock();
    }

    // Must be called in main thread
    void processEvents() {
        m_eventQueueMutex.lock();
        std::queue<sol::table> tempQueue = std::move(m_eventQueue);
        m_eventQueue = std::queue<sol::table>();
        m_eventQueueMutex.unlock();

        std::lock_guard luaGuard(*m_luaMutex);
        m_processing = true;

        while (!tempQueue.empty()) {
            auto& e = tempQueue.front();

            std::string eventType = e.get_or("type", std::string{});
            if (eventType.empty()) {
                std::cout << "event with no event type!\n";
                return;
            }

            sol::object callbackObject = m_lua->operator[](eventType + "Event");
            if (callbackObject.is<sol::protected_function>()) {
                // std::cout << "Calling callback for " << eventType << "\n";
                sol::protected_function eventFunction = callbackObject;

                auto result = eventFunction(*m_gameState, e, *this);
                if (!result.valid()) {
                    sol::error err = result;
                    std::cout << "Error in callback (" << eventType << "): " << err.what() << "\n";
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
    std::queue<sol::table> m_eventQueue;

    GameState* m_gameState;
};

struct TextureManager {
    void assignBoardTexture(const std::string &path) {
        using namespace std::filesystem;
        if (!exists(path) || !is_regular_file(path)) {
            std::cout << "Error in assignTileTexture: " << path << ", file does not exists or is a folder\n";
            return;
        }

        m_texture_paths[TEX_ID_BOARD] = path;
    }

    void loadTextures() {
        m_textures.clear();

        using namespace std::filesystem;
        for (auto &[id, pathString]: m_texture_paths) {
            m_textures[id] = LoadTexture(pathString.c_str());
        }
    }

    void setBoardTile(float x, float y, float w, float h) {
        m_board_tile = {
            .x = x, .y = y, .width = w, .height = h
        };
    }

    Rectangle getTileRectangle(int x, int y) {
        return Rectangle{
            .x = m_board_tile.x + m_board_tile.width * x,
            .y = m_board_tile.y + m_board_tile.height * y,
            .width = m_board_tile.width,
            .height = m_board_tile.height
        };
    }

    const Texture2D& getBoardTexture() {
        return m_textures[TEX_ID_BOARD];
    }

    static constexpr auto TEX_ID_BOARD = "board";

private:
    std::map<std::string, std::string> m_texture_paths;
    std::map<std::string, Texture2D> m_textures;

    Rectangle m_board_tile{0, 0, 0, 0};
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
        GameConfiguration_table["getTileType"] = &GameConfiguration::getTileType;
        GameConfiguration_table["getDirectionP1"] = &GameConfiguration::getDirectionP1;
        GameConfiguration_table["getDirectionP2"] = &GameConfiguration::getDirectionP2;

        sol::usertype TextureManager_table = lua.new_usertype<TextureManager>("TextureManager");
        TextureManager_table["assignBoardTexture"] = &TextureManager::assignBoardTexture;
        TextureManager_table["setBoardTile"] = &TextureManager::setBoardTile;

        sol::usertype EventManager_table = lua.new_usertype<EventManager>("EventManager");
        EventManager_table["send"] = &EventManager::sendEvent;

        sol::usertype GameState_table = lua.new_usertype<GameState>("GameState");
        GameState_table["getTileState"] = &GameState::getTileState;
        GameState_table["getConfiguration"] = &GameState::getConfiguration;
        GameState_table["movePebbleP1"] = &GameState::movePebbleP1;
        GameState_table["movePebbleP2"] = &GameState::movePebbleP2;
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

    GameState game_state(game_configuration);
    EventManager eventManager(&lua, &luaMutex, &game_state);

    raylib::Window window(1280, 720, "Ur", FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);

    std::cout << "Loading textures\n";
    texture_manager.loadTextures();

    Camera2D camera;
    camera.offset = {GetScreenWidth() / 2.f, GetScreenHeight() / 2.f};
    camera.target = {0, 0};
    camera.rotation = 0;
    camera.zoom = 0.5;

    while (!raylib::Window::ShouldClose()) {
        eventManager.processEvents();

        auto &tex = texture_manager.getBoardTexture();
        Rectangle source = {
            .x = 0, .y = 0, .width = static_cast<float>(tex.width), .height = static_cast<float>(tex.height)
        };

        Rectangle dest = {
            .x = 0, .y = 0,
            .width = source.width, .height = source.height
        };

        BeginDrawing();
        ClearBackground(RAYWHITE);

        camera.offset = {GetScreenWidth() / 2.f, GetScreenHeight() / 2.f};
        camera.target = {
            dest.x + dest.width / 2,
            dest.y + dest.height / 2
        };

        camera.zoom *= 1. + GetMouseWheelMove() * 0.1;
        BeginMode2D(camera);

        DrawTexturePro(tex, source, dest, {.x = 0, .y = 0}, 0, WHITE);

        for (int i = 0; i < game_configuration.rows; i++) {
            for (int j = 0; j < game_configuration.columns; j++) {
                auto c = WHITE;
                if (i == game_configuration.p1_start_y && j == game_configuration.p1_start_x) c = RED;
                if (i == game_configuration.p2_start_y && j == game_configuration.p2_start_x) c = RED;
                if (i == game_configuration.p1_end_y && j == game_configuration.p1_end_x) c = BLUE;
                if (i == game_configuration.p2_end_y && j == game_configuration.p2_end_x) c = BLUE;

                c.a = 100;

                Rectangle tileRect = texture_manager.getTileRectangle(j, i);
                DrawRectangleLines(tileRect.x, tileRect.y, tileRect.width, tileRect.height, c);

                char id = game_configuration.schematic[i][j];
                char p1_dir = game_configuration.circuit_p1[i][j];
                char p2_dir = game_configuration.circuit_p2[i][j];
                auto [p1_pebbles, p2_pebbles] = game_state.getTileState(j, i);

                std::string info = std::format("{} {}\n{} {}", p1_dir, p2_dir, p1_pebbles, p2_pebbles);

                DrawText(
                    info.c_str(),
                    tileRect.x + tileRect.width / 2 - MeasureText(info.c_str(), 60) / 2,
                    tileRect.y + tileRect.height / 2 - 60,
                    60,
                    WHITE
                );
            }
        }

        EndDrawing();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            auto mousePos = GetMousePosition();
            auto world = GetScreenToWorld2D(mousePos, camera);

            Rectangle boardRectangle = texture_manager.getTileRectangle(0, 0);

            Vector2 relativeToStart = {
                world.x - boardRectangle.x,
                world.y - boardRectangle.y
            };

            int tileX = relativeToStart.x / boardRectangle.width;
            int tileY = relativeToStart.y / boardRectangle.height;

            if (tileX >= 0 && tileX < game_configuration.columns && tileY >= 0 && tileY < game_configuration.rows) {
                std::cout << "Tile click: " << tileX << ", " << tileY << "\n";

                eventManager.sendEvent(
                    "tileClicked",
                    lua.create_table_with(
                        "tileX", tileX,
                        "tileY", tileY
                    ),
                    false
                );
            }
        }
    }
}
