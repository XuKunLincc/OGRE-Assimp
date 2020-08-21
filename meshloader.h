#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <OgreMesh.h>

class MeshLoader{

public:
    MeshLoader(const std::string &file);
    Ogre::MeshPtr getMesh();

private:
    void loadModel();
    void processMesh(const aiScene* scene, const aiNode* node);
    void loadMaterials(const aiScene* scene);
    void loadTexture(const std::string& textureFile);

private:
    std::string mFile;
    Ogre::MeshPtr mMeshPtr;
    Ogre::AxisAlignedBox mAabb;
    std::vector<Ogre::MaterialPtr> mMaterials;
};
