#pragma once

struct aiScene;

const aiScene* loadAssimpSceneFile(const char* filepath);
void freeAssimpScene(const aiScene* scene);
void analyzeAssimpScene(const aiScene* scene);
