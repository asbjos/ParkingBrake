/*
	Parking Brake MFD
	Coded by Asbjørn 'asbjos' Krüger, 2021.
	Idea by N_Molson: https://www.orbiter-forum.com/threads/parking-brake-mfd.39703/
	"I currently have my hands full, but I think it could be a good idea to implement the "touchdownpoints fix" into a MFD that can be toggled on or off at the user will. Much like a parking brake on a car!
	A few ideas for features :
	1. - Manual mode (on/off switch at the user will with a key combo).
	2. - Auto/semi-auto mode with setting such as :
	2.1 - "Glue mode" : the vessel will be immobilized as soon as hits the ground (not very realistic, but might be useful for some purpose. Like simulating an arrow planting in the ground).
	2.2 - "Near-0 velocity mode" : enables the parking brake only when the horizontal velocity of the vessel is very low (like it is actually stopped, but does those infamous micro-jumps). Most useful in many cases IMHO."

	This source code is released under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
	For other use, please contact me (I'm username 'asbjos' on Orbiter-Forum).
*/

#define STRICT
#define ORBITER_MODULE

#include "orbitersdk.h"

class Parker : public MFD2
{
public:
	Parker(DWORD w, DWORD h, VESSEL* vessel);
	~Parker(void);
	char* ButtonLabel(int bt);
	int ButtonMenu(const MFDBUTTONMENU** menu) const;
	bool Update(oapi::Sketchpad* skp);
	static int MsgProc(UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam);
	bool ConsumeButton(int bt, int event);
	bool ConsumeKeyBuffered(DWORD key);
private:
	// Private stuff
	int W, H;
	VESSEL* Vessel; // current target vessel
};

enum PARKMODE { LOWSPEED, GLUE, LASTENTRY };

int g_MFDmode; // identifier for new MFD mode

bool AutoPark = true;
PARKMODE ParkMode = LOWSPEED;
double SpeedLimit = 0.1; // m/s

bool wantToLand = false;
double wantToLandTime = -10.0; // initial value, must be within 5 seconds of current systime, which is always > 0.

// Constructor
Parker::Parker(DWORD w, DWORD h, VESSEL* vessel) : MFD2(w, h, vessel)
{
	W = w;
	H = h;
	Vessel = vessel;
}

// Deconstructor
Parker::~Parker(void)
{
}

const int NUM_BUT = 3;
char* Parker::ButtonLabel(int bt)
{
	static char* labelOff[NUM_BUT] = { "ON", "NOW", "MDE" };
	static char* labelOn[NUM_BUT] = { "OFF", "NOW", "MDE" };

	if (AutoPark) return (bt < NUM_BUT ? labelOn[bt] : 0);
	else return (bt < NUM_BUT ? labelOff[bt] : 0);
}

int Parker::ButtonMenu(const MFDBUTTONMENU** menu) const
{
	static const MFDBUTTONMENU MnuOff[NUM_BUT] = {
		{"Switch autopark on", 0, 'O'},
		{"Park now", 0, 'N'},
		{"Toggle autopark mode", 0, 'M'}
	};

	static const MFDBUTTONMENU MnuOn[NUM_BUT] = {
		{"Switch autopark off", 0, 'O'},
		{"Park now", 0, 'N'},
		{"Toggle autopark mode", 0, 'M'}
	};

	if (menu)
	{
		if (AutoPark) *menu = MnuOn;
		else *menu = MnuOff;

		return NUM_BUT;
	}

	return 0; // just in case we haven't returned until now
}

bool Parker::Update(oapi::Sketchpad* skp)
{
	Title(skp, "Parking Brake");

	char cbuf[50];

	int X = W / 10;
	int Y = H / 10;
	int yIdx = 1;
	double simt = oapiGetSimTime();
	double syst = oapiGetSysTime();

	sprintf(cbuf, "Auto: %s", AutoPark ? "ON" : "OFF");
	skp->Text(X, Y * yIdx, cbuf, strlen(cbuf));
	yIdx++;

	if (AutoPark)
	{
		switch (ParkMode)
		{
		case LOWSPEED:
			sprintf(cbuf, "Auto mode: low speed");
			break;
		case GLUE:
			sprintf(cbuf, "Auto mode: contact");
			break;
		case LASTENTRY:
			sprintf(cbuf, "Hæ? %.2f", simt);
			break;
		default:
			sprintf(cbuf, "Ka? %.2f", simt);
			break;
		}

		skp->Text(X, Y * yIdx, cbuf, strlen(cbuf));
		yIdx++;
	}

	if (wantToLand && syst < wantToLandTime + 5.0)
	{
		sprintf(cbuf, "Not in contact with ground!");
		skp->Text(X, Y * yIdx, cbuf, strlen(cbuf));
		yIdx++;

		sprintf(cbuf, "  Press NOW to confirm %.2f", wantToLandTime + 5.0 - syst);
		skp->Text(X, Y * yIdx, cbuf, strlen(cbuf));
		yIdx++;
	}

	int flightStatus = Vessel->GetFlightStatus();
	if (flightStatus == 1 || flightStatus == 3)
	{
		sprintf(cbuf, "This vessel is LANDED");
		skp->Text(X, Y * 9, cbuf, strlen(cbuf));
	}
	else
	{

		sprintf(cbuf, "This vessel is NOT landed");
		skp->Text(X, Y * 9, cbuf, strlen(cbuf));
	}
	
	return true;
}

// MFD message parser
int Parker::MsgProc(UINT msg, UINT mfd, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case OAPI_MSG_MFD_OPENED:
		// Our new MFD mode has been selected, so we create the MFD and return a pointer to it.
		return (int)(new Parker(LOWORD(wparam), HIWORD(wparam), (VESSEL*)lparam));
	}
	return 0;
}

bool Parker::ConsumeButton(int bt, int event)
{
	if (!(event & PANEL_MOUSE_LBDOWN)) return false;

	if (bt == 0) return ConsumeKeyBuffered(OAPI_KEY_O);
	if (bt == 1) return ConsumeKeyBuffered(OAPI_KEY_N);
	if (bt == 2) return ConsumeKeyBuffered(OAPI_KEY_M);

	return false;
}

// Land the vessel
void ParkVessel(VESSEL* ves)
{
	// Thanks to "jarmonik": https://www.orbiter-forum.com/threads/mars-2020-rover-and-mars-helicopter-scout.37153/page-6#post-577597
	// Also honorably mention to "face": https://www.orbiter-forum.com/threads/orbiter-economic-simulation.39629/page-2#post-578205
	OBJHANDLE ref = ves->GetSurfaceRef();
	double longitude, latitude, radius;
	ves->GetEquPos(longitude, latitude, radius);
	double heading = 0.0;
	OBJHANDLE obj = ves->GetHandle();
	oapiGetHeading(obj, &heading);
	VESSELSTATUS2 vs;
	memset(&vs, 0, sizeof(vs));
	vs.version = 2;
	vs.rbody = ref;
	vs.status = 1; // Landed
	vs.arot.x = 10; // <----- Undocumented feature "magic value" to land on touchdown points !! IMPORTANT !! It has to be 10, no more, no less !
	vs.surf_lng = longitude;
	vs.surf_lat = latitude;
	vs.surf_hdg = heading;
	ves->DefSetStateEx(&vs);

	char vName[50];
	oapiGetObjectName(obj, vName, 50);
	oapiWriteLogV("Parking Brake parked %s at %.1f", vName, oapiGetSimTime());

	wantToLand = false; // reset want to land wish, as we now have landed. (for manual landing when not in contact with ground)
}

bool Parker::ConsumeKeyBuffered(DWORD key)
{
	double syst = oapiGetSysTime();

	switch (key)
	{
	case OAPI_KEY_O:
		AutoPark = !AutoPark;
		return true;
	case OAPI_KEY_N:
		// Park current vessel now! (if on surface, that is?) Or maybe have a "You sure" if not landed?
		if (Vessel->GroundContact())
		{
			ParkVessel(Vessel);
		}
		else if (wantToLand && syst < wantToLandTime + 5.0)
		{
			ParkVessel(Vessel);
		}
		else
		{
			wantToLand = true;
			wantToLandTime = syst;
		}
		return true;
	case OAPI_KEY_M:
		ParkMode = PARKMODE((int(ParkMode) + 1) % int(LASTENTRY));
		return true;
	}

	return false;
}

DLLCLBK void opcPreStep(double simt, double simdt, double mjd)
{
	if (AutoPark)
	{
		for (int i = 0; i < oapiGetVesselCount(); i++)
		{
			VESSEL* v = oapiGetVesselInterface(oapiGetVesselByIndex(i));

			// Only do extensive check if not already landed, but in contact with planet.
			int flightStatus = v->GetFlightStatus(); // 0 - free, 1 - landed, 2 - docked free, 3 - docked landed
			if ((flightStatus == 0 || flightStatus == 2) && v->GroundContact()) // not landed state, but with ground contact. Investigate further.
			{
				int thrustersActive = 0;
				for (int j = 0; j < v->GetThrusterCount(); j++)
				{
					if (v->GetThrusterLevel(v->GetThrusterHandleByIndex(j)) != 0.0)
					{
						thrustersActive++;
						break; // already found one active thruster, so stop searching, as next condition is already void.
					}
				}

				if (thrustersActive == 0) // i.e. we are now in contact with a planet, but not landed, and have no thrusters on.
				{
					if (ParkMode == GLUE || v->GetGroundspeed() < SpeedLimit) // either glue mode, or must have speed lower than some value.
					{
						// Park the current vessel
						ParkVessel(v);
					}
				}
			}
		}
	}
}

DLLCLBK void InitModule(HINSTANCE hDLL)
{
	MFDMODESPECEX spec;
	spec.name = "Parking Brake";
	spec.key = OAPI_KEY_P;                // MFD mode selection key
	spec.context = NULL;
	spec.msgproc = Parker::MsgProc;  // MFD mode callback function

	// Register the new MFD mode with Orbiter
	g_MFDmode = oapiRegisterMFDMode(spec);

	FILEHANDLE cfgFile = oapiOpenFile("MFD\\ParkingBrake.cfg", FILE_IN, CONFIG);

	if (!oapiReadItem_bool(cfgFile, "DefAutoPark", AutoPark)) oapiWriteLog("Parking Brake could not read AutoPark setting.");
	int parkMode = int(LOWSPEED);
	if (!oapiReadItem_int(cfgFile, "DefParkMode", parkMode)) oapiWriteLog("Parking Brake could not read ParkMode setting.");
	ParkMode = PARKMODE(parkMode % int(LASTENTRY));
	if (!oapiReadItem_float(cfgFile, "DefSpeedLimit", SpeedLimit)) oapiWriteLog("Parking Brake could not read SpeedLimit setting.");

	oapiCloseFile(cfgFile, FILE_IN);
}

DLLCLBK void ExitModule(HINSTANCE hDLL)
{
	// Unregister the custom MFD mode when the module is unloaded
	oapiUnregisterMFDMode(g_MFDmode);
}