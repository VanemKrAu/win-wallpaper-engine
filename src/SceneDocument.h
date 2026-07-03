#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SceneObject {
    int64_t  id = 0;
    std::string name;
    std::string type;    // "image", "particle", "sound", "light", "text", "model", "container"
    bool visible = true;
    std::string imageRef;  // from "image" field, e.g. "textures/0.png"
    std::vector<std::string> texturePaths; // resolved texture files from imageRef
    float size[2] = {0,0};
    int64_t parent = -1;
};

struct SceneDocument {
    std::string pkgVersion;  // e.g. "PKGV0023"
    int width  = 1920;
    int height = 1080;
    std::vector<SceneObject> objects;

    bool Parse(const std::string& jsonText, const std::string& pkgVer);
    std::vector<std::string> AllTexturePaths() const;
};
