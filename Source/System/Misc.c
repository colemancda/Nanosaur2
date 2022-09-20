/****************************/
/*      MISC ROUTINES       */
/* (c)2003 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/


#include "internet.h"


extern	Boolean		gHIDInitialized;
extern	OGLSetupOutputType		*gGameViewInfoPtr;
extern	int			gPolysThisFrame;
extern	AGLContext		gAGLContext;
extern	float			gDemoVersionTimer;
extern	short	gPrefsFolderVRefNum;
extern	long		gPrefsFolderDirID;
extern	PrefsType			gGamePrefs;
extern	IBNibRef 			gNibs;
extern	CFBundleRef 		gBundle;
extern	CGGammaValue gOriginalRedTable[256], gOriginalGreenTable[256], gOriginalBlueTable[256];
extern	Boolean				gPlayFullScreen;


/****************************/
/*    CONSTANTS             */
/****************************/

#define		ERROR_ALERT_ID		401

#define	MAX_FPS				190
#define	DEFAULT_FPS			13


/**********************/
/*     VARIABLES      */
/**********************/

long	gRAMAlloced = 0;

u_long 	gSeed0 = 0, gSeed1 = 0, gSeed2 = 0;

float	gFramesPerSecond, gFramesPerSecondFrac;

int		gNumPointers = 0;

Str255  gSerialFileName = "\p:Nanosaur2:Info";

Boolean	gGameIsRegistered = false;

unsigned char	gRegInfo[64];


WindowRef 		gDialogWindow = nil;
EventHandlerUPP gWinEvtHandler;


u_long			gSerialWasVerifiedMode = 0;

Boolean			gPanther = false;

Boolean			gLittleSnitch = false;

Boolean			gAltivec = false;

Boolean			gLowRAM = false;

/**********************/
/*     PROTOTYPES     */
/**********************/


#include "serialVerify.h"


/****************** DO SYSTEM ERROR ***************/

void ShowSystemErr(long err)
{
Str255		numStr;
SInt16      alertItemHit;

	Enter2D();
	NumToString(err, numStr);

	StandardAlert(kAlertStopAlert, numStr, NULL, NULL, &alertItemHit);

	Exit2D();

	CleanQuit();
}

/****************** DO SYSTEM ERROR : NONFATAL ***************/
//
// nonfatal
//
void ShowSystemErr_NonFatal(long err)
{
Str255		numStr;
SInt16      alertItemHit;

	Enter2D();
	NumToString(err, numStr);

	StandardAlert(kAlertStopAlert, numStr, NULL, NULL, &alertItemHit);

	Exit2D();
}


/*********************** DO ALERT *******************/

void DoAlert(Str255 s)
{
SInt16      alertItemHit;

	Enter2D();

	StandardAlert(kAlertStopAlert, s, NULL, NULL, &alertItemHit);

	Exit2D();
}




/*********************** DO FATAL ALERT *******************/

void DoFatalAlert(Str255 s)
{
SInt16      alertItemHit;

	Enter2D();

	StandardAlert(kAlertNoteAlert, s, NULL, NULL, &alertItemHit);

	Exit2D();
	CleanQuit();
}



/************ CLEAN QUIT ***************/

void CleanQuit(void)
{
static Boolean	beenHere = false;


	if (!beenHere)
	{
		beenHere = true;

		SavePrefs();									// save prefs before bailing

		DeleteAllObjects();
		DisposeTerrain();								// dispose of any memory allocated by terrain manager
		DisposeAllBG3DContainers();						// nuke all models
		DisposeAllSpriteGroups();						// nuke all sprites

		if (gGameViewInfoPtr)							// see if need to dispose this
			OGL_DisposeWindowSetup(&gGameViewInfoPtr);

		if (!gGameIsRegistered)
		{
			GammaFadeOut();
			ShowDemoQuitScreen();
		}

		ShutdownSound();								// cleanup sound stuff
	}

	CleanupDisplay();								// unloads Draw Sprocket

	if (gHIDInitialized)							// unload HID
	{
		ShutdownHID();
	}

	InitCursor();
	MyFlushEvents();

	if (gNibs != NULL)
		DisposeNibReference(gNibs);

	if (gBundle != NULL)
		CFRelease(gBundle);


			/* RESTORE ORIGINAL GAMMA */

	if (gPlayFullScreen)
		CGSetDisplayTransferByTable(0, 256, gOriginalRedTable, gOriginalGreenTable, gOriginalBlueTable);


	ExitToShell();
}



#pragma mark -


/******************** MY RANDOM LONG **********************/
//
// My own random number generator that returns a LONG
//
// NOTE: call this instead of MyRandomShort if the value is going to be
//		masked or if it just doesnt matter since this version is quicker
//		without the 0xffff at the end.
//

unsigned long MyRandomLong(void)
{
  return gSeed2 ^= (((gSeed1 ^= (gSeed2>>5)*1568397607UL)>>7)+
                   (gSeed0 = (gSeed0+1)*3141592621UL))*2435386481UL;
}


/************************* RANDOM RANGE *************************/
//
// THE RANGE *IS* INCLUSIVE OF MIN AND MAX
//

u_short	RandomRange(unsigned short min, unsigned short max)
{
u_short		qdRdm;											// treat return value as 0-65536
u_long		range, t;

	qdRdm = MyRandomLong();
	range = max+1 - min;
	t = (qdRdm * range)>>16;	 							// now 0 <= t <= range

	return( t+min );
}



/************** RANDOM FLOAT ********************/
//
// returns a random float between 0 and 1
//

float RandomFloat(void)
{
unsigned long	r;
float	f;

	r = MyRandomLong() & 0xfff;
	if (r == 0)
		return(0);

	f = (float)r;							// convert to float
	f = f * (1.0f/(float)0xfff);			// get # between 0..1
	return(f);
}


/************** RANDOM FLOAT 2 ********************/
//
// returns a random float between -1 and +1
//

float RandomFloat2(void)
{
unsigned long	r;
float	f;

	r = MyRandomLong() & 0xfff;
	if (r == 0)
		return(0);

	f = (float)r;							// convert to float
	f = f * (2.0f/(float)0xfff);			// get # between 0..2
	f -= 1.0f;								// get -1..+1
	return(f);
}



/**************** SET MY RANDOM SEED *******************/

void SetMyRandomSeed(unsigned long seed)
{
	gSeed0 = seed;
	gSeed1 = 0;
	gSeed2 = 0;

}

/**************** INIT MY RANDOM SEED *******************/

void InitMyRandomSeed(void)
{
	gSeed0 = 0x2a80ce30;
	gSeed1 = 0;
	gSeed2 = 0;
}


#pragma mark -


/******************* FLOAT TO STRING *******************/

void FloatToString(float num, Str255 string)
{
Str255	sf;
long	i,f;

	i = num;						// get integer part


	f = (fabs(num)-fabs((float)i)) * 10000;		// reduce num to fraction only & move decimal --> 5 places

	if ((i==0) && (num < 0))		// special case if (-), but integer is 0
	{
		string[0] = 2;
		string[1] = '-';
		string[2] = '0';
	}
	else
		NumToString(i,string);		// make integer into string

	NumToString(f,sf);				// make fraction into string

	string[++string[0]] = '.';		// add "." into string

	if (f >= 1)
	{
		if (f < 1000)
			string[++string[0]] = '0';	// add 1000's zero
		if (f < 100)
			string[++string[0]] = '0';	// add 100's zero
		if (f < 10)
			string[++string[0]] = '0';	// add 10's zero
	}

	for (i = 0; i < sf[0]; i++)
	{
		string[++string[0]] = sf[i+1];	// copy fraction into string
	}
}

/*********************** STRING TO FLOAT *************************/

float StringToFloat(Str255 textStr)
{
short	i;
short	length;
Byte	mode = 0;
long	integer = 0;
long	mantissa = 0,mantissaSize = 0;
float	f;
float	tens[8] = {1,10,100,1000,10000,100000,1000000,10000000};
char	c;
float	sign = 1;												// assume positive

	length = textStr[0];										// get string length

	if (length== 0)												// quick check for empty
		return(0);


			/* SCAN THE NUMBER */

	for (i = 1; i <= length; i++)
	{
		c  = textStr[i];										// get this char

		if (c == '-')											// see if negative
		{
			sign = -1;
			continue;
		}
		else
		if (c == '.')											// see if hit the decimal
		{
			mode = 1;
			continue;
		}
		else
		if ((c < '0') || (c > '9'))								// skip all but #'s
			continue;


		if (mode == 0)
			integer = (integer * 10) + (c - '0');
		else
		{
			mantissa = (mantissa * 10) + (c - '0');
			mantissaSize++;
		}
	}

			/* BUILT A FLOAT FROM IT */

	f = (float)integer + ((float)mantissa/tens[mantissaSize]);
	f *= sign;

	return(f);
}





#pragma mark -

/****************** ALLOC HANDLE ********************/

Handle	AllocHandle(long size)
{
Handle	hand;
OSErr	err;

	hand = NewHandle(size);							// alloc in APPL
	if (hand == nil)
	{
		DoAlert("\pAllocHandle: using temp mem");
		hand = TempNewHandle(size,&err);			// try TEMP mem
		if (hand == nil)
		{
			DoAlert("\pAllocHandle: failed!");
			return(nil);
		}
		else
			return(hand);
	}
	return(hand);

}



/****************** ALLOC PTR ********************/

void *AllocPtr(long size)
{
Ptr	pr;
u_long	*cookiePtr;

	size += 16;								// make room for our cookie & whatever else (also keep to 16-byte alignment!)

	pr = malloc(size);
	if (pr == nil)
		DoFatalAlert("\pAllocPtr: NewPtr failed");

	cookiePtr = (u_long *)pr;

	*cookiePtr++ = 'FACE';
	*cookiePtr++ = size;
	*cookiePtr++ = 'PTR3';
	*cookiePtr = 'PTR4';

	pr += 16;

	gNumPointers++;


	gRAMAlloced += size;

	return(pr);
}


/****************** ALLOC PTR CLEAR ********************/

void *AllocPtrClear(long size)
{
Ptr	pr;
u_long	*cookiePtr;

	size += 16;								// make room for our cookie & whatever else (also keep to 16-byte alignment!)

	pr = calloc(1, size);

	if (pr == nil)
		DoFatalAlert("\pAllocPtr: NewPtr failed");

	cookiePtr = (u_long *)pr;

	*cookiePtr++ = 'FACE';
	*cookiePtr++ = size;
	*cookiePtr++ = 'PTC3';
	*cookiePtr = 'PTC4';

	pr += 16;

	gNumPointers++;


	gRAMAlloced += size;

	return(pr);
}


/***************** SAFE DISPOSE PTR ***********************/

void SafeDisposePtr(void *ptr)
{
u_long	*cookiePtr;
Ptr		p = ptr;

	p -= 16;					// back up to pt to cookie

	cookiePtr = (u_long *)p;

	if (*cookiePtr != 'FACE')
		DoFatalAlert("\pSafeSafeDisposePtr: invalid cookie!");

	gRAMAlloced -= cookiePtr[1];

	*cookiePtr = 0;									// zap cookie

	free(p);

	gNumPointers--;

}



#pragma mark -

/******************* COPY P STRING ********************/

void CopyPString(Str255 from, Str255 to)
{
short	i,n;

	n = from[0];			// get length

	for (i = 0; i <= n; i++)
		to[i] = from[i];

}


/***************** P STRING TO C ************************/

void PStringToC(char *pString, char *cString)
{
Byte	pLength,i;

	pLength = pString[0];

	for (i=0; i < pLength; i++)					// copy string
		cString[i] = pString[i+1];

	cString[pLength] = 0x00;					// add null character to end of c string
}


/***************** DRAW C STRING ********************/

void DrawCString(char *string)
{
	while(*string != 0x00)
		DrawChar(*string++);
}


/******************* VERIFY SYSTEM ******************/

void VerifySystem(void)
{
OSErr	iErr;
NumVersion	vers;
u_long  flags;
long	response;




		/* CHECK MEMORY */


	{
		u_long	mem;
		iErr = Gestalt(gestaltPhysicalRAMSize,(long *)&mem);
		if (iErr == noErr)
		{
					/* CHECK FOR LOW-MEMORY SITUATIONS */

			mem /= 1024;
			mem /= 1024;
			if (mem <= 500)						// see if have < 500MB of real RAM or less installed
			{
				gLowRAM = true;
			}
		}
	}



		/* DETERMINE IF RUNNING ON OS X.2 */

	iErr = Gestalt(gestaltSystemVersion,(long *)&vers);
	if (iErr != noErr)
		DoFatalAlert("\pVerifySystem: gestaltSystemVersion failed!");

	if (vers.stage >= 0x10)													// see if at least OS 10
	{
		if ((vers.stage == 0x10) && (vers.nonRelRev < 0x20))				// must be at least OS 10.2 !!!
			DoFatalAlert("\pThis game requires at least MacOS 10.2.6.");

		if ((vers.stage == 0x10) && (vers.nonRelRev >= 0x30))				// see if 10.3 or higher
			gPanther = true;
		else
			gPanther = false;
	}
	else
		DoFatalAlert("\pThis game requires at least MacOS 10.2.6.");


		/**********************/
		/* SEE IF HAS ALTIVEC */
		/**********************/

	gAltivec = false;									// assume no altivec

#if __BIG_ENDIAN__						// temp for now

	if (!Gestalt(gestaltNativeCPUtype, &response))
	{
		if (response > gestaltCPU750)					// skip if on G3 or go into this if > G3
		{
			iErr = Gestalt(gestaltPowerPCProcessorFeatures,(long *)&flags);		// see if AltiVec available
			if (iErr != noErr)
				DoFatalAlert("\pVerifySystem: gestaltPowerPCProcessorFeatures failed!");
			gAltivec = ((flags & (1<<gestaltPowerPCHasVectorInstructions)) != 0);
		}
	}
#endif

		/***********************************/
		/* SEE IF LITTLE-SNITCH IS RUNNING */
		/***********************************/

#if (DEMO || OEM)
		gLittleSnitch = false;
#else
	{
		ProcessSerialNumber psn = {kNoProcess, kNoProcess};
		ProcessInfoRec	info;
		Str255		s;

		info.processName = s;
		info.processInfoLength = sizeof(ProcessInfoRec);
		info.processAppSpec = nil;

		while(GetNextProcess(&psn) == noErr)
		{
			char	pname[256];
			char	*matched;

			iErr = GetProcessInformation(&psn, &info);
			if (iErr)
				break;

			p2cstrcpy(pname, &s[0]);					// convert pstring to cstring

			matched = strstr (pname, "Snitc");			// does "Snitc" appear anywhere in the process name?
			if (matched != nil)
			{
				gLittleSnitch = true;
				break;
			}
		}
	}
#endif


		/***************************************/
		/* SEE IF QUICKEN SCHEDULER IS RUNNING */
		/***************************************/

	{
		ProcessSerialNumber psn = {kNoProcess, kNoProcess};
		ProcessInfoRec	info;
		short			i;
		Str255		s;
		const char snitch[] = "\pQuicken Scheduler";

		info.processName = s;
		info.processInfoLength = sizeof(ProcessInfoRec);
		info.processAppSpec = nil;

		while(GetNextProcess(&psn) == noErr)
		{
			iErr = GetProcessInformation(&psn, &info);
			if (iErr)
				break;

			if (s[0] != snitch[0])					// see if string matches
				goto next_process2;

			for (i = 1; i <= s[0]; i++)
			{
				if (s[i] != snitch[i])
					goto next_process2;
			}

			DoAlert("\pIMPORTANT:  Quicken Scheduler is known to cause certain keyboard access functions in OS X to malfunction.  If the keyboard does not appear to be working in this game, quit Quicken Scheduler to fix it.");

next_process2:;
		}
	}




#if 0
			/* CHECK TIME-BOMB */
	{
		unsigned long secs;
		DateTimeRec	d;

		GetDateTime(&secs);
		SecondsToDate(secs, &d);

		if ((d.year > 2004) ||
			((d.year == 2004) && (d.month > 2)))
		{
			DoFatalAlert("\pSorry, but this beta has expired");
		}
	}
#endif

}


/******************** REGULATE SPEED ***************/

void RegulateSpeed(short fps)
{
u_long	n;
static u_long oldTick = 0;

	n = 60 / fps;
	while ((TickCount() - oldTick) < n) {}			// wait for n ticks
	oldTick = TickCount();							// remember current time
}


/************* COPY PSTR **********************/

void CopyPStr(ConstStr255Param	inSourceStr, StringPtr	outDestStr)
{
short	dataLen = inSourceStr[0] + 1;

	BlockMoveData(inSourceStr, outDestStr, dataLen);
	outDestStr[0] = dataLen - 1;
}





#pragma mark -



/************** CALC FRAMES PER SECOND *****************/
//
// This version uses UpTime() which is only available on PCI Macs.
//

void CalcFramesPerSecond(void)
{
AbsoluteTime currTime,deltaTime;
static AbsoluteTime time = {0,0};
Nanoseconds	nano;
float		fps;

int				i;
static int		sampIndex = 0;
static float	sampleList[16] = {60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60};

wait:
	if (gTimeDemo)
	{
		fps = 40;
	}
	else
	{
		currTime = UpTime();

		deltaTime = SubAbsoluteFromAbsolute(currTime, time);
		nano = AbsoluteToNanoseconds(deltaTime);

		fps = (float)kSecondScale / (float)nano.lo;

		if (fps > MAX_FPS)				// limit to avoid issues
			goto wait;
	}

			/* ADD TO LIST */

	sampleList[sampIndex] = fps;
	sampIndex++;
	sampIndex &= 0x7;


			/* CALC AVERAGE */

	gFramesPerSecond = 0;
	for (i = 0; i < 8; i++)
		gFramesPerSecond += sampleList[i];
	gFramesPerSecond *= 1.0f / 8.0f;



	if (gFramesPerSecond < DEFAULT_FPS)			// (avoid divide by 0's later)
		gFramesPerSecond = DEFAULT_FPS;
	gFramesPerSecondFrac = 1.0f/gFramesPerSecond;		// calc fractional for multiplication


	time = currTime;	// reset for next time interval
}


/********************* IS POWER OF 2 ****************************/

Boolean IsPowerOf2(int num)
{
int		i;

	i = 2;
	do
	{
		if (i == num)				// see if this power of 2 matches
			return(true);
		i *= 2;						// next power of 2
	}while(i <= num);				// search until power is > number

	return(false);
}

#pragma mark-

/******************* MY FLUSH EVENTS **********************/
//
// This call makes damed sure that there are no events anywhere in any event queue.
//

void MyFlushEvents(void)
{
#if 1
	while (1)
	{
		EventRef        theEvent;
		OSStatus err = ReceiveNextEvent(0, NULL, kEventDurationNoWait, true, &theEvent);
		if (err == noErr) {
			(void) SendEventToEventTarget(theEvent, GetEventDispatcherTarget());
			ReleaseEvent(theEvent);
		}
		else
			break;
	}

#endif
#if 0
	//EventRecord 	theEvent;

	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());

	/* POLL EVENT QUEUE TO BE SURE THINGS ARE FLUSHED OUT */

	while (GetNextEvent(mDownMask|mUpMask|keyDownMask|keyUpMask|autoKeyMask, &theEvent));


	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());
#endif
}


#pragma mark -



/******************** PSTR CAT / COPY *************************/

StringPtr PStrCat(StringPtr dst, ConstStr255Param   src)
{
SInt16 size = src[0];

	if (0xff - dst[0] < size)
		size = 0xff - dst[0];

	BlockMoveData(&src[1], &dst[dst[0]], size);
	dst[0] = dst[0] + size;

	return dst;
}

StringPtr PStrCopy(StringPtr dst, ConstStr255Param   src)
{
	dst[0] = src[0]; BlockMoveData(&src[1], &dst[1], src[0]); return dst;
}


#pragma mark -


/************************* WE ARE FRONT PROCESS ******************************/

Boolean WeAreFrontProcess(void)
{
ProcessSerialNumber	frontProcess, myProcess;
Boolean				same;

	GetFrontProcess(&frontProcess); 					// get the front process
	MacGetCurrentProcess(&myProcess);					// get the current process

	SameProcess(&frontProcess, &myProcess, &same);		// if they're the same then we're in front

	return(same);
}


#pragma mark -


/********************* SWIZZLE SHORT **************************/

short SwizzleShort(short *shortPtr)
{
short	theShort = *shortPtr;

#if __LITTLE_ENDIAN__

	Byte	b1 = theShort & 0xff;
	Byte	b2 = (theShort & 0xff00) >> 8;

	theShort = (b1 << 8) | b2;
#endif

	return(theShort);
}


/********************* SWIZZLE USHORT **************************/

u_short SwizzleUShort(u_short *shortPtr)
{
u_short	theShort = *shortPtr;

#if __LITTLE_ENDIAN__

	Byte	b1 = theShort & 0xff;
	Byte	b2 = (theShort & 0xff00) >> 8;

	theShort = (b1 << 8) | b2;
#endif

	return(theShort);
}



/********************* SWIZZLE LONG **************************/

long SwizzleLong(long *longPtr)
{
long	theLong = *longPtr;

#if __LITTLE_ENDIAN__

	Byte	b1 = theLong & 0xff;
	Byte	b2 = (theLong & 0xff00) >> 8;
	Byte	b3 = (theLong & 0xff0000) >> 16;
	Byte	b4 = (theLong & 0xff000000) >> 24;

	theLong = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;

#endif

	return(theLong);
}


/********************* SWIZZLE U LONG **************************/

u_long SwizzleULong(u_long *longPtr)
{
u_long	theLong = *longPtr;

#if __LITTLE_ENDIAN__

	Byte	b1 = theLong & 0xff;
	Byte	b2 = (theLong & 0xff00) >> 8;
	Byte	b3 = (theLong & 0xff0000) >> 16;
	Byte	b4 = (theLong & 0xff000000) >> 24;

	theLong = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;

#endif

	return(theLong);
}



/********************* SWIZZLE FLOAT **************************/

float SwizzleFloat(float *floatPtr)
{
float	*theFloat;
u_long	bytes = SwizzleULong((u_long *)floatPtr);

	theFloat = (float *)&bytes;

	return(*theFloat);
}
















