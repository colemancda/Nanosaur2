/****************************/
/*   	MISCSCREENS.C	    */
/* (c)2002 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "3DMath.h"

extern	float				gFramesPerSecondFrac,gFramesPerSecond;
extern	FSSpec		gDataSpec;
extern	NewObjectDefinitionType	gNewObjectDefinition;
extern	Boolean		gDisableAnimSounds;
extern	Boolean		gHIDInitialized;
extern	PrefsType	gGamePrefs;
extern	OGLPoint3D	gCoord;
extern	OGLSetupOutputType		*gGameViewInfoPtr;
extern	OGLVector3D	gDelta;
extern	CFBundleRef 		gBundle;
extern	IBNibRef 			gNibs;
extern	WindowRef 		gDialogWindow;
extern	EventHandlerUPP gWinEvtHandler;


/****************************/
/*    PROTOTYPES            */
/****************************/

static void DisplayPicture_Draw(OGLSetupOutputType *info);
static pascal OSStatus DoGameSettingsDialog_EventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData);

static void DrawLoading_Callback(OGLSetupOutputType *info);

static pascal OSStatus DoCheatDialog_EventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData);


/****************************/
/*    CONSTANTS             */
/****************************/



/*********************/
/*    VARIABLES      */
/*********************/

MOPictureObject 	*gBackgoundPicture = nil;

OGLSetupOutputType	*gScreenViewInfoPtr = nil;

float		gLoadingThermoPercent = 0;

static Boolean		gLanguageChanged = false;

static	Boolean		gDontShowDemoQuit = false;


/********************** DISPLAY PICTURE **************************/
//
// Displays a picture using OpenGL until the user clicks the mouse or presses any key.
// If showAndBail == true, then show it and bail out
//

void DisplayPicture(FSSpec *spec)
{
OGLSetupInputType	viewDef;
float	timeout = 40.0f;

	gNumPlayers = 1;									// make sure don't do split-screen


			/* SETUP VIEW */

	OGL_NewViewDef(&viewDef);

	viewDef.camera.hither 			= 10;
	viewDef.camera.yon 				= 3000;
	viewDef.view.clearColor.r 		= 0;
	viewDef.view.clearColor.g 		= 0;
	viewDef.view.clearColor.b		= 0;
	viewDef.styles.useFog			= false;

	OGL_SetupWindow(&viewDef, &gGameViewInfoPtr);



			/* CREATE BACKGROUND OBJECT */

	gBackgoundPicture = MO_CreateNewObjectOfType(MO_TYPE_PICTURE, (u_long)gGameViewInfoPtr, spec);
	if (!gBackgoundPicture)
		DoFatalAlert("\pDisplayPicture: MO_CreateNewObjectOfType failed");



		/***********/
		/* SHOW IT */
		/***********/


	MakeFadeEvent(true, 2.0);
	UpdateInput();
	CalcFramesPerSecond();

					/* MAIN LOOP */

		while(!Button())
		{
			CalcFramesPerSecond();
			MoveObjects();
			OGL_DrawScene(gGameViewInfoPtr, DisplayPicture_Draw);

			UpdateInput();
			if (AreAnyNewKeysPressed())
				break;

			timeout -= gFramesPerSecondFrac;
			if (timeout < 0.0f)
				break;
		}


			/* CLEANUP */

	DeleteAllObjects();
	MO_DisposeObjectReference(gBackgoundPicture);
	DisposeAllSpriteGroups();


			/* FADE OUT */

	GammaFadeOut();


	OGL_DisposeWindowSetup(&gGameViewInfoPtr);


}


/***************** DISPLAY PICTURE: DRAW *******************/

static void DisplayPicture_Draw(OGLSetupOutputType *info)
{
	MO_DrawObject(gBackgoundPicture, info);
	DrawObjects(info);
}


#pragma mark -

/************** DO LEGAL SCREEN *********************/

void DoLegalScreen(void)
{
FSSpec	spec;

	GammaFadeOut();

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:images:legal.gif", &spec);

	DisplayPicture(&spec);

}



#pragma mark -


/****************** DO DEMO EXPIRED SCREEN **************************/

void DoDemoExpiredScreen(void)
{
FSSpec	spec;

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:images:DemoExpired", &spec);
	DisplayPicture(&spec);

#if ALLOW_2P_INDEMO
	LaunchURL("\phttp://www.pangeasoft.net/nano2/buydotmac.html");
#else
	LaunchURL("\phttp://www.pangeasoft.net/nano2/buy.html");
#endif

	gDontShowDemoQuit = true;					// make sure we don't also get the Demo Quit screen

	CleanQuit();
}


/*************** SHOW DEMO QUIT SCREEN **********************/

void ShowDemoQuitScreen(void)
{
FSSpec	spec;

	SaveDemoTimer();						// make sure to save this before we bail


	if (gDontShowDemoQuit)
		return;

#if DEMO
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:images:DemoQuitSmall", &spec);
#else
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:images:DemoQuitFull", &spec);
#endif
	DisplayPicture(&spec);

	if (IsInternetAvailable())				// if we've got TCP connection then launch URL
	{
		CleanupDisplay();								// unloads Draw Sprocket
#if ALLOW_2P_INDEMO
		LaunchURL("\phttp://www.pangeasoft.net/nano2/buydotmac.html");
#else
		LaunchURL("\phttp://www.pangeasoft.net/nano2/buy.html");
#endif
	}
}

#pragma mark -

/********************* DO LEVEL CHEAT DIALOG **********************/

Boolean DoLevelCheatDialog(void)
{
OSErr			err;
EventHandlerRef	ref;
EventTypeSpec	list[] = { { kEventClassCommand,  kEventProcessCommand } };

	Enter2D();

    		/***************/
    		/* INIT DIALOG */
    		/***************/

				/* CREATE WINDOW FROM THE NIB */

    err = CreateWindowFromNib(gNibs, CFSTR ("Cheat"), &gDialogWindow);
	if (err)
		DoFatalAlert("\pDoLevelCheatDialog: CreateWindowFromNib failed!");


			/* CREATE NEW WINDOW EVENT HANDLER */

    gWinEvtHandler = NewEventHandlerUPP(DoCheatDialog_EventHandler);
    InstallWindowEventHandler(gDialogWindow, gWinEvtHandler, GetEventTypeCount(list), list, 0, &ref);


			/* PROCESS THE DIALOG */

    ShowWindow(gDialogWindow);
	RunAppModalLoopForWindow(gDialogWindow);


				/* CLEANUP */

	DisposeEventHandlerUPP (gWinEvtHandler);
	DisposeWindow (gDialogWindow);

	Exit2D();


	return(false);
}


/****************** DO CHEAT DIALOG EVENT HANDLER *************************/

static pascal OSStatus DoCheatDialog_EventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
#pragma unused (myHandler, userData)
OSStatus			result = eventNotHandledErr;
HICommand 			command;

	switch(GetEventKind(event))
	{

				/*******************/
				/* PROCESS COMMAND */
				/*******************/

		case	kEventProcessCommand:
				GetEventParameter (event, kEventParamDirectObject, kEventParamHICommand, NULL, sizeof(command), NULL, &command);
				switch(command.commandID)
				{
					case	'lev1':
							gLevelNum = 0;
		                    QuitAppModalLoopForWindow(gDialogWindow);
		                    break;

					case	'lev2':
							gLevelNum = 1;
		                    QuitAppModalLoopForWindow(gDialogWindow);
		                    break;

					case	'lev3':
							gLevelNum = 2;
		                    QuitAppModalLoopForWindow(gDialogWindow);
		                    break;
				}
				break;
    }

    return (result);
}



#pragma mark -

/********************** DO GAME SETTINGS DIALOG ************************/

void DoGameOptionsDialog(void)
{
OSErr			err;
EventTypeSpec	list[] = { { kEventClassCommand,  kEventProcessCommand } };
WindowRef 		dialogWindow = nil;
EventHandlerUPP winEvtHandler;
ControlID 		idControl;
ControlRef 		control;
EventHandlerRef	ref;

const char		*rezNames[MAX_LANGUAGES] =
{
	"Settings_English",
	"Settings_French",
	"Settings_German",
	"Settings_Spanish",
	"Settings_Italian",
	"Settings_Swedish",
	"Settings_Dutch",
};


	if (gGamePrefs.language >= MAX_LANGUAGES)		// verify prefs for the hell of it.
		InitDefaultPrefs();

	Enter2D();
	InitCursor();
	MyFlushEvents();


    		/***************/
    		/* INIT DIALOG */
    		/***************/

do_again:

	gLanguageChanged  = false;

			/* CREATE WINDOW FROM THE NIB */

    err = CreateWindowFromNib(gNibs,CFStringCreateWithCString(nil, rezNames[gGamePrefs.language],
    						kCFStringEncodingMacRoman), &dialogWindow);
	if (err)
		DoFatalAlert("\pDoGameOptionsDialog: CreateWindowFromNib failed!");

			/* CREATE NEW WINDOW EVENT HANDLER */

    winEvtHandler = NewEventHandlerUPP(DoGameSettingsDialog_EventHandler);
    InstallWindowEventHandler(dialogWindow, winEvtHandler, GetEventTypeCount(list), list, dialogWindow, &ref);


			/* SET "SHOW VIDEO" CHECKBOX */

    idControl.signature = 'vido';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.showScreenModeDialog);


			/* SET "KIDDIE MODE" CHECKBOX */

    idControl.signature = 'kidm';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.kiddieMode);


			/* SET "SHOW CROSSHAIRS" CHECKBOX */

    idControl.signature = 'targ';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.showTargetingCrosshairs);


			/* SET "VERT/HORIZ" BUTTONS */

    idControl.signature = '2pla';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, (gGamePrefs.splitScreenMode == SPLITSCREEN_MODE_HORIZ) + 1);


			/* SET LANGUAGE MENU */

    idControl.signature = 'lang';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.language + 1);



			/**********************/
			/* PROCESS THE DIALOG */
			/**********************/

    ShowWindow(dialogWindow);
	RunAppModalLoopForWindow(dialogWindow);


			/*********************/
			/* GET RESULT VALUES */
			/*********************/

			/* GET "SHOW VIDEO" CHECKBOX */

    idControl.signature = 'vido';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.showScreenModeDialog = GetControlValue(control);


			/* GET "SHOW CROSSHAIRS" CHECKBOX */

    idControl.signature = 'targ';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.showTargetingCrosshairs = GetControlValue(control);

			/* GET "KIDDIE MODE" CHECKBOX */

    idControl.signature = 'kidm';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.kiddieMode = GetControlValue(control);


			/* GET LANGUAGE */

    idControl.signature = 'lang';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.language = GetControlValue(control) - 1;


			/* GET PANE BUTTONS */

    idControl.signature = '2pla';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	if (GetControlValue(control) == 1)
		gGamePrefs.splitScreenMode = SPLITSCREEN_MODE_VERT;
	else
		gGamePrefs.splitScreenMode = SPLITSCREEN_MODE_HORIZ;

	if (gActiveSplitScreenMode != SPLITSCREEN_MODE_NONE)					// should we set the current active split mode?
	{
		gActiveSplitScreenMode = gGamePrefs.splitScreenMode;


		if (gGamePrefs.splitScreenMode == SPLITSCREEN_MODE_HORIZ)			// smaller FOV for wide panes
		{
			gGameViewInfoPtr->fov[0] =
			gGameViewInfoPtr->fov[1] = HORIZ_PANE_FOV;
		}
		else
			gGameViewInfoPtr->fov[0] =
			gGameViewInfoPtr->fov[1] = VERT_PANE_FOV;
	}

				/***********/
				/* CLEANUP */
				/***********/

	DisposeEventHandlerUPP (winEvtHandler);
	DisposeWindow (dialogWindow);

			/* IF IT WAS JUST A LANGUAGE CHANGE THEN GO BACK TO THE DIALOG */

	if (gLanguageChanged)
		goto do_again;


	HideCursor();
	Exit2D();
	SavePrefs();

	CalcFramesPerSecond();				// reset this so things dont go crazy when we return
	CalcFramesPerSecond();

}


/****************** DO GAME SETTINGS DIALOG EVENT HANDLER *************************/

static pascal OSStatus DoGameSettingsDialog_EventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
#pragma unused (myHandler, userData)
OSStatus			result = eventNotHandledErr;
HICommand 			command;

	switch(GetEventKind(event))
	{

				/*******************/
				/* PROCESS COMMAND */
				/*******************/

		case	kEventProcessCommand:
				GetEventParameter (event, kEventParamDirectObject, kEventParamHICommand, NULL, sizeof(command), NULL, &command);
				switch(command.commandID)
				{
							/* OK BUTTON */

					case	kHICommandOK:
		                    QuitAppModalLoopForWindow((WindowRef) userData);
		                    break;


							/* CONFIGURE INPUT BUTTON */

					case	'cnfg':
							DoInputConfigDialog();
							break;


							/* LANGUAGE */

					case	'lang':
							gLanguageChanged = true;
		                    QuitAppModalLoopForWindow((WindowRef) userData);
							break;

				}
				break;
    }

    return (result);
}


#if 0

/********************* DO GAME SETTINGS DIALOG *********************/

void DoGameOptionsDialog(void)
{
DialogRef 		myDialog;
DialogItemType	itemHit;
ControlRef		ctrlHandle;
Boolean			dialogDone;
Boolean			doAgain = false;

	if (gGamePrefs.language >= MAX_LANGUAGES)		// verify prefs for the hell of it.
		InitDefaultPrefs();

	Enter2D();


	InitCursor();

	MyFlushEvents();

	UseResFile(gMainAppRezFile);

do_again:

	myDialog = GetNewDialog(1000 + gGamePrefs.language,nil,MOVE_TO_FRONT);
	if (myDialog == nil)
	{
		DoAlert("\pDoGameOptionsDialog: GetNewDialog failed!");
		ShowSystemErr(gGamePrefs.language);
	}


				/* SET CONTROL VALUES */

	GetDialogItemAsControl(myDialog,4,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.kiddieMode);

	GetDialogItemAsControl(myDialog,6,&ctrlHandle);				// language
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_ENGLISH);
	GetDialogItemAsControl(myDialog,7,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_FRENCH);
	GetDialogItemAsControl(myDialog,8,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_GERMAN);
	GetDialogItemAsControl(myDialog,9,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_SPANISH);
	GetDialogItemAsControl(myDialog,10,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_ITALIAN);
	GetDialogItemAsControl(myDialog,11,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_SWEDISH);
	GetDialogItemAsControl(myDialog,12,&ctrlHandle);
	SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_DUTCH);

	AutoSizeDialog(myDialog);



				/* DO IT */

	dialogDone = false;
	while(dialogDone == false)
	{
		ModalDialog(nil, &itemHit);
		switch (itemHit)
		{
			case 	1:									// hit ok
					dialogDone = true;
					break;

			case	4:
					gGamePrefs.kiddieMode = !gGamePrefs.kiddieMode;
					GetDialogItemAsControl(myDialog,itemHit,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.kiddieMode);
					break;

			case	5:
//					ClearHighScores();
					break;

			case	6:
			case	7:
			case	8:
			case	9:
			case	10:
			case	11:
			case	12:
					gGamePrefs.language = LANGUAGE_ENGLISH + (itemHit - 6);
					GetDialogItemAsControl(myDialog,6,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_ENGLISH);
					GetDialogItemAsControl(myDialog,7,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_FRENCH);
					GetDialogItemAsControl(myDialog,8,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_GERMAN);
					GetDialogItemAsControl(myDialog,9,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_SPANISH);
					GetDialogItemAsControl(myDialog,10,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_ITALIAN);
					GetDialogItemAsControl(myDialog,11,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_SWEDISH);
					GetDialogItemAsControl(myDialog,12,&ctrlHandle);
					SetControlValue(ctrlHandle,gGamePrefs.language == LANGUAGE_DUTCH);

					doAgain = true;
					dialogDone  = true;
					break;

		}
	}

			/* CLEAN UP */

	DisposeDialog(myDialog);

	if (doAgain)								// see if need to do dialog again (after language switch)
	{
		doAgain = false;
		goto do_again;
	}

	HideCursor();

	Exit2D();


	SavePrefs();

	CalcFramesPerSecond();				// reset this so things dont go crazy when we return
	CalcFramesPerSecond();
}

#endif




#pragma mark -

/********************** DRAW LOADING *************************/

void DrawLoading(float percent)
{
	OGL_DrawScene(gGameViewInfoPtr, DrawLoading_Callback);

	gLoadingThermoPercent = percent;
}




/***************** DRAW LOADING CALLBACK *******************/

#define	THERMO_WIDTH		80.0f
#define	THERMO_HEIGHT		4.0f
#define	THERMO_Y			180.0f
#define	THERMO_LEFT			(320 - (THERMO_WIDTH/2))
#define	THERMO_RIGHT		(320 + (THERMO_WIDTH/2))

static void DrawLoading_Callback(OGLSetupOutputType *info)
{
float	w, x;

#pragma unused (info)

	if (gCurrentSplitScreenPane != 0)						// only show in player 1's pane
		return;

	SetInfobarSpriteState(0);

	DrawInfobarSprite_Centered(640/2, THERMO_Y-6, 100, INFOBAR_SObjType_Loading);

			/* DRAW THERMO FRAME */

	OGL_SetColor4f(.5,.5,.5,1);
	OGL_DisableTexture2D();

	glBegin(GL_QUADS);
	glVertex2f(THERMO_LEFT,THERMO_Y);					glVertex2f(THERMO_RIGHT,THERMO_Y);
	glVertex2f(THERMO_RIGHT,THERMO_Y+THERMO_HEIGHT);		glVertex2f(THERMO_LEFT,THERMO_Y+THERMO_HEIGHT);
	glEnd();


			/* DRAW THERMO METER */

	w = gLoadingThermoPercent * THERMO_WIDTH;
	x = THERMO_LEFT + w;

	if (gGamePrefs.stereoGlassesMode == STEREO_GLASSES_MODE_ANAGLYPH)
		OGL_SetColor4f(.3,.3,.2,1);
	else
		OGL_SetColor4f(.8,0,0,1);

	glBegin(GL_QUADS);
	glVertex2f(THERMO_LEFT,THERMO_Y);		glVertex2f(x,THERMO_Y);
	glVertex2f(x,THERMO_Y+THERMO_HEIGHT);	glVertex2f(THERMO_LEFT,THERMO_Y+THERMO_HEIGHT);
	glEnd();


	OGL_SetColor4f(1,1,1,1);


}









