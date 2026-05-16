#include "../include/RenderContext.h"
#include <rlgl.h>
#include <string>

void RenderContext::Initialize(const EngineState &engineState, const SceneManager &sceneManager,
                               const int renderWidth, const int renderHeight) {
    const auto sPath = std::string(Config::Paths::Shaders);

    // SHADER LOADING
    // ---------------------------------------------------------------------------------------------

    // Deferred Pipeline Shaders
    geometryPassShader = LoadShader((sPath + std::string(Config::Shaders::GBufferVert)).c_str(),
                                    (sPath + std::string(Config::Shaders::GBufferFrag)).c_str());
    instancedShader =
        LoadShader((sPath + std::string(Config::Shaders::GBufferInstancedVert)).c_str(),
                   (sPath + std::string(Config::Shaders::GBufferInstancedFrag)).c_str());
    lightingPassShader =
        LoadShader(nullptr, (sPath + std::string(Config::Shaders::DeferredUberFrag)).c_str());
    postShader =
        LoadShader(nullptr, (sPath + std::string(Config::Shaders::PostProcessFrag)).c_str());
    lightVolumeShader =
        LoadShader((sPath + std::string(Config::Shaders::DeferredVolumeVert)).c_str(),
                   (sPath + std::string(Config::Shaders::DeferredVolumeFrag)).c_str());
    nprResolveShader =
        LoadShader(nullptr, (sPath + std::string(Config::Shaders::DeferredResolveFrag)).c_str());

    // Forward Pipeline Shaders (Baseline comparison)
    forwardBlinnShader =
        LoadShader((sPath + std::string(Config::Shaders::ForwardInstancedVert)).c_str(),
                   (sPath + std::string(Config::Shaders::ForwardBlinnFrag)).c_str());
    forwardGoochShader =
        LoadShader((sPath + std::string(Config::Shaders::ForwardInstancedVert)).c_str(),
                   (sPath + std::string(Config::Shaders::ForwardGoochFrag)).c_str());
    forwardToonShader =
        LoadShader((sPath + std::string(Config::Shaders::ForwardInstancedVert)).c_str(),
                   (sPath + std::string(Config::Shaders::ForwardToonFrag)).c_str());
    forwardOutlineShader =
        LoadShader((sPath + std::string(Config::Shaders::ForwardOutlineVert)).c_str(),
                   (sPath + std::string(Config::Shaders::ForwardOutlineFrag)).c_str());
    forwardUnlitShader =
        LoadShader((sPath + std::string(Config::Shaders::ForwardInstancedVert)).c_str(),
                   (sPath + std::string(Config::Shaders::ForwardUnlitFrag)).c_str());

    // Bind instanceTransform attribute for all hardware-instanced vertex shaders
    forwardBlinnShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardBlinnShader, "instanceTransform");
    forwardGoochShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardGoochShader, "instanceTransform");
    forwardToonShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardToonShader, "instanceTransform");
    forwardOutlineShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardOutlineShader, "instanceTransform");
    forwardUnlitShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardUnlitShader, "instanceTransform");

    // GEOMETRY & ASSET GENERATION
    // ---------------------------------------------------------------------------------------------

    // Generate base LOD meshes (Cube + varying resolution Spheres)
    lodMeshes.push_back(GenMeshCube(0.75f, 0.75f, 0.75f));
    lodMeshes.push_back(GenMeshSphere(0.75f, 8, 8));
    lodMeshes.push_back(GenMeshSphere(0.75f, 16, 16));
    lodMeshes.push_back(GenMeshSphere(0.75f, 32, 32));
    lodMeshes.push_back(GenMeshSphere(0.75f, 64, 64));

    for (auto &mesh : lodMeshes)
        GenMeshTangents(&mesh);

    const std::string texPath =
        std::string(Config::Paths::Textures) + std::string(Config::Assets::ObstacleTexture);
    obstacleTexture = LoadTexture(texPath.c_str());

    // Generate floor plane and tile texture coordinates for the NPR Room
    floorMesh  = GenMeshPlane(1000.0f, 1000.0f, 100, 100);
    floorModel = LoadModelFromMesh(floorMesh);
    const std::string woodTexPath =
        std::string(Config::Paths::Textures) + std::string(Config::Assets::WoodTexture);
    floorTexture = LoadTexture(woodTexPath.c_str());
    SetTextureWrap(floorTexture, TEXTURE_WRAP_REPEAT);

    for (int i = 0; i < floorMesh.vertexCount; i++) {
        floorMesh.texcoords[i * 2] *= 100.0f;
        floorMesh.texcoords[i * 2 + 1] *= 100.0f;
    }
    UpdateMeshBuffer(floorMesh, 1, floorMesh.texcoords,
                     floorMesh.vertexCount * static_cast<int>(2 * sizeof(float)), 0);

    for (int i = 0; i < floorModel.materialCount; i++) {
        floorModel.materials[i].shader                            = geometryPassShader;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = floorTexture;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
    }

    // Proxy geometry for light volumes
    lightningSphereMesh                      = GenMeshSphere(0.5f, 16, 16);
    lightningSourceModel                     = LoadModelFromMesh(lightningSphereMesh);
    lightningSourceModel.materials[0].shader = geometryPassShader;

    // INSTANCING: VAO & VBO SETUP
    // ---------------------------------------------------------------------------------------------

    instancedShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(instancedShader, "instanceTransform");
    // Load dynamic VBO for style IDs (Obstacles)
    styleIdVboId = rlLoadVertexBuffer(sceneManager.GetMasterStyleIds().data(),
                                      Config::EngineSettings::MAX_OBSTACLES * sizeof(float), true);

    // Bind the styleId VBO to attribute location 10 with a divisor of 1 (per instance) for all LODs
    for (const auto &mesh : lodMeshes) {
        rlEnableVertexArray(mesh.vaoId);
        rlEnableVertexBuffer(styleIdVboId);
        rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(10);
        rlSetVertexAttributeDivisor(10, 1);
        rlDisableVertexBuffer();
        rlDisableVertexArray();
    }

    instancedMaterial                                   = LoadMaterialDefault();
    instancedMaterial.shader                            = instancedShader;
    instancedMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    // Load dynamic VBO for style IDs (Lights)
    const std::vector instancedLightStyleIds(Config::EngineSettings::MAX_LIGHTS, 0.0f);
    lightStyleIdVboId = rlLoadVertexBuffer(
        instancedLightStyleIds.data(), Config::EngineSettings::MAX_LIGHTS * sizeof(float), false);

    rlEnableVertexArray(lightningSphereMesh.vaoId);
    rlEnableVertexBuffer(lightStyleIdVboId);
    rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(10);
    rlSetVertexAttributeDivisor(10, 1);
    rlDisableVertexBuffer();
    rlDisableVertexArray();

    instancedLightMaterial        = LoadMaterialDefault();
    instancedLightMaterial.shader = instancedShader;

    // G-BUFFER INITIALIZATION
    // ---------------------------------------------------------------------------------------------

    FboId = rlLoadFramebuffer();
    if (FboId == 0)
        TraceLog(LOG_ERROR, "Failed to create G-Buffer FBO");

    // Layout: Color0 = Albedo, Color1 = Normals, Depth = Hardware Depth
    albedoTexId =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    normalTexId =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    depthTexId = rlLoadTextureDepth(renderWidth, renderHeight, false);

    rlEnableFramebuffer(FboId);
    rlFramebufferAttach(FboId, albedoTexId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, normalTexId, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, depthTexId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(FboId))
        TraceLog(LOG_ERROR, "G-Buffer FBO is incomplete.");
    rlDisableFramebuffer();

    // Wrap raw GL IDs into Raylib Textures
    gAlbedo = {albedoTexId, renderWidth, renderHeight, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    gNormal = {normalTexId, renderWidth, renderHeight, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    gDepth  = {depthTexId, renderWidth, renderHeight, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};

    // Point filtering is crucial for G-Buffer sampling to avoid interpolating across object edges
    SetTextureFilter(gAlbedo, TEXTURE_FILTER_POINT);
    SetTextureFilter(gNormal, TEXTURE_FILTER_POINT);
    SetTextureFilter(gDepth, TEXTURE_FILTER_POINT);

    // RENDER TARGETS & HDR
    // ---------------------------------------------------------------------------------------------

    // Initialize lighting accumulation targets
    litSceneTarget = LoadRenderTexture(renderWidth, renderHeight);
    resolveTarget  = LoadRenderTexture(renderWidth, renderHeight);
    RebuildHDRTargets(engineState.use16BitHDR, renderWidth, renderHeight);

    // Bind G-Buffer outputs as inputs for the Light Volume shader
    lightVolumeMaterial                                      = LoadMaterialDefault();
    lightVolumeMaterial.shader                               = lightVolumeShader;
    lightVolumeMaterial.maps[MATERIAL_MAP_ALBEDO].texture    = gAlbedo;
    lightVolumeMaterial.maps[MATERIAL_MAP_NORMAL].texture    = gNormal;
    lightVolumeMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = gDepth;

    lightVolumeShader.locs[SHADER_LOC_MAP_ALBEDO] =
        GetShaderLocation(lightVolumeShader, "texture0");
    lightVolumeShader.locs[SHADER_LOC_MAP_NORMAL] =
        GetShaderLocation(lightVolumeShader, "gNormalTex");
    lightVolumeShader.locs[SHADER_LOC_MAP_ROUGHNESS] =
        GetShaderLocation(lightVolumeShader, "gDepthTex");

    // SHADER LOCATIONS & STATIC UNIFORMS
    // ---------------------------------------------------------------------------------------------

    // Geometry Pass
    modelMatLoc         = GetShaderLocation(geometryPassShader, "modelMat");
    normalMatLoc        = GetShaderLocation(geometryPassShader, "normalMat");
    styleIdLoc          = GetShaderLocation(geometryPassShader, "styleId");
    isLightLocInstanced = GetShaderLocation(instancedShader, "isLightSource");

    // Uber Shader
    lightPosArrayLoc                 = GetShaderLocation(lightingPassShader, "lightPositions");
    lightColorLoc                    = GetShaderLocation(lightingPassShader, "lightColor");
    LightingResolutionLoc            = GetShaderLocation(lightingPassShader, "resolution");
    normalTexLoc                     = GetShaderLocation(lightingPassShader, "gNormalTex");
    depthTexLoc                      = GetShaderLocation(lightingPassShader, "gDepthTex");
    invViewProjLoc                   = GetShaderLocation(lightingPassShader, "invViewProj");
    postViewPosLoc                   = GetShaderLocation(lightingPassShader, "viewPos");
    const int postBackgroundColorLoc = GetShaderLocation(lightingPassShader, "backgroundColor");
    intensityLoc                     = GetShaderLocation(lightingPassShader, "lightIntensity");
    ambientLoc               = GetShaderLocation(lightingPassShader, "ambientLightStrength");
    activeLightsLoc          = GetShaderLocation(lightingPassShader, "activeLights");
    maxLightRadiusLoc        = GetShaderLocation(lightingPassShader, "maxLightRadius");
    attenuationConstantLoc   = GetShaderLocation(lightingPassShader, "attenuationConstant");
    attenuationLinearLoc     = GetShaderLocation(lightingPassShader, "attenuationLinear");
    attenuationQuadraticLoc  = GetShaderLocation(lightingPassShader, "attenuationQuadratic");
    uberEnableOutlinesLoc    = GetShaderLocation(lightingPassShader, "enableOutlines");
    uberEnableKuwaharaLoc    = GetShaderLocation(lightingPassShader, "enableKuwahara");
    uberEnableGoochLoc       = GetShaderLocation(lightingPassShader, "enableGooch");
    uberEnableToonLoc        = GetShaderLocation(lightingPassShader, "enableToon");
    uberKuwaharaRadiusLoc    = GetShaderLocation(lightingPassShader, "kuwaharaRadius");
    uberKuwaharaIntensityLoc = GetShaderLocation(lightingPassShader, "kuwaharaIntensity");

    constexpr Vector3 BgColor = {Config::EngineSettings::BackgroundColor.x / 255.0f,
                                 Config::EngineSettings::BackgroundColor.y / 255.0f,
                                 Config::EngineSettings::BackgroundColor.z / 255.0f};

    SetShaderValue(lightingPassShader, postBackgroundColorLoc, &BgColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingPassShader, attenuationConstantLoc,
                   &Config::EngineSettings::AttenuationConstant, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, attenuationLinearLoc,
                   &Config::EngineSettings::AttenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, attenuationQuadraticLoc,
                   &Config::EngineSettings::AttenuationQuadratic, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, intensityLoc, &engineState.lightIntensity,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, ambientLoc, &engineState.ambientLightStrength,
                   SHADER_UNIFORM_FLOAT);

    // Light Volume Pass
    lvResolutionLoc        = GetShaderLocation(lightVolumeShader, "resolution");
    lvViewPosLoc           = GetShaderLocation(lightVolumeShader, "viewPos");
    lvLightColorLoc        = GetShaderLocation(lightVolumeShader, "lightColor");
    lvIntensityLoc         = GetShaderLocation(lightVolumeShader, "lightIntensity");
    lvMaxRadiusLoc         = GetShaderLocation(lightVolumeShader, "maxLightRadius");
    lvAttenuationConstLoc  = GetShaderLocation(lightVolumeShader, "attenuationConstant");
    lvAttenuationLinearLoc = GetShaderLocation(lightVolumeShader, "attenuationLinear");
    lvAttenuationQuadLoc   = GetShaderLocation(lightVolumeShader, "attenuationQuadratic");
    lvInvViewProjLoc       = GetShaderLocation(lightVolumeShader, "invViewProj");
    lvAlbedoTexLoc         = GetShaderLocation(lightVolumeShader, "texture0");
    lvNormalTexLoc         = GetShaderLocation(lightVolumeShader, "gNormalTex");
    lvDepthTexLoc          = GetShaderLocation(lightVolumeShader, "gDepthTex");
    lvEnableGoochLoc       = GetShaderLocation(lightVolumeShader, "enableGooch");
    lvEnableToonLoc        = GetShaderLocation(lightVolumeShader, "enableToon");

    SetShaderValue(lightVolumeShader, lvAttenuationConstLoc,
                   &Config::EngineSettings::AttenuationConstant, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightVolumeShader, lvAttenuationLinearLoc,
                   &Config::EngineSettings::AttenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightVolumeShader, lvAttenuationQuadLoc,
                   &Config::EngineSettings::AttenuationQuadratic, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightVolumeShader, lvIntensityLoc, &engineState.lightIntensity,
                   SHADER_UNIFORM_FLOAT);
    lightVolumeShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(lightVolumeShader, "instanceTransform");

    // NPR Resolve Pass
    nprResolutionLoc        = GetShaderLocation(nprResolveShader, "resolution");
    nprViewPosLoc           = GetShaderLocation(nprResolveShader, "viewPos");
    const int nprBgColorLoc = GetShaderLocation(nprResolveShader, "backgroundColor");
    nprAmbientLoc           = GetShaderLocation(nprResolveShader, "ambientLightStrength");
    nprInvViewProjLoc       = GetShaderLocation(nprResolveShader, "invViewProj");
    nprLitSceneTexLoc       = GetShaderLocation(nprResolveShader, "litSceneTex");
    nprAlbedoTexLoc         = GetShaderLocation(nprResolveShader, "texture0");
    nprNormalTexLoc         = GetShaderLocation(nprResolveShader, "gNormalTex");
    nprDepthTexLoc          = GetShaderLocation(nprResolveShader, "gDepthTex");
    resEnableOutlinesLoc    = GetShaderLocation(nprResolveShader, "enableOutlines");
    resEnableKuwaharaLoc    = GetShaderLocation(nprResolveShader, "enableKuwahara");
    resEnableGoochLoc       = GetShaderLocation(nprResolveShader, "enableGooch");
    resEnableToonLoc        = GetShaderLocation(nprResolveShader, "enableToon");
    resKuwaharaRadiusLoc    = GetShaderLocation(nprResolveShader, "kuwaharaRadius");
    resKuwaharaIntensityLoc = GetShaderLocation(nprResolveShader, "kuwaharaIntensity");

    SetShaderValue(nprResolveShader, nprBgColorLoc, &BgColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(nprResolveShader, nprAmbientLoc, &engineState.ambientLightStrength,
                   SHADER_UNIFORM_FLOAT);

    // Post-Process Pass
    postResolutionLoc = GetShaderLocation(postShader, "resolution");

    // Forward Blinn Pass
    fwdBlinnViewPosLoc     = GetShaderLocation(forwardBlinnShader, "viewPos");
    fwdBlinnIntensityLoc   = GetShaderLocation(forwardBlinnShader, "lightIntensity");
    fwdBlinnAmbientLoc     = GetShaderLocation(forwardBlinnShader, "ambientLightStrength");
    fwdBlinnActiveLightLoc = GetShaderLocation(forwardBlinnShader, "activeLights");
    fwdBlinnMaxRadiusLoc   = GetShaderLocation(forwardBlinnShader, "maxLightRadius");
    fwdBlinnAttConstLoc    = GetShaderLocation(forwardBlinnShader, "attenuationConstant");
    fwdBlinnAttLinLoc      = GetShaderLocation(forwardBlinnShader, "attenuationLinear");
    fwdBlinnAttQuadLoc     = GetShaderLocation(forwardBlinnShader, "attenuationQuadratic");
    fwdBlinnLightPosLoc    = GetShaderLocation(forwardBlinnShader, "lightPositions");
    fwdBlinnLightColorLoc  = GetShaderLocation(forwardBlinnShader, "lightColor");

    SetShaderValue(forwardBlinnShader, fwdBlinnAttConstLoc,
                   &Config::EngineSettings::AttenuationConstant, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardBlinnShader, fwdBlinnAttLinLoc,
                   &Config::EngineSettings::AttenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardBlinnShader, fwdBlinnAttQuadLoc,
                   &Config::EngineSettings::AttenuationQuadratic, SHADER_UNIFORM_FLOAT);

    // Forward Gooch Pass
    fwdGoochViewPosLoc     = GetShaderLocation(forwardGoochShader, "viewPos");
    fwdGoochIntensityLoc   = GetShaderLocation(forwardGoochShader, "lightIntensity");
    fwdGoochAmbientLoc     = GetShaderLocation(forwardGoochShader, "ambientLightStrength");
    fwdGoochActiveLightLoc = GetShaderLocation(forwardGoochShader, "activeLights");
    fwdGoochMaxRadiusLoc   = GetShaderLocation(forwardGoochShader, "maxLightRadius");
    fwdGoochAttConstLoc    = GetShaderLocation(forwardGoochShader, "attenuationConstant");
    fwdGoochAttLinLoc      = GetShaderLocation(forwardGoochShader, "attenuationLinear");
    fwdGoochAttQuadLoc     = GetShaderLocation(forwardGoochShader, "attenuationQuadratic");
    fwdGoochLightPosLoc    = GetShaderLocation(forwardGoochShader, "lightPositions");

    SetShaderValue(forwardGoochShader, fwdGoochAttConstLoc,
                   &Config::EngineSettings::AttenuationConstant, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardGoochShader, fwdGoochAttLinLoc,
                   &Config::EngineSettings::AttenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardGoochShader, fwdGoochAttQuadLoc,
                   &Config::EngineSettings::AttenuationQuadratic, SHADER_UNIFORM_FLOAT);

    // Forward Toon Pass
    fwdToonViewPosLoc     = GetShaderLocation(forwardToonShader, "viewPos");
    fwdToonIntensityLoc   = GetShaderLocation(forwardToonShader, "lightIntensity");
    fwdToonAmbientLoc     = GetShaderLocation(forwardToonShader, "ambientLightStrength");
    fwdToonActiveLightLoc = GetShaderLocation(forwardToonShader, "activeLights");
    fwdToonMaxRadiusLoc   = GetShaderLocation(forwardToonShader, "maxLightRadius");
    fwdToonAttConstLoc    = GetShaderLocation(forwardToonShader, "attenuationConstant");
    fwdToonAttLinLoc      = GetShaderLocation(forwardToonShader, "attenuationLinear");
    fwdToonAttQuadLoc     = GetShaderLocation(forwardToonShader, "attenuationQuadratic");
    fwdToonLightPosLoc    = GetShaderLocation(forwardToonShader, "lightPositions");
    fwdToonLightColorLoc  = GetShaderLocation(forwardToonShader, "lightColor");

    SetShaderValue(forwardToonShader, fwdToonAttConstLoc,
                   &Config::EngineSettings::AttenuationConstant, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardToonShader, fwdToonAttLinLoc, &Config::EngineSettings::AttenuationLinear,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardToonShader, fwdToonAttQuadLoc,
                   &Config::EngineSettings::AttenuationQuadratic, SHADER_UNIFORM_FLOAT);

    fwdOutlineViewPosLoc = GetShaderLocation(forwardOutlineShader, "viewPos");

    // Forward Materials Setup
    fwdBlinnMaterial                                   = LoadMaterialDefault();
    fwdBlinnMaterial.shader                            = forwardBlinnShader;
    fwdBlinnMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    fwdGoochMaterial                                   = LoadMaterialDefault();
    fwdGoochMaterial.shader                            = forwardGoochShader;
    fwdGoochMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    fwdToonMaterial                                   = LoadMaterialDefault();
    fwdToonMaterial.shader                            = forwardToonShader;
    fwdToonMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    fwdOutlineMaterial        = LoadMaterialDefault();
    fwdOutlineMaterial.shader = forwardOutlineShader;

    fwdFloorMaterial                                   = LoadMaterialDefault();
    fwdFloorMaterial.shader                            = forwardBlinnShader;
    fwdFloorMaterial.maps[MATERIAL_MAP_ALBEDO].texture = floorTexture;

    fwdLightProxyMaterial        = LoadMaterialDefault();
    fwdLightProxyMaterial.shader = forwardUnlitShader;
}

// Dynamically rebuilds internal accumulation buffers to evaluate bandwidth costs
// of 16-bit Float vs 8-bit Clamped rendering.
void RenderContext::RebuildHDRTargets(const bool use16BitHDR, const int renderWidth,
                                      const int renderHeight) {
    const int targetFormat =
        use16BitHDR ? PIXELFORMAT_UNCOMPRESSED_R16G16B16A16 : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    rlUnloadTexture(litSceneTarget.texture.id);
    litSceneTarget.texture.id = rlLoadTexture(nullptr, renderWidth, renderHeight, targetFormat, 1);
    litSceneTarget.texture.format = targetFormat;
    rlEnableFramebuffer(litSceneTarget.id);
    rlFramebufferAttach(litSceneTarget.id, litSceneTarget.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlDisableFramebuffer();

    rlUnloadTexture(resolveTarget.texture.id);
    resolveTarget.texture.id = rlLoadTexture(nullptr, renderWidth, renderHeight, targetFormat, 1);
    resolveTarget.texture.format = targetFormat;
    rlEnableFramebuffer(resolveTarget.id);
    rlFramebufferAttach(resolveTarget.id, resolveTarget.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlDisableFramebuffer();
}

// CLEANUP FUNCTION
// -------------------------------------------------------------------------------------------------

void RenderContext::Destroy() const {
    // Unload Materials
    UnloadMaterial(instancedMaterial);
    UnloadMaterial(instancedLightMaterial);
    UnloadMaterial(lightVolumeMaterial);
    UnloadMaterial(fwdBlinnMaterial);
    UnloadMaterial(fwdGoochMaterial);
    UnloadMaterial(fwdToonMaterial);
    UnloadMaterial(fwdOutlineMaterial);
    UnloadMaterial(fwdFloorMaterial);
    UnloadMaterial(fwdLightProxyMaterial);

    // Unload Custom VBOs
    rlUnloadVertexBuffer(styleIdVboId);
    rlUnloadVertexBuffer(lightStyleIdVboId);

    // Unload FBOs and Textures
    rlUnloadFramebuffer(FboId);
    rlUnloadTexture(albedoTexId);
    rlUnloadTexture(normalTexId);
    rlUnloadTexture(depthTexId);
    UnloadRenderTexture(litSceneTarget);
    UnloadRenderTexture(resolveTarget);

    // Unload Shaders
    UnloadShader(geometryPassShader);
    UnloadShader(instancedShader);
    UnloadShader(lightingPassShader);
    UnloadShader(lightVolumeShader);
    UnloadShader(nprResolveShader);
    UnloadShader(forwardBlinnShader);
    UnloadShader(forwardGoochShader);
    UnloadShader(forwardToonShader);
    UnloadShader(forwardOutlineShader);
    UnloadShader(forwardUnlitShader);
    UnloadShader(postShader);

    // Standard unloads
    for (auto &mesh : lodMeshes)
        UnloadMesh(mesh);
    UnloadTexture(obstacleTexture);
    UnloadTexture(floorTexture);
    UnloadModel(lightningSourceModel);
    UnloadModel(floorModel);
}