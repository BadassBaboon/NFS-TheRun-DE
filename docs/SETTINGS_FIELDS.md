# Settings field maps

Extracted from the game's own reflection data in `Need For Speed The Run.exe`.

Each settings class has a `?c_TypeInfoData@<Class>@fb@@` record in the `typeinfo`
segment holding the class name, a field count and a pointer to a field array. Each
field entry is 12 bytes: name pointer, flags, offset, type pointer.

The stored field count is unreliable for some classes, so each array is additionally
bounded by where the next class's array begins. Without that bound a class reads its
neighbour's fields, which is why an earlier version of this file listed 53 fields for
almost everything and showed duplicate offsets.

Types: `02A95E38` bool, `02A95E88`/`02A95E98` int32, `02A95EC8` float,
`02760A68` vec2, `02760AA0`/`02760AD0` vec4.


### VisualTerrainSettings  (53 fields)

  0x014  RenderMode                           type_02AAF710
  0x020  TriangleSizeMin                      type_02A95EC8
  0x024  PatchFov                             type_02A95EC8
  0x028  LodCenterExtrapolationDistanceMax    type_02A95EC8
  0x02C  LodCenterExtrapolationTime           type_02A95EC8
  0x030  TextureSkipMipSpeed                  type_02A95EC8
  0x038  DxTessellatedTriWidth                type_02A95EC8
  0x03C  DxTessellationPatchShrink            type_02A95EC8
  0x040  DxTessellationPatchFacesPerSide      type_02A95E88
  0x04C  TextureAtlasSampleCountXFactor       type_02A95E88
  0x050  TextureAtlasSampleCountYFactor       type_02A95E88
  0x054  TextureSamplesPerMeterMax            type_02A95EC8
  0x058  TextureDetailFalloffFactor           type_02A95EC8
  0x05C  TextureDetailFalloffDistance         type_02A95EC8
  0x060  TextureDetailFalloffCurve            type_02A95EC8
  0x064  TextureInvisibleDetailReductionFactor type_02A95EC8
  0x068  TextureOccludedDetailReductionFactor type_02A95EC8
  0x06C  TextureRenderJobCount                type_02A95E88
  0x078  TextureRenderJobsLaunchedPerFrameCountMax type_02A95E88
  0x07C  TextureTileSamplesPerSide            type_02A95E88
  0x080  TextureTileBorderWidth               type_02A95E88
  0x084  TextureLevelOffset                   type_02A95E98
  0x088  TextureClodFrameCount                type_02A95E88
  0x090  TextureClodCutoffPriority            type_02A95EC8
  0x098  TextureCompressJobCount              type_02A95E88
  0x0A0  TextureCompressionQuality            type_02A95E98
  0x0A4  TextureDetailSlopeBoost              type_02A95EC8
  0x0A8  TextureGenerationMipBias             type_02A95EC8
  0x0CC  TextureQuadsPerTileLevel             type_02A95E88
  0x116  PrioritizationSpuJobEnable           type_02A95E38
  0x11A  DxDisplacementMappingEnable          type_02A95E38
  0x11C  DetailOverlayEnable                  type_02A95E38
  0x11F  TextureClodEnable                    type_02A95E38
  0x122  DrawTextureDebugDepthComplexity      type_02A95E38
  0x123  DrawEnable                           type_02A95E38
  0x124  DrawPatchesEnable                    type_02A95E38
  0x12A  PrioritizationOcclusionEnable        type_02A95E38
  0x130  GpuTextureCompressionEnable          type_02A95E38
  0x131  TextureBlockOnStreamingEnable        type_02A95E38
  0x134  VertexBufferHeightsEnable            type_02A95E38
  0x13F  TextureVtIndirectionJobEnable        type_02A95E38
  0x141  TextureKeepPoolFullEnable            type_02A95E38
  0x14C  TextureLayerCullingEnable            type_02A95E38
  0x14D  TextureDrawTerrainLayersEnable       type_02A95E38
  0x14E  TextureForceUpdateEnable             type_02A95E38
  0x150  TextureCompressSpuJobsEnable         type_02A95E38
  0x151  DrawTextureDebugColors               type_02A95E38
  0x154  TextureCompressFastAlgorithmEnable   type_02A95E38
  0x157  WireframeEnable                      type_02A95E38
  0x15D  TextureVtIndirectionSpuJobEnable     type_02A95E38
  0x15E  Enable                               type_02A95E38
  0x15F  DrawVertexYTextureEnable             type_02A95E38
  0x16C  EditServiceEnable                    type_02A95E38

### TextureSettings  (11 fields)

  0x000  ShaderRenderPathCount                type_00000007
  0x000  ShaderRenderPath_Dx10                ?
  0x000  ShaderRenderPath_Dx10Plus            type_00000001
  0x000  ShaderRenderPath_Dx10_1              type_00000002
  0x000  ShaderRenderPath_Dx11                type_00000003
  0x000  ShaderRenderPath_Gl                  type_00000006
  0x000  ShaderRenderPath_Ps3                 type_00000005
  0x000  ShaderRenderPath_Xenon               type_00000004
  0x00C  SkipMipmapCount                      type_02A95E88
  0x010  LoadingEnabled                       type_02A95E38
  0x011  RenderTexturesEnabled                type_02A95E38

### TextureStreamingSettings  (49 fields)

  0x00C  OnDemandPoolSize                     type_02A95E88
  0x010  PoolSize                             type_02A95E98
  0x014  PoolHeadroomSize                     type_02A95E88
  0x018  DrawDebugOutsourceYellow             type_02A95E88
  0x01C  ListViewPageIndex                    type_02A95E88
  0x020  MaxTextureSizeKb                     type_02A95E88
  0x024  MaxFrameTextureCreateCount           type_02A95E88
  0x028  MaxPendingLoadCount                  type_02A95E88
  0x02C  MaxFrameTextureCreateSize            type_02A95E88
  0x030  MaxMipmapCount                       type_02A95E88
  0x034  MipmapBias                           type_02A95EC8
  0x038  XenonFinalPoolSizeAdjustment         type_02A95E88
  0x03C  XenonRetailPoolSizeAdjustment        type_02A95E88
  0x040  PriorityThreshold                    type_02A95EC8
  0x044  ForceMipmap                          type_02A95E98
  0x048  MinMipmapCount                       type_02A95E88
  0x04C  DrawDebugOutsourceRed                type_02A95E88
  0x050  FadeMipmapTime                       type_02A95EC8
  0x054  MinTextureSize                       type_02A95E88
  0x058  DefragFrameTransferLimit             type_02A95E88
  0x05C  LoadMipmapsEnable                    type_02A95E38
  0x05D  MipmapsEnable                        type_02A95E38
  0x05E  UpdateEnable                         type_02A95E38
  0x05F  TextureUpdateEnable                  type_02A95E38
  0x060  DxImmutableUsageEnable               type_02A95E38
  0x061  UploadMipmapsEnable                  type_02A95E38
  0x062  ForceWantedEnable                    type_02A95E38
  0x063  PoolEnable                           type_02A95E38
  0x064  DefragEnable                         type_02A95E38
  0x065  DefragTransfersEnable                type_02A95E38
  0x066  AsyncCreatesEnable                   type_02A95E38
  0x067  InstantUnloadingEnable               type_02A95E38
  0x068  FadeMipmapsEnable                    type_02A95E38
  0x069  OnlyWantedInPool                     type_02A95E38
  0x06A  DynamicLoadingEnable                 type_02A95E38
  0x06B  DrawStatsEnable                      type_02A95E38
  0x06C  DrawTextureGroupStatsEnable          type_02A95E38
  0x06D  DrawTextureFormatStatsEnable         type_02A95E38
  0x06E  IncludeOnDemandInTextureFormatStats  type_02A95E38
  0x06F  DrawLoadingListEnable                type_02A95E38
  0x070  DrawPriorityListEnable               type_02A95E38
  0x071  DebugDumpPool                        type_02A95E38
  0x072  DumpPoolOnForceLoadNotFitting        type_02A95E38
  0x073  DrawDebugOutsourceInfo               type_02A95E38
  0x074  ChunkLoadEnable                      type_02A95E38
  0x075  PushBasedLoadingEnable               type_02A95E38
  0x076  Enable                               type_02A95E38
  0x077  DumpLoadedList                       type_02A95E38
  0x078  DumpLoadedListOnTooManyTextures      type_02A95E38

### MeshSettings  (6 fields)

  0x000  InvalidMeshHandle                    ?
  0x00C  OverrideShadersShaderName            type_02A95F18
  0x010  OverrideShadersMeshName              type_02A95F18
  0x014  ForceLod                             type_02A95E98
  0x018  GlobalLodScale                       type_02A95EC8
  0x01C  LoadingEnabled                       type_02A95E38

### ShaderSystemSettings  (53 fields)

  0x010  DebugNonFiniteColor                  type_02760AA0
  0x024  FrameMemoryBufferSize                type_02A95E88
  0x07C  OverdrawMaxLayerCount                type_02A95E88
  0x080  DrawCallMultiplier                   type_02A95E88
  0x084  ZOnlyMaxAnisotropy                   type_02A95E88
  0x088  XenonTrilinearThreshold              type_02A95E88
  0x094  MaxAnisotropy                        type_02A95E88
  0x09C  MipmapBias                           type_02A95EC8
  0x0A4  DxMaxInstructionCount                type_02A95E88
  0x0AC  StencilEnable                        type_02A95E38
  0x0AD  ClipPlanesEnable                     type_02A95E38
  0x0AE  ZeroViewportEnable                   type_02A95E38
  0x0AF  PixBlockEventsEnable                 type_02A95E38
  0x0B0  DatabaseLoadingEnable                type_02A95E38
  0x0B1  DepthWriteEnable                     type_02A95E38
  0x0B2  DepthTestEnable                      type_02A95E38
  0x0B3  SinglePrimitiveEnable                type_02A95E38
  0x0B4  DepthEnable                          type_02A95E38
  0x0B5  ForceDoubleSided                     type_02A95E38
  0x0B6  LogEnable                            type_02A95E38
  0x0B7  SingleFrameLogEnable                 type_02A95E38
  0x0B8  SingleFrameLogOverwrite              type_02A95E38
  0x0B9  TileClassificationEnable             type_02A95E38
  0x0BA  ForcePointFiltering                  type_02A95E38
  0x0BB  DrawCallEnable                       type_02A95E38
  0x0BC  AlphaBlendEnable                     type_02A95E38
  0x0C4  ShaderPixScopeEnable                 type_02A95E38
  0x0C5  GcmReplayMarkersEnable               type_02A95E38
  0x0C6  DrawNonStreamedTextureBlocks         type_02A95E38
  0x0C7  SortBlocksEnable                     type_02A95E38
  0x0C8  DrawInlineBlocks                     type_02A95E38
  0x0C9  SimpleTexturesEnable                 type_02A95E38
  0x0CB  OnDemandPrimingEnable                type_02A95E38
  0x0CC  FlushEnable                          type_02A95E38
  0x0CD  OnDemandMonitoringEnable             type_02A95E38
  0x0CE  SimpleTextureFilteringEnable         type_02A95E38
  0x0CF  DispatchDirectEnable                 type_02A95E38
  0x0D0  OnDemandBuildingEnable               type_02A95E38
  0x0D1  DumpAllocatorContents                type_02A95E38
  0x0D9  SingleFrameBlockLogEnable            type_02A95E38
  0x0DA  DrawNonInstancedBlocks               type_02A95E38
  0x0EF  DrawTileClassifiedBlocks             type_02A95E38
  0x0F2  DrawInstancedBlocks                  type_02A95E38
  0x0F9  DrawStreamedTextureBlocks            type_02A95E38
  0x0FF  DrawTransparent                      type_02A95E38
  0x107  DrawTransparentDecal                 type_02A95E38
  0x108  DrawOpaqueAlphaTestDecal             type_02A95E38
  0x109  DrawOpaque                           type_02A95E38
  0x10A  DrawOpaqueAlphaTestSimple            type_02A95E38
  0x10D  DrawOpaqueAlphaTest                  type_02A95E38
  0x10E  DrawAdvancedStats                    type_02A95E38
  0x10F  DrawZOnly                            type_02A95E38
  0x111  DrawStats                            type_02A95E38

### DxDisplaySettings  (31 fields)

  0x010  NvidiaMinDriverVersion               type_02A95E88
  0x014  FullscreenOutputIndex                type_02A95E98
  0x018  FullscreenWidth                      type_02A95E88
  0x01C  AmdMinDriverVersion                  type_02A95F18
  0x020  FullscreenHeight                     type_02A95E88
  0x024  DebugBreakIgnoredIDs                 type_02760C94
  0x028  RenderAheadLimit                     type_02A95E88
  0x02C  FullscreenRefreshRate                type_02A95E88
  0x030  PresentInterval                      type_02A95E98
  0x034  CreateMinimalWindow                  type_02A95E38
  0x035  Dx10PlusEnable                       type_02A95E38
  0x036  Dx11Enable                           type_02A95E38
  0x037  DebugBreakOnInfoEnable               type_02A95E38
  0x038  Dx10Dot0Enable                       type_02A95E38
  0x039  Dx10Dot1Enable                       type_02A95E38
  0x03A  DriverInternalThreadingEnable        type_02A95E38
  0x03B  RefDriverEnable                      type_02A95E38
  0x03C  WarpDriverEnable                     type_02A95E38
  0x03D  DebugBreakOnErrorEnable              type_02A95E38
  0x03E  DebugInfoEnable                      type_02A95E38
  0x03F  DebugBreakOnWarningEnable            type_02A95E38
  0x040  VSyncEnable                          type_02A95E38
  0x041  TripleBufferingEnable                type_02A95E38
  0x042  FullscreenModeEnable                 type_02A95E38
  0x043  NvApiEnable                          type_02A95E38
  0x044  NvPerfHudEnable                      type_02A95E38
  0x045  NvStereoEnable                       type_02A95E38
  0x046  MinDriverRequired                    type_02A95E38
  0x047  NullDriverEnable                     type_02A95E38
  0x048  Fullscreen                           type_02A95E38
  0x049  MultiGpuValidationEnable             type_02A95E38

### PhysicsSettings  (40 fields)

  0x000  Aabb                                 type_02760C30
  0x000  RBTypeCharacter                      type_00000002
  0x000  RBTypeCollision                      ?
  0x000  RBTypeDetail                         type_00000001
  0x000  RBTypeGroup                          type_00000004
  0x000  RBTypeRaycast                        type_00000003
  0x000  RBTypeSize                           type_00000005
  0x000  RigidBodyMotionType_Dynamic          type_00000003
  0x000  RigidBodyMotionType_Fixed            type_00000001
  0x000  RigidBodyMotionType_Invalid          ?
  0x000  RigidBodyMotionType_Keyframed        type_00000002
  0x000  ShapeType_Box                        type_00000001
  0x000  ShapeType_Capsule                    type_00000005
  0x000  ShapeType_Cylinder                   type_00000004
  0x000  ShapeType_Hull                       ?
  0x000  ShapeType_Mesh                       type_00000006
  0x000  ShapeType_OBB                        type_00000002
  0x000  ShapeType_Sphere                     type_00000003
  0x000  ShapeType_Unknown                    type_00000007
  0x00C  IntegrateJobCount                    type_02A95E88
  0x010  ClientEffectWorldThreadCount         type_02A95E88
  0x014  ClientWorldThreadCount               type_02A95E88
  0x018  ServerWorldThreadCount               type_02A95E88
  0x01C  CollideJobCount                      type_02A95E88
  0x020  Enable                               type_02A95E38
  0x020  Translation                          type_02760AA0
  0x021  EnableAIRigidBody                    type_02A95E38
  0x022  ForestEnable                         type_02A95E38
  0x023  EnableJobs                           type_02A95E38
  0x024  RemoveRagdollWhenWoken               type_02A95E38
  0x025  RemoveFromWorldOnCollisionOverflow   type_02A95E38
  0x026  SingleStepCharacter                  type_02A95E38
  0x027  ForceSingleStepCharacterInSP         type_02A95E38
  0x028  EnableFollowWheelRaycasts            type_02A95E38
  0x029  EnableClientWheelRaycasts            type_02A95E38
  0x02A  EnableASyncWheelRaycasts             type_02A95E38
  0x02B  UseDelayedWakeUpClient               type_02A95E38
  0x02C  UseDelayedWakeUpServer               type_02A95E38
  0x02D  SuppressDebrisSpawnUntilReady        type_02A95E38
  0x02E  HeightfieldRSXStreaming              type_02A95E38

### AudioSettings  (25 fields)

  0x000  AudioCurveType_Spline                ?
  0x000  FiveDotOne                           type_00000006
  0x000  IsValid                              type_02A95E38
  0x000  SevenDotOne                          type_00000008
  0x000  Value                                type_02A95EC8
  0x000  Value                                type_02A95EC8
  0x000  X                                    type_02A95EC8
  0x001  VoiceIndex                           type_02A95E58
  0x002  PluginIndex                          type_02A95E58
  0x004  Index                                type_02A95E58
  0x004  Index                                type_02A95E58
  0x004  Y                                    type_02A95EC8
  0x008  K                                    type_02A95EC8
  0x00C  WaveCacheSize                        type_02A95E88
  0x010  WaveCacheHeadroom                    type_02A95E88
  0x014  WaveCachePruneTimeLimit              type_02A95EC8
  0x018  WaveCacheRsxSize                     type_02A95E88
  0x01C  WaveCacheRsxPruneTimeLimit           type_02A95EC8
  0x020  AudioCoreCpuLoadLimit                type_02A95EC8
  0x024  AudioCoreCpuLoadRecovery             type_02A95EC8
  0x028  AudioCoreThread                      type_02A95E98
  0x02C  AudioCoreMixJobThreadCount           type_02A95E98
  0x030  AudioCoreMaxMixJobThreadCount        type_02A95E98
  0x034  AudioCoreMinProcStageVoicesToGoWide  type_02A95E98
  0x038  AudioCoreMultipleMixJobsEnabled      type_02A95E38

### SoundSettings  (8 fields)

  0x010  VOEnglish                            type_02A95F18
  0x014  AudioSystemUri                       type_02A95F18
  0x018  VOCommon                             type_02A95F18
  0x01C  VOItalian                            type_02A95F18
  0x020  VOSpanish                            type_02A95F18
  0x024  VOFrench                             type_02A95F18
  0x028  VOGerman                             type_02A95F18
  0x02C  Enable                               type_02A95E38

### GameSettings  (42 fields)

  0x010  MaxPlayerCount                       type_02A95E88
  0x014  MaxSpectatorCount                    type_02A95E88
  0x018  Version                              type_02AC049C
  0x01C  Level                                type_02A95F18
  0x020  DynamicSubLevels                     type_02A95F18
  0x024  DefaultTerrainMeshPoolOffset         type_02A95E88
  0x028  DefaultTerrainPs3CellMeshPoolOffset  type_02A95E88
  0x02C  DefaultLayerInclusion                type_02A95F18
  0x030  InputConfiguration                   type_02AB5978
  0x034  DefaultTeamId                        type_02AB4C6C
  0x038  LevelWarmUpTime                      type_02A95EC8
  0x03C  TimeToWaitForQuitTaskCompletion      type_02A95EC8
  0x040  Platform                             type_02AB5474
  0x044  LayerInclusionTable                  type_02A95744
  0x048  PS3ContentRatingAge                  type_02A95E88
  0x04C  MetadataContainers                   type_02897F40
  0x050  TimeBeforeSpawnIsAllowed             type_02A95EC8
  0x054  DifficultySettings                   type_02AB4E94
  0x058  LogHistory                           type_02A95E88
  0x05C  DifficultyIndex                      type_02A95E88
  0x060  Player                               type_02AB88A4
  0x064  UIGraphContainers                    type_02902074
  0x068  SoldierWeaponSwitching               type_02ABA6F0
  0x06C  AutoAimEnabled                       type_02A95E38
  0x06D  RotateLogs                           type_02A95E38
  0x06E  EnableLoadingProfile                 type_02A95E38
  0x06F  AdjustVehicleCenterOfMass            type_02A95E38
  0x070  LogFileEnable                        type_02A95E38
  0x071  UseSpeedBasedDetailedCollision       type_02A95E38
  0x072  HasUnlimitedAmmo                     type_02A95E38
  0x073  HasUnlimitedMags                     type_02A95E38
  0x074  ResourceRefreshAlwaysAllowed         type_02A95E38
  0x075  UseSingleWeaponSelector              type_02A95E38
  0x076  AimAssistEnabled                     type_02A95E38
  0x077  AimAssistUsePolynomials              type_02A95E38
  0x078  ForceFreeStreaming                   type_02A95E38
  0x079  IsGodMode                            type_02A95E38
  0x07A  IsJesusMode                          type_02A95E38
  0x07B  IsJesusModeAi                        type_02A95E38
  0x07C  GameAdministrationEnabled            type_02A95E38
  0x07D  AllowDestructionOutsideCombatArea    type_02A95E38
  0x07E  DisableDestructionAndDamage          type_02A95E38

### EmitterSystemSettings  (44 fields)

  0x00C  MeshDrawCountLimit                   type_02A95E88
  0x010  TimeScale                            type_02A95EC8
  0x014  GlobalResetStartTimeInterval         type_02A95EC8
  0x018  QuadHalfResSlice2ThresholdLow        type_02A95EC8
  0x01C  QuadHalfResSlice2ThresholdHigh       type_02A95EC8
  0x020  QuadHalfResSlice1ThresholdLow        type_02A95EC8
  0x024  QuadHalfResSlice1ThresholdHigh       type_02A95EC8
  0x028  DebugOverdrawCount                   type_02A95E98
  0x02C  DrawStats                            type_02A95E88
  0x030  MeshCullingDistance                  type_02A95EC8
  0x034  QuadZOcclusionBias                   type_02A95EC8
  0x038  DrawBoundingBoxes                    type_02A95E88
  0x03C  MinScreenArea                        type_02A95EC8
  0x040  MeshStreamingPriorityMultiplier      type_02A95EC8
  0x044  QuadTechnique                        type_02A95E98
  0x048  ZBufferCullEnable                    type_02A95E38
  0x049  QuadSimpleRenderingEnable            type_02A95E38
  0x04A  QuadEnableOpaque                     type_02A95E38
  0x04B  QuadEnableOverdrawRendering          type_02A95E38
  0x04C  DrawProjectedBoxes                   type_02A95E38
  0x04D  EnableFixedTimeStep                  type_02A95E38
  0x04E  EnableJobs                           type_02A95E38
  0x04F  QuadEnableZOcclusion                 type_02A95E38
  0x050  EnableRendering                      type_02A95E38
  0x051  QuadHalfResEnable                    type_02A95E38
  0x052  QuadEnableRendering                  type_02A95E38
  0x053  EnableFixedDelta                     type_02A95E38
  0x054  QuadClipScaleEnable                  type_02A95E38
  0x055  QuadNiceRenderingEnable              type_02A95E38
  0x056  QuadGroupsJoinAll                    type_02A95E38
  0x057  QuadGroupsJoinNone                   type_02A95E38
  0x058  QuadGroupsJoinNiceAndSimple          type_02A95E38
  0x059  QuadColorShaderCostsEnable           type_02A95E38
  0x05A  QuadVertexShadowsEnable              type_02A95E38
  0x05B  QuadPointLightsEnable                type_02A95E38
  0x05C  QuadSpotLightsEnable                 type_02A95E38
  0x05D  MeshRenderingEnable                  type_02A95E38
  0x05E  MeshDrawTransforms                   type_02A95E38
  0x05F  MeshDrawBoundingBoxes                type_02A95E38
  0x060  MeshShadowEnable                     type_02A95E38
  0x061  QuadEnableSorting                    type_02A95E38
  0x062  Enable                               type_02A95E38
  0x063  QuadEnableWireframe                  type_02A95E38
  0x064  MeshDrawCullStats                    type_02A95E38

### GlobalPostProcessSettings  (53 fields)

  0x020  ForceBloomScale                      type_02760AA0
  0x030  ForceVignetteColor                   type_02760AD0
  0x070  FilmGrainColorScale                  type_02760AA0
  0x090  FilmGrainTextureScale                type_02760A68
  0x098  ForceVignetteScale                   type_02760A68
  0x0B0  LdrBloomRange                        type_02A95EC8
  0x0B4  BloomBlurFilter                      type_02AA275C
  0x0B8  ForceChromostereopsisScale           type_02A95EC8
  0x0BC  BloomBlurIterationCount              type_02A95E88
  0x0C0  ForceMiddleGray                      type_02A95EC8
  0x0C4  DebugMode                            type_02AA268C
  0x0C8  BloomPyramidLevelCount               type_02A95E88
  0x0CC  BloomPyramidFinalLevel               type_02A95E88
  0x0DC  DownsampleAverageStartMipmap         type_02A95E88
  0x0F0  Ps3TiledLdrMipmapCount               type_02A95E88
  0x0F4  Ps3TiledFloatMipmapCount             type_02A95E88
  0x0F8  Ps3TiledBloomMipmapCount             type_02A95E88
  0x100  ForceExposure                        type_02A95EC8
  0x104  ForceBlurAdd                         type_02A95EC8
  0x108  ForceDofEnable                       type_02A95E98
  0x10C  ForceDofFocusDistance                type_02A95EC8
  0x110  ForceDofNearDistanceScale            type_02A95EC8
  0x114  ForceDofFarDistanceScale             type_02A95EC8
  0x118  ForceDofScale                        type_02A95EC8
  0x11C  ForceDofBlurFilterDeviation          type_02A95EC8
  0x120  DebugBlurStep                        type_02A95E88
  0x12C  ForceVignetteExponent                type_02A95EC8
  0x134  TonemapMethod                        type_02AA269C
  0x13C  ForceChromostereopsisEnable          type_02A95E98
  0x140  ForceChromostereopsisOffset          type_02A95E98
  0x164  BloomBlurEnable                      type_02A95E38
  0x165  ColorGradingEnable                   type_02A95E38
  0x166  BloomPyramidEnable                   type_02A95E38
  0x168  BloomEnable                          type_02A95E38
  0x169  HdrBlurEnable                        type_02A95E38
  0x16A  Ps3BackBufferAsLdrTargetEnable       type_02A95E38
  0x16F  HdrBloomEnable                       type_02A95E38
  0x170  Ps3CompressedRenderTargetsEnable     type_02A95E38
  0x171  DownsampleAverageFromBloomEnable     type_02A95E38
  0x172  BloomQuarterResEnable                type_02A95E38
  0x173  BloomPyramidAttenuateEnable          type_02A95E38
  0x174  FilmGrainEnable                      type_02A95E38
  0x175  BlurBlendEnable                      type_02A95E38
  0x177  QuarterDownsamplingEnable            type_02A95E38
  0x178  Ps3GBuffer0AsLdr2TargetEnable        type_02A95E38
  0x179  FilmGrainLinearFilteringEnable       type_02A95E38
  0x17C  DrawDebugInfo                        type_02A95E38
  0x181  DownsampleBeforeBlurEnable           type_02A95E38
  0x188  PostTonemapBlurEnable                type_02A95E38
  0x189  Ldr16BitBloomEnable                  type_02A95E38
  0x18A  ExposureClampEnable                  type_02A95E38
  0x18B  DownsampleLogAverageEnable           type_02A95E38
  0x18C  DirectExposureEnable                 type_02A95E38

### WorldRenderSettings  (53 fields)

  0x020  ShadowMinScreenArea                  type_02A95EC8
  0x024  ViewportScale                        type_02A95EC8
  0x028  ShadowViewportScale                  type_02A95EC8
  0x030  CullScreenAreaScale                  type_02A95EC8
  0x034  ShadowmapMinFov                      type_02A95EC8
  0x040  ShadowmapSizeZScale                  type_02A95EC8
  0x044  ShadowmapResolution                  type_02A95E88
  0x048  ShadowmapQuality                     type_02A95E88
  0x04C  ShadowmapSliceCount                  type_02A95E88
  0x050  ShadowmapSliceSchemeWeight           type_02A95EC8
  0x054  ShadowmapFirstSliceScale             type_02A95EC8
  0x058  ShadowmapViewDistance                type_02A95EC8
  0x05C  ShadowmapExtrusionLength             type_02A95EC8
  0x060  ShadowmapMinScreenArea               type_02A95EC8
  0x090  VinylTargetSize                      type_02A95E88
  0x098  MotionBlurScale                      type_02A95EC8
  0x09C  MotionBlurNoiseScale                 type_02A95EC8
  0x0A0  MotionBlurQuality                    type_02A95E88
  0x0AC  MotionBlurMaxSampleCount             type_02A95E88
  0x0B0  MotionBlurFrameAverageCount          type_02A95E88
  0x0B4  MotionBlurMaxFrameTime               type_02A95EC8
  0x0B8  MultisampleCount                     type_02A95E88
  0x0C4  OnlyShadowmapSlice                   type_02A95E98
  0x0C8  ViewMode                             type_02AE6E78
  0x0DC  XenonHdrColorScale                   type_02A95EC8
  0x1A9  DrawTransparent                      type_02A95E38
  0x1AE  MotionBlurEnable                     type_02A95E38
  0x1B5  ShadowmapQuarterDownsampleEnable     type_02A95E38
  0x1CD  TransparencyShadowmapsEnable         type_02A95E38
  0x1EA  Enable                               type_02A95E38
  0x1EF  Ps3TilingEnable                      type_02A95E38
  0x1F0  Ps3ZCullEnable                       type_02A95E38
  0x1F1  Ps3HdrClearEnable                    type_02A95E38
  0x1F2  XenonFloatDepthBufferEnable          type_02A95E38
  0x1F3  ConsoleRenderTargetPoolSharingEnable type_02A95E38
  0x1F4  XenonFastHdrEnable                   type_02A95E38
  0x1F5  HdrEnable                            type_02A95E38
  0x1F6  DxLinearDepth32BitFormatEnable       type_02A95E38
  0x1F7  DrawTransparentDecal                 type_02A95E38
  0x1F9  MotionBlurStencilPassEnable          type_02A95E38
  0x1FC  MotionBlurGeometryPassEnable         type_02A95E38
  0x20A  SimpleShadowmapsEnable               type_02A95E38
  0x20B  ColoredShadowmapSlicesEnable         type_02A95E38
  0x20C  ApplyShadowmapsEnable                type_02A95E38
  0x20D  GenerateShadowmapsEnable             type_02A95E38
  0x20E  DxShadowmap16BitEnable               type_02A95E38
  0x20F  DxSpotLightShadowmap16BitEnable      type_02A95E38
  0x211  ShadowmapsEnable                     type_02A95E38
  0x21A  ShadowmapFixedMovementEnable         type_02A95E38
  0x21E  Ps3SpotLightShadowmap16BitEnable     type_02A95E38
  0x21F  Ps3Shadowmap16BitEnable              type_02A95E38
  0x229  Ps3ShadowmapTilingEnable             type_02A95E38
  0x22C  ShadowmapFixedDepthEnable            type_02A95E38

### VisualEnvironmentSettings  (3 fields)

  0x00C  SunRotationX                         type_02A95EC8
  0x010  SunRotationY                         type_02A95EC8
  0x014  DrawStats                            type_02A95E38

### DebrisSystemSettings  (22 fields)

  0x000  AngularVelocity                      type_02760AA0
  0x00C  MeshDrawCountLimit                   type_02A95E88
  0x010  LinearVelocity                       type_02760AA0
  0x010  TimeScale                            type_02A95EC8
  0x014  MeshCullingDistance                  type_02A95EC8
  0x018  DrawStats                            type_02A95E88
  0x01C  MeshStreamingPriorityMultiplier      type_02A95EC8
  0x020  EnableJobs                           type_02A95E38
  0x020  NumberOfChildren                     type_02A95E98
  0x021  MeshRenderingEnable                  type_02A95E38
  0x022  MeshDrawBoundingBoxes                type_02A95E38
  0x023  MeshShadowEnable                     type_02A95E38
  0x024  MeshViewCullingEnable                type_02A95E38
  0x024  PartIndex                            type_02A95E98
  0x025  MeshHavokRenderingEnable             type_02A95E38
  0x026  Enable                               type_02A95E38
  0x027  MeshDrawTransforms                   type_02A95E38
  0x028  MeshDrawCullStats                    type_02A95E38
  0x028  SplitSpeedThreshold                  type_02A95EC8
  0x02C  InEffectWorldOnly                    type_02A95E38
  0x02D  SyncRestPosition                     type_02A95E38
  0x02E  SyncContinous                        type_02A95E38

### VegetationSystemSettings  (21 fields)

  0x00C  WindVariation                        type_02A95EC8
  0x010  WindVariationRate                    type_02A95EC8
  0x014  WindStrength                         type_02A95EC8
  0x018  JointTensionLimit                    type_02A95EC8
  0x01C  ForceShadowLod                       type_02A95E98
  0x020  ShadowDistanceOffset                 type_02A95EC8
  0x024  MaxActiveDistance                    type_02A95EC8
  0x028  SimulationMemKbServer                type_02A95E88
  0x02C  SimulationMemKbClient                type_02A95E88
  0x030  JointTensionLimitIndex               type_02A95E98
  0x034  TimeScale                            type_02A95EC8
  0x038  JobCount                             type_02A95E88
  0x03C  LocalInfluencesEnabled               type_02A95E38
  0x03D  Enable                               type_02A95E38
  0x03E  SimulateServerSide                   type_02A95E38
  0x03F  EnableJobs                           type_02A95E38
  0x040  DestructionEnabled                   type_02A95E38
  0x041  DrawNodes                            type_02A95E38
  0x042  MeshRenderingEnable                  type_02A95E38
  0x043  DissolveEnable                       type_02A95E38
  0x044  ShadowMeshEnable                     type_02A95E38

### EnlightenRuntimeSettings  (39 fields)

  0x010  AlbedoDefaultColor                   type_02760AA0
  0x020  LocalLightForceRadius                type_02A95EC8
  0x024  DrawDebugSystemDependenciesEnable    type_02A95E98
  0x028  TemporalCoherenceThreshold           type_02A95EC8
  0x02C  SkyBoxScale                          type_02A95EC8
  0x030  MinSystemUpdateCount                 type_02A95E88
  0x034  JobCount                             type_02A95E88
  0x038  DrawDebugSystemBoundingBoxEnable     type_02A95E98
  0x03C  LightProbeMaxUpdateSolveCount        type_02A95E88
  0x040  DrawDebugLightProbeSize              type_02A95EC8
  0x044  CompensateSunShadowHeightScale       type_02A95E38
  0x045  SaveRadiosityTexturesEnable          type_02A95E38
  0x046  ShadowsEnable                        type_02A95E38
  0x047  LightMapsEnable                      type_02A95E38
  0x048  LocalLightsEnable                    type_02A95E38
  0x049  LocalLightCullingEnable              type_02A95E38
  0x04A  LocalLightCustumFalloff              type_02A95E38
  0x04B  LightProbeForceUpdate                type_02A95E38
  0x04C  DrawDebugEntities                    type_02A95E38
  0x04D  ForceDynamic                         type_02A95E38
  0x04E  LightProbeEnable                     type_02A95E38
  0x04F  LightProbeJobsEnable                 type_02A95E38
  0x050  DrawDebugLightProbes                 type_02A95E38
  0x051  DrawDebugLightProbeOcclusion         type_02A95E38
  0x052  DrawDebugLightProbeStats             type_02A95E38
  0x053  DrawDebugLightProbeBoundingBoxes     type_02A95E38
  0x054  Enable                               type_02A95E38
  0x055  DrawDebugDefaultLightProbe           type_02A95E38
  0x056  DrawSolveTaskPerformance             type_02A95E38
  0x057  DrawDebugColoringEnable              type_02A95E38
  0x058  DrawDebugTextures                    type_02A95E38
  0x059  DrawDebugBackFaces                   type_02A95E38
  0x05A  DrawDebugTargetMeshes                type_02A95E38
  0x05B  DrawWarningsEnable                   type_02A95E38
  0x05C  AlbedoForceUpdateEnable              type_02A95E38
  0x05D  AlbedoForceColorEnable               type_02A95E38
  0x05E  DrawDebugSystemsEnable               type_02A95E38
  0x05F  TerrainMapEnable                     type_02A95E38
  0x060  EmissiveEnable                       type_02A95E38

### DecalSettings  (14 fields)

  0x000  TileIndexX                           type_02A95EC8
  0x004  TileIndexY                           type_02A95EC8
  0x008  TileCountX                           type_02A95EC8
  0x00C  TileCountY                           type_02A95EC8
  0x010  FlipX                                type_02A95E38
  0x010  StaticBufferMaxVertexCount           type_02A95E88
  0x011  FlipY                                type_02A95E38
  0x014  RingBufferMaxVertexCount             type_02A95E88
  0x018  SystemEnable                         type_02A95E38
  0x019  DrawEnable                           type_02A95E38
  0x01A  Enable                               type_02A95E38
  0x01B  DebugMemUsageEnable                  type_02A95E38
  0x01C  DebugWarningsEnable                  type_02A95E38
  0x01D  NvidiaStreamOutputWorkaroundEnable   type_02A95E38

### EffectManagerSettings  (2 fields)

  0x010  MaxNewEffectsPerFrameCount           type_02A95E88
  0x014  SizeToGrowNewEffectsContainer        type_02A95E88

### NfsGameSettings  (33 fields)

  0x000  CountryCode                          type_02A95F18
  0x000  Region                               type_02A95F18
  0x004  AgeLevel5                            type_02A95E98
  0x004  MinimumAge                           type_02A95E98
  0x008  AgeLevel6                            type_02A95E98
  0x00C  AgeLevel7                            type_02A95E98
  0x010  AgeLevel8                            type_02A95E98
  0x010  FELevel                              type_02A95F18
  0x014  AgeLevel9                            type_02A95E98
  0x014  MetagameConfig                       type_02AD66E4
  0x018  AgeLevel10                           type_02A95E98
  0x018  ProfileBadgeInfo                     type_02AD6F6C
  0x01C  HurryTimer                           type_02A95EC8
  0x020  ActivityTimer                        type_02A95EC8
  0x024  ResultsTimer                         type_02A95EC8
  0x028  RewardsTimer                         type_02A95EC8
  0x02C  SessionSummaryTimer                  type_02A95EC8
  0x030  VotingTimer                          type_02A95EC8
  0x034  WaitForVotesTimer                    type_02A95EC8
  0x038  PostVotingTimer                      type_02A95EC8
  0x03C  WaitingForPlayersTimer               type_02A95EC8
  0x040  PreLoadTimer                         type_02A95EC8
  0x044  IntermissionTimer                    type_02A95EC8
  0x048  CarSelectTimer                       type_02A95EC8
  0x04C  SpawnTimeOutTimer                    type_02A95EC8
  0x050  PlaygroupDisconnectFadeTimer         type_02A95EC8
  0x054  GameplayReadyTimeOutTimer            type_02A95EC8
  0x058  PreRaceTimeOutTimer                  type_02A95EC8
  0x05C  CareerStateTimeOutTimer              type_02A95EC8
  0x060  SkillUpdateTimer                     type_02A95EC8
  0x064  SkillLevelCloseFinishTimeDiff        type_02A95EC8
  0x068  DefaultSkillLevel                    type_02A95E88
  0x06C  IsDemo                               type_02A95E38
