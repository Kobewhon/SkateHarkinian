#include "NativeSkateObjectRegistry.h"
#include <iostream>
int main() {
    using namespace NativeSkateObjects;
    std::cout << "[";
    for (int i = 0; i < kTypeCount; ++i) {
        if (i)
            std::cout << ",";
        const auto& d = Definitions()[i];
        auto m = Geometry((Type)i);
        std::cout << "{\"key\":\"" << d.id << "\",\"name\":\"" << d.name << "\",\"resource\":\""
                  << (d.resource ? d.resource : "") << "\",\"surface\":" << (int)d.audio << ",\"type\":" << i
                  << ",\"category\":" << (int)d.category << ",\"behavior\":" << (int)d.behavior << ",\"bounds\":["
                  << d.width << "," << d.height << "," << d.length << "],\"vertices\":[";
        for (int j = 0; j < m.vertices.size(); ++j) {
            if (j)
                std::cout << ",";
            auto v = m.vertices[j];
            std::cout << "[" << v.x << "," << v.y << "," << v.z << "]";
        }
        std::cout << "],\"triangles\":[";
        for (int j = 0; j < m.triangles.size(); ++j) {
            if (j)
                std::cout << ",";
            auto t = m.triangles[j];
            std::cout << "[" << t[0] << "," << t[1] << "," << t[2] << "]";
        }
        std::cout << "],\"grinds\":[";
        auto g = GrindPath((Type)i);
        for (int j = 0; j < g.size(); ++j) {
            if (j)
                std::cout << ",";
            std::cout << "[" << g[j].x << "," << g[j].y << "," << g[j].z << "]";
        }
        std::cout << "],\"grindPaths\":[";
        auto paths = GrindPaths((Type)i);
        for (int k = 0; k < paths.size(); ++k) {
            if (k)
                std::cout << ",";
            std::cout << "[";
            for (int j = 0; j < paths[k].size(); ++j) {
                if (j)
                    std::cout << ",";
                auto v = paths[k][j];
                std::cout << "[" << v.x << "," << v.y << "," << v.z << "]";
            }
            std::cout << "]";
        }
        std::cout << "]}";
    }
    std::cout << "]";
}
