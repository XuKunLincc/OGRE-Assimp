#include "meshloader.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <OgreMeshManager.h>
#include <OgreSubMesh.h>
#include <OgreMaterialManager.h>
#include <OgreTextureManager.h>
#include <OgreHardwareBufferManager.h>
#include <OgreTechnique.h>
#include <OgreMaterial.h>
#include <iostream>
#include <boost/filesystem.hpp>

MeshLoader::MeshLoader(const std::string &file)
{
    mFile = file;
    loadModel();
}

Ogre::MeshPtr MeshLoader::getMesh()
{
    return mMeshPtr;
}

void MeshLoader::loadModel()
{
    Assimp::Importer modelReader;
    const aiScene *scene = modelReader.ReadFile(mFile, \
                                                aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    loadMaterials(scene);

    mMeshPtr = Ogre::MeshManager::getSingletonPtr()->createManual("test",\
                                                                  Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    processMesh(scene, scene->mRootNode);

    mMeshPtr->_setBounds(mAabb);
    mMeshPtr->buildEdgeList();
    mMeshPtr->load();

}

void MeshLoader::processMesh(const aiScene *scene, const aiNode* node)
{

    for(uint32_t i = 0; i < node->mNumMeshes; i++){

        Ogre::SubMesh *subMesh = mMeshPtr->createSubMesh();
        aiMesh *inputMesh = scene->mMeshes[node->mMeshes[i]];

        subMesh->useSharedVertices = false;
        subMesh->vertexData = new Ogre::VertexData();

        Ogre::VertexData* vertexData = subMesh->vertexData;
        Ogre::VertexDeclaration* vertexDecl = vertexData->vertexDeclaration;

        size_t offset = 0;
        vertexDecl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);

        if(inputMesh->HasNormals()){
            vertexDecl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL);
            offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
        }

        if(inputMesh->HasTextureCoords(0)){
            vertexDecl->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0);
            offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);
        }

        vertexData->vertexCount = inputMesh->mNumVertices;
        std::cout << "vertexCount = " << vertexData->vertexCount << std::endl;
        Ogre::HardwareVertexBufferSharedPtr vertextBuf =
                Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
                    vertexDecl->getVertexSize(0), vertexData->vertexCount,
                    Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY, false);

        vertexData->vertexBufferBinding->setBinding(0, vertextBuf);
        float* vertices = static_cast<float*>(vertextBuf->lock(Ogre::HardwareBuffer::HBL_DISCARD));

        for(uint32_t j = 0; j < inputMesh->mNumVertices; j++){

            aiVector3D p = inputMesh->mVertices[j];
            p.x *= 10;
            p.y *= 10;
            p.z *= 10;
            *vertices++ = p.x;
            *vertices++ = p.y;
            *vertices++ = p.z;

            mAabb.merge(Ogre::Vector3(p.x, p.y, p.z));

            if(inputMesh->HasNormals()){
                aiVector3D n = inputMesh->mNormals[j];
                *vertices++ = n.x;
                *vertices++ = n.y;
                *vertices++ = n.z;
            }

            if(inputMesh->HasTextureCoords(0)){
                *vertices++ = inputMesh->mTextureCoords[0][j].x;
                *vertices++ = inputMesh->mTextureCoords[0][j].y;
            }
        }

        /**
         * 计算绘制索引数量
         */
        subMesh->indexData->indexCount = 0;
        for (uint32_t j = 0; j < inputMesh->mNumFaces; j++)
        {
            aiFace& face = inputMesh->mFaces[j];
            subMesh->indexData->indexCount += face.mNumIndices;
        }

        subMesh->indexData->indexBuffer = Ogre::HardwareBufferManager::getSingletonPtr()->
                createIndexBuffer(Ogre::HardwareIndexBuffer::IT_16BIT,
                                  subMesh->indexData->indexCount, Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY, false);

        Ogre::HardwareIndexBufferSharedPtr indexBuf = subMesh->indexData->indexBuffer;
        uint16_t* indices = static_cast<uint16_t*>(indexBuf->lock(Ogre::HardwareBuffer::HBL_DISCARD));

        for(uint32_t j = 0; j < inputMesh->mNumFaces; j++){
            aiFace& face = inputMesh->mFaces[j];
            for (uint32_t k = 0; k < face.mNumIndices; ++k)
            {
                *indices++ = face.mIndices[k];
            }
        }
        indexBuf->unlock();
        vertextBuf->unlock();
        subMesh->setMaterialName(mMaterials[inputMesh->mMaterialIndex]->getName());
    }

    for(uint32_t i = 0; i < node->mNumChildren; i++){
        processMesh(scene, node->mChildren[i]);
    }
}

void MeshLoader::loadMaterials(const aiScene *scene)
{
    for(uint32_t i = 0; i < scene->mNumMaterials; i++){

        std::stringstream ss;
        ss << mFile << "Material" << i;
        Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().\
                create(ss.str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);

        Ogre::Technique* tech = material->getTechnique(0);
        Ogre::Pass* pass = tech->getPass(0);
        mMaterials.push_back(material);

        aiMaterial* amat = scene->mMaterials[i];
        Ogre::ColourValue diffuse(1.0, 1.0, 1.0, 1.0);
        Ogre::ColourValue specular(1.0, 1.0, 1.0, 1.0);
        Ogre::ColourValue ambient(0, 0, 0, 1.0);
        bool geted = false;

        for(uint32_t j = 0; j < amat->mNumProperties; j++){

            aiMaterialProperty* prop = amat->mProperties[j];
            std::string propKey = prop->mKey.data;

            if (propKey == "$tex.file" && !geted)
            {
                aiString texName;
                amat->GetTexture(aiTextureType_DIFFUSE, 0, &texName);
                std::string texturePath = boost::filesystem::path(mFile).parent_path().string() + "/" + texName.data;
                loadTexture(texturePath);
                Ogre::TextureUnitState* tu = pass->createTextureUnitState();
                tu->setTextureName(texturePath);
                geted = true;
            }else if (propKey == "$clr.diffuse"){
                aiColor3D clr;
                amat->Get(AI_MATKEY_COLOR_DIFFUSE, clr);
                diffuse = Ogre::ColourValue(clr.r, clr.g, clr.b);
            }else if (propKey == "$clr.ambient"){
                aiColor3D clr;
                amat->Get(AI_MATKEY_COLOR_AMBIENT, clr);
                ambient = Ogre::ColourValue(clr.r, clr.g, clr.b);
            }else if (propKey == "$clr.specular"){
                aiColor3D clr;
                amat->Get(AI_MATKEY_COLOR_SPECULAR, clr);
                specular = Ogre::ColourValue(clr.r, clr.g, clr.b);
            }else if (propKey == "$clr.emissive"){
                aiColor3D clr;
                amat->Get(AI_MATKEY_COLOR_EMISSIVE, clr);
                material->setSelfIllumination(clr.r, clr.g, clr.b);
            }else if (propKey == "$clr.opacity"){
                float o;
                amat->Get(AI_MATKEY_OPACITY, o);
                diffuse.a = o;
            }else if (propKey == "$mat.shininess"){
                float s;
                amat->Get(AI_MATKEY_SHININESS, s);
                material->setShininess(s);
            }else if (propKey == "$mat.shadingm"){
                int model;
                amat->Get(AI_MATKEY_SHADING_MODEL, model);
                switch (model)
                {
                case aiShadingMode_Flat:
                    material->setShadingMode(Ogre::SO_FLAT);
                    break;
                case aiShadingMode_Phong:
                    material->setShadingMode(Ogre::SO_PHONG);
                    break;
                case aiShadingMode_Gouraud:
                default:
                    material->setShadingMode(Ogre::SO_GOURAUD);
                    break;
                }
            }
        }

        int mode = aiBlendMode_Default;
        amat->Get(AI_MATKEY_BLEND_FUNC, mode);
        switch (mode)
        {
        case aiBlendMode_Additive:
            material->setSceneBlending(Ogre::SBT_ADD);
            break;
        case aiBlendMode_Default:
        default:
        {
            if (diffuse.a < 0.99)
            {
                pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
            }
            else
            {
                pass->setSceneBlending(Ogre::SBT_REPLACE);
            }
        }
            break;
        }

        material->setAmbient(ambient);
        material->setDiffuse(diffuse);
        specular.a = diffuse.a;
        material->setSpecular(specular);
    }
}

void MeshLoader::loadTexture(const std::string &textureFile)
{
    // 该纹理已经加载过了
    if(Ogre::TextureManager::getSingleton().resourceExists(textureFile))
        return;

    std::ifstream f;
    f.open(textureFile, std::ios::binary | std::ios::in);
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    f.seekg(0, std::ios::beg);
    char* buf = new char[len];
    f.read(buf, len);
    f.close();

    Ogre::DataStreamPtr imgData(new Ogre::MemoryDataStream(buf, len));
    std::string extension = boost::filesystem::extension(boost::filesystem::path(textureFile));
    if (extension[0] == '.')
    {
        extension = extension.substr(1, extension.size() - 1);
    }

    Ogre::Image image;
    try{
        image.load(imgData, extension);
        Ogre::TextureManager::getSingleton().loadImage(
                    textureFile, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, image);
    }catch(Ogre::Exception& e){
        std::cout << "can't load texture[" << textureFile << "], type = "\
                  << extension << ",len = " << len << ", what" << e.what();
    }

}
