#include "SceneDocument.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <cctype>

using json = nlohmann::json;

// Extract PKGV number from version stamp like "PKGV0023"
static int ParsePkgVersion(const std::string& ver) {
    if (ver.size() < 5) return 0;
    int v = 0;
    for (size_t i = 4; i < ver.size(); i++) {
        if (ver[i] >= '0' && ver[i] <= '9')
            v = v * 10 + (ver[i] - '0');
    }
    return v;
}

// Normalize a pkg path to lookup key
static std::string Normalize(const std::string& p) {
    std::string s;
    s.reserve(p.size() + 1);
    s += '/';
    for (size_t i = 0; i < p.size(); i++) {
        char c = p[i];
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        s += c;
    }
    return s;
}

bool SceneDocument::Parse(const std::string& jsonText, const std::string& pkgVer) {
    pkgVersion = pkgVer;
    int pkgv = ParsePkgVersion(pkgVer);

    try {
        json root = json::parse(jsonText);

        // Read general/canvas info
        auto genIt = root.find("general");
        if (genIt != root.end() && genIt->is_object()) {
            auto orthoIt = genIt->find("orthogonalprojection");
            if (orthoIt != root.end() && orthoIt->is_object()) {
                width  = orthoIt->value("width", 1920);
                height = orthoIt->value("height", 1080);
            }
        }

        // Read objects array
        auto objIt = root.find("objects");
        if (objIt == root.end() || !objIt->is_array()) return true;

        const auto& objs = *objIt;
        for (size_t i = 0; i < objs.size(); i++) {
            if (!objs[i].is_object()) continue;

            SceneObject obj;
            obj.id = objs[i].value("id", (int64_t)i);
            obj.name = objs[i].value("name", "");
            obj.visible = objs[i].value("visible", true);
            obj.parent = objs[i].value("parent", (int64_t)-1);

            // Read size
            auto sizeIt = objs[i].find("size");
            if (sizeIt != objs[i].end() && sizeIt->is_array() && sizeIt->size() >= 2) {
                obj.size[0] = (*sizeIt)[0].get<float>();
                obj.size[1] = (*sizeIt)[1].get<float>();
            }

            // Determine object type
            if (objs[i].contains("image") && !objs[i].at("image").is_null()) {
                obj.type = "image";
                obj.imageRef = objs[i]["image"].get<std::string>();

                // The imageRef could be:
                // - a direct image path like "textures/0.png"
                // - a JSON descriptor (also in the pkg)
                // Add both as potential texture paths
                obj.texturePaths.push_back(Normalize(obj.imageRef));
            }
            else if (objs[i].contains("particle") && !objs[i].at("particle").is_null()) {
                obj.type = "particle";
                obj.imageRef = objs[i]["particle"].get<std::string>();
            }
            else if (objs[i].contains("sound") && !objs[i].at("sound").is_null()) {
                obj.type = "sound";
            }
            else if (objs[i].contains("light") && !objs[i].at("light").is_null()) {
                obj.type = "light";
            }
            else if (objs[i].contains("text") && !objs[i].at("text").is_null()) {
                obj.type = "text";
            }
            else if (objs[i].contains("model") && !objs[i].at("model").is_null()) {
                obj.type = "model";
            }
            else if (objs[i].contains("camera") && !objs[i].at("camera").is_null()) {
                obj.type = "camera";
            }
            else {
                obj.type = "container";
            }

            objects.push_back(obj);
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> SceneDocument::AllTexturePaths() const {
    std::vector<std::string> paths;
    for (size_t i = 0; i < objects.size(); i++) {
        for (size_t j = 0; j < objects[i].texturePaths.size(); j++) {
            const std::string& p = objects[i].texturePaths[j];
            if (p.empty()) continue;
            bool found = false;
            for (size_t k = 0; k < paths.size(); k++) {
                if (paths[k] == p) { found = true; break; }
            }
            if (!found) paths.push_back(p);
        }
    }
    return paths;
}
