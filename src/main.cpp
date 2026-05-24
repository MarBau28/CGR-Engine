// INCLUDES/IMPORTS
#include "../include/BenchmarkOrchestrator.h"
#include "../include/CameraController.h"
#include "../include/Config.h"
#include "../include/EngineState.h"
#include "../include/InputController.h"
#include "../include/Profilers.h"
#include "../include/RenderContext.h"
#include "../include/SceneManager.h"
#include "../include/Telemetry.h"
#include "../include/TelemetryDashboard.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <string>
#include <vector>

int main() {
    // SYSTEM INSTANTIATION
    // ---------------------------------------------------------------------------------------------

    // Core engine subsystems
    EngineState engineState;
    SceneManager sceneManager;
    CameraController cameraController;
    InputController inputController;

    // Hardware profilers and telemetry writers
    CpuProfiler cpuProfiler;
    GpuProfiler geomProfiler;
    GpuProfiler lightProfiler;
    CsvTelemetryWriter telemetryWriter;

    // Orchestrator governs deterministic state during active test suites
    BenchmarkOrchestrator benchController(telemetryWriter, cpuProfiler, geomProfiler, lightProfiler,
                                          cameraController);

    // Context & Window Creation
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
    InitWindow(Config::EngineSettings::ScreenWidth, Config::EngineSettings::ScreenHeight, "HyDra");
    rlSetClipPlanes(Config::EngineSettings::CameraNearPlane,
                    Config::EngineSettings::CameraFarPlane);

    // Hardware Profilers Init
    geomProfiler.Init();
    lightProfiler.Init();

    // RENDER PIPELINE SETUP
    // ---------------------------------------------------------------------------------------------

    // Bootstrap initial scene data so the RenderContext has master data to load into its VBOs
    sceneManager.RebuildScene(engineState);

    // Centralized repository for all GPU handles (Shaders, Materials, FBOs, Textures)
    RenderContext ctx;
    ctx.Initialize(engineState, sceneManager, engineState.renderWidth, engineState.renderHeight);

    Vector3 lightColor = {Config::EngineSettings::MainLightColor.x / 255.0f,
                          Config::EngineSettings::MainLightColor.y / 255.0f,
                          Config::EngineSettings::MainLightColor.z / 255.0f};

    // MAIN GAME LOOP
    // ---------------------------------------------------------------------------------------------

    while (!WindowShouldClose()) {
        // Screen Resolution setup
        const Rectangle screenDestRec = {0.0f, 0.0f, static_cast<float>(GetScreenWidth()),
                                         static_cast<float>(GetScreenHeight())};
        float internalRes[2]          = {static_cast<float>(engineState.renderWidth),
                                         static_cast<float>(engineState.renderHeight)};
        const Rectangle sourceRec     = {0.0f, 0.0f, static_cast<float>(engineState.renderWidth),
                                         -static_cast<float>(engineState.renderHeight)};
        const Rectangle fboDestRec    = {0.0f, 0.0f, static_cast<float>(engineState.renderWidth),
                                         static_cast<float>(engineState.renderHeight)};

        // INPUT & STATE MANAGEMENT
        // -----------------------------------------------------------------------------------------

        // Pass input authority to the controller and receive structural event flags
        auto [triggerBenchmarkStart, triggerSceneRebuild, triggerHdrFboRebuild,
              triggerResolutionRebuild] =
            inputController.ProcessInputs(engineState, cameraController, benchController.IsActive(),
                                          sceneManager.GetActualGeneratedLights(),
                                          static_cast<int>(ctx.lodMeshes.size()));

        Camera3D &camera = cameraController.GetCamera();

        // Benchmark Boot Sequence
        if (triggerBenchmarkStart) {
            if (telemetryWriter.Initialize("hydra_benchmark_results.csv")) {
                benchController.Start(BenchmarkSuite::SuiteA_Geom);
            }
        }

        // State Interception: Prioritize benchmark dictates over manual user input
        if (benchController.IsActive()) {
            engineState = benchController.GetCurrentState();
            if (benchController.DidStateChangeThisFrame()) {
                sceneManager.RebuildScene(engineState);
                rlUpdateVertexBuffer(
                    ctx.styleIdVboId, sceneManager.GetMasterStyleIds().data(),
                    static_cast<int>(sceneManager.GetMasterStyleIds().size() * sizeof(float)), 0);
            }
        } else {
            // Process manually triggered structural rebuilds (e.g., toggling NPR room or clustered
            // styles)
            if (triggerSceneRebuild) {
                sceneManager.RebuildScene(engineState);
                rlUpdateVertexBuffer(
                    ctx.styleIdVboId, sceneManager.GetMasterStyleIds().data(),
                    static_cast<int>(sceneManager.GetMasterStyleIds().size() * sizeof(float)), 0);
            }
            // Execute heavy FBO reallocation if user toggles 8-bit/16-bit HDR
            if (triggerHdrFboRebuild) {
                ctx.RebuildHDRTargets(engineState.use16BitHDR, engineState.renderWidth,
                                      engineState.renderHeight);
            }
            if (triggerResolutionRebuild) {
                ctx.ResizeTargets(engineState.renderWidth, engineState.renderHeight,
                                  engineState.use16BitHDR);
            }
        }

        // Guarantee the physical GPU texture memory matches the engine state
        if (engineState.renderWidth != ctx.gAlbedo.width ||
            engineState.renderHeight != ctx.gAlbedo.height) {
            ctx.ResizeTargets(engineState.renderWidth, engineState.renderHeight,
                              engineState.use16BitHDR);
        }

        // CPU CULLING & DATA PREP
        // -----------------------------------------------------------------------------------------

        cpuProfiler.Begin();

        // Extract frustum planes and populate visibility arrays to avoid drawing occluded objects
        sceneManager.UpdateVisibility(cameraController, engineState);

        int actualVisibleCount =
            static_cast<int>(sceneManager.GetVisibleObstacleTransforms().size());
        int actualVisibleVolumeCount =
            static_cast<int>(sceneManager.GetVisibleLightVolumeTransforms().size());
        int actualVisibleProxyCount =
            static_cast<int>(sceneManager.GetVisibleLightProxyTransforms().size());

        // Stream only the visible subset of Style IDs to the GPU
        if (actualVisibleCount > 0) {
            rlUpdateVertexBuffer(ctx.styleIdVboId, sceneManager.GetVisibleObstacleStyleIds().data(),
                                 actualVisibleCount * static_cast<int>(sizeof(float)), 0);
        }

        // Dynamically calculate the maximum radius of lights before they attenuate to sub-visible
        // thresholds
        float c = Config::EngineSettings::AttenuationConstant -
                  (engineState.lightIntensity / Config::EngineSettings::MinLightThreshold);
        float dynamicMaxRadius = 0.0f;
        if (engineState.lightIntensity > 0.0f) {
            dynamicMaxRadius =
                (-Config::EngineSettings::AttenuationLinear +
                 std::sqrt(Config::EngineSettings::AttenuationLinear *
                               Config::EngineSettings::AttenuationLinear -
                           (4.0f * Config::EngineSettings::AttenuationQuadratic * c))) /
                (2.0f * Config::EngineSettings::AttenuationQuadratic);
        }

        double aspect = static_cast<double>(engineState.renderWidth) /
                        static_cast<double>(engineState.renderHeight);
        Matrix view        = GetCameraMatrix(camera);
        Matrix proj        = MatrixPerspective(camera.fovy * DEG2RAD, aspect,
                                               Config::EngineSettings::CameraNearPlane,
                                               Config::EngineSettings::CameraFarPlane);
        Matrix viewProj    = MatrixMultiply(view, proj);
        Matrix invViewProj = MatrixInvert(viewProj);

        // Fetch current active geometry based on LOD state
        Mesh &currentMesh   = ctx.lodMeshes[engineState.currentLodIndex];
        int activeTriangles = currentMesh.triangleCount * actualVisibleCount;

        // RENDER EXECUTION
        // -----------------------------------------------------------------------------------------

        BeginDrawing();
        {
            // GEOMETRY PASS (G-BUFFER)
            // -------------------------------------------------------------------------------------

            if (engineState.activeRenderPath == RenderPath::DeferredUber ||
                engineState.activeRenderPath == RenderPath::DeferredVolume) {
                rlDrawRenderBatchActive();
                rlEnableFramebuffer(ctx.FboId);
                rlViewport(0, 0, engineState.renderWidth, engineState.renderHeight);
                rlActiveDrawBuffers(2); // Albedo (Color0) + Normals (Color1)

                ClearBackground(BLANK);
                rlDisableColorBlend(); // Prevent alpha-blending artifacts in deep buffer data

                rlDrawRenderBatchActive();
                geomProfiler.Begin();

                BeginMode3D(camera);
                {
                    rlSetMatrixProjection(proj);

                    // conditionally draw floor
                    if (!engineState.useNprRoom && engineState.renderFloor) {
                        SetShaderValueMatrix(ctx.geometryPassShader, ctx.modelMatLoc,
                                             MatrixIdentity());
                        SetShaderValueMatrix(ctx.geometryPassShader, ctx.normalMatLoc,
                                             MatrixIdentity());
                        float generalStyleId = 1;
                        SetShaderValue(ctx.geometryPassShader, ctx.styleIdLoc, &generalStyleId,
                                       SHADER_UNIFORM_FLOAT);
                        DrawModel(ctx.floorModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
                    }

                    // Render Light Proxies
                    int isLight = 1;
                    SetShaderValue(ctx.instancedShader, ctx.isLightLocInstanced, &isLight,
                                   SHADER_UNIFORM_INT);
                    if (actualVisibleProxyCount > 0) {
                        DrawMeshInstanced(ctx.lightningSphereMesh, ctx.instancedLightMaterial,
                                          sceneManager.GetVisibleLightProxyTransforms().data(),
                                          std::min(actualVisibleProxyCount, 500));
                    }

                    // Render Standard Instances
                    isLight = 0;
                    SetShaderValue(ctx.instancedShader, ctx.isLightLocInstanced, &isLight,
                                   SHADER_UNIFORM_INT);
                    if (actualVisibleCount > 0) {
                        DrawMeshInstanced(currentMesh, ctx.instancedMaterial,
                                          sceneManager.GetVisibleObstacleTransforms().data(),
                                          actualVisibleCount);
                    }
                }
                EndMode3D();

                rlDrawRenderBatchActive();
                GpuProfiler::End();

                rlEnableColorBlend();
                rlDisableFramebuffer();
                rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
            } else {
                geomProfiler.elapsedMs = 0.0; // Geometry cost is unified into forward pass
            }

            // LIGHTING & RESOLVE PASSES
            // -------------------------------------------------------------------------------------

            int outInt   = engineState.enableOutlines ? 1 : 0;
            int kuwInt   = engineState.enableKuwahara ? 1 : 0;
            int goochInt = engineState.enableGooch ? 1 : 0;
            int toonInt  = engineState.enableToon ? 1 : 0;

            // Pipeline Branch A: Deferred Shading (Single Uber-Shader)
            if (engineState.activeRenderPath == RenderPath::DeferredUber) {
                BeginTextureMode(ctx.litSceneTarget);
                {
                    ClearBackground(BLANK);

                    rlDrawRenderBatchActive();
                    lightProfiler.Begin();

                    BeginShaderMode(ctx.lightingPassShader);
                    {
                        // Inject environment factors
                        SetShaderValue(ctx.lightingPassShader, ctx.intensityLoc,
                                       &engineState.lightIntensity, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(ctx.lightingPassShader, ctx.ambientLoc,
                                       &engineState.ambientLightStrength, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(ctx.lightingPassShader, ctx.maxLightRadiusLoc,
                                       &dynamicMaxRadius, SHADER_UNIFORM_FLOAT);

                        // Inject light array and position data
                        int safeVisibleLightCount = std::min(actualVisibleVolumeCount, 500);
                        SetShaderValue(ctx.lightingPassShader, ctx.LightingResolutionLoc,
                                       internalRes, SHADER_UNIFORM_VEC2);
                        SetShaderValue(ctx.lightingPassShader, ctx.activeLightsLoc,
                                       &safeVisibleLightCount, SHADER_UNIFORM_INT);
                        SetShaderValueV(ctx.lightingPassShader, ctx.lightPosArrayLoc,
                                        sceneManager.GetVisibleLightPositions().data(),
                                        SHADER_UNIFORM_VEC3, safeVisibleLightCount);
                        SetShaderValue(ctx.lightingPassShader, ctx.lightColorLoc, &lightColor,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValue(ctx.lightingPassShader, ctx.postViewPosLoc, &camera.position,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValueMatrix(ctx.lightingPassShader, ctx.invViewProjLoc,
                                             invViewProj);

                        // Bind G-Buffer outputs as shader inputs
                        SetShaderValueTexture(ctx.lightingPassShader, ctx.normalTexLoc,
                                              ctx.gNormal);
                        SetShaderValueTexture(ctx.lightingPassShader, ctx.depthTexLoc, ctx.gDepth);

                        // NPR State Injections
                        SetShaderValue(ctx.lightingPassShader, ctx.uberEnableOutlinesLoc, &outInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.lightingPassShader, ctx.uberEnableKuwaharaLoc, &kuwInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.lightingPassShader, ctx.uberEnableGoochLoc, &goochInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.lightingPassShader, ctx.uberEnableToonLoc, &toonInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.lightingPassShader, ctx.uberKuwaharaRadiusLoc,
                                       &engineState.kuwaharaRadius, SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.lightingPassShader, ctx.uberKuwaharaIntensityLoc,
                                       &engineState.kuwaharaIntensity, SHADER_UNIFORM_FLOAT);

                        // Execute screen-space quad
                        DrawTexturePro(ctx.gAlbedo, sourceRec, fboDestRec, {0, 0}, 0.0f, WHITE);
                    }
                    EndShaderMode();

                    rlDrawRenderBatchActive();
                    GpuProfiler::End();
                }
                EndTextureMode();

                // Pipeline Branch B: Deferred Shading (Volumetric Light Rendering)
            } else if (engineState.activeRenderPath == RenderPath::DeferredVolume) {
                // Pass 1: Accumulate Lighting
                BeginTextureMode(ctx.litSceneTarget);
                {
                    ClearBackground(BLANK);

                    rlDrawRenderBatchActive();
                    lightProfiler.Begin();

                    BeginBlendMode(BLEND_ADDITIVE); // Accumulate overlapping light volumes
                    {
                        rlDisableDepthMask(); // Prevent light spheres from occluding each other

                        BeginMode3D(camera);
                        {
                            BeginShaderMode(ctx.lightVolumeShader);
                            {
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvIntensityLoc,
                                               &engineState.lightIntensity, SHADER_UNIFORM_FLOAT);
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvMaxRadiusLoc,
                                               &dynamicMaxRadius, SHADER_UNIFORM_FLOAT);
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvResolutionLoc,
                                               internalRes, SHADER_UNIFORM_VEC2);
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvViewPosLoc,
                                               &camera.position, SHADER_UNIFORM_VEC3);
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvLightColorLoc,
                                               &lightColor, SHADER_UNIFORM_VEC3);
                                SetShaderValueMatrix(ctx.lightVolumeShader, ctx.lvInvViewProjLoc,
                                                     invViewProj);
                                SetShaderValueTexture(ctx.lightVolumeShader, ctx.lvAlbedoTexLoc,
                                                      ctx.gAlbedo);
                                SetShaderValueTexture(ctx.lightVolumeShader, ctx.lvNormalTexLoc,
                                                      ctx.gNormal);
                                SetShaderValueTexture(ctx.lightVolumeShader, ctx.lvDepthTexLoc,
                                                      ctx.gDepth);
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvEnableGoochLoc,
                                               &goochInt, SHADER_UNIFORM_INT);
                                SetShaderValue(ctx.lightVolumeShader, ctx.lvEnableToonLoc, &toonInt,
                                               SHADER_UNIFORM_INT);

                                if (actualVisibleVolumeCount > 0) {
                                    DrawMeshInstanced(
                                        ctx.lightningSphereMesh, ctx.lightVolumeMaterial,
                                        sceneManager.GetVisibleLightVolumeTransforms().data(),
                                        actualVisibleVolumeCount);
                                }
                            }
                            EndShaderMode();
                        }
                        EndMode3D();
                        rlDrawRenderBatchActive();

                        rlEnableDepthMask();
                    }
                    EndBlendMode();
                }
                EndTextureMode();

                // Pass 2: NPR Resolve (Combine lighting buffer with NPR effects)
                BeginTextureMode(ctx.resolveTarget);
                {
                    ClearBackground(BLANK);
                    BeginShaderMode(ctx.nprResolveShader);
                    {
                        SetShaderValue(ctx.nprResolveShader, ctx.nprAmbientLoc,
                                       &engineState.ambientLightStrength, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(ctx.nprResolveShader, ctx.nprResolutionLoc, internalRes,
                                       SHADER_UNIFORM_VEC2);
                        SetShaderValue(ctx.nprResolveShader, ctx.nprViewPosLoc, &camera.position,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValueMatrix(ctx.nprResolveShader, ctx.nprInvViewProjLoc,
                                             invViewProj);

                        SetShaderValueTexture(ctx.nprResolveShader, ctx.nprLitSceneTexLoc,
                                              ctx.litSceneTarget.texture);
                        SetShaderValueTexture(ctx.nprResolveShader, ctx.nprAlbedoTexLoc,
                                              ctx.gAlbedo);
                        SetShaderValueTexture(ctx.nprResolveShader, ctx.nprNormalTexLoc,
                                              ctx.gNormal);
                        SetShaderValueTexture(ctx.nprResolveShader, ctx.nprDepthTexLoc, ctx.gDepth);

                        SetShaderValue(ctx.nprResolveShader, ctx.resEnableOutlinesLoc, &outInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.nprResolveShader, ctx.resEnableKuwaharaLoc, &kuwInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.nprResolveShader, ctx.resEnableGoochLoc, &goochInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.nprResolveShader, ctx.resEnableToonLoc, &toonInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.nprResolveShader, ctx.resKuwaharaRadiusLoc,
                                       &engineState.kuwaharaRadius, SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.nprResolveShader, ctx.resKuwaharaIntensityLoc,
                                       &engineState.kuwaharaIntensity, SHADER_UNIFORM_FLOAT);

                        DrawTexturePro(ctx.gAlbedo, sourceRec, fboDestRec, {0, 0}, 0.0f, WHITE);
                    }
                    EndShaderMode();

                    rlDrawRenderBatchActive();
                    GpuProfiler::End();
                }
                EndTextureMode();

                // Pipeline Branch C: forward Shading (Baseline Evaluation)
            } else if (engineState.activeRenderPath == RenderPath::Forward) {
                rlEnableDepthTest();
                rlEnableDepthMask();
                rlEnableBackfaceCulling();
                rlSetCullFace(RL_CULL_FACE_BACK);

                rlEnableFramebuffer(ctx.litSceneTarget.id);
                rlViewport(0, 0, engineState.renderWidth, engineState.renderHeight);
                rlActiveDrawBuffers(1);

                Color bgCol = {
                    static_cast<unsigned char>(Config::EngineSettings::BackgroundColor.x),
                    static_cast<unsigned char>(Config::EngineSettings::BackgroundColor.y),
                    static_cast<unsigned char>(Config::EngineSettings::BackgroundColor.z), 255};
                ClearBackground(bgCol);

                rlDrawRenderBatchActive();
                lightProfiler
                    .Begin(); // Tracks both Geometry & Lighting simultaneously in forward mode

                BeginMode3D(camera);
                {
                    rlSetMatrixProjection(proj);

                    int safeVisibleLightCount = std::min(actualVisibleVolumeCount, 500);
                    int blinnCount = static_cast<int>(sceneManager.GetFwdTransformsBlinn().size());
                    int goochCount = static_cast<int>(sceneManager.GetFwdTransformsGooch().size());
                    int toonCount  = static_cast<int>(sceneManager.GetFwdTransformsToon().size());
                    int outlineCount =
                        static_cast<int>(sceneManager.GetFwdTransformsOutline().size());

                    if (actualVisibleProxyCount > 0) {
                        BeginShaderMode(ctx.forwardUnlitShader);
                        {
                            DrawMeshInstanced(ctx.lightningSphereMesh, ctx.fwdLightProxyMaterial,
                                              sceneManager.GetVisibleLightProxyTransforms().data(),
                                              std::min(actualVisibleProxyCount, 500));
                        }
                        EndShaderMode();
                    }

                    // Blinn-Phong Bin (Default)
                    BeginShaderMode(ctx.forwardBlinnShader);
                    {
                        SetShaderValue(ctx.forwardBlinnShader, ctx.fwdBlinnViewPosLoc,
                                       &camera.position, SHADER_UNIFORM_VEC3);
                        SetShaderValue(ctx.forwardBlinnShader, ctx.fwdBlinnIntensityLoc,
                                       &engineState.lightIntensity, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(ctx.forwardBlinnShader, ctx.fwdBlinnAmbientLoc,
                                       &engineState.ambientLightStrength, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(ctx.forwardBlinnShader, ctx.fwdBlinnMaxRadiusLoc,
                                       &dynamicMaxRadius, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(ctx.forwardBlinnShader, ctx.fwdBlinnActiveLightLoc,
                                       &safeVisibleLightCount, SHADER_UNIFORM_INT);
                        SetShaderValue(ctx.forwardBlinnShader, ctx.fwdBlinnLightColorLoc,
                                       &lightColor, SHADER_UNIFORM_VEC3);

                        if (safeVisibleLightCount > 0) {
                            SetShaderValueV(ctx.forwardBlinnShader, ctx.fwdBlinnLightPosLoc,
                                            sceneManager.GetVisibleLightPositions().data(),
                                            SHADER_UNIFORM_VEC3, safeVisibleLightCount);
                        }

                        if (!engineState.useNprRoom && engineState.renderFloor) {
                            Matrix floorIdentity = MatrixIdentity();
                            DrawMeshInstanced(ctx.floorMesh, ctx.fwdFloorMaterial, &floorIdentity,
                                              1);
                        }

                        if (blinnCount > 0) {
                            DrawMeshInstanced(currentMesh, ctx.fwdBlinnMaterial,
                                              sceneManager.GetFwdTransformsBlinn().data(),
                                              blinnCount);
                        }
                    }
                    EndShaderMode();

                    // Gooch Shading Bin
                    if (goochCount > 0) {
                        BeginShaderMode(ctx.forwardGoochShader);
                        {
                            SetShaderValue(ctx.forwardGoochShader, ctx.fwdGoochViewPosLoc,
                                           &camera.position, SHADER_UNIFORM_VEC3);
                            SetShaderValue(ctx.forwardGoochShader, ctx.fwdGoochIntensityLoc,
                                           &engineState.lightIntensity, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(ctx.forwardGoochShader, ctx.fwdGoochMaxRadiusLoc,
                                           &dynamicMaxRadius, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(ctx.forwardGoochShader, ctx.fwdGoochAmbientLoc,
                                           &engineState.ambientLightStrength, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(ctx.forwardGoochShader, ctx.fwdGoochActiveLightLoc,
                                           &safeVisibleLightCount, SHADER_UNIFORM_INT);

                            if (safeVisibleLightCount > 0) {
                                SetShaderValueV(ctx.forwardGoochShader, ctx.fwdGoochLightPosLoc,
                                                sceneManager.GetVisibleLightPositions().data(),
                                                SHADER_UNIFORM_VEC3, safeVisibleLightCount);
                            }

                            DrawMeshInstanced(currentMesh, ctx.fwdGoochMaterial,
                                              sceneManager.GetFwdTransformsGooch().data(),
                                              goochCount);
                        }
                        EndShaderMode();
                    }

                    // Toon Shading Bin
                    if (toonCount > 0) {
                        BeginShaderMode(ctx.forwardToonShader);
                        {
                            SetShaderValue(ctx.forwardToonShader, ctx.fwdToonViewPosLoc,
                                           &camera.position, SHADER_UNIFORM_VEC3);
                            SetShaderValue(ctx.forwardToonShader, ctx.fwdToonIntensityLoc,
                                           &engineState.lightIntensity, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(ctx.forwardToonShader, ctx.fwdToonAmbientLoc,
                                           &engineState.ambientLightStrength, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(ctx.forwardToonShader, ctx.fwdToonMaxRadiusLoc,
                                           &dynamicMaxRadius, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(ctx.forwardToonShader, ctx.fwdToonActiveLightLoc,
                                           &safeVisibleLightCount, SHADER_UNIFORM_INT);
                            SetShaderValue(ctx.forwardToonShader, ctx.fwdToonLightColorLoc,
                                           &lightColor, SHADER_UNIFORM_VEC3);

                            if (safeVisibleLightCount > 0) {
                                SetShaderValueV(ctx.forwardToonShader, ctx.fwdToonLightPosLoc,
                                                sceneManager.GetVisibleLightPositions().data(),
                                                SHADER_UNIFORM_VEC3, safeVisibleLightCount);
                            }

                            DrawMeshInstanced(currentMesh, ctx.fwdToonMaterial,
                                              sceneManager.GetFwdTransformsToon().data(),
                                              toonCount);
                        }
                        EndShaderMode();

                        // Inverted Hull Outline Pass
                        if (engineState.enableOutlines && outlineCount > 0) {
                            BeginShaderMode(ctx.forwardOutlineShader);
                            {
                                SetShaderValue(ctx.forwardOutlineShader, ctx.fwdOutlineViewPosLoc,
                                               &camera.position, SHADER_UNIFORM_VEC3);
                                rlDisableBackfaceCulling(); // Required to draw the inverted
                                                            // back-faces
                                DrawMeshInstanced(currentMesh, ctx.fwdOutlineMaterial,
                                                  sceneManager.GetFwdTransformsOutline().data(),
                                                  outlineCount);
                                rlEnableBackfaceCulling();
                            }
                            EndShaderMode();
                        }
                    }
                }
                EndMode3D();

                rlDrawRenderBatchActive();
                GpuProfiler::End();

                rlDisableFramebuffer();
                rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
            }

            // POST-PROCESSING PASS
            // -------------------------------------------------------------------------------------

            ClearBackground(BLACK);

            // Project the accumulated lighting/NPR FBO texture to the screen quad
            BeginShaderMode(ctx.postShader);
            {
                SetShaderValue(ctx.postShader, ctx.postResolutionLoc, internalRes,
                               SHADER_UNIFORM_VEC2);

                Texture2D finalRender = (engineState.activeRenderPath == RenderPath::DeferredVolume)
                                            ? ctx.resolveTarget.texture
                                            : ctx.litSceneTarget.texture;
                DrawTexturePro(finalRender, sourceRec, screenDestRec, {0, 0}, 0.0f, WHITE);
            }
            EndShaderMode();

            // UI & DASHBOARD OVERLAY
            // -------------------------------------------------------------------------------------

            TelemetryDashboard::Draw(engineState, cpuProfiler, geomProfiler, lightProfiler,
                                     cameraController, sceneManager, currentMesh.triangleCount);
        }
        EndDrawing();

        // PROFILING & BENCHMARKING
        // -----------------------------------------------------------------------------------------

        // Poll GPU Queries safely
        geomProfiler.Update();
        lightProfiler.Update();
        cpuProfiler.End();

        if (benchController.IsActive()) {
            // Note: Theoretical overdraw is managed by the orchestrator/scene manager logically.
            double currentOverdraw = 0.0;
            benchController.UpdateAndRecord(GetFrameTime() * 1000.0, activeTriangles,
                                            currentOverdraw);
        }
    }

    // CLEANUP
    // ---------------------------------------------------------------------------------------------

    geomProfiler.Destroy();
    lightProfiler.Destroy();
    ctx.Destroy();
    CloseWindow();

    return 0;
}