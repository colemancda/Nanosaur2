/****************************/
/*      FILE ROUTINES       */
/* (c)2003 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

extern	short			gCurrentSong;
extern	short			gPrefsFolderVRefNum;
extern	long			gPrefsFolderDirID;
extern	FSSpec			gDataSpec;
extern	float			gDemoVersionTimer,gTerrainPolygonSize,gMapToUnitValue;
extern	float			**gMapYCoords,**gMapYCoordsOriginal;
extern	Byte			**gMapSplitMode;
extern	short			**gSuperTileTextureGrid;
extern	FenceDefType	*gFenceList;
extern	long			gNumFences,gNumSplines,gNumWaterPatches;
extern	int				gNumLineMarkers;
extern	MOMaterialObject	*gSuperTileTextureObjects[MAX_SUPERTILE_TEXTURES];
extern	PrefsType			gGamePrefs;
extern	AGLContext		gAGLContext;
extern	Boolean			gMuteMusicFlag,gMuteMusicFlag;
extern	WaterDefType	**gWaterListHandle, *gWaterList;
extern	Boolean			gPlayingFromSavedGame;
extern	LineMarkerDefType		gLineMarkerList[];
extern	Boolean					gDisableHiccupTimer;
extern	Movie				gSongMovie;

/****************************/
/*    PROTOTYPES            */
/****************************/

static void ReadDataFromSkeletonFile(SkeletonDefType *skeleton, FSSpec *fsSpec, int skeletonType, OGLSetupOutputType *setupInfo);
static void ReadDataFromPlayfieldFile(FSSpec *specPtr, OGLSetupOutputType *setupInfo);

static OSErr GetFileWithNavServices(FSSpec *documentFSSpec);
static OSErr PutFileWithNavServices(NavReplyRecord *reply, FSSpec *outSpec);

/****************************/
/*    CONSTANTS             */
/****************************/

#define	BASE_PATH_TILE		900					// tile # of 1st path tile

#define	PICT_HEADER_SIZE	512

#define	SKELETON_FILE_VERS_NUM	0x0110			// v1.1

#define	SAVE_GAME_VERSION	0x0100		// 1.0

		/* SAVE GAME */

typedef struct
{
	u_long		version;
	short		level;
	short		numLives;
	float		health;
	float		jetpackFuel;
	float		shieldPower;
	short		weaponQuantity[NUM_WEAPON_TYPES];
}SaveGameType;


		/* PLAYFIELD HEADER */

typedef struct
{
	NumVersion	version;							// version of file
	long		numItems;							// # items in map
	long		mapWidth;							// width of map
	long		mapHeight;							// height of map
	float		tileSize;							// 3D unit size of a tile
	float		minY,maxY;							// min/max height values
	long		numSplines;							// # splines
	long		numFences;							// # fences
	long		numUniqueSuperTiles;				// # unique supertile
	long        numWaterPatches;                    // # water patches
	long		numCheckpoints;						// # checkpoints
	long        unused[10];
}PlayfieldHeaderType;


		/* FENCE STRUCTURE IN FILE */
		//
		// note: we copy this data into our own fence list
		//		since the game uses a slightly different
		//		data structure.
		//

typedef	struct
{
	long		x,z;
}FencePointType;


typedef struct
{
	u_short			type;				// type of fence
	short			numNubs;			// # nubs in fence
	FencePointType	**nubList;			// handle to nub list
	Rect			bBox;				// bounding box of fence area
}FileFenceDefType;




/**********************/
/*     VARIABLES      */
/**********************/


float	g3DTileSize, g3DMinY, g3DMaxY;

static 	FSSpec		gSavedGameSpec;

static	Boolean		gTerrainIs32Bit = false;


/****************** SET DEFAULT DIRECTORY ********************/
//
// This function needs to be called for OS X because OS X doesnt automatically
// set the default directory to the application directory.
//

void SetDefaultDirectory(void)
{
ProcessSerialNumber serial;
ProcessInfoRec info;
FSSpec	app_spec;
static WDPBRec wpb;
OSErr	iErr;

	serial.highLongOfPSN = 0;
	serial.lowLongOfPSN = kCurrentProcess;


	info.processInfoLength = sizeof(ProcessInfoRec);
	info.processName = NULL;
	info.processAppSpec = &app_spec;

	iErr = GetProcessInformation(&serial, &info);

	wpb.ioVRefNum = &app_spec.vRefNum;
	wpb.ioWDDirID = &app_spec.parID;
	wpb.ioNamePtr = NULL;

	iErr = PBHSetVolSync(&wpb);


		/* ALSO SET SAVED GAME SPEC TO DESKTOP */

	iErr = FindFolder(kOnSystemDisk,kDesktopFolderType,kDontCreateFolder,			// locate the desktop folder
					&gSavedGameSpec.vRefNum,&gSavedGameSpec.parID);
	gSavedGameSpec.name[0] = 0;

}



/******************* LOAD SKELETON *******************/
//
// Loads a skeleton file & creates storage for it.
//
// NOTE: Skeleton types 0..NUM_CHARACTERS-1 are reserved for player character skeletons.
//		Skeleton types NUM_CHARACTERS and over are for other skeleton entities.
//
// OUTPUT:	Ptr to skeleton data
//

SkeletonDefType *LoadSkeletonFile(short skeletonType, OGLSetupOutputType *setupInfo)
{
QDErr		iErr;
short		fRefNum;
FSSpec		fsSpec;
SkeletonDefType	*skeleton;
const Str63	fileNames[MAX_SKELETON_TYPES] =
{
	"\p:Skeletons:nano.skeleton",
	"\p:Skeletons:wormhole.skeleton",
	"\p:Skeletons:raptor.skeleton",
	"\p:Skeletons:bonusworm.skeleton",
	"\p:Skeletons:brach.skeleton",
	"\p:Skeletons:worm.skeleton",
	"\p:Skeletons:ramphor.skeleton",
};


	if (skeletonType < MAX_SKELETON_TYPES)
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, fileNames[skeletonType], &fsSpec);
	else
		DoFatalAlert("\pLoadSkeleton: Unknown skeletonType!");


			/* OPEN THE FILE'S REZ FORK */

	fRefNum = FSpOpenResFile(&fsSpec,fsRdPerm);
	if (fRefNum == -1)
	{
		iErr = ResError();
		DoAlert("\pError opening Skel Rez file");
		ShowSystemErr(iErr);
	}

	UseResFile(fRefNum);
	if (ResError())
		DoFatalAlert("\pError using Rez file!");


			/* ALLOC MEMORY FOR SKELETON INFO STRUCTURE */

	skeleton = (SkeletonDefType *)AllocPtr(sizeof(SkeletonDefType));
	if (skeleton == nil)
		DoFatalAlert("\pCannot alloc SkeletonInfoType");


			/* READ SKELETON RESOURCES */

	ReadDataFromSkeletonFile(skeleton,&fsSpec,skeletonType,setupInfo);
	PrimeBoneData(skeleton);

			/* CLOSE REZ FILE */

	CloseResFile(fRefNum);

	return(skeleton);
}


/************* READ DATA FROM SKELETON FILE *******************/
//
// Current rez file is set to the file.
//

static void ReadDataFromSkeletonFile(SkeletonDefType *skeleton, FSSpec *fsSpec, int skeletonType, OGLSetupOutputType *setupInfo)
{
Handle				hand;
int					i,k,j;
long				numJoints,numAnims,numKeyframes;
SkeletonFile_Header_Type	*headerPtr;
short				version;
AliasHandle				alias;
OSErr					iErr;
FSSpec					target;
Boolean					wasChanged;
OGLPoint3D				*pointPtr;


			/************************/
			/* READ HEADER RESOURCE */
			/************************/

	hand = GetResource('Hedr',1000);
	if (hand == nil)
		DoFatalAlert("\pReadDataFromSkeletonFile: Error reading header resource!");
	headerPtr = (SkeletonFile_Header_Type *)*hand;
	version = SwizzleShort(&headerPtr->version);
	if (version != SKELETON_FILE_VERS_NUM)
		DoFatalAlert("\pSkeleton file has wrong version #");

	numAnims = skeleton->NumAnims = SwizzleShort(&headerPtr->numAnims);			// get # anims in skeleton
	numJoints = skeleton->NumBones = SwizzleShort(&headerPtr->numJoints);		// get # joints in skeleton
	ReleaseResource(hand);

	if (numJoints > MAX_JOINTS)										// check for overload
		DoFatalAlert("\pReadDataFromSkeletonFile: numJoints > MAX_JOINTS");


				/* ALLOCATE MEMORY FOR SKELETON DATA */

	AllocSkeletonDefinitionMemory(skeleton);



		/********************************/
		/* 	LOAD THE REFERENCE GEOMETRY */
		/********************************/

	alias = (AliasHandle)GetResource(rAliasType,1000);				// alias to geometry BG3D file
	if (alias != nil)
	{
		iErr = ResolveAlias(fsSpec, alias, &target, &wasChanged);	// try to resolve alias
		if (!iErr)
			LoadBonesReferenceModel(&target,skeleton, skeletonType, setupInfo);
		else
			DoFatalAlert("\pReadDataFromSkeletonFile: Cannot find Skeleton's BG3D file!");
 		ReleaseResource((Handle)alias);
	}
	else
		DoFatalAlert("\pReadDataFromSkeletonFile: file is missing the Alias resource");



		/***********************************/
		/*  READ BONE DEFINITION RESOURCES */
		/***********************************/

	for (i=0; i < numJoints; i++)
	{
		File_BoneDefinitionType	*bonePtr;
		u_short					*indexPtr;

			/* READ BONE DATA */

		hand = GetResource('Bone',1000+i);
		if (hand == nil)
			DoFatalAlert("\pError reading Bone resource!");
		HLock(hand);
		bonePtr = (File_BoneDefinitionType *)*hand;

			/* COPY BONE DATA INTO ARRAY */

		skeleton->Bones[i].parentBone = SwizzleLong(&bonePtr->parentBone);					// index to previous bone
		skeleton->Bones[i].coord.x = SwizzleFloat(&bonePtr->coord.x);						// absolute coord (not relative to parent!)
		skeleton->Bones[i].coord.y = SwizzleFloat(&bonePtr->coord.y);
		skeleton->Bones[i].coord.z = SwizzleFloat(&bonePtr->coord.z);
		skeleton->Bones[i].numPointsAttachedToBone = SwizzleUShort(&bonePtr->numPointsAttachedToBone);		// # vertices/points that this bone has
		skeleton->Bones[i].numNormalsAttachedToBone = SwizzleUShort(&bonePtr->numNormalsAttachedToBone);	// # vertex normals this bone has
		ReleaseResource(hand);

			/* ALLOC THE POINT & NORMALS SUB-ARRAYS */

		skeleton->Bones[i].pointList = (u_short *)AllocPtr(sizeof(u_short) * (int)skeleton->Bones[i].numPointsAttachedToBone);
		if (skeleton->Bones[i].pointList == nil)
			DoFatalAlert("\pReadDataFromSkeletonFile: AllocPtr/pointList failed!");

		skeleton->Bones[i].normalList = (u_short *)AllocPtr(sizeof(u_short) * (int)skeleton->Bones[i].numNormalsAttachedToBone);
		if (skeleton->Bones[i].normalList == nil)
			DoFatalAlert("\pReadDataFromSkeletonFile: AllocPtr/normalList failed!");

			/* READ POINT INDEX ARRAY */

		hand = GetResource('BonP',1000+i);
		if (hand == nil)
			DoFatalAlert("\pError reading BonP resource!");
		HLock(hand);
		indexPtr = (u_short *)(*hand);

			/* COPY POINT INDEX ARRAY INTO BONE STRUCT */

		for (j=0; j < skeleton->Bones[i].numPointsAttachedToBone; j++)
			skeleton->Bones[i].pointList[j] = SwizzleUShort(&indexPtr[j]);
		ReleaseResource(hand);


			/* READ NORMAL INDEX ARRAY */

		hand = GetResource('BonN',1000+i);
		if (hand == nil)
			DoFatalAlert("\pError reading BonN resource!");
		HLock(hand);
		indexPtr = (u_short *)(*hand);

			/* COPY NORMAL INDEX ARRAY INTO BONE STRUCT */

		for (j=0; j < skeleton->Bones[i].numNormalsAttachedToBone; j++)
			skeleton->Bones[i].normalList[j] = SwizzleUShort(&indexPtr[j]);
		ReleaseResource(hand);

	}


		/*******************************/
		/* READ POINT RELATIVE OFFSETS */
		/*******************************/
		//
		// The "relative point offsets" are the only things
		// which do not get rebuilt in the ModelDecompose function.
		// We need to restore these manually.

	hand = GetResource('RelP', 1000);
	if (hand == nil)
		DoFatalAlert("\pError reading RelP resource!");
	HLock(hand);
	pointPtr = (OGLPoint3D *)*hand;

	i = GetHandleSize(hand) / sizeof(OGLPoint3D);
	if (i != skeleton->numDecomposedPoints)
		DoFatalAlert("\p# of points in Reference Model has changed!");
	else
	{
		for (i = 0; i < skeleton->numDecomposedPoints; i++)
		{
			skeleton->decomposedPointList[i].boneRelPoint.x = SwizzleFloat(&pointPtr[i].x);
			skeleton->decomposedPointList[i].boneRelPoint.y = SwizzleFloat(&pointPtr[i].y);
			skeleton->decomposedPointList[i].boneRelPoint.z = SwizzleFloat(&pointPtr[i].z);
		}
	}
	ReleaseResource(hand);


			/*********************/
			/* READ ANIM INFO   */
			/*********************/

	for (i=0; i < numAnims; i++)
	{
		AnimEventType		*animEventPtr;
		SkeletonFile_AnimHeader_Type	*animHeaderPtr;

				/* READ ANIM HEADER */

		hand = GetResource('AnHd',1000+i);
		if (hand == nil)
			DoFatalAlert("\pError getting anim header resource");
		HLock(hand);
		animHeaderPtr = (SkeletonFile_AnimHeader_Type *)*hand;

		skeleton->NumAnimEvents[i] = SwizzleShort(&animHeaderPtr->numAnimEvents);			// copy # anim events in anim
		ReleaseResource(hand);

			/* READ ANIM-EVENT DATA */

		hand = GetResource('Evnt',1000+i);
		if (hand == nil)
			DoFatalAlert("\pError reading anim-event data resource!");
		animEventPtr = (AnimEventType *)*hand;
		for (j=0;  j < skeleton->NumAnimEvents[i]; j++)
		{
			skeleton->AnimEventsList[i][j] = *animEventPtr++;							// copy whole thing
			skeleton->AnimEventsList[i][j].time = SwizzleShort(&skeleton->AnimEventsList[i][j].time);	// then swizzle the 16-bit short value
		}
		ReleaseResource(hand);


			/* READ # KEYFRAMES PER JOINT IN EACH ANIM */

		hand = GetResource('NumK',1000+i);									// read array of #'s for this anim
		if (hand == nil)
			DoFatalAlert("\pError reading # keyframes/joint resource!");
		for (j=0; j < numJoints; j++)
			skeleton->JointKeyframes[j].numKeyFrames[i] = (*hand)[j];
		ReleaseResource(hand);
	}


	for (j=0; j < numJoints; j++)
	{
		JointKeyframeType	*keyFramePtr;

				/* ALLOC 2D ARRAY FOR KEYFRAMES */

		Alloc_2d_array(JointKeyframeType,skeleton->JointKeyframes[j].keyFrames,	numAnims,MAX_KEYFRAMES);

		if ((skeleton->JointKeyframes[j].keyFrames == nil) || (skeleton->JointKeyframes[j].keyFrames[0] == nil))
			DoFatalAlert("\pReadDataFromSkeletonFile: Error allocating Keyframe Array.");

					/* READ THIS JOINT'S KF'S FOR EACH ANIM */

		for (i=0; i < numAnims; i++)
		{
			numKeyframes = skeleton->JointKeyframes[j].numKeyFrames[i];					// get actual # of keyframes for this joint
			if (numKeyframes > MAX_KEYFRAMES)
				DoFatalAlert("\pError: numKeyframes > MAX_KEYFRAMES");

					/* READ A JOINT KEYFRAME */

			hand = GetResource('KeyF',1000+(i*100)+j);
			if (hand == nil)
				DoFatalAlert("\pError reading joint keyframes resource!");
			keyFramePtr = (JointKeyframeType *)*hand;
			for (k = 0; k < numKeyframes; k++)												// copy this joint's keyframes for this anim
			{
				skeleton->JointKeyframes[j].keyFrames[i][k].tick				= SwizzleLong(&keyFramePtr->tick);
				skeleton->JointKeyframes[j].keyFrames[i][k].accelerationMode	= SwizzleLong(&keyFramePtr->accelerationMode);
				skeleton->JointKeyframes[j].keyFrames[i][k].coord.x				= SwizzleFloat(&keyFramePtr->coord.x);
				skeleton->JointKeyframes[j].keyFrames[i][k].coord.y				= SwizzleFloat(&keyFramePtr->coord.y);
				skeleton->JointKeyframes[j].keyFrames[i][k].coord.z				= SwizzleFloat(&keyFramePtr->coord.z);
				skeleton->JointKeyframes[j].keyFrames[i][k].rotation.x			= SwizzleFloat(&keyFramePtr->rotation.x);
				skeleton->JointKeyframes[j].keyFrames[i][k].rotation.y			= SwizzleFloat(&keyFramePtr->rotation.y);
				skeleton->JointKeyframes[j].keyFrames[i][k].rotation.z			= SwizzleFloat(&keyFramePtr->rotation.z);
				skeleton->JointKeyframes[j].keyFrames[i][k].scale.x				= SwizzleFloat(&keyFramePtr->scale.x);
				skeleton->JointKeyframes[j].keyFrames[i][k].scale.y				= SwizzleFloat(&keyFramePtr->scale.y);
				skeleton->JointKeyframes[j].keyFrames[i][k].scale.z				= SwizzleFloat(&keyFramePtr->scale.z);

				keyFramePtr++;
			}
			ReleaseResource(hand);
		}
	}

}

#pragma mark -



/******************** LOAD PREFS **********************/
//
// Load in standard preferences
//

OSErr LoadPrefs(PrefsType *prefBlock)
{
OSErr		iErr;
short		refNum;
FSSpec		file;
long		count;
Boolean		retry;

				/*************/
				/* READ FILE */
				/*************/

try_again:

	retry = false;

	FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, "\p:Nanosaur2:Preferences", &file);
	iErr = FSpOpenDF(&file, fsRdPerm, &refNum);
	if (iErr)
		return(iErr);

	count = sizeof(PrefsType);
	iErr = FSRead(refNum, &count,  (Ptr)prefBlock);		// read data from file
	if (iErr)
	{
		FSClose(refNum);
		return(iErr);
	}

	FSClose(refNum);

			/****************/
			/* VERIFY PREFS */
			/****************/

	if ((gGamePrefs.depth != 16) && (gGamePrefs.depth != 32))
		goto err;

	if (gGamePrefs.version != CURRENT_PREFS_VERS)
	{
		retry = true;
		goto err;
	}

		/* THEY'RE GOOD, SO ALSO RESTORE THE HID CONTROL SETTINGS */

	RestoreHIDControlSettings(&gGamePrefs.controlSettings);

	return(noErr);

err:
	InitDefaultPrefs();
	SavePrefs();

	if (retry)
		goto try_again;

	return(noErr);
}



/******************** SAVE PREFS **********************/

void SavePrefs(void)
{
FSSpec				file;
OSErr				iErr;
short				refNum;
long				count;

		/* GET THE CURRENT CONTROL SETTINGS */

	if (!gHIDInitialized)								// can't save prefs unless HID is initialized!
		return;

	BuildHIDControlSettings(&gGamePrefs.controlSettings);


				/* CREATE BLANK FILE */

	FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, "\p:Nanosaur2:Preferences", &file);
	FSpDelete(&file);															// delete any existing file
	iErr = FSpCreate(&file, kGameID, 'Pref', smSystemScript);					// create blank file
	if (iErr)
		return;

				/* OPEN FILE */

	iErr = FSpOpenDF(&file, fsRdWrPerm, &refNum);
	if (iErr)
	{
		FSpDelete(&file);
		return;
	}

				/* WRITE DATA */

	count = sizeof(PrefsType);
	FSWrite(refNum, &count, &gGamePrefs);
	FSClose(refNum);


}

#pragma mark -




/**************** DRAW PICTURE INTO GWORLD ***********************/
//
// Uses Quicktime to load any kind of picture format file and draws
// it into the GWorld
//
//
// INPUT: myFSSpec = spec of image file
//
// OUTPUT:	theGWorld = gworld contining the drawn image.
//

OSErr DrawPictureIntoGWorld(FSSpec *myFSSpec, GWorldPtr *theGWorld, short depth)
{
OSErr						iErr;
GraphicsImportComponent		gi;
Rect						r;
ComponentResult				result;


			/* PREP IMPORTER COMPONENT */

	result = GetGraphicsImporterForFile(myFSSpec, &gi);		// load importer for this image file
	if (result != noErr)
	{
		DoAlert("\pDrawPictureIntoGWorld: GetGraphicsImporterForFile failed!  One of Quicktime's importer components is missing.  You should reinstall OS X to fix this.");
		return(result);
	}
	if (GraphicsImportGetBoundsRect(gi, &r) != noErr)		// get dimensions of image
		DoFatalAlert("\pDrawPictureIntoGWorld: GraphicsImportGetBoundsRect failed!");


			/* MAKE GWORLD */

	iErr = NewGWorld(theGWorld, depth, &r, nil, nil, 0);					// try app mem
	if (iErr)
	{
		DoAlert("\pDrawPictureIntoGWorld: using temp mem");
		iErr = NewGWorld(theGWorld, depth, &r, nil, nil, useTempMem);		// try sys mem
		if (iErr)
		{
			DoAlert("\pDrawPictureIntoGWorld: MakeMyGWorld failed");
			return(1);
		}
	}

//	if (depth == 32)
//	{
//		hPixMap = GetGWorldPixMap(*theGWorld);				// get gworld's pixmap
//		(**hPixMap).cmpCount = 4;							// we want full 4-component argb (defaults to only rgb)
//	}


			/* DRAW INTO THE GWORLD */

	DoLockPixels(*theGWorld);
	GraphicsImportSetGWorld(gi, *theGWorld, nil);			// set the gworld to draw image into
	GraphicsImportSetQuality(gi,codecLosslessQuality);		// set import quality

	result = GraphicsImportDraw(gi);						// draw into gworld
	CloseComponent(gi);										// cleanup
	if (result != noErr)
	{
		DoAlert("\pDrawPictureIntoGWorld: GraphicsImportDraw failed!");
		ShowSystemErr(result);
		DisposeGWorld (*theGWorld);
		*theGWorld= nil;
		return(result);
	}
	return(noErr);
}


#pragma mark -



/******************* GET DEMO TIMER *************************/

void GetDemoTimer(void)
{
	OSErr				iErr;
	short				refNum;
	FSSpec				file;
	long				count;

				/* READ TIMER FROM FILE */

	FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, "\pxFac00", &file);
	iErr = FSpOpenDF(&file, fsRdPerm, &refNum);
	if (iErr)
	{
		gDemoVersionTimer = 0;
	}
	else
	{
		count = sizeof(float);
		iErr = FSRead(refNum, &count,  &gDemoVersionTimer);			// read data from file
		if (iErr)
		{
			FSClose(refNum);
			FSpDelete(&file);										// file is corrupt, so delete
			gDemoVersionTimer = 0;
			return;
		}
		FSClose(refNum);
	}

//	{
//		Str255	s;
//
//		FloatToString(gDemoVersionTimer, s);
//		DoAlert(s);
//	}

		/* SEE IF TIMER HAS EXPIRED */

	if (gDemoVersionTimer > (60 * 100))								// let play for n minutes
	{
		DoDemoExpiredScreen();
	}
}


/************************ SAVE DEMO TIMER ******************************/

void SaveDemoTimer(void)
{
FSSpec				file;
OSErr				iErr;
short				refNum;
long				count;

				/* CREATE BLANK FILE */

	if (FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, "\pxFac00", &file) == noErr)
		FSpDelete(&file);														// delete any existing file
	iErr = FSpCreate(&file, '    ', 'xxxx', smSystemScript);					// create blank file
	if (iErr)
		return;


				/* OPEN FILE */

	iErr = FSpOpenDF(&file, fsRdWrPerm, &refNum);
	if (iErr)
		return;

				/* WRITE DATA */

	count = sizeof(float);
	FSWrite(refNum, &count, &gDemoVersionTimer);
	FSClose(refNum);
}


#pragma mark -

/******************* LOAD PLAYFIELD *******************/

void LoadPlayfield(FSSpec *specPtr, OGLSetupOutputType *setupInfo)
{

	gDisableHiccupTimer = true;

			/* READ PLAYFIELD RESOURCES */

	ReadDataFromPlayfieldFile(specPtr, setupInfo);


				/* DO ADDITIONAL SETUP */

	CreateSuperTileMemoryList(setupInfo);		// allocate memory for the supertile geometry
	CalculateSplitModeMatrix();					// precalc the tile split mode matrix
	InitSuperTileGrid();						// init the supertile state grid

	BuildTerrainItemList();						// build list of items & find player start coords


			/* CAST ITEM SHADOWS */

	DoItemShadowCasting(setupInfo);
}


/********************** READ DATA FROM PLAYFIELD FILE ************************/

static void ReadDataFromPlayfieldFile(FSSpec *specPtr, OGLSetupOutputType *setupInfo)
{
Handle					hand;
PlayfieldHeaderType		**header;
long					row,col,j,i,size, texSize;
float					yScale;
float					*src;
short					fRefNum;
OSErr					iErr;
Ptr						textureBuffer32 = nil;
Ptr						textureBuffer16 = nil;
GWorldPtr				buffGWorld = nil;
Rect					toRect, srcRect;

			/* USE 16-BIT IF IN LOW-QUALITY RENDER MODES OR LOW ON VRAM */

	if (gGamePrefs.lowRenderQuality || (gGamePrefs.depth != 32) || (gVRAMAfterBuffers < 0x6000000))
		gTerrainIs32Bit = false;
	else
		gTerrainIs32Bit = true;


				/* OPEN THE REZ-FORK */

	fRefNum = FSpOpenResFile(specPtr,fsCurPerm);
	if (fRefNum == -1)
		DoFatalAlert("\pLoadPlayfield: FSpOpenResFile failed.  You seem to have a corrupt or missing file.  Please reinstall the game.");
	UseResFile(fRefNum);


			/************************/
			/* READ HEADER RESOURCE */
			/************************/

	hand = GetResource('Hedr',1000);
	if (hand == nil)
	{
		DoAlert("\pReadDataFromPlayfieldFile: Error reading header resource!");
		return;
	}

	header = (PlayfieldHeaderType **)hand;
	gNumTerrainItems		= SwizzleLong(&(**header).numItems);
	gTerrainTileWidth		= SwizzleLong(&(**header).mapWidth);
	gTerrainTileDepth		= SwizzleLong(&(**header).mapHeight);
	g3DTileSize				= SwizzleFloat(&(**header).tileSize);
	g3DMinY					= SwizzleFloat(&(**header).minY);
	g3DMaxY					= SwizzleFloat(&(**header).maxY);
	gNumSplines				= SwizzleLong(&(**header).numSplines);
	gNumFences				= SwizzleLong(&(**header).numFences);
	gNumWaterPatches		= SwizzleLong(&(**header).numWaterPatches);
	gNumUniqueSuperTiles	= SwizzleLong(&(**header).numUniqueSuperTiles);
	gNumLineMarkers			= SwizzleLong(&(**header).numCheckpoints);

	ReleaseResource(hand);

	if ((gTerrainTileWidth % SUPERTILE_SIZE) != 0)		// terrain must be non-fractional number of supertiles in w/h
		DoFatalAlert("\pReadDataFromPlayfieldFile: terrain width not a supertile multiple");
	if ((gTerrainTileDepth % SUPERTILE_SIZE) != 0)
		DoFatalAlert("\pReadDataFromPlayfieldFile: terrain depth not a supertile multiple");


				/* CALC SOME GLOBALS HERE */

	gTerrainTileWidth = (gTerrainTileWidth/SUPERTILE_SIZE)*SUPERTILE_SIZE;		// round size down to nearest supertile multiple
	gTerrainTileDepth = (gTerrainTileDepth/SUPERTILE_SIZE)*SUPERTILE_SIZE;
	gTerrainUnitWidth = gTerrainTileWidth*gTerrainPolygonSize;					// calc world unit dimensions of terrain
	gTerrainUnitDepth = gTerrainTileDepth*gTerrainPolygonSize;
	gNumSuperTilesDeep = gTerrainTileDepth/SUPERTILE_SIZE;						// calc size in supertiles
	gNumSuperTilesWide = gTerrainTileWidth/SUPERTILE_SIZE;


			/*******************************/
			/* SUPERTILE RELATED RESOURCES */
			/*******************************/

			/* READ SUPERTILE GRID MATRIX */

	if (gSuperTileTextureGrid)														// free old array
		Free_2d_array(gSuperTileTextureGrid);
	Alloc_2d_array(short, gSuperTileTextureGrid, gNumSuperTilesDeep, gNumSuperTilesWide);

	hand = GetResource('STgd',1000);												// load grid from rez
	if (hand == nil)
		DoFatalAlert("\pReadDataFromPlayfieldFile: Error reading supertile rez resource!");
	else																			// copy rez into 2D array
	{
		short *src = (short *)*hand;

		for (row = 0; row < gNumSuperTilesDeep; row++)
			for (col = 0; col < gNumSuperTilesWide; col++)
			{
				gSuperTileTextureGrid[row][col] = SwizzleShort(src++);
			}
		ReleaseResource(hand);
	}


			/* READ HEIGHT DATA MATRIX */

	yScale = gTerrainPolygonSize / g3DTileSize;											// need to scale original geometry units to game units

	Alloc_2d_array(float, gMapYCoords, gTerrainTileDepth+1, gTerrainTileWidth+1);			// alloc 2D array for map
	Alloc_2d_array(float, gMapYCoordsOriginal, gTerrainTileDepth+1, gTerrainTileWidth+1);	// and the copy of it

	hand = GetResource('YCrd',1000);
	if (hand == nil)
		DoAlert("\pReadDataFromPlayfieldFile: Error reading height data resource!");
	else
	{
		src = (float *)*hand;
		for (row = 0; row <= gTerrainTileDepth; row++)
			for (col = 0; col <= gTerrainTileWidth; col++)
			{
				gMapYCoordsOriginal[row][col] = gMapYCoords[row][col] = SwizzleFloat(src++) * yScale;
			}
		ReleaseResource(hand);
	}

				/**************************/
				/* ITEM RELATED RESOURCES */
				/**************************/

				/* READ ITEM LIST */

	hand = GetResource('Itms',1000);
	if (hand == nil)
		DoFatalAlert("\pReadDataFromPlayfieldFile: Error reading itemlist resource!");
	else
	{
		File_TerrainItemEntryType   *rezItems;
		HLock(hand);
		rezItems = (File_TerrainItemEntryType *)*hand;


					/* COPY INTO OUR STRUCT */

		gMasterItemList = AllocPtr(sizeof(TerrainItemEntryType) * gNumTerrainItems);			// alloc array of items

		for (i = 0; i < gNumTerrainItems; i++)
		{
			gMasterItemList[i].x = SwizzleULong(&rezItems[i].x) * gMapToUnitValue;								// convert coordinates
			gMasterItemList[i].y = SwizzleULong(&rezItems[i].y) * gMapToUnitValue;

			gMasterItemList[i].type = SwizzleUShort(&rezItems[i].type);
			gMasterItemList[i].parm[0] = rezItems[i].parm[0];
			gMasterItemList[i].parm[1] = rezItems[i].parm[1];
			gMasterItemList[i].parm[2] = rezItems[i].parm[2];
			gMasterItemList[i].parm[3] = rezItems[i].parm[3];
			gMasterItemList[i].flags = SwizzleUShort(&rezItems[i].flags);
		}

		ReleaseResource(hand);																// nuke the rez
	}



			/****************************/
			/* SPLINE RELATED RESOURCES */
			/****************************/

			/* READ SPLINE LIST */

	hand = GetResource('Spln',1000);
	if (hand)
	{
		SplineDefType	*splinePtr = (SplineDefType *)*hand;

		gSplineList = AllocPtr(sizeof(SplineDefType) * gNumSplines);				// allocate memory for spline data

		for (i = 0; i < gNumSplines; i++)
		{
			gSplineList[i].numNubs = SwizzleShort(&splinePtr[i].numNubs);
			gSplineList[i].numPoints = SwizzleLong(&splinePtr[i].numPoints);
			gSplineList[i].numItems = SwizzleShort(&splinePtr[i].numItems);

			gSplineList[i].bBox.top = SwizzleShort(&splinePtr[i].bBox.top);
			gSplineList[i].bBox.bottom = SwizzleShort(&splinePtr[i].bBox.bottom);
			gSplineList[i].bBox.left = SwizzleShort(&splinePtr[i].bBox.left);
			gSplineList[i].bBox.right = SwizzleShort(&splinePtr[i].bBox.right);
		}

		ReleaseResource(hand);																// nuke the rez
	}
	else
	{
		gNumSplines = 0;
		gSplineList = nil;
	}




			/* READ SPLINE POINT LIST */

	for (i = 0; i < gNumSplines; i++)
	{
		SplineDefType	*spline = &gSplineList[i];									// point to Nth spline

		hand = GetResource('SpPt',1000+i);											// read this point list
		if (hand)
		{
			SplinePointType	*ptList = (SplinePointType *)*hand;

			spline->pointList = AllocPtr(sizeof(SplinePointType) * spline->numPoints);	// alloc memory for point list

			for (j = 0; j < spline->numPoints; j++)			// swizzle
			{
				spline->pointList[j].x = SwizzleFloat(&ptList[j].x);
				spline->pointList[j].z = SwizzleFloat(&ptList[j].z);
			}
			ReleaseResource(hand);																// nuke the rez
		}
		else
			DoFatalAlert("\pReadDataFromPlayfieldFile: cant get spline points rez");
	}


			/* READ SPLINE ITEM LIST */

	for (i = 0; i < gNumSplines; i++)
	{
		SplineDefType	*spline = &gSplineList[i];									// point to Nth spline

		hand = GetResource('SpIt',1000+i);
		if (hand)
		{
			SplineItemType	*itemList = (SplineItemType *)*hand;

			spline->itemList = AllocPtr(sizeof(SplineItemType) * spline->numItems);	// alloc memory for item list

			for (j = 0; j < spline->numItems; j++)			// swizzle
			{
				spline->itemList[j].placement = SwizzleFloat(&itemList[j].placement);
				spline->itemList[j].type	= SwizzleUShort(&itemList[j].type);
				spline->itemList[j].flags	= SwizzleUShort(&itemList[j].flags);
			}
			ReleaseResource(hand);																// nuke the rez
		}
		else
			DoFatalAlert("\pReadDataFromPlayfieldFile: cant get spline items rez");
	}

			/****************************/
			/* FENCE RELATED RESOURCES */
			/****************************/

			/* READ FENCE LIST */

	hand = GetResource('Fenc',1000);
	if (hand)
	{
		FileFenceDefType *inData;

		gFenceList = (FenceDefType *)AllocPtr(sizeof(FenceDefType) * gNumFences);	// alloc new ptr for fence data
		if (gFenceList == nil)
			DoFatalAlert("\pReadDataFromPlayfieldFile: AllocPtr failed");

		inData = (FileFenceDefType *)*hand;								// get ptr to input fence list

		for (i = 0; i < gNumFences; i++)								// copy data from rez to new list
		{
			gFenceList[i].type 		= SwizzleUShort(&inData[i].type);
			gFenceList[i].numNubs 	= SwizzleShort(&inData[i].numNubs);
			gFenceList[i].nubList 	= nil;
			gFenceList[i].sectionVectors = nil;
		}
		ReleaseResource(hand);
	}
	else
		gNumFences = 0;


			/* READ FENCE NUB LIST */

	for (i = 0; i < gNumFences; i++)
	{
		hand = GetResource('FnNb',1000+i);					// get rez
		HLock(hand);
		if (hand)
		{
   			FencePointType *fileFencePoints = (FencePointType *)*hand;

			gFenceList[i].nubList = (OGLPoint3D *)AllocPtr(sizeof(FenceDefType) * gFenceList[i].numNubs);	// alloc new ptr for nub array
			if (gFenceList[i].nubList == nil)
				DoFatalAlert("\pReadDataFromPlayfieldFile: AllocPtr failed");



			for (j = 0; j < gFenceList[i].numNubs; j++)		// convert x,z to x,y,z
			{
				gFenceList[i].nubList[j].x = SwizzleLong(&fileFencePoints[j].x);
				gFenceList[i].nubList[j].z = SwizzleLong(&fileFencePoints[j].z);
				gFenceList[i].nubList[j].y = 0;
			}
			ReleaseResource(hand);
		}
		else
			DoFatalAlert("\pReadDataFromPlayfieldFile: cant get fence nub rez");
	}


			/****************************/
			/* WATER RELATED RESOURCES */
			/****************************/


			/* READ WATER LIST */

	hand = GetResource('Liqd',1000);
	if (hand)
	{
		DetachResource(hand);
		HLockHi(hand);
		gWaterListHandle = (WaterDefType **)hand;
		gWaterList = *gWaterListHandle;

		for (i = 0; i < gNumWaterPatches; i++)						// swizzle
		{
			gWaterList[i].type = SwizzleUShort(&gWaterList[i].type);
			gWaterList[i].flags = SwizzleULong(&gWaterList[i].flags);
			gWaterList[i].height = SwizzleLong(&gWaterList[i].height);
			gWaterList[i].numNubs = SwizzleShort(&gWaterList[i].numNubs);

			gWaterList[i].hotSpotX = SwizzleFloat(&gWaterList[i].hotSpotX);
			gWaterList[i].hotSpotZ = SwizzleFloat(&gWaterList[i].hotSpotZ);

			gWaterList[i].bBox.top = SwizzleShort(&gWaterList[i].bBox.top);
			gWaterList[i].bBox.bottom = SwizzleShort(&gWaterList[i].bBox.bottom);
			gWaterList[i].bBox.left = SwizzleShort(&gWaterList[i].bBox.left);
			gWaterList[i].bBox.right = SwizzleShort(&gWaterList[i].bBox.right);

			for (j = 0; j < gWaterList[i].numNubs; j++)
			{
				gWaterList[i].nubList[j].x = SwizzleFloat(&gWaterList[i].nubList[j].x);
				gWaterList[i].nubList[j].y = SwizzleFloat(&gWaterList[i].nubList[j].y);
			}
		}
	}
	else
		gNumWaterPatches = 0;


			/*************************/
			/* LINE MARKER RESOURCES */
			/*************************/

	if (gNumLineMarkers > 0)
	{
		if (gNumLineMarkers > MAX_LINEMARKERS)
			DoFatalAlert("\pReadDataFromPlayfieldFile: gNumLineMarkers > MAX_LINEMARKERS");

				/* READ CHECKPOINT LIST */

		hand = GetResource('CkPt',1000);
		if (hand)
		{
			HLock(hand);
			BlockMove(*hand, &gLineMarkerList[0], GetHandleSize(hand));
			ReleaseResource(hand);

						/* CONVERT COORDINATES */

			for (i = 0; i < gNumLineMarkers; i++)
			{
				LineMarkerDefType	*lm = &gLineMarkerList[i];

				lm->infoBits = SwizzleShort(&gLineMarkerList[i].infoBits);			// swizzle data
				lm->x[0] = SwizzleFloat(&lm->x[0]);
				lm->x[1] = SwizzleFloat(&lm->x[1]);
				lm->z[0] = SwizzleFloat(&lm->z[0]);
				lm->z[1] = SwizzleFloat(&lm->z[1]);

				gLineMarkerList[i].x[0] *= gMapToUnitValue;
				gLineMarkerList[i].z[0] *= gMapToUnitValue;
				gLineMarkerList[i].x[1] *= gMapToUnitValue;
				gLineMarkerList[i].z[1] *= gMapToUnitValue;
			}
		}
		else
			gNumLineMarkers = 0;
	}



			/* CLOSE REZ FILE */

	CloseResFile(fRefNum);



		/********************************************/
		/* READ SUPERTILE IMAGE DATA FROM DATA FORK */
		/********************************************/

				/* ALLOC BUFFERS */
	if (gLowRAM)
		texSize = SUPERTILE_TEXMAP_SIZE / 4;
	else
		texSize = SUPERTILE_TEXMAP_SIZE;


	if (gTerrainIs32Bit)
		textureBuffer32 = AllocPtr(texSize * texSize * 4);		// alloc for 32bit pixels
	else
		textureBuffer16 = AllocPtr(texSize * texSize * 2);		// alloc for 16bit pixels



			/* MAKE GWORLD FROM BUFFER */

	toRect.left = toRect.top = 0;
	toRect.right = toRect.bottom = texSize;


	if (gTerrainIs32Bit)
		iErr = QTNewGWorldFromPtr(&buffGWorld, k32ARGBPixelFormat, &toRect, nil, nil, 0, textureBuffer32, texSize * 4);
	else
		iErr = QTNewGWorldFromPtr(&buffGWorld, k16BE555PixelFormat, &toRect, nil, nil, 0, textureBuffer16, texSize * 2);

	if (iErr || (buffGWorld == nil))
		DoFatalAlert("\pReadDataFromPlayfieldFile: QTNewGWorldFromPtr failed.");


	srcRect.left = srcRect.top = 0;
	srcRect.right = srcRect.bottom = SUPERTILE_TEXMAP_SIZE;





				/* OPEN THE DATA FORK */

	iErr = FSpOpenDF(specPtr, fsCurPerm, &fRefNum);
	if (iErr)
		DoFatalAlert("\pReadDataFromPlayfieldFile: FSpOpenDF failed!");


	for (i = 0; i < gNumUniqueSuperTiles; i++)
	{
		long					dataSize, descSize;
		MOMaterialData			matData;
		Ptr						tempBuff, imageDataPtr;
		ImageDescriptionHandle	imageDescHandle;
		ImageDescriptionPtr		imageDescPtr;


				/* READ THE SIZE OF THE NEXT COMPRESSED SUPERTILE TEXTURE */

		size = sizeof(long);
		iErr = FSRead(fRefNum, &size, &dataSize);
		if (iErr)
			DoFatalAlert("\pReadDataFromPlayfieldFile: FSRead failed!");

		dataSize = SwizzleLong(&dataSize);

		tempBuff = AllocPtr(dataSize);


				/* READ THE IMAGE DESC DATA + THE COMPRESSED IMAGE DATA */

		size = dataSize;
		iErr = FSRead(fRefNum, &size, tempBuff);
		if (iErr)
			DoFatalAlert("\pReadDataFromPlayfieldFile: FSRead failed!");


				/* EXTRACT THE IMAGE DESC */

		imageDescPtr = (ImageDescriptionPtr)tempBuff;								// create ptr into buffer to fake the imagedesc
		descSize = imageDescPtr->idSize;											// get size of the imagedesc data
		descSize = SwizzleLong(&descSize);

		imageDescHandle = (ImageDescriptionHandle)AllocHandle(descSize);			// convert Image Desc into a "real" handle
		BlockMove(imageDescPtr, *imageDescHandle, descSize);



				/* DECOMPRESS THE IMAGE */

		imageDataPtr = tempBuff + descSize;											// compressed image data is after the imagedesc data

		DecompressJPEGToGWorld(imageDescHandle, imageDataPtr, buffGWorld, &srcRect);

		DisposeHandle((Handle)imageDescHandle);										// free our image desc handle
		SafeDisposePtr(tempBuff);													// free the temp buff
		tempBuff = nil;


		if (gTerrainIs32Bit)
		{
			SetAlphaInARGBBuffer(texSize, texSize, (u_long *)textureBuffer32);
			SwizzleARGBtoBGRA(texSize,texSize, (u_long *)textureBuffer32);
		}
		else
		{
			SetAlphaIn16BitBuffer(texSize, texSize, (u_short *)textureBuffer16);
		}




				/**************************/
				/* CREATE MATERIAL OBJECT */
				/**************************/

		if (gTerrainIs32Bit)
		{
			matData.pixelSrcFormat 	= GL_UNSIGNED_INT_8_8_8_8_REV;
			matData.pixelDstFormat 	= GL_RGBA8;
			matData.textureName[0] 	= OGL_TextureMap_Load(textureBuffer32, texSize, texSize,
													 GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, false);
		}
		else
		{
			matData.pixelSrcFormat 	= GL_UNSIGNED_SHORT_1_5_5_5_REV;
			matData.pixelDstFormat 	= GL_RGB5_A1;
			matData.textureName[0] 	= OGL_TextureMap_Load(textureBuffer16, texSize, texSize,
													 GL_RGBA, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, false);
		}


		matData.setupInfo				= setupInfo;								// remember which draw context this material is assigned to
		matData.flags 					= 	BG3D_MATERIALFLAG_CLAMP_U|
											BG3D_MATERIALFLAG_CLAMP_V|
											BG3D_MATERIALFLAG_TEXTURED;

		matData.multiTextureMode		= MULTI_TEXTURE_MODE_REFLECTIONSPHERE;
		matData.multiTextureCombine		= MULTI_TEXTURE_COMBINE_ADD;
		matData.diffuseColor.r			= 1;
		matData.diffuseColor.g			= 1;
		matData.diffuseColor.b			= 1;
		matData.diffuseColor.a			= 1;
		matData.numMipmaps				= 1;										// 1 texture
		matData.width					= texSize;
		matData.height					= texSize;
		matData.texturePixels[0] 		= nil;										// the original pixels are gone (or will be soon)
		gSuperTileTextureObjects[i] 	= MO_CreateNewObjectOfType(MO_TYPE_MATERIAL, 0, &matData);		// create the new object


			/* PRE-LOAD */
			//
			// By drawing a phony triangle using this texture we can get it pre-loaded into VRAM.
			//

		SetInfobarSpriteState(0);
		MO_DrawMaterial(gSuperTileTextureObjects[i], setupInfo);
		glBegin(GL_TRIANGLES);
		glVertex3f(0,1,0);
		glVertex3f(0,0,0);
		glVertex3f(1,0,0);
		glEnd();


			/* UPDATE LOADING THERMOMETER */

		if ((i & 0x3) == 0)
			DrawLoading((float)i / (float)(gNumUniqueSuperTiles));
	}

	DrawLoading(1.0);


			/* CLOSE THE FILE */

	FSClose(fRefNum);

	if (textureBuffer32)
		SafeDisposePtr(textureBuffer32);
	if (textureBuffer16)
		SafeDisposePtr(textureBuffer16);
	if (buffGWorld)
		DisposeGWorld(buffGWorld);
}


/************************** DECOMPRESS JPEG TO GWORLD ****************************/
//
// NOTE:  we're going to assume the ImageDescription is in PPC big-endian format, so we need to swizzzle everything.
//

void DecompressJPEGToGWorld(ImageDescriptionHandle imageDesc, Ptr compressedData, GWorldPtr gworld, Rect *srcRect)
{
OSErr				err;
Rect				bounds, bounds2;
PixMapHandle 		myPixMap;
GDHandle		oldGD;
GWorldPtr		oldGW;
ImageDescriptionPtr imageDescPtr = *imageDesc;

			/* SWIZZLE */

	imageDescPtr->idSize	= SwizzleLong(&imageDescPtr->idSize);			// idSize
	imageDescPtr->cType		= SwizzleULong(&imageDescPtr->cType);			// cType
	imageDescPtr->resvd1	= SwizzleLong(&imageDescPtr->resvd1);			// resvd1
	imageDescPtr->resvd2	= SwizzleShort(&imageDescPtr->resvd2);			// resvd2
	imageDescPtr->dataRefIndex = SwizzleShort(&imageDescPtr->dataRefIndex);			// dataRefIndex
	imageDescPtr->version	= SwizzleShort(&imageDescPtr->version);			// version
	imageDescPtr->revisionLevel = SwizzleShort(&imageDescPtr->revisionLevel);			// revisionLevel
	imageDescPtr->vendor	= SwizzleLong(&imageDescPtr->vendor);			// vendor
	imageDescPtr->temporalQuality = SwizzleULong(&imageDescPtr->temporalQuality);			// temporalQuality
	imageDescPtr->spatialQuality = SwizzleULong(&imageDescPtr->spatialQuality);			// spatialQuality
	imageDescPtr->width		= SwizzleShort(&imageDescPtr->width);			// width
	imageDescPtr->height	= SwizzleShort(&imageDescPtr->height);			// height
	imageDescPtr->hRes		= (Fixed)SwizzleULong((u_long *)&imageDescPtr->hRes);			// hRes
	imageDescPtr->vRes		= (Fixed)SwizzleULong((u_long *)&imageDescPtr->vRes);			// vRes
	imageDescPtr->dataSize	= SwizzleLong(&imageDescPtr->dataSize);			// dataSize
	imageDescPtr->frameCount = SwizzleShort(&imageDescPtr->frameCount);			// frameCount
	imageDescPtr->depth		= SwizzleShort(&imageDescPtr->depth);			// depth
	imageDescPtr->clutID	= SwizzleShort(&imageDescPtr->clutID);			// clutID


	GetGWorld (&oldGW,&oldGD);

	SetGWorld(gworld, nil);

	myPixMap = GetGWorldPixMap(gworld);							// get gworld's pixmap
	bounds2 = (**myPixMap).bounds;

	if (srcRect == nil)
		bounds = bounds2;
	else
		bounds = *srcRect;


				/* DECOMPRESS INTO THE GWORLD */

	err = DecompressImage(compressedData, imageDesc, myPixMap, &bounds, &bounds2, srcCopy , nil);
	if (err)
		DoFatalAlert("\pDecompressJPEGToGWorld: DecompressImage failed!");

	SetGWorld (oldGW,oldGD);
}





#pragma mark -


/******************** NAV SERVICES: GET DOCUMENT ***********************/

static OSErr GetFileWithNavServices(FSSpec *documentFSSpec)
{
NavDialogOptions 	dialogOptions;
AEDesc 				defaultLocation;
NavObjectFilterUPP filterProc 	= nil; //NewNavObjectFilterUPP(myFilterProc);
OSErr 				anErr 		= noErr;

			/* Specify default options for dialog box */

	anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)
	{
			/* Adjust the options to fit our needs */

		dialogOptions.dialogOptionFlags |= kNavSelectDefaultLocation;	// Set default location option
		dialogOptions.dialogOptionFlags ^= kNavAllowPreviews;			// Clear preview option
		dialogOptions.dialogOptionFlags ^= kNavAllowMultipleFiles;		// Clear multiple files option

		dialogOptions.location.h = dialogOptions.location.v = -1;		// use default position
		CopyPStr("\pSelect A Saved Game File", dialogOptions.windowTitle);

				/* make descriptor for default location */

		anErr = AECreateDesc(typeFSS,&gSavedGameSpec, sizeof(FSSpec), &defaultLocation);
//		if (anErr ==noErr)
		{
			/* Get 'open'resource.  A nil handle being returned is OK, this simply means no automatic file filtering. */

			static NavTypeList	typeList = {kGameID, 0, 1, 'N2sv'};		// set types to filter
			NavTypeListPtr 		typeListPtr = &typeList;
			NavReplyRecord 		reply;


			/* Call NavGetFile() with specified options and declare our app-defined functions and type list */

			anErr = NavGetFile(&defaultLocation, &reply, &dialogOptions, nil, nil, filterProc, &typeListPtr,nil);
			if ((anErr == noErr) && (reply.validRecord))
			{
					/* Deal with multiple file selection */

				long 	count;

				anErr = AECountItems(&(reply.selection),&count);


					/* Set up index for file list */

				if (anErr == noErr)
				{
					long i;

					for (i = 1; i <= count; i++)
					{
						AEKeyword 	theKeyword;
						DescType 	actualType;
						Size 		actualSize;

						/* Get a pointer to selected file */

						anErr = AEGetNthPtr(&(reply.selection), i, typeFSS,&theKeyword, &actualType,
											documentFSSpec, sizeof(FSSpec), &actualSize);
					}
				}


				/* Dispose of NavReplyRecord,resources,descriptors */

				anErr = NavDisposeReply(&reply);
			}

			(void)AEDisposeDesc(&defaultLocation);
		}
	}


		/* CLEAN UP */

	if (filterProc)
	{
		DisposeNavObjectFilterUPP(filterProc);
		filterProc = nil;
	}

	return anErr;
}


/********************** PUT FILE WITH NAV SERVICES *************************/

static OSErr PutFileWithNavServices(NavReplyRecord *reply, FSSpec *outSpec)
{
OSErr 				anErr 			= noErr;
NavDialogOptions 	dialogOptions;
OSType 				fileTypeToSave 	='PSav';
OSType 				creatorType 	= kGameID;
const Str255		name = "\pNanosaur 2 Saved Game";
AEDesc 				defaultLocation;

	anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)
	{
		CopyPStr(name, dialogOptions.savedFileName);					// set default name

		dialogOptions.location.h = dialogOptions.location.v = -1;		// use default position


				/* TRY TO CREATE DEFAULT LOCATION */

		AECreateDesc(typeFSS,&gSavedGameSpec, sizeof(gSavedGameSpec), &defaultLocation);

					/* PUT FILE */

		anErr = NavPutFile(&defaultLocation, reply, &dialogOptions, nil, fileTypeToSave, creatorType, nil);
		if ((anErr == noErr) && (reply->validRecord))
		{
			AEKeyword	theKeyword;
			DescType 	actualType;
			Size 		actualSize;

			anErr = AEGetNthPtr(&(reply->selection),1,typeFSS, &theKeyword,&actualType, outSpec, sizeof(FSSpec), &actualSize);
		}
	}

	return anErr;
}


#pragma mark -


/***************************** SAVE GAME ********************************/
//
// Returns true if saving was successful
//

Boolean SaveGame(void)
{
SaveGameType	saveData;
short			fRefNum;
FSSpec			*specPtr;
NavReplyRecord	navReply;
long			count, i;
Boolean			success = false;

	Enter2D();

			/*************************/
			/* CREATE SAVE GAME DATA */
			/*************************/
			//
			// Swizzle as we save so that we always save Big-Endian
			//

	saveData.version		= SAVE_GAME_VERSION;				// save file version #
	saveData.version = SwizzleULong(&saveData.version);

	saveData.level			= SwizzleShort(&gLevelNum);						// save @ beginning of next level
	saveData.numLives 		= SwizzleShort(&gPlayerInfo[0].numFreeLives);
	saveData.health			= SwizzleFloat(&gPlayerInfo[0].health);
	saveData.jetpackFuel	= SwizzleFloat(&gPlayerInfo[0].jetpackFuel);
	saveData.shieldPower	= SwizzleFloat(&gPlayerInfo[0].shieldPower);
	for (i = 0; i < NUM_WEAPON_TYPES; i++)
		saveData.weaponQuantity[i]	= SwizzleShort(&gPlayerInfo[0].weaponQuantity[i]);


		/*******************/
		/* DO NAV SERVICES */
		/*******************/

	if (PutFileWithNavServices(&navReply, &gSavedGameSpec))
		goto bail;
	specPtr = &gSavedGameSpec;
	if (navReply.replacing)										// see if delete old
		FSpDelete(specPtr);


				/* CREATE & OPEN THE REZ-FORK */

	if (FSpCreate(specPtr, kGameID,'N2sv',nil) != noErr)
	{
		DoAlert("\pError creating Save file");
		goto bail;
	}

	FSpOpenDF(specPtr,fsRdWrPerm, &fRefNum);
	if (fRefNum == -1)
	{
		DoAlert("\pError opening Save file");
		goto bail;
	}


				/* WRITE TO FILE */

	count = sizeof(SaveGameType);
	if (FSWrite(fRefNum, &count, (Ptr)&saveData) != noErr)
	{
		DoAlert("\pError writing Save file");
		FSClose(fRefNum);
		goto bail;
	}

			/* CLOSE FILE */

	FSClose(fRefNum);


			/* CLEANUP NAV SERVICES */

	NavCompleteSave(&navReply, kNavTranslateInPlace);

	success = true;

bail:
	NavDisposeReply(&navReply);
	HideCursor();
	Exit2D();
	return(success);
}


/***************************** LOAD SAVED GAME ********************************/

Boolean LoadSavedGame(void)
{
SaveGameType	saveData;
short			fRefNum;
long			count, i;
Boolean			success = false;
//short			oldSong;

//	oldSong = gCurrentSong;							// turn off playing music to get around bug in OS X
//	KillSong();

	Enter2D();
	MyFlushEvents();


				/* GET FILE WITH NAVIGATION SERVICES */

	if (GetFileWithNavServices(&gSavedGameSpec) != noErr)
		goto bail;


				/* OPEN THE REZ-FORK */

	FSpOpenDF(&gSavedGameSpec,fsRdPerm, &fRefNum);
	if (fRefNum == -1)
	{
		DoAlert("\pError opening Save file");
		goto bail;
	}

				/* READ FROM FILE */

	count = sizeof(SaveGameType);
	if (FSRead(fRefNum, &count, &saveData) != noErr)
	{
		DoAlert("\pError reading Save file");
		FSClose(fRefNum);
		goto bail;
	}

			/* CLOSE FILE */

	FSClose(fRefNum);


			/**********************/
			/* USE SAVE GAME DATA */
			/**********************/

	gLevelNum						= SwizzleShort(&saveData.level);
	gPlayerInfo[0].numFreeLives 	= SwizzleShort(&saveData.numLives);
	gPlayerInfo[0].health			= SwizzleFloat(&saveData.health);
	gPlayerInfo[0].jetpackFuel		= SwizzleFloat(&saveData.jetpackFuel);
	gPlayerInfo[0].shieldPower		= SwizzleFloat(&saveData.shieldPower);

	for (i = 0; i < NUM_WEAPON_TYPES; i++)
		gPlayerInfo[0].weaponQuantity[i] = SwizzleShort(&saveData.weaponQuantity[i]);


	success = true;


bail:
	Exit2D();
	HideCursor();

//	if (!success)								// user cancelled, so start song again before returning
//		PlaySong(oldSong, true);

	return(success);
}

