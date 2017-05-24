# For this script to work, you must first install the FBX SKD Python bindings
# Make sure your python version is supported by the bindings as well

import sys

def DisplayMetaData(pScene):
    sceneInfo = pScene.GetSceneInfo()
    if sceneInfo:
        print("\n\n--------------------\nMeta-Data\n--------------------\n")
        print("    Title: %s" % sceneInfo.mTitle.Buffer())
        print("    Subject: %s" % sceneInfo.mSubject.Buffer())
        print("    Author: %s" % sceneInfo.mAuthor.Buffer())
        print("    Keywords: %s" % sceneInfo.mKeywords.Buffer())
        print("    Revision: %s" % sceneInfo.mRevision.Buffer())
        print("    Comment: %s" % sceneInfo.mComment.Buffer())

        thumbnail = sceneInfo.GetSceneThumbnail()
        if thumbnail:
            print("    Thumbnail:")

            if thumbnail.GetDataFormat() == FbxThumbnail.eRGB_24 :
                print("        Format: RGB")
            elif thumbnail.GetDataFormat() == FbxThumbnail.eRGBA_32:
                print("        Format: RGBA")

            if thumbnail.GetSize() == FbxThumbnail.eNOT_SET:
                print("        Size: no dimensions specified (%ld bytes)", thumbnail.GetSizeInBytes())
            elif thumbnail.GetSize() == FbxThumbnail.e64x64:
                print("        Size: 64 x 64 pixels (%ld bytes)", thumbnail.GetSizeInBytes())
            elif thumbnail.GetSize() == FbxThumbnail.e128x128:
                print("        Size: 128 x 128 pixels (%ld bytes)", thumbnail.GetSizeInBytes())

def DisplayGlobalLightSettings(pScene):
    lGlobalLightSettings = pScene.GlobalLightSettings()
    DisplayColor("Ambient Color: ", lGlobalLightSettings.GetAmbientColor())
    DisplayBool("Fog Enabled: ", lGlobalLightSettings.GetFogEnable())
    DisplayColor("Fog Color: ", lGlobalLightSettings.GetFogColor())
    
    lFogMode = lGlobalLightSettings.GetFogMode()
    
    if(lFogMode == FbxGlobalLightSettings.eLinear):
        DisplayString("Fog Mode: Linear")
    elif(lFogMode == FbxGlobalLightSettings.eExponential):
        DisplayString("Fog Mode: Exponential")
    elif(lFogMode == FbxGlobalLightSettings.eExponentialSquareRoot):
        DisplayString("Fog Mode: Exponential Square Root")
    else:
        DisplayString("Fog Mode: UNKNOWN")
        
    DisplayDouble("Fog Density: ", lGlobalLightSettings.GetFogDensity())
    DisplayDouble("Fog Start: ", lGlobalLightSettings.GetFogStart())
    DisplayDouble("Fog End: ", lGlobalLightSettings.GetFogEnd())
        
    DisplayBool("Shadow Enabled: ", lGlobalLightSettings.GetShadowEnable())
    DisplayDouble("Fog Density: ", lGlobalLightSettings.GetShadowIntensity())
    DisplayInt("Shadow Planes Count: ", lGlobalLightSettings.GetShadowPlaneCount())
    
    DisplayString("")

def DisplayGlobalCameraSettings(pScene):
    lGlobalSettings = pScene.GetGlobalSettings()
    DisplayString("Default Camera: ", lGlobalSettings.GetDefaultCamera().Buffer())
    DisplayString("")

def DisplayGlobalTimeSettings(pGlobalSettings):
    lTimeModes = [ "Default Mode", "Cinema", "PAL", "Frames 30", 
        "NTSC Drop Frame", "Frames 50", "Frames 60",
        "Frames 100", "Frames 120", "NTSC Full Frame", 
        "Frames 30 Drop", "Frames 1000" ] 

    DisplayString("Time Mode: ", lTimeModes[pGlobalSettings.GetTimeMode()])

    lTs = pGlobalSettings.GetTimelineDefaultTimeSpan()
    lStart = lTs.GetStart()
    lEnd   = lTs.GetStop()
    DisplayString("Timeline default timespan: ")
    lTmpStr=""
    DisplayString("     Start: ", lStart.GetTimeString(lTmpStr, 10))
    DisplayString("     Stop : ", lEnd.GetTimeString(lTmpStr, 10))

    DisplayString("")

def DisplayHierarchy(pScene):
    lRootNode = pScene.GetRootNode()

    for i in range(lRootNode.GetChildCount()):
        DisplayNodeHierarchy(lRootNode.GetChild(i), 0)

def DisplayNodeHierarchy(pNode, pDepth):
    lString = ""
    for i in range(pDepth):
        lString += "     "

    lString += pNode.GetName()

    print(lString)

    for i in range(pNode.GetChildCount()):
        DisplayNodeHierarchy(pNode.GetChild(i), pDepth + 1)

def DisplayContent(pScene):
    lNode = pScene.GetRootNode()

    if lNode:
        for i in range(lNode.GetChildCount()):
            DisplayNodeContent(lNode.GetChild(i))

def DisplayNodeContent(pNode):
    if pNode.GetNodeAttribute() == None:
        print("NULL Node Attribute\n")
    else:
        lAttributeType = (pNode.GetNodeAttribute().GetAttributeType())

        if lAttributeType == FbxNodeAttribute.eMarker:
            DisplayMarker(pNode)
        elif lAttributeType == FbxNodeAttribute.eSkeleton:
            DisplaySkeleton(pNode)
        elif lAttributeType == FbxNodeAttribute.eMesh:
            DisplayMesh(pNode)
        elif lAttributeType == FbxNodeAttribute.eNurbs:
            DisplayNurb(pNode)
        elif lAttributeType == FbxNodeAttribute.ePatch:
            DisplayPatch(pNode)
        elif lAttributeType == FbxNodeAttribute.eCamera:
            DisplayCamera(pNode)
        elif lAttributeType == FbxNodeAttribute.eLight:
            DisplayLight(pNode)

    DisplayUserProperties(pNode)
    DisplayTarget(pNode)
    DisplayPivotsAndLimits(pNode)
    DisplayTransformPropagation(pNode)
    DisplayGeometricTransform(pNode)

    for i in range(pNode.GetChildCount()):
        DisplayNodeContent(pNode.GetChild(i))

def DisplayUserProperties(pObject):
    lCount = 0
    lTitleStr = "    Property Count: "

    lProperty = pObject.GetFirstProperty()
    while lProperty.IsValid():
        if lProperty.GetFlag(FbxPropertyFlags.eUserDefined):
            lCount += 1

        lProperty = pObject.GetNextProperty(lProperty)

    if lCount == 0:
        return # there are no user properties to display

    DisplayInt(lTitleStr, lCount)

    lProperty = pObject.GetFirstProperty()
    i = 0
    while lProperty.IsValid():
        if lProperty.GetFlag(FbxPropertyFlags.eUserDefined):
            DisplayInt("        Property ", i)
            lString = lProperty.GetLabel()
            DisplayString("            Display Name: ", lString.Buffer())
            lString = lProperty.GetName()
            DisplayString("            Internal Name: ", lString.Buffer())
            DisplayString("            Type: ", lProperty.GetPropertyDataType().GetName())
            if lProperty.HasMinLimit():
                DisplayDouble("            Min Limit: ", lProperty.GetMinLimit())
            if lProperty.HasMaxLimit():
                DisplayDouble("            Max Limit: ", lProperty.GetMaxLimit())
            DisplayBool  ("            Is Animatable: ", lProperty.GetFlag(FbxPropertyFlags.eAnimatable))
            
            lPropertyDataType=lProperty.GetPropertyDataType()
            
            # BOOL
            if lPropertyDataType.GetType() == eFbxBool:
                lProperty = FbxPropertyBool1(lProperty)
                val = lProperty.Get()
                DisplayBool("            Default Value: ", val)
            # REAL
            elif lPropertyDataType.GetType() == eFbxDouble:
                lProperty = FbxPropertyDouble1(lProperty)
                val = lProperty.Get()
                DisplayDouble("            Default Value: ", val)        
            elif lPropertyDataType.GetType() == eFbxFloat:
                lProperty = FbxPropertyFloat1(lProperty)
                val = lProperty.Get()
                DisplayDouble("            Default Value: ",val)
            # COLOR
            #elif lPropertyDataType.Is(DTColor3) or lPropertyDataType.Is(DTColor4):
                #val = lProperty.Get()
                #lDefault=FbxGet <FbxColor> (lProperty)
                #sprintf(lBuf, "R=%f, G=%f, B=%f, A=%f", lDefault.mRed, lDefault.mGreen, lDefault.mBlue, lDefault.mAlpha)
                #DisplayString("            Default Value: ", lBuf)
            #    pass
            # INTEGER
            elif lPropertyDataType.GetType() == eFbxInt:
                lProperty = FbxPropertyInteger1(lProperty)
                val = lProperty.Get()
                DisplayInt("            Default Value: ", val)
            # VECTOR
            elif lPropertyDataType.GetType() == eFbxDouble3:
                lProperty = FbxPropertyDouble3(lProperty)
                val = lProperty.Get()
                lBuf = "X=%f, Y=%f, Z=%f", (val[0], val[1], val[2])
                DisplayString("            Default Value: ", lBuf)
            elif lPropertyDataType.GetType() == eFbxDouble4:
                lProperty = FbxPropertyDouble4(lProperty)
                val = lProperty.Get()
                lBuf = "X=%f, Y=%f, Z=%f, W=%f", (val[0], val[1], val[2], val[3])
                DisplayString("            Default Value: ", lBuf)
#            # LIST
#            elif lPropertyDataType.GetType() == eFbxEnum:
#                val = lProperty.Get()
#                DisplayInt("            Default Value: ", val)
            # UNIDENTIFIED
            else:
                DisplayString("            Default Value: UNIDENTIFIED")
            i += 1

        lProperty = pObject.GetNextProperty(lProperty)

def DisplayTarget(pNode):
    if pNode.GetTarget():
        DisplayString("    Target Name: ", pNode.GetTarget().GetName())

def DisplayPivotsAndLimits(pNode):
    # Pivots
    print("    Pivot Information")

    lPivotState = pNode.GetPivotState(FbxNode.eSourcePivot)
    if lPivotState == FbxNode.ePivotActive:
        print("        Pivot State: Active")
    else:
        print("        Pivot State: Reference")

    lTmpVector = pNode.GetPreRotation(FbxNode.eSourcePivot)
    print("        Pre-Rotation: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    lTmpVector = pNode.GetPostRotation(FbxNode.eSourcePivot)
    print("        Post-Rotation: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    lTmpVector = pNode.GetRotationPivot(FbxNode.eSourcePivot)
    print("        Rotation Pivot: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    lTmpVector = pNode.GetRotationOffset(FbxNode.eSourcePivot)
    print("        Rotation Offset: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    lTmpVector = pNode.GetScalingPivot(FbxNode.eSourcePivot)
    print("        Scaling Pivot: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    lTmpVector = pNode.GetScalingOffset(FbxNode.eSourcePivot)
    print("        Scaling Offset: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    print("    Limits Information")

    lIsActive = pNode.TranslationActive
    lMinXActive = pNode.TranslationMinX
    lMinYActive = pNode.TranslationMinY
    lMinZActive = pNode.TranslationMinZ
    lMaxXActive = pNode.TranslationMaxX
    lMaxYActive = pNode.TranslationMaxY
    lMaxZActive = pNode.TranslationMaxZ
    lMinValues = pNode.TranslationMin
    lMaxValues = pNode.TranslationMax

    if lIsActive:
        print("        Translation limits: Active")
    else:
        print("        Translation limits: Inactive")
    print("            X")
    if lMinXActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f" % lMinValues.Get()[0])
    if lMaxXActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[0])
    
    print("            Y")
    if lMinYActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f" % lMinValues.Get()[1])
    if lMaxYActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[1])
    
    print("            Z")
    if lMinZActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f"% lMinValues.Get()[2])
    if lMaxZActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[2])

    lIsActive = pNode.RotationActive
    lMinXActive = pNode.RotationMinX
    lMinYActive = pNode.RotationMinY
    lMinZActive = pNode.RotationMinZ
    lMaxXActive = pNode.RotationMaxX
    lMaxYActive = pNode.RotationMaxY
    lMaxZActive = pNode.RotationMaxZ
    lMinValues = pNode.RotationMin
    lMaxValues = pNode.RotationMax

    if lIsActive:
        print("        Rotation limits: Active")
    else:
        print("        Rotation limits: Inactive")    
    print("            X")
    if lMinXActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f" % lMinValues.Get()[0])
    if lMaxXActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[0])
    
    print("            Y")
    if lMinYActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f" % lMinValues.Get()[1])
    if lMaxYActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[1])
    
    print("            Z")
    if lMinZActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f"% lMinValues.Get()[2])
    if lMaxZActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[2])

    lIsActive = pNode.ScalingActive
    lMinXActive = pNode.ScalingMinX
    lMinYActive = pNode.ScalingMinY
    lMinZActive = pNode.ScalingMinZ
    lMaxXActive = pNode.ScalingMaxX
    lMaxYActive = pNode.ScalingMaxY
    lMaxZActive = pNode.ScalingMaxZ
    lMinValues = pNode.ScalingMin
    lMaxValues = pNode.ScalingMax

    if lIsActive:
        print("        Scaling limits: Active")
    else:
        print("        Scaling limits: Inactive")    
    print("            X")
    if lMinXActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f" % lMinValues.Get()[0])
    if lMaxXActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[0])
    
    print("            Y")
    if lMinYActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f" % lMinValues.Get()[1])
    if lMaxYActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[1])
    
    print("            Z")
    if lMinZActive:
        print("                Min Limit: Active")
    else:
        print("                Min Limit: Inactive")
    print("                Min Limit Value: %f"% lMinValues.Get()[2])
    if lMaxZActive:
        print("                Max Limit: Active")
    else:
        print("                Max Limit: Inactive")
    print("                Max Limit Value: %f" % lMaxValues.Get()[2])

def DisplayTransformPropagation(pNode):
    print("    Transformation Propagation")
    
    # Rotation Space
    lRotationOrder = pNode.GetRotationOrder(FbxNode.eSourcePivot)

    print("        Rotation Space:",)

    if lRotationOrder == eEulerXYZ:
        print("Euler XYZ")
    elif lRotationOrder == eEulerXZY:
        print("Euler XZY")
    elif lRotationOrder == eEulerYZX:
        print("Euler YZX")
    elif lRotationOrder == eEulerYXZ:
        print("Euler YXZ")
    elif lRotationOrder == eEulerZXY:
        print("Euler ZXY")
    elif lRotationOrder == eEulerZYX:
        print("Euler ZYX")
    elif lRotationOrder == eSphericXYZ:
        print("Spheric XYZ")
    
    # Use the Rotation space only for the limits
    # (keep using eEULER_XYZ for the rest)
    if pNode.GetUseRotationSpaceForLimitOnly(FbxNode.eSourcePivot):
        print("        Use the Rotation Space for Limit specification only: Yes")
    else:
        print("        Use the Rotation Space for Limit specification only: No")


    # Inherit Type
    lInheritType = pNode.GetTransformationInheritType()

    print("        Transformation Inheritance:",)

    if lInheritType == FbxTransform.eInheritRrSs:
        print("RrSs")
    elif lInheritType == FbxTransform.eInheritRSrs:
        print("RSrs")
    elif lInheritType == FbxTransform.eInheritRrs:
        print("Rrs")

def DisplayGeometricTransform(pNode):
    print("    Geometric Transformations")

    # Translation
    lTmpVector = pNode.GetGeometricTranslation(FbxNode.eSourcePivot)
    print("        Translation: %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    # Rotation
    lTmpVector = pNode.GetGeometricRotation(FbxNode.eSourcePivot)
    print("        Rotation:    %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

    # Scaling
    lTmpVector = pNode.GetGeometricScaling(FbxNode.eSourcePivot)
    print("        Scaling:     %f %f %f" % (lTmpVector[0], lTmpVector[1], lTmpVector[2]))

def DisplayMarker(pNode):
    lMarker = pNode.GetNodeAttribute()

    DisplayString("Marker Name: ", pNode.GetName())

    # Type
    lString = "    Marker Type: "
    if lMarker.GetType() == FbxMarker.eStandard:
        lString += "Standard"
    elif lMarker.GetType() == FbxMarker.eOptical:
         lString += "Optical"
    elif lMarker.GetType() == FbxMarker.eEffectorIK:
         lString += "IK Effector"
    elif lMarker.GetType() == FbxMarker.eEffectorFK:
         lString += "FK Effector"
    DisplayString(lString)

    # Look
    lString = "    Marker Look: "
    if lMarker.Look.Get() == FbxMarker.eCube:
        lString += "Cube"
    elif lMarker.Look.Get() == FbxMarker.eHardCross:
        lString += "Hard Cross"
    elif lMarker.Look.Get() == FbxMarker.eLightCross:
        lString += "Light Cross"
    elif lMarker.Look.Get() == FbxMarker.eSphere:
        lString += "Sphere"
    DisplayString(lString)

    # Size
    #lString = "    Size: "
    #lString += str(lMarker.Size.Get())
    DisplayDouble("    Size: ", lMarker.Size.Get())

    # Color
    c = lMarker.Color.Get()
    color = FbxColor(c[0], c[1], c[2])
    DisplayColor("    Color: ", color)

    # IKPivot
    Display3DVector("    IKPivot: ", lMarker.IKPivot.Get())

def DisplaySkeleton(pNode):
    lSkeleton = pNode.GetNodeAttribute()

    DisplayString("Skeleton Name: ", pNode.GetName())

    lSkeletonTypes = [ "Root", "Limb", "Limb Node", "Effector" ]

    DisplayString("    Type: ", lSkeletonTypes[lSkeleton.GetSkeletonType()])

    if lSkeleton.GetSkeletonType() == FbxSkeleton.eLimb:
        DisplayDouble("    Limb Length: ", lSkeleton.LimbLength.Get())
    elif lSkeleton.GetSkeletonType() == FbxSkeleton.eLimbNode:
        DisplayDouble("    Limb Node Size: ", lSkeleton.Size.Get())
    elif lSkeleton.GetSkeletonType() == FbxSkeleton.eRoot:
        DisplayDouble("    Limb Root Size: ", lSkeleton.Size.Get())

    DisplayColor("    Color: ", lSkeleton.GetLimbNodeColor())

def DisplayMesh(pNode):
    lMesh = pNode.GetNodeAttribute ()

    DisplayString("Mesh Name: ", pNode.GetName())
    DisplayControlsPoints(lMesh)
    DisplayPolygons(lMesh)
    DisplayMaterialMapping(lMesh)
    DisplayMaterial(lMesh)
    DisplayTexture(lMesh)
    DisplayMaterialConnections(lMesh)
    DisplayLink(lMesh)
    DisplayShape(lMesh)

def DisplayControlsPoints(pMesh):
    lControlPointsCount = pMesh.GetControlPointsCount()
    lControlPoints = pMesh.GetControlPoints()

    DisplayString("    Control Points")

    for i in range(lControlPointsCount):
        DisplayInt("        Control Point ", i)
        Display3DVector("            Coordinates: ", lControlPoints[i])

        for j in range(pMesh.GetLayerCount()):
            leNormals = pMesh.GetLayer(j).GetNormals()
            if leNormals:
                if leNormals.GetMappingMode() == FbxLayerElement.eByControlPoint:
                    header = "            Normal Vector (on layer %d): " % j 
                    if leNormals.GetReferenceMode() == FbxLayerElement.eDirect:
                        Display3DVector(header, leNormals.GetDirectArray().GetAt(i))

    DisplayString("")

def DisplayPolygons(pMesh):
    lPolygonCount = pMesh.GetPolygonCount()
    lControlPoints = pMesh.GetControlPoints() 

    DisplayString("    Polygons")

    vertexId = 0
    for i in range(lPolygonCount):
        DisplayInt("        Polygon ", i)

        for l in range(pMesh.GetLayerCount()):
            lePolgrp = pMesh.GetLayer(l).GetPolygonGroups()
            if lePolgrp:
                if lePolgrp.GetMappingMode() == FbxLayerElement.eByPolygon:
                    if lePolgrp.GetReferenceMode() == FbxLayerElement.eIndex:
                        header = "        Assigned to group (on layer %d): " % l 
                        polyGroupId = lePolgrp.GetIndexArray().GetAt(i)
                        DisplayInt(header, polyGroupId)
                else:
                    # any other mapping modes don't make sense
                    DisplayString("        \"unsupported group assignment\"")

        lPolygonSize = pMesh.GetPolygonSize(i)

        for j in range(lPolygonSize):
            lControlPointIndex = pMesh.GetPolygonVertex(i, j)

            Display3DVector("            Coordinates: ", lControlPoints[lControlPointIndex])

            for l in range(pMesh.GetLayerCount()):
                leVtxc = pMesh.GetLayer(l).GetVertexColors()
                if leVtxc:
                    header = "            Color vertex (on layer %d): " % l 

                    if leVtxc.GetMappingMode() == FbxLayerElement.eByControlPoint:
                        if leVtxc.GetReferenceMode() == FbxLayerElement.eDirect:
                            DisplayColor(header, leVtxc.GetDirectArray().GetAt(lControlPointIndex))
                        elif leVtxc.GetReferenceMode() == FbxLayerElement.eIndexToDirect:
                                id = leVtxc.GetIndexArray().GetAt(lControlPointIndex)
                                DisplayColor(header, leVtxc.GetDirectArray().GetAt(id))
                    elif leVtxc.GetMappingMode() == FbxLayerElement.eByPolygonVertex:
                            if leVtxc.GetReferenceMode() == FbxLayerElement.eDirect:
                                DisplayColor(header, leVtxc.GetDirectArray().GetAt(vertexId))
                            elif leVtxc.GetReferenceMode() == FbxLayerElement.eIndexToDirect:
                                id = leVtxc.GetIndexArray().GetAt(vertexId)
                                DisplayColor(header, leVtxc.GetDirectArray().GetAt(id))
                    elif leVtxc.GetMappingMode() == FbxLayerElement.eByPolygon or \
                         leVtxc.GetMappingMode() ==  FbxLayerElement.eAllSame or \
                         leVtxc.GetMappingMode() ==  FbxLayerElement.eNone:       
                         # doesn't make much sense for UVs
                        pass

                leUV = pMesh.GetLayer(l).GetUVs()
                if leUV:
                    header = "            Texture UV (on layer %d): " % l 

                    if leUV.GetMappingMode() == FbxLayerElement.eByControlPoint:
                        if leUV.GetReferenceMode() == FbxLayerElement.eDirect:
                            Display2DVector(header, leUV.GetDirectArray().GetAt(lControlPointIndex))
                        elif leUV.GetReferenceMode() == FbxLayerElement.eIndexToDirect:
                            id = leUV.GetIndexArray().GetAt(lControlPointIndex)
                            Display2DVector(header, leUV.GetDirectArray().GetAt(id))
                    elif leUV.GetMappingMode() ==  FbxLayerElement.eByPolygonVertex:
                        lTextureUVIndex = pMesh.GetTextureUVIndex(i, j)
                        if leUV.GetReferenceMode() == FbxLayerElement.eDirect or \
                           leUV.GetReferenceMode() == FbxLayerElement.eIndexToDirect:
                            Display2DVector(header, leUV.GetDirectArray().GetAt(lTextureUVIndex))
                    elif leUV.GetMappingMode() == FbxLayerElement.eByPolygon or \
                         leUV.GetMappingMode() == FbxLayerElement.eAllSame or \
                         leUV.GetMappingMode() ==  FbxLayerElement.eNone:
                         # doesn't make much sense for UVs
                        pass
            # # end for layer
            vertexId += 1
        # # end for polygonSize
    # # end for polygonCount


    # check visibility for the edges of the mesh
    for l in range(pMesh.GetLayerCount()):
        leVisibility=pMesh.GetLayer(0).GetVisibility()
        if leVisibility:
            header = "    Edge Visibilty (on layer %d): " % l
            DisplayString(header)
            # should be eByEdge
            if leVisibility.GetMappingMode() == FbxLayerElement.eByEdge:
                # should be eDirect
                for j in range(pMesh.GetMeshEdgeCount()):
                    DisplayInt("        Edge ", j)
                    DisplayBool("              Edge visibilty: ", leVisibility.GetDirectArray().GetAt(j))

    DisplayString("")

def DisplayTextureNames(pProperty):
    lTextureName = ""
    
    lLayeredTextureCount = pProperty.GetSrcObjectCount(FbxLayeredTexture.ClassId)
    if lLayeredTextureCount > 0:
        for j in range(lLayeredTextureCount):
            lLayeredTexture = pProperty.GetSrcObject(FbxLayeredTexture.ClassId, j)
            lNbTextures = lLayeredTexture.GetSrcObjectCount(FbxTexture.ClassId)
            lTextureName = " Texture "

            for k in range(lNbTextures):
                lTextureName += "\""
                lTextureName += lLayeredTexture.GetName()
                lTextureName += "\""
                lTextureName += " "
            lTextureName += "of "
            lTextureName += pProperty.GetName().Buffer()
            lTextureName += " on layer "
            lTextureName += j
        lTextureName += " |"
    else:
        #no layered texture simply get on the property
        lNbTextures = pProperty.GetSrcObjectCount(FbxTexture.ClassId)

        if lNbTextures > 0:
            lTextureName = " Texture "
            lTextureName += " "

            for j in range(lNbTextures):
                lTexture = pProperty.GetSrcObject(FbxTexture.ClassId,j)
                if lTexture:
                    lTextureName += "\""
                    lTextureName += lTexture.GetName()
                    lTextureName += "\""
                    lTextureName += " "
            lTextureName += "of "
            lTextureName += pProperty.GetName().Buffer()
            lTextureName += " |"
            
    return lTextureName

def DisplayMaterialTextureConnections(pMaterial, pMatId, l ):
    lConnectionString = "            Material " + str(pMatId) + " (on layer " + str(l) +") -- "
    #Show all the textures

    #Diffuse Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sDiffuse)
    lConnectionString += DisplayTextureNames(lProperty)

    #DiffuseFactor Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sDiffuseFactor)
    lConnectionString += DisplayTextureNames(lProperty)

    #Emissive Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sEmissive)
    lConnectionString += DisplayTextureNames(lProperty)

    #EmissiveFactor Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sEmissiveFactor)
    lConnectionString += DisplayTextureNames(lProperty)


    #Ambient Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sAmbient)
    lConnectionString += DisplayTextureNames(lProperty)

    #AmbientFactor Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sAmbientFactor)
    lConnectionString += DisplayTextureNames(lProperty)     

    #Specular Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sSpecular)
    lConnectionString += DisplayTextureNames(lProperty)

    #SpecularFactor Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sSpecularFactor)
    lConnectionString += DisplayTextureNames(lProperty)

    #Shininess Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sShininess)
    lConnectionString += DisplayTextureNames(lProperty)

    #Bump Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sBump)
    lConnectionString += DisplayTextureNames(lProperty)

    #Normal Map Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sNormalMap)
    lConnectionString += DisplayTextureNames(lProperty)

    #Transparent Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sTransparentColor)
    lConnectionString += DisplayTextureNames(lProperty)

    #TransparencyFactor Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sTransparencyFactor)
    lConnectionString += DisplayTextureNames(lProperty)

    #Reflection Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sReflection)
    lConnectionString += DisplayTextureNames(lProperty)

    #ReflectionFactor Textures
    lProperty = pMaterial.FindProperty(FbxSurfaceMaterial.sReflectionFactor)
    lConnectionString += DisplayTextureNames(lProperty)

    #if(lMaterial != NULL)
    DisplayString(lConnectionString)

def DisplayMaterialConnections(pMesh):
    lPolygonCount = pMesh.GetPolygonCount()

    DisplayString("    Polygons Material Connections")

    #check whether the material maps with only one mesh
    lIsAllSame = True
    for l in range(pMesh.GetLayerCount()):
        lLayerMaterial = pMesh.GetLayer(l).GetMaterials()
        if lLayerMaterial:
            if lLayerMaterial.GetMappingMode() == FbxLayerElement.eByPolygon:
                lIsAllSame = False
                break

    #For eAllSame mapping type, just out the material and texture mapping info once
    if lIsAllSame:
        for l in range(pMesh.GetLayerCount()):
            lLayerMaterial = pMesh.GetLayer(l).GetMaterials()
            if lLayerMaterial:
                if lLayerMaterial.GetMappingMode() == FbxLayerElement.eAllSame:
                    lMaterial = pMesh.GetNode().GetMaterial(lLayerMaterial.GetIndexArray().GetAt(0))    
                    lMatId = lLayerMaterial.GetIndexArray().GetAt(0)
                    if lMatId >=0:
                        DisplayInt("        All polygons share the same material on layer ", l)
                        DisplayMaterialTextureConnections(lMaterial, lMatId, l)
            else:
                #layer 0 has no material
                if l == 0:
                    DisplayString("        no material applied")

    #For eByPolygon mapping type, just out the material and texture mapping info once
    else:
        for i in range(lPolygonCount):
            DisplayInt("        Polygon ", i)

            for l in range(pMesh.GetLayerCount()):
                lLayerMaterial = pMesh.GetLayer(l).GetMaterials()
                if lLayerMaterial:
                    lMatId = -1
                    lMaterial = pMesh.GetNode().GetMaterial(lLayerMaterial.GetIndexArray().GetAt(i))
                    lMatId = lLayerMaterial.GetIndexArray().GetAt(i)

                    if lMatId >= 0:
                        DisplayMaterialTextureConnections(lMaterial, lMatId, l)

def DisplayMaterialMapping(pMesh):
    lMappingTypes = [ "None", "By Control Point", "By Polygon Vertex", "By Polygon", "By Edge", "All Same" ]
    lReferenceMode = [ "Direct", "Index", "Index to Direct"]

    lMtrlCount = 0
    lNode = None
    if pMesh:
        lNode = pMesh.GetNode()
        if lNode:
            lMtrlCount = lNode.GetMaterialCount()

    for l in range(pMesh.GetLayerCount()):
        leMat = pMesh.GetLayer(l).GetMaterials()
        if leMat:
            header = "    Material layer %d: " % l
            DisplayString(header)

            DisplayString("           Mapping: ", lMappingTypes[leMat.GetMappingMode()])
            DisplayString("           ReferenceMode: ", lReferenceMode[leMat.GetReferenceMode()])

            lMaterialCount = 0

            if leMat.GetReferenceMode() == FbxLayerElement.eDirect or \
                leMat.GetReferenceMode() == FbxLayerElement.eIndexToDirect:
                lMaterialCount = lMtrlCount

            if leMat.GetReferenceMode() == FbxLayerElement.eIndex or \
                leMat.GetReferenceMode() == FbxLayerElement.eIndexToDirect:
                lString = "           Indices: "

                lIndexArrayCount = leMat.GetIndexArray().GetCount() 
                for i in range(lIndexArrayCount):
                    lString += str(leMat.GetIndexArray().GetAt(i))

                    if i < lIndexArrayCount - 1:
                        lString += ", "

                DisplayString(lString)

    DisplayString("")

def DisplayNurb(pNode):
    lNurb = pNode.GetNodeAttribute ()

    DisplayString("Nurb Name: ", pNode.GetName())

    lSurfaceModes = [ "Raw", "Low No Normals", "Low", "High No Normals", "High" ]

    DisplayString("    Surface Mode: ", lSurfaceModes[lNurb.GetSurfaceMode()])

    lControlPointsCount = lNurb.GetControlPointsCount()
    lControlPoints = lNurb.GetControlPoints()

    for i in range(lControlPointsCount):
        DisplayInt("    Control Point ", i)
        Display3DVector("        Coordinates: ", lControlPoints[i])
        DisplayDouble("        Weight: ", lControlPoints[i][3])

    lNurbTypes = [ "Periodic", "Closed", "Open" ]

    DisplayString("    Nurb U Type: ", lNurbTypes[lNurb.GetNurbsUType()])
    DisplayInt("    U Count: ", lNurb.GetUCount())
    DisplayString("    Nurb V Type: ", lNurbTypes[lNurb.GetNurbsVType()])
    DisplayInt("    V Count: ", lNurb.GetVCount())
    DisplayInt("    U Order: ", lNurb.GetUOrder())
    DisplayInt("    V Order: ", lNurb.GetVOrder())
    DisplayInt("    U Step: ", lNurb.GetUStep())
    DisplayInt("    V Step: ", lNurb.GetVStep())

    lUKnotCount = lNurb.GetUKnotCount()
    lVKnotCount = lNurb.GetVKnotCount()
    lUMultiplicityCount = lNurb.GetUCount()
    lVMultiplicityCount = lNurb.GetVCount()
    lUKnotVector = lNurb.GetUKnotVector()
    lVKnotVector = lNurb.GetVKnotVector()
    lUMultiplicityVector = lNurb.GetUMultiplicityVector()
    lVMultiplicityVector = lNurb.GetVMultiplicityVector()

    lString = "    U Knot Vector: "

    for i in range(lUKnotCount):
        lString += str(lUKnotVector[i])

        if i < lUKnotCount - 1:
            lString += ", "

    lString += "\n"
    print(lString)

    lString = "    V Knot Vector: "

    for i in range(lVKnotCount):
        lString += str(lVKnotVector[i])

        if i < lVKnotCount - 1:
            lString += ", "

    lString += "\n"
    print(lString)

    lString = "    U Multiplicity Vector: "

    for i in range(lUMultiplicityCount):
        lString += str(lUMultiplicityVector[i])

        if i < lUMultiplicityCount - 1:
            lString += ", "

    lString += "\n"
    print(lString)

    lString = "    V Multiplicity Vector: "

    for i in range(lVMultiplicityCount):
        lString += str(lVMultiplicityVector[i])

        if i < lVMultiplicityCount - 1:
            lString += ", "

    lString += "\n"
    print(lString)

    DisplayString("")

    DisplayTexture(lNurb)
    DisplayMaterial(lNurb)
    DisplayLink(lNurb)
    DisplayShape(lNurb)

def DisplayPatch(pNode):
    lPatch = pNode.GetNodeAttribute()

    DisplayString("Patch Name: ", pNode.GetName())

    lSurfaceModes = [ "Raw", "Low No Normals", "Low", "High No Normals", "High" ]

    DisplayString("    Surface Mode: ", lSurfaceModes[lPatch.GetSurfaceMode()])

    lControlPointsCount = lPatch.GetControlPointsCount()
    lControlPoints = lPatch.GetControlPoints()

    for i in range(lControlPointsCount):
        DisplayInt("    Control Point ", i)
        Display3DVector("        Coordinates: ", lControlPoints[i])
        DisplayDouble("        Weight: ", lControlPoints[i][3])

    lPatchTypes = [ "Bezier", "Bezier Quadric", "Cardinal", "B-Spline", "Linear" ]

    DisplayString("    Patch U Type: ", lPatchTypes[lPatch.GetPatchUType()])
    DisplayInt("    U Count: ", lPatch.GetUCount())
    DisplayString("    Patch V Type: ", lPatchTypes[lPatch.GetPatchVType()])
    DisplayInt("    V Count: ", lPatch.GetVCount())
    DisplayInt("    U Step: ", lPatch.GetUStep())
    DisplayInt("    V Step: ", lPatch.GetVStep())
    DisplayBool("    U Closed: ", lPatch.GetUClosed())
    DisplayBool("    V Closed: ", lPatch.GetVClosed())
    DisplayBool("    U Capped Top: ", lPatch.GetUCappedTop())
    DisplayBool("    U Capped Bottom: ", lPatch.GetUCappedBottom())
    DisplayBool("    V Capped Top: ", lPatch.GetVCappedTop())
    DisplayBool("    V Capped Bottom: ", lPatch.GetVCappedBottom())

    DisplayString("")

    DisplayTexture(lPatch)
    DisplayMaterial(lPatch)
    DisplayLink(lPatch)
    DisplayShape(lPatch)

def DisplayCamera(pNode):
    lCamera = pNode.GetNodeAttribute()
    lName = pNode.GetName()
    lTargetNode = pNode.GetTarget()
    lTargetUpNode = pNode.GetTargetUp()
    
    DisplayString("Camera Name: ", lName)
    if not lCamera:
        DisplayString("NOT FOUND")
        return
    
    DisplayCameraPositionAndOrientation(lCamera, lTargetNode, lTargetUpNode)

    lProjectionTypes = [ "Perspective", "Orthogonal" ]

    DisplayString("    Projection Type: ", lProjectionTypes[lCamera.ProjectionType.Get()])

    DisplayViewingAreaControls(lCamera)

    # If camera projection type is set to FbxCamera.eOrthogonal, the 
    # aperture and film controls are not relevant.
    if lCamera.ProjectionType.Get() != FbxCamera.eOrthogonal:
        DisplayApertureAndFilmControls(lCamera)

    DisplayBackgroundProperties(lCamera)
    DisplayCameraViewOptions(lCamera)
    DisplayRenderOptions(lCamera)
    DisplayDefaultAnimationValues(lCamera)

def DisplayCameraPositionAndOrientation(pCamera, pTargetNode, pTargetUpNode):
    DisplayString("    Camera Position and Orientation")
    Display3DVector("        Position: ", pCamera.Position.Get())

    if pTargetNode:
        DisplayString("        Camera Interest: ",pTargetNode.GetName())
    else:
        Display3DVector("        Default Camera Interest Position: ", pCamera.InterestPosition.Get())

    if pTargetUpNode:
        DisplayString("        Camera Up Target: ", pTargetUpNode.GetName())
    else:
        Display3DVector("        Up Vector: ", pCamera.UpVector.Get())

    DisplayDouble("        Roll: ", pCamera.Roll.Get())

def DisplayViewingAreaControls(pCamera):
    DisplayString("    Viewing Area Controls")

    lCameraFormat = [ "Custom", "D1 NTSC", "NTSC", "PAL", "D1 PAL", \
        "HD", "640x480", "320x200", "320x240", "128x128", "Full Screen"  ]

    DisplayString("        Format: ", lCameraFormat[pCamera.GetFormat()])

    lAspectRatioModes = [ "Window Size", "Fixed Ratio", "Fixed Resolution", "Fixed Width", "Fixed Height" ]

    DisplayString("        Aspect Ratio Mode: ", lAspectRatioModes[pCamera.GetAspectRatioMode()])

    # If the ratio mode is eWINDOW_SIZE, both width and height values aren't relevant.
    if pCamera.GetAspectRatioMode() != FbxCamera.eWindowSize:
        DisplayDouble("        Aspect Width: ", pCamera.AspectWidth.Get())
        DisplayDouble("        Aspect Height: ", pCamera.AspectHeight.Get())

    DisplayDouble("        Pixel Ratio: ", pCamera.PixelAspectRatio.Get())
    DisplayDouble("        Near Plane: ", pCamera.NearPlane.Get())
    DisplayDouble("        Far Plane: ", pCamera.FarPlane.Get())
    DisplayBool("        Mouse Lock: ", pCamera.LockMode.Get())

def DisplayApertureAndFilmControls(pCamera):
    DisplayString("    Aperture and Film Controls")

    lCameraApertureFormats = [ "Custom", "16mm Theatrical", "Super 16mm", "35mm Academy", \
                               "35mm TV Projection", "35mm Full Aperture", "35mm 1.85 Projection", \
                              "35mm Anamorphic", "70mm Projection", "VistaVision", "Dynavision", "Imax" ]

    DisplayString("        Aperture Format: ", lCameraApertureFormats[pCamera.GetApertureFormat()])

    lCameraApertureModes = [ "Horizontal and Vertical", "Horizontal", "Vertical", "Focal Length" ]

    DisplayString("        Aperture Mode: ", lCameraApertureModes[pCamera.GetApertureMode()])

    DisplayDouble("        Aperture Width: ", pCamera.GetApertureWidth(), " inches")
    DisplayDouble("        Aperture Height: ", pCamera.GetApertureHeight(), " inches")
    DisplayDouble("        Squeeze Ratio: ", pCamera.GetSqueezeRatio())
    DisplayDouble("        Focal Length: ", pCamera.FocalLength.Get(), "mm")
    DisplayDouble("        Field of View: ", pCamera.FieldOfView.Get(), " degrees")

def DisplayBackgroundProperties(pCamera):
    DisplayString("    Background Properties")

    if pCamera.GetBackgroundFileName():
        DisplayString("        Background File Name: \"", pCamera.GetBackgroundFileName(), "\"")
    else:
        DisplayString("        Background File Name: \"", "\"")

    lBackgroundDisplayModes = [ "Disabled", "Always", "When Media" ]

    DisplayString("        Background Display Mode: ", lBackgroundDisplayModes[pCamera.ViewFrustumBackPlaneMode.Get()])

    DisplayBool("        Foreground Matte Threshold Enable: ", pCamera.ShowFrontplate.Get())

    # This option is only relevant if background drawing mode is set to eFOREGROUND or eBACKGROUND_AND_FOREGROUND.
    if pCamera.ForegroundOpacity.Get():
        DisplayDouble("        Foreground Matte Threshold: ", pCamera.BackgroundAlphaTreshold.Get())

    lBackgroundPlacementOptions = FbxString()
    if pCamera.GetBackPlateFitImage():
        lBackgroundPlacementOptions += " Fit,"
    if pCamera.GetBackPlateCenter():
        lBackgroundPlacementOptions += " Center,"
    if pCamera.GetBackPlateKeepRatio():
        lBackgroundPlacementOptions += " Keep Ratio,"
    if pCamera.GetBackPlateCrop():
        lBackgroundPlacementOptions += " Crop,"
    if not lBackgroundPlacementOptions.IsEmpty():
        lString =  lBackgroundPlacementOptions.Left(lBackgroundPlacementOptions.GetLen() - 1)
        DisplayString("        Background Placement Options: ",lString.Buffer())

    DisplayDouble("        Background Distance: ", pCamera.BackPlaneDistance.Get())

    lCameraBackgroundDistanceModes = [ "Relative to Interest", "Absolute from Camera" ]

    DisplayString("        Background Distance Mode: ", lCameraBackgroundDistanceModes[pCamera.BackPlaneDistanceMode.Get()])

def DisplayCameraViewOptions(pCamera):
    DisplayString("    Camera View Options")

    DisplayBool("        View Camera Interest: ", pCamera.ViewCameraToLookAt.Get())
    DisplayBool("        View Near Far Planes: ", pCamera.ViewFrustumNearFarPlane.Get())
    DisplayBool("        Show Grid: ", pCamera.ShowGrid.Get())
    DisplayBool("        Show Axis: ", pCamera.ShowAzimut.Get())
    DisplayBool("        Show Name: ", pCamera.ShowName.Get())
    DisplayBool("        Show Info on Moving: ", pCamera.ShowInfoOnMoving.Get())
    DisplayBool("        Show Time Code: ", pCamera.ShowTimeCode.Get())
    DisplayBool("        Display Safe Area: ", pCamera.DisplaySafeArea.Get())

    lSafeAreaStyles = [ "Round", "Square" ]

    DisplayString("        Safe Area Style: ", lSafeAreaStyles[pCamera.SafeAreaDisplayStyle.Get()])
    DisplayBool("        Show Audio: ", pCamera.ShowAudio.Get())

    c = pCamera.BackgroundColor.Get()
    color = FbxColor(c[0], c[1], c[2])
    DisplayColor("        Background Color: ", color)

    c = pCamera.AudioColor.Get()
    color = FbxColor(c[0], c[1], c[2])
    DisplayColor("        Audio Color: ", color)

    DisplayBool("        Use Frame Color: ", pCamera.UseFrameColor.Get())

    c = pCamera.FrameColor.Get()
    color = FbxColor(c[0], c[1], c[2])
    DisplayColor("        Frame Color: ", color)

def DisplayRenderOptions(pCamera):
    DisplayString("    Render Options")

    lCameraRenderOptionsUsageTimes = [ "Interactive", "At Render" ]

    DisplayString("        Render Options Usage Time: ", lCameraRenderOptionsUsageTimes[pCamera.UseRealTimeDOFAndAA.Get()])
    DisplayBool("        Use Antialiasing: ", pCamera.UseAntialiasing.Get())
    DisplayDouble("        Antialiasing Intensity: ", pCamera.AntialiasingIntensity.Get())

    lCameraAntialiasingMethods = [ "Oversampling Antialiasing", "Hardware Antialiasing" ]

    DisplayString("        Antialiasing Method: ", lCameraAntialiasingMethods[pCamera.AntialiasingMethod.Get()])

    # This option is only relevant if antialiasing method is set to eAAOversampling.
    if pCamera.AntialiasingMethod.Get() == FbxCamera.eAAOversampling:
        DisplayInt("        Number of Samples: ", pCamera.FrameSamplingCount.Get())

    lCameraSamplingTypes = [ "Uniform", "Stochastic" ]

    DisplayString("        Sampling Type: ", lCameraSamplingTypes[pCamera.FrameSamplingType.Get()])
    DisplayBool("        Use Accumulation Buffer: ", pCamera.UseAccumulationBuffer.Get())
    DisplayBool("        Use Depth of Field: ", pCamera.UseDepthOfField.Get())

    lCameraFocusDistanceSources = [ "Camera Interest", "Specific Distance" ]

    DisplayString("        Focus Distance Source: ", lCameraFocusDistanceSources[pCamera.FocusSource.Get()])

    # This parameter is only relevant if focus distance source is set to eFocusSpecificDistance.
    if pCamera.FocusSource.Get() == FbxCamera.eFocusSpecificDistance:
        DisplayDouble("        Specific Distance: ", pCamera.FocusDistance.Get())

    DisplayDouble("        Focus Angle: ", pCamera.FocusAngle.Get(), " degrees")

def DisplayDefaultAnimationValues(pCamera):
    DisplayString("    Default Animation Values")

    DisplayDouble("        Default Field of View: ", pCamera.FieldOfView.Get())
    DisplayDouble("        Default Field of View X: ", pCamera.FieldOfViewX.Get())
    DisplayDouble("        Default Field of View Y: ", pCamera.FieldOfViewY.Get())
    DisplayDouble("        Default Optical Center X: ", pCamera.OpticalCenterX.Get())
    DisplayDouble("        Default Optical Center Y: ", pCamera.OpticalCenterY.Get())
    DisplayDouble("        Default Roll: ", pCamera.Roll.Get())

def DisplayLight(pNode):
    lLight = pNode.GetNodeAttribute()

    DisplayString("Light Name: ", pNode.GetName())

    lLightTypes = [ "Point", "Directional", "Spot" ]

    #DisplayString("    Type: ", lLightTypes[lLight.LightType.Get()])
    DisplayBool("    Cast Light: ", lLight.CastLight.Get())

    if not lLight.FileName.Get().IsEmpty():
        DisplayString("    Gobo")

        DisplayString("        File Name: \"", lLight.FileName.Get().Buffer(), "\"")
        DisplayBool("        Ground Projection: ", lLight.DrawGroundProjection.Get())
        DisplayBool("        Volumetric Projection: ", lLight.DrawVolumetricLight.Get())
        DisplayBool("        Front Volumetric Projection: ", lLight.DrawFrontFacingVolumetricLight.Get())

    DisplayDefaultAnimationValues(lLight)

def DisplayDefaultAnimationValues(pLight):
    DisplayString("    Default Animation Values")

    c = pLight.Color.Get()
    lColor = FbxColor(c[0], c[1], c[2])
    DisplayColor("        Default Color: ", lColor)
    DisplayDouble("        Default Intensity: ", pLight.Intensity.Get())
    DisplayDouble("        Default Inner Angle: ", pLight.InnerAngle.Get())
    DisplayDouble("        Default Outer Angle: ", pLight.OuterAngle.Get())
    DisplayDouble("        Default Fog: ", pLight.Fog.Get())

def DisplayPose(pScene):
    lPoseCount = pScene.GetPoseCount()

    for i in range(lPoseCount):
        lPose = pScene.GetPose(i)

        lName = lPose.GetName()
        DisplayString("Pose Name: ", lName)

        DisplayBool("    Is a bind pose: ", lPose.IsBindPose())

        DisplayInt("    Number of items in the pose: ", lPose.GetCount())

        DisplayString("","")

        for j in range(lPose.GetCount()):
            lName = lPose.GetNodeName(j).GetCurrentName()
            DisplayString("    Item name: ", lName)

            if not lPose.IsBindPose():
                # Rest pose can have local matrix
                DisplayBool("    Is local space matrix: ", lPose.IsLocalMatrix(j))

            DisplayString("    Matrix value: ","")

            lMatrixValue = ""
            for k in range(4):
                lMatrix = lPose.GetMatrix(j)
                lRow = lMatrix.GetRow(k)

                lRowValue = "%9.4f %9.4f %9.4f %9.4f\n" % (lRow[0], lRow[1], lRow[2], lRow[3])
                lMatrixValue += "        " + lRowValue

            DisplayString("", lMatrixValue)

    lPoseCount = pScene.GetCharacterPoseCount()

    for i in range(lPoseCount):
        lPose = pScene.GetCharacterPose(i)
        lCharacter = lPose.GetCharacter()

        if not lCharacter:
            break

        DisplayString("Character Pose Name: ", lCharacter.mName.Buffer())

        lNodeId = eCharacterHips

        while lCharacter.GetCharacterLink(lNodeId, lCharacterLink):
            lAnimStack = None
            if lAnimStack == None:
                lScene = lCharacterLink.mNode.GetScene()
                if lScene:
                    lAnimStack = lScene.GetMember(FBX_TYPE(FbxAnimStack), 0)

            lGlobalPosition = lCharacterLink.mNode.GetGlobalFromAnim(KTIME_ZERO, lAnimStack)

            DisplayString("    Matrix value: ","")

            lMatrixValue = ""
            for k in range(4):
                lRow = lGlobalPosition.GetRow(k)

                lRowValue = "%9.4f %9.4f %9.4f %9.4f\n" % (lRow[0], lRow[1], lRow[2], lRow[3])
                lMatrixValue += "        " + lRowValue

            DisplayString("", lMatrixValue)

            lNodeId = ECharacterNodeId(int(lNodeId) + 1)

def DisplayString(pHeader, pValue="" , pSuffix=""):
    lString = pHeader
    lString += str(pValue)
    lString += pSuffix
    print(lString)

def DisplayBool(pHeader, pValue, pSuffix=""):
    lString = pHeader
    if pValue:
        lString += "true"
    else:
        lString += "false"
    lString += pSuffix
    print(lString)

def DisplayInt(pHeader, pValue, pSuffix=""):
    lString = pHeader
    lString += str(pValue)
    lString += pSuffix
    print(lString)

def DisplayDouble(pHeader, pValue, pSuffix=""):
    print("%s%f%s" % (pHeader, pValue, pSuffix))

def Display2DVector(pHeader, pValue, pSuffix=""):
    print("%s%f, %f%s" % (pHeader, pValue[0], pValue[1], pSuffix))

def Display3DVector(pHeader, pValue, pSuffix=""):
    print("%s%f, %f, %f%s" % (pHeader, pValue[0], pValue[1], pValue[2], pSuffix))

def Display4DVector(pHeader, pValue, pSuffix=""):
    print("%s%f, %f, %f, %f%s" % (pHeader, pValue[0], pValue[1], pValue[2], pValue[3], pSuffix))

def DisplayColor(pHeader, pValue, pSuffix=""):
    print("%s%f (red), %f (green), %f (blue)%s" % (pHeader, pValue.mRed, pValue.mGreen, pValue.mBlue, pSuffix))

def DisplayAnimation(pScene):
    for i in range(pScene.GetSrcObjectCount(FbxAnimStack.ClassId)):
        lAnimStack = pScene.GetSrcObject(FbxAnimStack.ClassId, i)

        lOutputString = "Animation Stack Name: "
        lOutputString += lAnimStack.GetName()
        lOutputString += "\n"
        print(lOutputString)

        DisplayAnimationStack(lAnimStack, pScene.GetRootNode(), True)
        DisplayAnimationStack(lAnimStack, pScene.GetRootNode(), False)

def DisplayAnimationStack(pAnimStack, pNode, isSwitcher):
    nbAnimLayers = pAnimStack.GetSrcObjectCount(FbxAnimLayer.ClassId)

    lOutputString = "Animation stack contains "
    lOutputString += str(nbAnimLayers)
    lOutputString += " Animation Layer(s)"
    print(lOutputString)

    for l in range(nbAnimLayers):
        lAnimLayer = pAnimStack.GetSrcObject(FbxAnimLayer.ClassId, l)

        lOutputString = "AnimLayer "
        lOutputString += str(l)
        print(lOutputString)

        DisplayAnimationLayer(lAnimLayer, pNode, isSwitcher)

def DisplayAnimationLayer(pAnimLayer, pNode, isSwitcher=False):
    lOutputString = "     Node Name: "
    lOutputString += pNode.GetName()
    lOutputString += "\n"
    print(lOutputString)

    DisplayChannels(pNode, pAnimLayer, DisplayCurveKeys, DisplayListCurveKeys, isSwitcher)
    print

    for lModelCount in range(pNode.GetChildCount()):
        DisplayAnimationLayer(pAnimLayer, pNode.GetChild(lModelCount), isSwitcher)

def DisplayChannels(pNode, pAnimLayer, DisplayCurve, DisplayListCurve, isSwitcher):
    lAnimCurve = None

    KFCURVENODE_T_X = "X"
    KFCURVENODE_T_Y = "Y"
    KFCURVENODE_T_Z = "Z"

    KFCURVENODE_R_X = "X"
    KFCURVENODE_R_Y = "Y"
    KFCURVENODE_R_Z = "Z"
    KFCURVENODE_R_W = "W"

    KFCURVENODE_S_X = "X"
    KFCURVENODE_S_Y = "Y"
    KFCURVENODE_S_Z = "Z"

    # Display general curves.
    if not isSwitcher:
        lAnimCurve = pNode.LclTranslation.GetCurve(pAnimLayer, KFCURVENODE_T_X)
        if lAnimCurve:
            print("        TX")
            DisplayCurve(lAnimCurve)
        lAnimCurve = pNode.LclTranslation.GetCurve(pAnimLayer, KFCURVENODE_T_Y)
        if lAnimCurve:
            print("        TY")
            DisplayCurve(lAnimCurve)
        lAnimCurve = pNode.LclTranslation.GetCurve(pAnimLayer, KFCURVENODE_T_Z)
        if lAnimCurve:
            print("        TZ")
            DisplayCurve(lAnimCurve)

        lAnimCurve = pNode.LclRotation.GetCurve(pAnimLayer, KFCURVENODE_R_X)
        if lAnimCurve:
            print("        RX")
            DisplayCurve(lAnimCurve)
        lAnimCurve = pNode.LclRotation.GetCurve(pAnimLayer, KFCURVENODE_R_Y)
        if lAnimCurve:
            print("        RY")
            DisplayCurve(lAnimCurve)
        lAnimCurve = pNode.LclRotation.GetCurve(pAnimLayer, KFCURVENODE_R_Z)
        if lAnimCurve:
            print("        RZ")
            DisplayCurve(lAnimCurve)

        lAnimCurve = pNode.LclScaling.GetCurve(pAnimLayer, KFCURVENODE_S_X)
        if lAnimCurve:
            print("        SX")
            DisplayCurve(lAnimCurve)
        lAnimCurve = pNode.LclScaling.GetCurve(pAnimLayer, KFCURVENODE_S_Y)
        if lAnimCurve:
            print("        SY")
            DisplayCurve(lAnimCurve)
        lAnimCurve = pNode.LclScaling.GetCurve(pAnimLayer, KFCURVENODE_S_Z)
        if lAnimCurve:
            print("        SZ")
            DisplayCurve(lAnimCurve)

    # Display curves specific to a light or marker.
    lNodeAttribute = pNode.GetNodeAttribute()

    KFCURVENODE_COLOR_RED = "X"
    KFCURVENODE_COLOR_GREEN = "Y"
    KFCURVENODE_COLOR_BLUE = "Z"
    
    if lNodeAttribute:
        lAnimCurve = lNodeAttribute.Color.GetCurve(pAnimLayer, KFCURVENODE_COLOR_RED)
        if lAnimCurve:
            print("        Red")
            DisplayCurve(lAnimCurve)
        lAnimCurve = lNodeAttribute.Color.GetCurve(pAnimLayer, KFCURVENODE_COLOR_GREEN)
        if lAnimCurve:
            print("        Green")
            DisplayCurve(lAnimCurve)
        lAnimCurve = lNodeAttribute.Color.GetCurve(pAnimLayer, KFCURVENODE_COLOR_BLUE)
        if lAnimCurve:
            print("        Blue")
            DisplayCurve(lAnimCurve)

        # Display curves specific to a light.
        light = pNode.GetLight()
        if light:
            lAnimCurve = light.Intensity.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Intensity")
                DisplayCurve(lAnimCurve)

            lAnimCurve = light.OuterAngle.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Cone Angle")
                DisplayCurve(lAnimCurve)

            lAnimCurve = light.Fog.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Fog")
                DisplayCurve(lAnimCurve)

        # Display curves specific to a camera.
        camera = pNode.GetCamera()
        if camera:
            lAnimCurve = camera.FieldOfView.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Field of View")
                DisplayCurve(lAnimCurve)

            lAnimCurve = camera.FieldOfViewX.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Field of View X")
                DisplayCurve(lAnimCurve)

            lAnimCurve = camera.FieldOfViewY.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Field of View Y")
                DisplayCurve(lAnimCurve)

            lAnimCurve = camera.OpticalCenterX.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Optical Center X")
                DisplayCurve(lAnimCurve)

            lAnimCurve = camera.OpticalCenterY.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Optical Center Y")
                DisplayCurve(lAnimCurve)

            lAnimCurve = camera.Roll.GetCurve(pAnimLayer)
            if lAnimCurve:
                print("        Roll")
                DisplayCurve(lAnimCurve)

        # Display curves specific to a geometry.
        if lNodeAttribute.GetAttributeType() == FbxNodeAttribute.eMesh or \
            lNodeAttribute.GetAttributeType() == FbxNodeAttribute.eNurbs or \
            lNodeAttribute.GetAttributeType() == FbxNodeAttribute.ePatch:
            lGeometry = lNodeAttribute

            lBlendShapeDeformerCount = lGeometry.GetDeformerCount(FbxDeformer.eBlendShape)
            for lBlendShapeIndex in range(lBlendShapeDeformerCount):
                lBlendShape = lGeometry.GetDeformer(lBlendShapeIndex, FbxDeformer.eBlendShape)
                lBlendShapeChannelCount = lBlendShape.GetBlendShapeChannelCount()
                for lChannelIndex in range(lBlendShapeChannelCount):
                    lChannel = lBlendShape.GetBlendShapeChannel(lChannelIndex)
                    lChannelName = lChannel.GetName()
                    lAnimCurve = lGeometry.GetShapeChannel(lBlendShapeIndex, lChannelIndex, pAnimLayer, True)
                    if lAnimCurve:
                        print("        Shape %s" % lChannelName)
                        DisplayCurve(lAnimCurve)

    # Display curves specific to properties
    lProperty = pNode.GetFirstProperty()
    while lProperty.IsValid():
        if lProperty.GetFlag(FbxPropertyFlags.eUserDefined):
            lFbxFCurveNodeName  = lProperty.GetName()
            lCurveNode = lProperty.GetCurveNode(pAnimLayer)

            if not lCurveNode:
                lProperty = pNode.GetNextProperty(lProperty)
                continue

            lDataType = lProperty.GetPropertyDataType()
            if lDataType.GetType() == eFbxBool or lDataType.GetType() == eFbxDouble or lDataType.GetType() == eFbxFloat or lDataType.GetType() == eFbxInt:
                lMessage =  "        Property "
                lMessage += lProperty.GetName()
                if lProperty.GetLabel().GetLen() > 0:
                    lMessage += " (Label: "
                    lMessage += lProperty.GetLabel()
                    lMessage += ")"

                DisplayString(lMessage)

                for c in range(lCurveNode.GetCurveCount(0)):
                    lAnimCurve = lCurveNode.GetCurve(0, c)
                    if lAnimCurve:
                        DisplayCurve(lAnimCurve)
            elif lDataType.GetType() == eFbxDouble3 or lDataType.GetType() == eFbxDouble4 or lDataType.Is(FbxColor3DT) or lDataType.Is(FbxColor4DT):
                if lDataType.Is(FbxColor3DT) or lDataType.Is(FbxColor4DT):
                    lComponentName1 = KFCURVENODE_COLOR_RED
                    lComponentName2 = KFCURVENODE_COLOR_GREEN
                    lComponentName3 = KFCURVENODE_COLOR_BLUE                    
                else:
                    lComponentName1 = "X"
                    lComponentName2 = "Y"
                    lComponentName3 = "Z"
                
                lMessage =  "        Property "
                lMessage += lProperty.GetName()
                if lProperty.GetLabel().GetLen() > 0:
                    lMessage += " (Label: "
                    lMessage += lProperty.GetLabel()
                    lMessage += ")"
                DisplayString(lMessage)

                for c in range(lCurveNode.GetCurveCount(0)):
                    lAnimCurve = lCurveNode.GetCurve(0, c)
                    if lAnimCurve:
                        DisplayString("        Component ", lComponentName1)
                        DisplayCurve(lAnimCurve)

                for c in range(lCurveNode.GetCurveCount(1)):
                    lAnimCurve = lCurveNode.GetCurve(1, c)
                    if lAnimCurve:
                        DisplayString("        Component ", lComponentName2)
                        DisplayCurve(lAnimCurve)

                for c in range(lCurveNode.GetCurveCount(2)):
                    lAnimCurve = lCurveNode.GetCurve(2, c)
                    if lAnimCurve:
                        DisplayString("        Component ", lComponentName3)
                        DisplayCurve(lAnimCurve)
            elif lDataType.GetType() == eFbxEnum:
                lMessage =  "        Property "
                lMessage += lProperty.GetName()
                if lProperty.GetLabel().GetLen() > 0:
                    lMessage += " (Label: "
                    lMessage += lProperty.GetLabel()
                    lMessage += ")"
                DisplayString(lMessage)

                for c in range(lCurveNode.GetCurveCount(0)):
                    lAnimCurve = lCurveNode.GetCurve(0, c)
                    if lAnimCurve:
                        DisplayListCurve(lAnimCurve, lProperty)

        lProperty = pNode.GetNextProperty(lProperty)

def InterpolationFlagToIndex(flags):
    #if (flags&KFCURVE_INTERPOLATION_CONSTANT)==KFCURVE_INTERPOLATION_CONSTANT:
    #    return 1
    #if (flags&KFCURVE_INTERPOLATION_LINEAR)==KFCURVE_INTERPOLATION_LINEAR:
    #    return 2
    #if (flags&KFCURVE_INTERPOLATION_CUBIC)==KFCURVE_INTERPOLATION_CUBIC:
    #    return 3
    return 0

def ConstantmodeFlagToIndex(flags):
    #if (flags&KFCURVE_CONSTANT_STANDARD)==KFCURVE_CONSTANT_STANDARD:
    #    return 1
    #if (flags&KFCURVE_CONSTANT_NEXT)==KFCURVE_CONSTANT_NEXT:
    #    return 2
    return 0

def TangeantmodeFlagToIndex(flags):
    #if (flags&KFCURVE_TANGEANT_AUTO) == KFCURVE_TANGEANT_AUTO:
    #    return 1
    #if (flags&KFCURVE_TANGEANT_AUTO_BREAK)==KFCURVE_TANGEANT_AUTO_BREAK:
    #    return 2
    #if (flags&KFCURVE_TANGEANT_TCB) == KFCURVE_TANGEANT_TCB:
    #    return 3
    #if (flags&KFCURVE_TANGEANT_USER) == KFCURVE_TANGEANT_USER:
    #    return 4
    #if (flags&KFCURVE_GENERIC_BREAK) == KFCURVE_GENERIC_BREAK:
    #    return 5
    #if (flags&KFCURVE_TANGEANT_BREAK) ==KFCURVE_TANGEANT_BREAK:
    #    return 6
    return 0

def TangeantweightFlagToIndex(flags):
    #if (flags&KFCURVE_WEIGHTED_NONE) == KFCURVE_WEIGHTED_NONE:
    #    return 1
    #if (flags&KFCURVE_WEIGHTED_RIGHT) == KFCURVE_WEIGHTED_RIGHT:
    #    return 2
    #if (flags&KFCURVE_WEIGHTED_NEXT_LEFT) == KFCURVE_WEIGHTED_NEXT_LEFT:
    #    return 3
    return 0

def TangeantVelocityFlagToIndex(flags):
    #if (flags&KFCURVE_VELOCITY_NONE) == KFCURVE_VELOCITY_NONE:
    #    return 1
    #if (flags&KFCURVE_VELOCITY_RIGHT) == KFCURVE_VELOCITY_RIGHT:
    #    return 2
    #if (flags&KFCURVE_VELOCITY_NEXT_LEFT) == KFCURVE_VELOCITY_NEXT_LEFT:
    #    return 3
    return 0

def DisplayCurveKeys(pCurve):
    interpolation = [ "?", "constant", "linear", "cubic"]
    constantMode =  [ "?", "Standard", "Next" ]
    cubicMode =     [ "?", "Auto", "Auto break", "Tcb", "User", "Break", "User break" ]
    tangentWVMode = [ "?", "None", "Right", "Next left" ]

    lKeyCount = pCurve.KeyGetCount()

    for lCount in range(lKeyCount):
        lTimeString = ""
        lKeyValue = pCurve.KeyGetValue(lCount)
        lKeyTime  = pCurve.KeyGetTime(lCount)

        lOutputString = "            Key Time: "
        lOutputString += lKeyTime.GetTimeString(lTimeString)
        lOutputString += ".... Key Value: "
        lOutputString += str(lKeyValue)
        lOutputString += " [ "
        lOutputString += interpolation[ InterpolationFlagToIndex(pCurve.KeyGetInterpolation(lCount)) ]
        #if (pCurve.KeyGetInterpolation(lCount)&KFCURVE_INTERPOLATION_CONSTANT) == KFCURVE_INTERPOLATION_CONSTANT:
        #    lOutputString += " | "
        #    lOutputString += constantMode[ ConstantmodeFlagToIndex(pCurve.KeyGetConstantMode(lCount)) ]
        #elif (pCurve.KeyGetInterpolation(lCount)&KFCURVE_INTERPOLATION_CUBIC) == KFCURVE_INTERPOLATION_CUBIC:
        #    lOutputString += " | "
        #    lOutputString += cubicMode[ TangeantmodeFlagToIndex(pCurve.KeyGetTangeantMode(lCount)) ]
        #    lOutputString += " | "
        #    lOutputString += tangentWVMode[ TangeantweightFlagToIndex(pCurve.KeyGetTangeantWeightMode(lCount)) ]
        #    lOutputString += " | "
        #    lOutputString += tangentWVMode[ TangeantVelocityFlagToIndex(pCurve.KeyGetTangeantVelocityMode(lCount)) ]
            
        lOutputString += " ]"
        print(lOutputString)

def DisplayCurveDefault(pCurve):
    lOutputString = "            Default Value: "
    lOutputString += pCurve.GetValue()
    
    print(lOutputString)

def DisplayListCurveKeys(pCurve, pProperty):
    lKeyCount = pCurve.KeyGetCount()

    for lCount in range(lKeyCount):
        lKeyValue = static_cast<int>(pCurve.KeyGetValue(lCount))
        lKeyTime  = pCurve.KeyGetTime(lCount)

        lOutputString = "            Key Time: "
        lOutputString += lKeyTime.GetTimeString(lTimeString)
        lOutputString += ".... Key Value: "
        lOutputString += lKeyValue
        lOutputString += " ("
        lOutputString += pProperty.GetEnumValue(lKeyValue)
        lOutputString += ")"

        print(lOutputString)

def DisplayListCurveDefault(pCurve, pProperty):
    DisplayCurveDefault(pCurve)

def DisplayGenericInfo(pScene):
    lRootNode = pScene.GetRootNode()

    for i in range(lRootNode.GetChildCount()):
        DisplayNodeGenericInfo(lRootNode.GetChild(i), 0)

    #Other objects directly connected onto the scene
    for i in range(pScene.GetSrcObjectCount(FbxObject.ClassId)):
        DisplayProperties(pScene.GetSrcObject(FbxObject.ClassId, i))

def DisplayNodeGenericInfo(pNode, pDepth):
    lString = ""
    for i in range(pDepth):
        lString += "     "

    lString += pNode.GetName()
    lString += "\n"

    DisplayString(lString)

    #Display generic info about that Node
    DisplayProperties(pNode)
    DisplayString("")
    for i in range(pNode.GetChildCount()):
        DisplayNodeGenericInfo(pNode.GetChild(i), pDepth + 1)

def DisplayProperties(pObject):
    DisplayString("Type: %s     Name: %s" % (pObject.ClassId.GetFbxFileTypeName(), pObject.GetName()))

    # Display all the properties
    lCount = 0
    lProperty = pObject.GetFirstProperty()
    while lProperty.IsValid():
        lCount += 1
        lProperty = pObject.GetNextProperty(lProperty)

    lTitleStr = "    Property Count: "

    if lCount == 0:
        return # there are no properties to display

    DisplayInt(lTitleStr, lCount)

    i=0
    lProperty = pObject.GetFirstProperty()
    while lProperty.IsValid():
        # exclude user properties
        DisplayInt("        Property ", i)
        lString = lProperty.GetLabel()
        DisplayString("            Display Name: ", lString.Buffer())
        lString = lProperty.GetName()
        DisplayString("            Internal Name: ", lString.Buffer())
        lString = lProperty.GetPropertyDataType().GetName()
        DisplayString("            Type: ", lString)
        if lProperty.HasMinLimit():
            DisplayDouble("            Min Limit: ", lProperty.GetMinLimit())
        if lProperty.HasMaxLimit():
            DisplayDouble("            Max Limit: ", lProperty.GetMaxLimit())
        DisplayBool  ("            Is Animatable: ", lProperty.GetFlag(FbxPropertyFlags.eAnimatable))

        if lProperty.GetPropertyDataType().GetType() == eFbxBool:
            lProperty = FbxPropertyBool1(lProperty)
            DisplayBool("            Default Value: ", lProperty.Get())
        elif lProperty.GetPropertyDataType().GetType() == eFbxDouble:
            lProperty = FbxPropertyDouble1(lProperty)
            DisplayDouble("            Default Value: ",lProperty.Get())
        elif lProperty.GetPropertyDataType().GetType() == eFbxDouble4:
            lProperty = FbxPropertyDouble4(lProperty)
            lDefault = lProperty.Get()
            lBuf = "R=%f, G=%f, B=%f, A=%f" % (lDefault[0], lDefault[1], lDefault[2], lDefault[3])
            DisplayString("            Default Value: ", lBuf)
        elif lProperty.GetPropertyDataType().GetType() == eFbxInt:
            lProperty = FbxPropertyInteger1(lProperty)
            DisplayInt("            Default Value: ", lProperty.Get())
        elif lProperty.GetPropertyDataType().GetType() == eFbxDouble3:
            lProperty = FbxPropertyDouble3(lProperty)
            lDefault = lProperty.Get()
            lBuf  = "X=%f, Y=%f, Z=%f" % (lDefault[0], lDefault[1], lDefault[2])
            DisplayString("            Default Value: ", lBuf)
        #case  DTEnum:
        #    DisplayInt("            Default Value: ", lProperty.Get())
        #    break

        elif lProperty.GetPropertyDataType().GetType() == eFbxFloat:
            lProperty = FbxPropertyFloat1(lProperty)
            DisplayDouble("            Default Value: ", lProperty.Get())
        elif lProperty.GetPropertyDataType().GetType() == eFbxString:
            lProperty = FbxPropertyString(lProperty)
            lString = lProperty.Get()
            DisplayString("            Default Value: ", lString.Buffer())
        else:
            DisplayString("            Default Value: UNIDENTIFIED")
        
        i += 1
        lProperty = pObject.GetNextProperty(lProperty)

if __name__ == "__main__":
	try:
		from FbxCommon import *
	except ImportError:
		import platform
		msg = 'You need to copy the content in compatible subfolder under /lib/python<version> into your python install folder such as '
		if platform.system() == 'Windows' or platform.system() == 'Microsoft':
			msg += '"Python26/Lib/site-packages"'
		elif platform.system() == 'Linux':
			msg += '"/usr/local/lib/python2.6/site-packages"'
		elif platform.system() == 'Darwin':
			msg += '"/Library/Frameworks/Python.framework/Versions/2.6/lib/python2.6/site-packages"'        
		msg += ' folder.'
		print(msg) 
		sys.exit(1)

	# Prepare the FBX SDK.
	sdk_manager, scene = InitializeSdkObjects()

	if len(sys.argv) > 1:
		print("Loading FBX: %s" % sys.argv[1])
		result = LoadScene(sdk_manager, scene, sys.argv[1])
	else :
		result = False

		print("Usage: fbxdump <FBX file name>")

	if not result:
		print("An error occurred while loading the scene...")
	else :
		DisplayMetaData(scene)

		print("\n\n---------------------\nGlobal Light Settings\n---------------------\n")
		DisplayGlobalLightSettings(scene)

		print("\n\n----------------------\nGlobal Camera Settings\n----------------------\n")
		DisplayGlobalCameraSettings(scene)

		print("\n\n--------------------\nGlobal Time Settings\n--------------------\n")
		DisplayGlobalTimeSettings(scene.GetGlobalSettings())

		print("\n\n---------\nHierarchy\n---------\n")
		DisplayHierarchy(scene)

		print("\n\n------------\nNode Content\n------------\n")
		DisplayContent(scene)

		print("\n\n----\nPose\n----\n")
		DisplayPose(scene)

		print("\n\n---------\nAnimation\n---------\n")
		DisplayAnimation(scene)

		#now display generic information
		print("\n\n---------\nGeneric Information\n---------\n")
		DisplayGenericInfo(scene)

	# Destroy all objects created by the FBX SDK.
	sdk_manager.Destroy()

	sys.exit(0)