#pragma once

#include "EngineState.h"
#include "SceneManager.h"
#include <raylib.h>
#include <vector>

struct RenderContext {
    // SHADERS
    Shader geometryPassShader;
    Shader instancedShader;
    Shader lightingPassShader;
    Shader postShader;
    Shader lightVolumeShader;
    Shader nprResolveShader;
    Shader forwardBlinnShader;
    Shader forwardGoochShader;
    Shader forwardToonShader;
    Shader forwardOutlineShader;
    Shader forwardUnlitShader;

    // MESHES & MODELS
    std::vector<Mesh> lodMeshes;
    Mesh floorMesh;
    Model floorModel;
    Mesh lightningSphereMesh;
    Model lightningSourceModel;

    // TEXTURES & MATERIALS
    Texture2D obstacleTexture;
    Texture2D floorTexture;

    Material instancedMaterial;
    Material instancedLightMaterial;
    Material lightVolumeMaterial;
    Material fwdBlinnMaterial;
    Material fwdGoochMaterial;
    Material fwdToonMaterial;
    Material fwdOutlineMaterial;
    Material fwdFloorMaterial;
    Material fwdLightProxyMaterial;

    // FBOs & RENDER TARGETS
    unsigned int FboId;
    unsigned int albedoTexId;
    unsigned int normalTexId;
    unsigned int depthTexId;
    Texture2D gAlbedo;
    Texture2D gNormal;
    Texture2D gDepth;

    RenderTexture2D litSceneTarget;
    RenderTexture2D resolveTarget;

    unsigned int styleIdVboId;
    unsigned int lightStyleIdVboId;

    // SHADER LOCATIONS
    int modelMatLoc, normalMatLoc, styleIdLoc, isLightLocInstanced;
    int lightPosArrayLoc, lightColorLoc, LightingResolutionLoc, normalTexLoc, depthTexLoc;
    int invViewProjLoc, postViewPosLoc, intensityLoc, ambientLoc, activeLightsLoc;
    int maxLightRadiusLoc, attenuationConstantLoc, attenuationLinearLoc, attenuationQuadraticLoc;
    int uberEnableOutlinesLoc, uberEnableKuwaharaLoc, uberEnableGoochLoc, uberEnableToonLoc;
    int uberKuwaharaRadiusLoc, uberKuwaharaIntensityLoc;

    int lvResolutionLoc, lvViewPosLoc, lvLightColorLoc, lvIntensityLoc, lvMaxRadiusLoc;
    int lvAttenuationConstLoc, lvAttenuationLinearLoc, lvAttenuationQuadLoc;
    int lvInvViewProjLoc, lvAlbedoTexLoc, lvNormalTexLoc, lvDepthTexLoc;
    int lvEnableGoochLoc, lvEnableToonLoc;

    int nprResolutionLoc, nprViewPosLoc, nprAmbientLoc, nprInvViewProjLoc;
    int nprLitSceneTexLoc, nprAlbedoTexLoc, nprNormalTexLoc, nprDepthTexLoc;
    int resEnableOutlinesLoc, resEnableKuwaharaLoc, resEnableGoochLoc, resEnableToonLoc;
    int resKuwaharaRadiusLoc, resKuwaharaIntensityLoc;
    int postResolutionLoc;

    int fwdBlinnViewPosLoc, fwdBlinnIntensityLoc, fwdBlinnAmbientLoc, fwdBlinnActiveLightLoc;
    int fwdBlinnMaxRadiusLoc, fwdBlinnAttConstLoc, fwdBlinnAttLinLoc, fwdBlinnAttQuadLoc;
    int fwdBlinnLightPosLoc, fwdBlinnLightColorLoc;

    int fwdGoochViewPosLoc, fwdGoochIntensityLoc, fwdGoochAmbientLoc, fwdGoochActiveLightLoc;
    int fwdGoochMaxRadiusLoc, fwdGoochAttConstLoc, fwdGoochAttLinLoc, fwdGoochAttQuadLoc,
        fwdGoochLightPosLoc;

    int fwdToonViewPosLoc, fwdToonIntensityLoc, fwdToonAmbientLoc, fwdToonActiveLightLoc;
    int fwdToonMaxRadiusLoc, fwdToonAttConstLoc, fwdToonAttLinLoc, fwdToonAttQuadLoc;
    int fwdToonLightPosLoc, fwdToonLightColorLoc;
    int fwdOutlineViewPosLoc;

    // METHODS
    void Initialize(const EngineState &engineState, const SceneManager &sceneManager,
                    int renderWidth, int renderHeight);
    void RebuildHDRTargets(bool use16BitHDR, int renderWidth, int renderHeight);
    void ResizeTargets(int newWidth, int newHeight, bool use16BitHDR);
    void Destroy() const;
};