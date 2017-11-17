#include "common/tpt-minmax.h"
#include <cstring>
#include <sstream>
#include "json/json.h"

#include "PowderToy.h"
#include "defines.h"
#include "gravity.h"
#include "http.h"
#include "interface.h"
#include "luaconsole.h"
#include "powder.h"
#include "powdergraphics.h"
#include "misc.h"
#include "save.h"
#include "update.h"

#include "common/Platform.h"
#include "game/Authors.h"
#include "game/Brush.h"
#include "game/Download.h"
#include "game/Menus.h"
#include "game/Save.h"
#include "game/Sign.h"
#include "game/ToolTip.h"
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "interface/Button.h"
#include "interface/Engine.h"
#include "interface/Window.h"
#include "simulation/Simulation.h"
#include "simulation/Snapshot.h"
#include "simulation/Tool.h"
#include "simulation/ToolNumbers.h"

#include "gui/dialogs/ConfirmPrompt.h"
#include "gui/dialogs/InfoPrompt.h"
#include "gui/dialogs/ErrorPrompt.h"
#include "gui/profile/ProfileViewer.h"
#include "gui/sign/CreateSign.h"
#include "gui/rendermodes/RenderModesUI.h"

PowderToy::~PowderToy()
{
	Snapshot::ClearSnapshots();
	main_end_hack();
	free(clipboardData);
}

PowderToy::PowderToy():
	Window_(Point(0, 0), Point(XRES+BARSIZE, YRES+MENUSIZE)),
	mouse(Point(0, 0)),
	cursor(Point(0, 0)),
	lastMouseDown(0),
	heldKey(0),
	heldAscii(0),
	releasedKey(0),
	heldModifier(0),
	mouseWheel(0),
	mouseCanceled(false),
	numNotifications(0),
	voteDownload(NULL),
	delayedHttpChecks(true),
	drawState(POINTS),
	isMouseDown(false),
	isStampMouseDown(false),
	toolIndex(0),
	toolStrength(1.0f),
	lastDrawPoint(Point(0, 0)),
	initialDrawPoint(Point(0, 0)),
	ctrlHeld(false),
	shiftHeld(false),
	altHeld(false),
	mouseInZoom(false),
	skipDraw(false),
	placingZoom(false),
	placingZoomTouch(false),
	zoomEnabled(false),
	zoomedOnPosition(Point(0, 0)),
	zoomWindowPosition(Point(0, 0)),
	zoomMousePosition(Point(0, 0)),
	zoomSize(32),
	zoomFactor(8),
	state(NONE),
	loadPos(Point(0, 0)),
	loadSize(Point(0, 0)),
	stampData(NULL),
	stampSize(0),
	stampImg(NULL),
	waitToDraw(false),
#ifdef TOUCHUI
	stampClickedPos(Point(0, 0)),
	stampClickedOffset(Point(0, 0)),
	initialLoadPos(Point(0, 0)),
	stampQuadrant(0),
	stampMoving(false),
#endif
	savePos(Point(0, 0)),
	saveSize(Point(0, 0)),
	clipboardData(NULL),
	clipboardSize(0),
	loginCheckTicks(0),
	loginFinished(0),
	ignoreMouseUp(false),
	insideRenderOptions(false),
	deletingRenderOptions(false),
	previousPause(false),
	restorePreviousPause(false)
{
	ignoreQuits = true;
	hasBorder = false;

	sim = new Simulation();
	globalSim = sim;

	InitMenusections();
	FillMenus();
	regularTools[0] = "DEFAULT_PT_DUST";
	regularTools[1] = "DEFAULT_PT_NONE";
	regularTools[2] = "DEFAULT_PT_NONE";
	decoTools[0] = "DEFAULT_DECOR_SET";
	decoTools[1] = "DEFAULT_DECOR_CLR";
	decoTools[2] = "DEFAULT_PT_NONE";
	activeTools[0] = GetToolFromIdentifier(regularTools[0]);
	activeTools[1] = GetToolFromIdentifier(regularTools[1]);
	activeTools[2] = GetToolFromIdentifier(regularTools[2]);

	load_presets();

	if (doUpdates)
	{
		versionCheck = new Download("http://" UPDATESERVER "/Startup.json");
		if (svf_login)
			versionCheck->AuthHeaders(svf_user, NULL); //username instead of session
	}
	else
		versionCheck = NULL;

	if (svf_login)
	{
		sessionCheck = new Download("http://" SERVER "/Startup.json");
		sessionCheck->AuthHeaders(svf_user_id, svf_session_id);
	}
	else
		sessionCheck = NULL;

	// start placing the bottom row of buttons, starting from the left
#ifdef TOUCHUI
	const int ySize = 20;
	const int xOffset = 0;
	const int tooltipAlpha = 255;
	const int minWidth = 38;
#else
	const int ySize = 15;
	const int xOffset = 1;
	const int tooltipAlpha = -2;
	const int minWidth = 0;
#endif
	class OpenBrowserAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->OpenBrowser(b);
		}
	};
#ifdef TOUCHUI
	openBrowserButton = new Button(Point(xOffset, YRES+MENUSIZE-20), Point(minWidth, ySize), "\x81");
#else
	openBrowserButton = new Button(Point(xOffset, YRES+MENUSIZE-16), Point(18-xOffset, ySize), "\x81");
#endif
	openBrowserButton->SetCallback(new OpenBrowserAction());
#ifdef TOUCHUI
	openBrowserButton->SetState(Button::HOLD);
#endif
	openBrowserButton->SetTooltip(new ToolTip("Find & open a simulation", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(openBrowserButton);

	class ReloadAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->ReloadSave(b);
		}
	};
	reloadButton = new Button(openBrowserButton->Right(Point(1, 0)), Point(minWidth > 17 ? minWidth : 17, ySize), "\x91");
	reloadButton->SetCallback(new ReloadAction());
	reloadButton->SetEnabled(false);
#ifdef TOUCHUI
	reloadButton->SetState(Button::HOLD);
#endif
	reloadButton->SetTooltip(new ToolTip("Reload the simulation \bg(ctrl+r)", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(reloadButton);

	class SaveAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->DoSave(b);
		}
	};
	saveButton = new Button(reloadButton->Right(Point(1, 0)), Point(minWidth > 151 ? minWidth : 151, ySize), "\x82 [untitled simulation]");
	saveButton->SetAlign(Button::LEFT);
	saveButton->SetCallback(new SaveAction());
#ifdef TOUCHUI
	saveButton->SetState(Button::HOLD);
#endif
	saveButton->SetTooltip(new ToolTip("Upload a new simulation", Point(minWidth > 16 ? minWidth : 16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(saveButton);

	class VoteAction : public ButtonAction
	{
		bool voteType;
	public:
		VoteAction(bool up):
			ButtonAction()
		{
			voteType = up;
		}

		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->DoVote(voteType);
		}
	};
#ifdef TOUCHUI
	upvoteButton = new Button(saveButton->Right(Point(1, 0)), Point(minWidth > 40 ? minWidth: 40, ySize), "\xCB");
#else
	upvoteButton = new Button(saveButton->Right(Point(1, 0)), Point(minWidth > 40 ? minWidth: 40, ySize), "\xCB Vote");
#endif
	upvoteButton->SetColor(COLRGB(0, 187, 18));
	upvoteButton->SetCallback(new VoteAction(true));
	upvoteButton->SetTooltip(new ToolTip("Like this save", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(upvoteButton);

	downvoteButton = new Button(upvoteButton->Right(Point(0, 0)), Point(minWidth > 16 ? minWidth : 16, ySize), "\xCA");
	downvoteButton->SetColor(COLRGB(187, 40, 0));
	downvoteButton->SetCallback(new VoteAction(false));
	downvoteButton->SetTooltip(new ToolTip("Disike this save", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(downvoteButton);


	// We now start placing buttons from the right side, because tags button is in the middle and uses whatever space is leftover
	class PauseAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->TogglePause();
		}
	};
	Point size = Point(minWidth > 15 ? minWidth : 15, ySize);
	pauseButton = new Button(Point(XRES+BARSIZE-size.X-xOffset, openBrowserButton->GetPosition().Y), size, "\x90");
	pauseButton->SetCallback(new PauseAction());
	pauseButton->SetTooltip(new ToolTip("Pause the simulation \bg(space)", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(pauseButton);

	class RenderOptionsAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->RenderOptions();
		}
	};
	size = Point(minWidth > 17 ? minWidth : 17, ySize);
	renderOptionsButton = new Button(pauseButton->Left(Point(size.X+1, 0)), size, "\xD8");
	renderOptionsButton->SetCallback(new RenderOptionsAction());
	renderOptionsButton->SetTooltip(new ToolTip("Renderer options", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(renderOptionsButton);

	class LoginButtonAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->LoginButton();
		}
	};
	size = Point(minWidth > 95 ? minWidth : 95, ySize);
	loginButton = new Button(renderOptionsButton->Left(Point(size.X+1, 0)), size, "\x84 [sign in]");
	loginButton->SetAlign(Button::LEFT);
	loginButton->SetCallback(new LoginButtonAction());
	loginButton->SetTooltip(new ToolTip("Sign into the Simulation Server", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(loginButton);

	class ClearSimAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			NewSim();
		}
	};
	size = Point(minWidth > 17 ? minWidth : 17, ySize);
	clearSimButton = new Button(loginButton->Left(Point(size.X+1, 0)), size, "\x92");
	clearSimButton->SetCallback(new ClearSimAction());
	clearSimButton->SetTooltip(new ToolTip("Erase all particles and walls", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(clearSimButton);

	class OpenOptionsAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->OpenOptions();
		}
	};
	size = Point(minWidth > 15 ? minWidth : 15, ySize);
	optionsButton = new Button(clearSimButton->Left(Point(size.X+1, 0)), size, "\xCF");
	optionsButton->SetCallback(new OpenOptionsAction());
	optionsButton->SetTooltip(new ToolTip("Simulation options", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(optionsButton);

	class ReportBugAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->ReportBug();
		}
	};
	size = Point(minWidth > 15 ? minWidth : 15, ySize);
	reportBugButton = new Button(optionsButton->Left(Point(size.X+1, 0)), size, "\xE7");
	reportBugButton->SetCallback(new ReportBugAction());
	reportBugButton->SetTooltip(new ToolTip("Report bugs and feedback to jacob1", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(reportBugButton);

	class OpenTagsAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->OpenTags();
		}
	};
	Point tagsPos = downvoteButton->Right(Point(1, 0));
#ifdef TOUCHUI
	openTagsButton = new Button(tagsPos, Point((reportBugButton->Left(Point(1, 0))-tagsPos).X, ySize), "\x83");
#else
	openTagsButton = new Button(tagsPos, Point((reportBugButton->Left(Point(1, 0))-tagsPos).X, ySize), "\x83 [no tags set]");
	openTagsButton->SetAlign(Button::LEFT);
#endif
	openTagsButton->SetCallback(new OpenTagsAction());
	openTagsButton->SetTooltip(new ToolTip("Add simulation tags", Point(16, YRES-24), TOOLTIP, tooltipAlpha));
	AddComponent(openTagsButton);

#ifdef TOUCHUI
	class EraseAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->ToggleErase(b == 4);
		}
	};
	eraseButton = new Button(Point(XRES+1, 0), Point(BARSIZE-1, 25), "\xE8");
	eraseButton->SetState(Button::HOLD);
	eraseButton->SetCallback(new EraseAction());
	eraseButton->SetTooltip(GetQTip("Swap to erase tool (hold to clear the sim)", eraseButton->GetPosition().Y+10));
	AddComponent(eraseButton);

	class OpenConsoleAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->OpenConsole(b == 4);
		}
	};
	openConsoleButton = new Button(eraseButton->Below(Point(0, 1)), Point(BARSIZE-1, 25), "\xE9");
	openConsoleButton->SetState(Button::HOLD);
	openConsoleButton->SetCallback(new OpenConsoleAction());
	openConsoleButton->SetTooltip(GetQTip("Open console (hold to show on screen keyboard)", openConsoleButton->GetPosition().Y+10));
	AddComponent(openConsoleButton);

	class SettingAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->ToggleSetting(b == 4);
		}
	};
	settingsButton = new Button(openConsoleButton->Below(Point(0, 1)), Point(BARSIZE-1, 25), "\xEB");
	settingsButton->SetState(Button::HOLD);
	settingsButton->SetCallback(new SettingAction());
	settingsButton->SetTooltip(GetQTip("Toggle Decorations (hold to open options)", settingsButton->GetPosition().Y+10));
	AddComponent(settingsButton);

	class ZoomAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->StartZoom(b == 4);
		}
	};
	zoomButton = new Button(settingsButton->Below(Point(0, 1)), Point(BARSIZE-1, 25), "\xEC");
	zoomButton->SetState(Button::HOLD);
	zoomButton->SetCallback(new ZoomAction());
	zoomButton->SetTooltip(GetQTip("Start placing the zoom window", zoomButton->GetPosition().Y+10));
	AddComponent(zoomButton);

	class StampAction : public ButtonAction
	{
	public:
		virtual void ButtionActionCallback(Button *button, unsigned char b)
		{
			dynamic_cast<PowderToy*>(button->GetParent())->SaveStamp(b == 4);
		}
	};
	stampButton = new Button(zoomButton->Below(Point(0, 1)), Point(BARSIZE-1, 25), "\xEA");
	stampButton->SetState(Button::HOLD);
	stampButton->SetCallback(new StampAction());
	stampButton->SetTooltip(GetQTip("Save a stamp (hold to load a stamp)", stampButton->GetPosition().Y+10));
	AddComponent(stampButton);
#endif
}

void PowderToy::DelayedHttpInitialization()
{
	if (versionCheck)
		versionCheck->Start();
	if (sessionCheck)
		sessionCheck->Start();
#ifndef TOUCHUI
	if (prevDNS != prevDNSalt)
	{
		class SwapDNSAction : public ButtonAction
		{
		public:
			virtual void ButtionActionCallback(Button *button, unsigned char b)
			{
				if (b == 1)
				{
					prevDNS = prevDNSalt;
					save_presets();
					Platform::DoRestart(true);
				}
				dynamic_cast<PowderToy*>(button->GetParent())->RemoveComponent(button);
			}
		};
		std::string message;
		if (swappedDNS)
			message = "Using alternate DNS due to mismatch. Click here to undo if online does not work";
		else
			message = "DNS mismatch found. Click here to use an alternate DNS if online does not work";
		Button *notification = AddNotification(message);
		notification->SetCallback(new SwapDNSAction());
		AddComponent(notification);
	}
	if (prevDNSstatic != prevDNSstaticalt)
	{
		class SwapDNSStaticAction : public ButtonAction
		{
		public:
			virtual void ButtionActionCallback(Button *button, unsigned char b)
			{
				if (b == 1)
				{
					prevDNSstatic = prevDNSstaticalt;
					save_presets();
					Platform::DoRestart(true);
				}
				dynamic_cast<PowderToy*>(button->GetParent())->RemoveComponent(button);
			}
		};
		std::string message;
		if (swappedDNSstatic)
			message = "Using alternate static DNS due to mismatch. Click here to undo if online does not work";
		else
			message = "static DNS mismatch found. Click here to use an alternate DNS if online does not work";
		Button *notification = AddNotification(message);
		notification->SetCallback(new SwapDNSStaticAction());
		AddComponent(notification);
	}
#endif
}

void PowderToy::OpenBrowser(unsigned char b)
{
	if (voteDownload)
	{
		voteDownload->Cancel();
		voteDownload = NULL;
		svf_myvote = 0;
		SetInfoTip("Error: a previous vote may not have gone through");
	}
#ifdef TOUCHUI
	if (ctrlHeld || b != 1)
#else
	if (ctrlHeld)
#endif
		catalogue_ui(vid_buf);
	else
		search_ui(vid_buf);
}

void PowderToy::ReloadSave(unsigned char b)
{
	if (b == 1 || !strncmp(svf_id, "", 8))
	{
		Snapshot::TakeSnapshot(sim);
		parse_save(svf_last, svf_lsize, 1, 0, 0, bmap, sim->air->vx, sim->air->vy, sim->air->pv, sim->air->fvx, sim->air->fvy, signs, parts, pmap, &authors);
		if (!authors.size())
			DefaultSaveInfo();
	}
	else
		open_ui(vid_buf, svf_id, NULL, 0);
}

void PowderToy::DoSave(unsigned char b)
{
#ifdef TOUCHUI
	if (!svf_login || (sdl_mod & (KMOD_CTRL|KMOD_META)) || b != 1)
#else
	if (!svf_login || (sdl_mod & (KMOD_CTRL|KMOD_META)))
#endif
	{
		// local quick save
		if (mouse.X <= saveButton->GetPosition().X+18 && svf_fileopen)
		{
			int saveSize;

			Json::Value localSaveInfo;
			localSaveInfo["type"] = "localsave";
			localSaveInfo["username"] = svf_user;
			localSaveInfo["title"] = svf_filename;
			localSaveInfo["date"] = (Json::Value::UInt64)time(NULL);
			SaveAuthorInfo(&localSaveInfo);

			void *saveData = build_save(&saveSize, 0, 0, XRES, YRES, bmap, sim->air->vx, sim->air->vy, sim->air->pv, sim->air->fvx, sim->air->fvy, signs, parts, &localSaveInfo);
			if (!saveData)
			{
				SetInfoTip("Error creating save");
			}
			else
			{
				if (DoLocalSave(svf_filename, saveData, saveSize, true))
					SetInfoTip("Error writing local save");
				else
					SetInfoTip("Updated successfully");
			}
		}
		// local save
		else
			save_filename_ui(vid_buf);
	}
	else
	{
		// local save
		if (!svf_open || !svf_own || mouse.X > saveButton->GetPosition().X+18)
		{
			if (save_name_ui(vid_buf))
			{
				if (!execute_save(vid_buf) && svf_id[0])
				{
					copytext_ui(vid_buf, "Save ID", "Saved successfully!", svf_id);
				}
				else
				{
					SetInfoTip("Error saving");
				}
			}
		}
		// local quick save
		else
		{
			if (execute_save(vid_buf))
			{
				SetInfoTip("Error saving");
			}
			else
			{
				SetInfoTip("Saved successfully");
			}
		}
	}
}

void PowderToy::DoVote(bool up)
{
	if (voteDownload != NULL)
	{
		SetInfoTip("Error: could not vote");
		return;
	}
	voteDownload = new Download("http://" SERVER "/Vote.api");
	voteDownload->AuthHeaders(svf_user_id, svf_session_id);
	std::map<std::string, std::string> postData;
	postData.insert(std::pair<std::string, std::string>("ID", svf_id));
	postData.insert(std::pair<std::string, std::string>("Action", up ? "Up" : "Down"));
	voteDownload->AddPostData(postData);
	voteDownload->Start();
	svf_myvote = up ? 1 : -1; // will be reset later upon error
}

void PowderToy::OpenTags()
{
	tag_list_ui(vid_buf);
}

void PowderToy::ReportBug()
{
	report_ui(vid_buf, NULL, true);
}

void PowderToy::OpenOptions()
{
	simulation_ui(vid_buf);
	save_presets();
}

void PowderToy::LoginButton()
{
	if (svf_login && mouse.X <= loginButton->GetPosition().X+18)
	{
		ProfileViewer *temp = new ProfileViewer(svf_user);
		Engine::Ref().ShowWindow(temp);
	}
	else
	{
		int ret = login_ui(vid_buf);
		if (ret && svf_login)
		{
			save_presets();
			if (sessionCheck)
			{
				sessionCheck->Cancel();
				sessionCheck = NULL;
			}
			loginFinished = 1;
		}
	}
}

void PowderToy::RenderOptions()
{
	RenderModesUI *renderModes = new RenderModesUI();
	this->AddSubwindow(renderModes);
	renderModes->HasBorder(true);
	previousPause = sys_pause;
	SetPause(1);
	insideRenderOptions = true;
	deletingRenderOptions = false;
	restorePreviousPause = true;
}

void PowderToy::TogglePause()
{
	if (sys_pause && sim->debug_currentParticle)
	{
#ifdef LUACONSOLE
		std::stringstream logmessage;
		logmessage << "Updated particles from #" << sim->debug_currentParticle << " to end due to unpause";
		luacon_log(mystrdup(logmessage.str().c_str()));
#endif
		sim->UpdateParticles(sim->debug_currentParticle, NPART);
		sim->UpdateAfter();
		sim->debug_currentParticle = 0;
	}
	sys_pause = !sys_pause;
	restorePreviousPause = false;
}

void PowderToy::SetPause(bool pause)
{
	if (pause != sys_pause)
		TogglePause();
}

// functions called by touch interface buttons are here
#ifdef TOUCHUI
void PowderToy::ToggleErase(bool alt)
{
	if (alt)
	{
		NewSim();
		SetInfoTip("Cleared the simulation");
	}
	else
	{
		Tool *erase = GetToolFromIdentifier("DEFAULT_PT_NONE");
		if (activeTools[0] == erase)
		{
			activeTools[0] = activeTools[1];
			activeTools[1] = erase;
			SetInfoTip("Erase tool deselected");
		}
		else
		{
			activeTools[1] = activeTools[0];
			activeTools[0] = erase;
			SetInfoTip("Erase tool selected");
		}
	}
}

void PowderToy::OpenConsole(bool alt)
{
	if (alt)
		Platform::ShowOnScreenKeyboard("", false);
	else
		console_mode = 1;
}

void PowderToy::ToggleSetting(bool alt)
{
	if (alt)
		simulation_ui(vid_buf);
	else
	{
		if (active_menu == SC_DECO || ngrav_completedisable)
		{
			decorations_enable = !decorations_enable;
			if (decorations_enable)
				SetInfoTip("Decorations enabled");
			else
				SetInfoTip("Decorations disabled");
		}
		else
		{
			if (!ngrav_enable)
			{
				start_grav_async();
				SetInfoTip("Newtonian Gravity enabled");
			}
			else
			{
				stop_grav_async();
				SetInfoTip("Newtonian Gravity disabled");
			}
		}
	}
}

void PowderToy::StartZoom(bool alt)
{
	if (ZoomWindowShown() || placingZoomTouch)
		HideZoomWindow();
	else
	{
		placingZoomTouch = true;
		UpdateZoomCoordinates(mouse);
	}
}

void PowderToy::SaveStamp(bool alt)
{
	if (alt)
	{
		ResetStampState();

		int reorder = 1;
		int stampID = stamp_ui(vid_buf, &reorder);
		if (stampID >= 0)
			stampData = stamp_load(stampID, &stampSize, reorder);
		else
			stampData = NULL;

		if (stampData)
		{
			stampImg = prerender_save(stampData, stampSize, &loadSize.X, &loadSize.Y);
			if (stampImg)
			{
				state = LOAD;
				loadPos.X = CELL*((XRES-loadSize.X+CELL)/2/CELL);
				loadPos.Y = CELL*((YRES-loadSize.Y+CELL)/2/CELL);
				stampClickedPos = Point(XRES, YRES)/2;
				ignoreMouseUp = true;
				waitToDraw = true;
			}
			else
			{
				free(stampData);
				stampData = NULL;
			}
		}
	}
	else
	{
		if (state == NONE)
		{
			state = SAVE;
			isStampMouseDown = false;
			ignoreMouseUp = true;
		}
		else
			ResetStampState();
	}
}

#endif

// misc main gui functions
void PowderToy::ConfirmUpdate(std::string changelog, std::string file)
{
	class ConfirmUpdate : public ConfirmAction
	{
		std::string file;
	public:
		ConfirmUpdate(std::string file) : file(file) { }

		virtual void Action(bool isConfirmed)
		{
			if (isConfirmed)
			{
#ifdef ANDROID
				Platform::OpenLink(file);
#else
				if (do_update(file))
				{
					has_quit = true;
				}
				else
				{
					ErrorPrompt *error = new ErrorPrompt("Update failed - try downloading a new version.");
					Engine::Ref().ShowWindow(error);
				}
#endif
			}
		}
	};
#ifdef ANDROID
	std::string title = "\bwDo you want to update TPT?";
#else
	std::string title = "\bwDo you want to update Jacob1's Mod?";
#endif
	ConfirmPrompt *confirm = new ConfirmPrompt(new ConfirmUpdate(file), title, changelog, "\btUpdate");
	Engine::Ref().ShowWindow(confirm);
}

void PowderToy::UpdateDrawMode()
{
	if (ctrlHeld && shiftHeld)
	{
		int tool = ((ToolTool*)activeTools[toolIndex])->GetID();
		if (tool == -1 || tool == TOOL_PROP)
			drawState = FILL;
		else
			drawState = POINTS;
	}
	else if (ctrlHeld)
		drawState = RECT;
	else if (shiftHeld)
		drawState = LINE;
	else
		drawState = POINTS;
}

void PowderToy::UpdateToolStrength()
{
	if (shiftHeld)
		toolStrength = 10.0f;
	else if (ctrlHeld)
		toolStrength = .1f;
	else
		toolStrength = 1.0f;
}

Point PowderToy::LineSnapCoords(Point point1, Point point2)
{
	Point diff = point2 - point1;
	if (abs(diff.X / 2) > abs(diff.Y)) // vertical
		return point1 + Point(diff.X, 0);
	else if(abs(diff.X) < abs(diff.Y / 2)) // horizontal
		return point1 + Point(0, diff.Y);
	else if(diff.X * diff.Y > 0) // NW-SE
		return point1 + Point((diff.X + diff.Y)/2, (diff.X + diff.Y)/2);
	else // SW-NE
		return point1 + Point((diff.X - diff.Y)/2, (diff.Y - diff.X)/2);
}

Point PowderToy::RectSnapCoords(Point point1, Point point2)
{
	Point diff = point2 - point1;
	if (diff.X * diff.Y > 0) // NW-SE
		return point1 + Point((diff.X + diff.Y)/2, (diff.X + diff.Y)/2);
	else // SW-NE
		return point1 + Point((diff.X - diff.Y)/2, (diff.Y - diff.X)/2);
}

void PowderToy::SwapToDecoToolset()
{
	regularTools[0] = activeTools[0]->GetIdentifier();
	regularTools[1] = activeTools[1]->GetIdentifier();
	regularTools[2] = activeTools[2]->GetIdentifier();
	activeTools[0] = GetToolFromIdentifier("DEFAULT_DECOR_SET");
	activeTools[1] = GetToolFromIdentifier(decoTools[1], "DEFAULT_DECOR_CLR");
	activeTools[2] = GetToolFromIdentifier(decoTools[2], "DEFAULT_PT_NONE");
}

void PowderToy::SwapToRegularToolset()
{
	decoTools[0] = activeTools[0]->GetIdentifier();
	decoTools[1] = activeTools[1]->GetIdentifier();
	decoTools[2] = activeTools[2]->GetIdentifier();
	activeTools[0] = GetToolFromIdentifier(regularTools[0], "DEFAULT_PT_DUST");
	activeTools[1] = GetToolFromIdentifier(regularTools[1], "DEFAULT_PT_NONE");
	activeTools[2] = GetToolFromIdentifier(regularTools[2], "DEFAULT_PT_NONE");
}

bool PowderToy::MouseClicksIgnored()
{
	return PlacingZoomWindow() || state != NONE;
}

Point PowderToy::AdjustCoordinates(Point mouse)
{
	//adjust coords into the simulation area
	mouse.Clamp(Point(0, 0), Point(XRES-1, YRES-1));

	//Change mouse coords to take zoom window into account
	if (ZoomWindowShown())
	{
		if (mouse >= zoomWindowPosition && mouse < Point(zoomWindowPosition.X+zoomFactor*zoomSize, zoomWindowPosition.Y+zoomFactor*zoomSize))
		{
			mouse.X = ((mouse.X-zoomWindowPosition.X)/zoomFactor) + zoomedOnPosition.X;
			mouse.Y = ((mouse.Y-zoomWindowPosition.Y)/zoomFactor) + zoomedOnPosition.Y;
		}
	}
	return mouse;
}

Point PowderToy::SnapCoordinatesWall(Point pos1, Point pos2)
{
	if (activeTools[toolIndex]->GetType() == WALL_TOOL)
	{
		pos1 = pos1/4*4;
		if (drawState == PowderToy::RECT)
		{
			if (pos1.X >= pos2.X)
				pos1.X += CELL-1;

			if (pos1.Y >= pos2.Y)
				pos1.Y += CELL-1;
		}
	}
	return pos1;
}

bool PowderToy::IsMouseInZoom(Point mouse)
{
	//adjust coords into the simulation area
	mouse.Clamp(Point(0, 0), Point(XRES-1, YRES-1));

	return mouse != AdjustCoordinates(mouse);
}

void PowderToy::SetInfoTip(std::string infotip)
{
	UpdateToolTip(infotip, Point(XCNTR-VideoBuffer::TextSize(infotip).X/2, YCNTR-10), INFOTIP, 1000);
}

ToolTip * PowderToy::GetQTip(std::string qtip, int y)
{
	return new ToolTip(qtip, Point(XRES-5-VideoBuffer::TextSize(qtip).X, y), QTIP, -2);
}

void PowderToy::UpdateZoomCoordinates(Point mouse)
{
	zoomMousePosition = mouse;
	zoomedOnPosition = mouse-Point(zoomSize/2, zoomSize/2);
	zoomedOnPosition.Clamp(Point(0, 0), Point(XRES-zoomSize, YRES-zoomSize));

	if (mouse.X < XRES/2)
		zoomWindowPosition = Point(XRES-zoomSize*zoomFactor, 1);
	else
		zoomWindowPosition = Point(1, 1);
}

void PowderToy::UpdateStampCoordinates(Point cursor, Point offset)
{
	loadPos.X = CELL*((cursor.X-loadSize.X/2+CELL/2)/CELL);
	loadPos.Y = CELL*((cursor.Y-loadSize.Y/2+CELL/2)/CELL);
	loadPos -= offset;
	loadPos.Clamp(Point(0, 0), Point(XRES, YRES)-loadSize);
}

void PowderToy::ResetStampState()
{
	if (state == LOAD)
	{
		free(stampData);
		stampData = NULL;
		free(stampImg);
		stampImg = NULL;
#ifdef TOUCHUI
		stampMoving = false;
#endif
	}
	state = NONE;
	isStampMouseDown = false;
	isMouseDown = false; // do this here also because we always want to cancel mouse drawing when going into a new stamp state
}

void PowderToy::HideZoomWindow()
{
	placingZoom = false;
	placingZoomTouch = false;
	zoomEnabled = false;
}

Button * PowderToy::AddNotification(std::string message)
{
	int messageSize = VideoBuffer::TextSize(message).X;
	Button *notificationButton = new Button(Point(XRES-19-messageSize-5, YRES-22-20*numNotifications), Point(messageSize+5, 15), message);
	notificationButton->SetColor(COLRGB(255, 216, 32));
	AddComponent(notificationButton);
	numNotifications++;
	return notificationButton;
}

void PowderToy::LoadRenderPreset(int preset)
{
	if (Renderer::Ref().LoadRenderPreset(preset))
	{
		std::string tooltip = Renderer::Ref().GetRenderPresetToolTip(preset);
		UpdateToolTip(tooltip, Point(XCNTR-VideoBuffer::TextSize(tooltip).X/2, YCNTR-10), INFOTIP, 255);
		save_presets();
	}
}

// Engine events
void PowderToy::OnTick(uint32_t ticks)
{
	int mouseX, mouseY;
	int mouseDown = mouse_get_state(&mouseX, &mouseY);
#ifdef LUACONSOLE
	// lua mouse "tick", call the function every frame. When drawing is rewritten, this needs to be changed to cancel drawing.
	if (mouseDown && !luacon_mouseevent(mouseX, mouseY, mouseDown, LUACON_MPRESS, 0))
		mouseCanceled = true;
	if (mouseCanceled)
		mouseDown = 0;
	
	/*std::stringstream orStr;
	orStr << orientation[0] << ", " << orientation[1] << ", " << orientation[2] << std::endl;
	luacon_log(mystrdup(orStr.str().c_str()));*/
#endif
	sdl_key = heldKey; // ui_edit_process in deco editor uses these two globals so we have to set them ):
	sdl_ascii = heldAscii;
	main_loop_temp(mouseDown, lastMouseDown, heldKey, releasedKey, heldModifier, mouseX, mouseY, mouseWheel);
	lastMouseDown = mouseDown;
	heldKey = heldAscii = releasedKey = mouseWheel = 0;

	if (!loginFinished)
		loginCheckTicks = (loginCheckTicks+1)%51;
	waitToDraw = false;

	if (skipDraw)
		skipDraw = false;
	else if (isMouseDown)
	{
		if (drawState == POINTS)
		{
			activeTools[toolIndex]->DrawLine(sim, currentBrush, lastDrawPoint, cursor, true, toolStrength);
			lastDrawPoint = cursor;
		}
		else if (drawState == LINE)
		{
			if (((ToolTool*)activeTools[toolIndex])->GetID() == TOOL_WIND)
			{
				Point drawPoint2 = cursor;
				if (altHeld)
					drawPoint2 = LineSnapCoords(initialDrawPoint, cursor);
				activeTools[toolIndex]->DrawLine(sim, currentBrush, initialDrawPoint, drawPoint2, false, toolStrength);
			}
		}
		else if (drawState == FILL)
		{
			activeTools[toolIndex]->FloodFill(sim, currentBrush, cursor);
		}
	}

	if (versionCheck && versionCheck->CheckDone())
	{
		int status = 200;
		char *ret = versionCheck->Finish(NULL, &status);
		if (status != 200 || ParseServerReturn(ret, status, true))
		{
#ifndef ANDROID
			SetInfoTip("Error, could not find update server. Press Ctrl+u to go check for a newer version manually on the tpt website");
			UpdateToolTip("", Point(16, 20), INTROTIP, 0);
#endif
		}
		else
		{
			std::istringstream datastream(ret);
			Json::Value root;

			try
			{
				datastream >> root;

				//std::string motd = root["MessageOfTheDay"].asString();

				class DoUpdateAction : public ButtonAction
				{
					std::string changelog, filename;
				public:
					DoUpdateAction(std::string changelog_, std::string filename_):
						ButtonAction(),
						changelog(changelog_),
						filename(filename_)
					{

					}

					virtual void ButtionActionCallback(Button *button, unsigned char b)
					{
						if (b == 1)
							dynamic_cast<PowderToy*>(button->GetParent())->ConfirmUpdate(changelog, filename);
						button->GetParent()->RemoveComponent(button);
					}
				};
				Json::Value updates = root["Updates"];
				Json::Value stable = updates["Stable"];
				int major = stable["Major"].asInt();
				int minor = stable["Minor"].asInt();
				int buildnum = stable["Build"].asInt();
				std::string file = UPDATESERVER + stable["File"].asString();
				std::string changelog = stable["Changelog"].asString();
				if (buildnum > MOD_BUILD_VERSION)
				{
					std::stringstream changelogStream;
#ifdef ANDROID
					changelogStream << "\bbYour version: " << MOBILE_MAJOR << "." << MOBILE_MINOR << " (" << MOBILE_BUILD << ")\nNew version: " << major << "." << minor << " (" << buildnum << ")\n\n\bwChangeLog:\n";
#else
					changelogStream << "\bbYour version: " << MOD_VERSION << "." << MOD_MINOR_VERSION << " (" << MOD_BUILD_VERSION << ")\nNew version: " << major << "." << minor << " (" << buildnum << ")\n\n\bwChangeLog:\n";
#endif
					changelogStream << changelog;

					Button *notification = AddNotification("A new version is available - click here!");
					notification->SetCallback(new DoUpdateAction(changelogStream.str(), file));
					AddComponent(notification);
				}


				class NotificationOpenAction : public ButtonAction
				{
					std::string link;
				public:
					NotificationOpenAction(std::string link_):
						ButtonAction()
					{
						link = link_;
					}

					virtual void ButtionActionCallback(Button *button, unsigned char b)
					{
						if (b == 1)
							Platform::OpenLink(link);
						dynamic_cast<PowderToy*>(button->GetParent())->RemoveComponent(button);
					}
				};
				Json::Value notifications = root["Notifications"];
				for (int i = 0; i < (int)notifications.size(); i++)
				{
					std::string message = notifications[i]["Text"].asString();
					std::string link = notifications[i]["Link"].asString();

					Button *notification = AddNotification(message);
					notification->SetCallback(new NotificationOpenAction(link));
				}
			}
			catch (std::exception &e)
			{
				SetInfoTip("Error, the update server returned invalid data");
				UpdateToolTip("", Point(16, 20), INTROTIP, 0);
			}
		}
		free(ret);
		versionCheck = NULL;
	}
	if (sessionCheck && sessionCheck->CheckDone())
	{
		int status = 200;
		char *ret = sessionCheck->Finish(NULL, &status);
		// ignore timeout errors or others, since the user didn't actually click anything
		if (status != 200 || ParseServerReturn(ret, status, true))
		{
			// key icon changes to red
			loginFinished = -1;
		}
		else
		{
			std::istringstream datastream(ret);
			Json::Value root;

			try
			{
				datastream >> root;

				loginFinished = 1;
				if (!root["Session"].asInt())
				{
					if (svf_login)
						loginFinished = -1;
					// TODO: better login system, why do we reset all these
					strcpy(svf_user, "");
					strcpy(svf_user_id, "");
					strcpy(svf_session_id, "");
					svf_login = 0;
					svf_own = 0;
					svf_admin = 0;
					svf_mod = 0;
				}

				//std::string motd = root["MessageOfTheDay"].asString();

				class NotificationOpenAction : public ButtonAction
				{
					std::string link;
				public:
					NotificationOpenAction(std::string link_):
						ButtonAction()
					{
						link = link_;
					}

					virtual void ButtionActionCallback(Button *button, unsigned char b)
					{
						if (b == 1)
							Platform::OpenLink(link);
						dynamic_cast<PowderToy*>(button->GetParent())->RemoveComponent(button);
					}
				};
				Json::Value notifications = root["Notifications"];
				for (int i = 0; i < (int)notifications.size(); i++)
				{
					std::string message = notifications[i]["Text"].asString();
					std::string link = notifications[i]["Link"].asString();

					Button *notification = AddNotification(message);
					notification->SetCallback(new NotificationOpenAction(link));
				}
			}
			catch (std::exception &e)
			{
				// this shouldn't happen because the server hopefully won't return bad data ...
				loginFinished = -1;
			}
		}
		free(ret);
		sessionCheck = NULL;
	}
	// delay these a frame, due to the way startup initializatin is ordered
	// http hasn't been initialized when the PowderToy object is created
	if (delayedHttpChecks)
	{
		DelayedHttpInitialization();
		delayedHttpChecks = false;
	}
	if (voteDownload && voteDownload->CheckDone())
	{
		int status;
		char *ret = voteDownload->Finish(NULL, &status);
		if (ParseServerReturn(ret, status, false))
			svf_myvote = 0;
		else
			SetInfoTip("Voted Successfully");
		free(ret);
		voteDownload = NULL;
	}

	if (openConsole)
	{
		if (console_ui(GetVid()->GetVid()) == -1)
		{
			this->ignoreQuits = false;
			this->toDelete = true;
		}
		openConsole = false;
	}
	if (openSign)
	{
		// if currently moving a sign, stop doing so
		if (MSIGN != -1)
			MSIGN = -1;
		else
		{
			Point cursor = AdjustCoordinates(Point(mouseX, mouseY));
			int signID = InsideSign(sim, cursor.X, cursor.Y, true);
			if (signID == -1 && signs.size() >= MAXSIGNS)
				SetInfoTip("Sign limit reached");
			else
				Engine::Ref().ShowWindow(new CreateSign(signID, cursor));
		}
		openSign = false;
	}
	if (openProp)
	{
		prop_edit_ui(GetVid()->GetVid());
		openProp = false;
	}
	if (doubleScreenDialog)
	{
		std::stringstream message;
		message << "Switching to double size mode since your screen was determined to be large enough: ";
		message << screenWidth << "x" << screenHeight << " detected, " << (XRES+BARSIZE)*2 << "x" << (YRES+MENUSIZE)*2 << " required";
		message << "\nTo undo this, hit Cancel. You can toggle double size mode in settings at any time.";
		class ConfirmScale : public ConfirmAction
		{
		public:
			virtual void Action(bool isConfirmed)
			{
				if (!isConfirmed)
				{
					Engine::Ref().SetScale(1);
				}
			}
		};
		ConfirmPrompt *confirm = new ConfirmPrompt(new ConfirmScale(), "Large screen detected", message.str());
		Engine::Ref().ShowWindow(confirm);
		doubleScreenDialog = false;
	}

	// a ton of stuff with the buttons on the bottom row has to be updated
	// later, this will only be done when an event happens
	reloadButton->SetEnabled(svf_last ? true : false);
#ifdef TOUCHUI
	openBrowserButton->SetState(ctrlHeld ? Button::INVERTED : Button::HOLD);
	saveButton->SetState((svf_login && ctrlHeld) ? Button::INVERTED : Button::HOLD);
#else
	openBrowserButton->SetState(ctrlHeld ? Button::INVERTED : Button::NORMAL);
	saveButton->SetState((svf_login && ctrlHeld) ? Button::INVERTED : Button::NORMAL);
#endif
	std::string saveButtonText = "\x82 ";
	std::string saveButtonTip;
	if (!svf_login || ctrlHeld)
	{
		// button text
		if (svf_fileopen)
			saveButtonText += svf_filename;
		else
			saveButtonText += "[save to disk]";

		// button tooltip
		if (svf_fileopen && mouse.X <= saveButton->GetPosition().X+18)
			saveButtonTip = "Overwrite the open simulation on your hard drive.";
		else
		{
			if (!svf_login)
				saveButtonTip = "Save the simulation to your hard drive. Login to save online.";
			else
				saveButtonTip = "Save the simulation to your hard drive";
		}
	}
	else
	{
		// button text
		if (svf_open)
			saveButtonText += svf_name;
		else
			saveButtonText += "[untitled simulation]";

		// button tooltip
		if (svf_open && svf_own)
		{
			if (mouse.X <= saveButton->GetPosition().X+18)
				saveButtonTip = "Re-upload the current simulation";
			else
				saveButtonTip = "Modify simulation properties";
		}
		else
			saveButtonTip = "Upload a new simulation";
	}
	saveButton->SetText(saveButtonText);
	saveButton->SetTooltipText(saveButtonTip);

	bool votesAllowed = svf_login && svf_open && svf_own == 0 && svf_myvote == 0;
	upvoteButton->SetEnabled(votesAllowed && voteDownload == NULL);
	downvoteButton->SetEnabled(votesAllowed && voteDownload == NULL);
	upvoteButton->SetState(svf_myvote == 1 ? Button::HIGHLIGHTED : Button::NORMAL);
	downvoteButton->SetState(svf_myvote == -1 ? Button::HIGHLIGHTED : Button::NORMAL);
	if (svf_myvote == 1)
	{
		upvoteButton->SetTooltipText("You like this");
		downvoteButton->SetTooltipText("You like this");
	}
	else if (svf_myvote == -1)
	{
		upvoteButton->SetTooltipText("You dislike this");
		downvoteButton->SetTooltipText("You dislike this");
	}
	else
	{
		upvoteButton->SetTooltipText("Like this save");
		downvoteButton->SetTooltipText("Dislike this save");
	}

#ifndef TOUCHUI
	if (svf_tags[0])
		openTagsButton->SetText("\x83 " + std::string(svf_tags));
	else
		openTagsButton->SetText("\x83 [no tags set]");
	openTagsButton->SetEnabled(svf_open);
	if (svf_own)
		openTagsButton->SetTooltipText("Add and remove simulation tags");
	else
		openTagsButton->SetTooltipText("Add simulation tags");
#endif

	// set login button text, key turns green or red depending on whether session check succeeded
	std::string loginButtonText;
	std::string loginButtonTip;
	if (svf_login)
	{
		if (loginFinished == 1)
		{
			loginButtonText = "\x0F\x01\xFF\x01\x84\x0E " + std::string(svf_user);
			if (mouse.X <= loginButton->GetPosition().X+18)
				loginButtonTip = "View and edit your profile";
			else if (svf_mod && mouse.X >= loginButton->Right(Point(-15, 0)).X)
				loginButtonTip = "You're a moderator";
			else if (svf_admin && mouse.X >= loginButton->Right(Point(-15, 0)).X)
				loginButtonTip = "Annuit C\245ptis";
			else
				loginButtonTip = "Sign into the simulation server under a new name";
		}
		else if (loginFinished == -1)
		{
			loginButtonText = "\x0F\xFF\x01\x01\x84\x0E " + std::string(svf_user);
			loginButtonTip = "Could not validate login";
		}
		else
		{
			loginButtonText = "\x84 " + std::string(svf_user);
			loginButtonTip = "Waiting for login server ...";
		}
	}
	else
	{
		if (loginFinished == -1)
			loginButtonText = "\x0F\xFF\x01\x01\x84\x0E [sign in]";
		else
			loginButtonText = "\x84 [sign in]";
		loginButtonTip = "Sign into the Simulation Server";
	}
	loginButton->SetText(loginButtonText);
	loginButton->SetTooltipText(loginButtonTip);

	pauseButton->SetState(sys_pause ? Button::INVERTED : Button::NORMAL);
	if (sys_pause)
		pauseButton->SetTooltipText("Resume the simulation \bg(space)");
	else
		pauseButton->SetTooltipText("Pause the simulation \bg(space)");

	if (placingZoomTouch)
		UpdateToolTip("\x0F\xEF\xEF\020Tap any location to place a zoom window (volume keys to resize, click zoom button to cancel)", Point(16, YRES-24), TOOLTIP, 255);
#ifdef TOUCHUI
	if (state == SAVE || state == COPY)
		UpdateToolTip("\x0F\xEF\xEF\020Click-and-drag to specify a rectangle to copy (click save button to cancel)", Point(16, YRES-24), TOOLTIP, 255);
	else if (state == CUT)
		UpdateToolTip("\x0F\xEF\xEF\020Click-and-drag to specify a rectangle to copy and then cut (click save button to cancel)", Point(16, YRES-24), TOOLTIP, 255);
	else if (state == LOAD)
		UpdateToolTip("\x0F\xEF\xEF\020Drag the stamp around to move it, and tap it to place. Tap or drag outside the stamp to shift and rotate.", Point(16, YRES-24), TOOLTIP, 255);
#else
	if (state == SAVE || state == COPY)
		UpdateToolTip("\x0F\xEF\xEF\020Click-and-drag to specify a rectangle to copy (right click = cancel)", Point(16, YRES-24), TOOLTIP, 255);
	else if (state == CUT)
		UpdateToolTip("\x0F\xEF\xEF\020Click-and-drag to specify a rectangle to copy and then cut (right click = cancel)", Point(16, YRES-24), TOOLTIP, 255);
#endif
	if (insideRenderOptions && this->Subwindows.size() == 0)
	{
		insideRenderOptions = false;
		deletingRenderOptions = false;
		if (restorePreviousPause)
		{
			sys_pause = previousPause;
			restorePreviousPause = false;
		}
		save_presets();
	}
	VideoBufferHack();
}

void PowderToy::OnDraw(VideoBuffer *buf)
{
#ifdef LUACONSOLE
	luacon_step(mouse.X, mouse.Y);
	ExecuteEmbededLuaCode();
#endif
	if (insideRenderOptions)
		return;
	ARGBColour dotColor = 0;
	if (svf_fileopen && svf_login && ctrlHeld)
		dotColor = COLPACK(0x000000);
	else if ((!svf_login && svf_fileopen) || (svf_open && svf_own && !ctrlHeld))
		dotColor = COLPACK(0xFFFFFF);
	if (dotColor)
	{
		for (int i = 1; i <= saveButton->GetSize().Y-1; i+= 2)
			buf->DrawPixel(saveButton->GetPosition().X+18, saveButton->GetPosition().Y+i, COLR(dotColor), COLG(dotColor), COLB(dotColor), 255);
	}

	if (svf_login)
	{
		for (int i = 1; i <= saveButton->GetSize().Y-1; i+= 2)
			buf->DrawPixel(loginButton->GetPosition().X+18, loginButton->GetPosition().Y+i, 255, 255, 255, 255);

		// login check hasn't finished, key icon is dynamic
		if (loginFinished == 0)
			buf->FillRect(loginButton->GetPosition().X+2+loginCheckTicks/3, loginButton->GetPosition().Y+1, 16-loginCheckTicks/3, 13, 0, 0, 0, 255);

		if (svf_admin)
		{
			Point iconPos = loginButton->Right(Point(-12, (saveButton->GetSize().Y-12)/2+2));
			buf->DrawText(iconPos.X, iconPos.Y, "\xC9", 232, 127, 35, 255);
			buf->DrawText(iconPos.X, iconPos.Y, "\xC7", 255, 255, 255, 255);
			buf->DrawText(iconPos.X, iconPos.Y, "\xC8", 255, 255, 255, 255);
		}
		else if (svf_mod)
		{
			Point iconPos = loginButton->Right(Point(-12, (saveButton->GetSize().Y-12)/2+2));
			buf->DrawText(iconPos.X, iconPos.Y, "\xC9", 35, 127, 232, 255);
			buf->DrawText(iconPos.X, iconPos.Y, "\xC7", 255, 255, 255, 255);
		}
		// amd logo
		/*else if (true)
		{
			Point iconPos = loginButton->Right(Point(-12, (saveButton->GetSize().Y-10)/2));
			buf->DrawText(iconPos.X, iconPos.Y, "\x97", 0, 230, 153, 255);
		}*/
	}
}

void PowderToy::OnMouseMove(int x, int y, Point difference)
{
	mouse = Point(x, y);
	cursor = AdjustCoordinates(mouse);
	bool tmpMouseInZoom = IsMouseInZoom(mouse);
	if (placingZoom)
		UpdateZoomCoordinates(mouse);
	if (state == LOAD)
	{
#ifdef TOUCHUI
		if (stampMoving)
			UpdateStampCoordinates(cursor, stampClickedOffset);
#else
		UpdateStampCoordinates(cursor);
#endif
	}
	else if (state == SAVE || state == COPY || state == CUT)
	{
		if (isStampMouseDown)
		{
			saveSize.X = cursor.X + 1 - savePos.X;
			saveSize.Y = cursor.Y + 1 - savePos.Y;
			if (savePos.X + saveSize.X < 0)
				saveSize.X = 0;
			else if (savePos.X + saveSize.X > XRES)
				saveSize.X = XRES - savePos.X;
			if (savePos.Y + saveSize.Y < 0)
				saveSize.Y = 0;
			else if (savePos.Y + saveSize.Y > YRES)
				saveSize.Y = YRES - savePos.Y;
		}
	}
	else if (isMouseDown)
	{
		if (mouseInZoom == tmpMouseInZoom)
		{
			if (drawState == POINTS)
			{
				activeTools[toolIndex]->DrawLine(sim, currentBrush, lastDrawPoint, cursor, true, toolStrength);
				lastDrawPoint = cursor;
				skipDraw = true;
			}
			else if (drawState == FILL)
			{
				activeTools[toolIndex]->FloodFill(sim, currentBrush, cursor);
				skipDraw = true;
			}
		}
		else if (drawState == POINTS || drawState == FILL)
		{
			isMouseDown = false;
			drawState = POINTS;
#ifdef LUACONSOLE
			// special lua mouse event
			luacon_mouseevent(x, y, 0, LUACON_MUPZOOM, 0);
#endif
		}
		mouseInZoom = tmpMouseInZoom;
	}

	// moving sign, update coordinates here
	if (MSIGN >= 0 && MSIGN < (int)signs.size())
	{
		signs[MSIGN]->SetPos(cursor);
	}
}

bool PowderToy::BeforeMouseDown(int x, int y, unsigned char button)
{
#ifdef LUACONSOLE
	// lua mouse event, cancel mouse action if the function returns false
	if (!luacon_mouseevent(x, y, button, LUACON_MDOWN, 0))
		return false;
#endif
	return true;
}

void PowderToy::OnMouseDown(int x, int y, unsigned char button)
{
	mouse = Point(x, y);
	cursor = AdjustCoordinates(mouse);
	mouseInZoom = IsMouseInZoom(mouse);
	deletingRenderOptions = false;
	if (deco_disablestuff)
	{

	}
	else if (placingZoomTouch)
	{
		if (x < XRES && y < YRES)
		{
			placingZoomTouch = false;
			placingZoom = true;
			UpdateZoomCoordinates(mouse);
		}
	}
	else if (placingZoom)
	{

	}
	else if (state == LOAD)
	{
		isStampMouseDown = true;
#ifdef TOUCHUI
		stampClickedPos = cursor;
		initialLoadPos = loadPos;
		UpdateStampCoordinates(cursor);
		stampClickedOffset = loadPos-initialLoadPos;
		loadPos -= stampClickedOffset;
		if (cursor.IsInside(loadPos, loadPos+loadSize))
			stampMoving = true;
		// calculate which side of the stamp this touch is on
		else
		{
			int xOffset = (loadSize.X-loadSize.Y)/2;
			Point diff = cursor-(loadPos+loadSize/2);
			if (std::abs(diff.X)-xOffset > std::abs(diff.Y))
				stampQuadrant = (diff.X > 0) ? 3 : 1; // right : left
			else
				stampQuadrant = (diff.Y > 0) ? 2 : 0; // down : up
		}
#endif
	}
	else if (state == SAVE || state == COPY || state == CUT)
	{
		// right click cancel
		if (button == 4)
		{
			ResetStampState();
		}
		// placing initial coordinate
		else if (!isStampMouseDown)
		{
			savePos = cursor;
			saveSize = Point(1, 1);
			isStampMouseDown = true;
		}
	}
	else if (InsideSign(sim, cursor.X, cursor.Y, ctrlHeld) != -1 || MSIGN != -1)
	{
		// do nothing
	}
	else if (insideRenderOptions)
	{
		deletingRenderOptions = true;
	}
	else if (sim->InBounds(mouse.X, mouse.Y))
	{
		toolIndex = ((button&1) || button == 2) ? 0 : 1;
		UpdateDrawMode();
		// this was in old drawing code, still needed?
		//if (activeTools[0]->GetType() == DECO_TOOL && button == 4)
		//	activeTools[1] = GetToolFromIdentifier("DEFAULT_DECOR_CLR");
		if (button == 2 || (altHeld && !shiftHeld && !ctrlHeld))
		{
			Tool *tool = activeTools[toolIndex]->Sample(sim, cursor);
			if (tool)
				activeTools[toolIndex] = activeTools[toolIndex]->Sample(sim, cursor);
			return;
		}

		isMouseDown = true;
		if (drawState == LINE || drawState == RECT)
		{
			initialDrawPoint = cursor;
		}
		else if (drawState == POINTS)
		{
			Snapshot::TakeSnapshot(sim);
			lastDrawPoint = cursor;
			activeTools[toolIndex]->DrawPoint(sim, currentBrush, cursor, toolStrength);
		}
		else if (drawState == FILL)
		{
			Snapshot::TakeSnapshot(sim);
			activeTools[toolIndex]->FloodFill(sim, currentBrush, cursor);
		}
	}
}

bool PowderToy::BeforeMouseUp(int x, int y, unsigned char button)
{
	mouseCanceled = false;
#ifdef LUACONSOLE
	// lua mouse event, cancel mouse action if the function returns false
	if (!luacon_mouseevent(x, y, button, LUACON_MUP, 0))
		return false;
#endif
	return true;
}

void PowderToy::OnMouseUp(int x, int y, unsigned char button)
{
	mouse = Point(x, y);
	cursor = AdjustCoordinates(mouse);

	if (placingZoom)
	{
		placingZoom = false;
		zoomEnabled = true;
	}
	else if (ignoreMouseUp)
	{
		// ignore mouse up when some touch ui buttons on the right side are pressed
		ignoreMouseUp = false;
	}
	else if (state == LOAD)
	{
		UpdateDrawMode(); // LOAD branch always returns early, so run this here
		if (button == 4 || y >= YRES+MENUSIZE-16)
		{
			ResetStampState();
			return;
		}
		// never had a mouse down event while in LOAD state, return
		if (!isStampMouseDown)
			return;
		isStampMouseDown = false;
#ifdef TOUCHUI
		if (loadPos != initialLoadPos)
		{
			stampMoving = false;
			return;
		}
		else if (cursor.IsInside(Point(0, 0), Point(XRES, YRES)) && !cursor.IsInside(loadPos, loadPos+loadSize))
		{
			// figure out which side this touch started and ended on (arbitrary direction numbers)
			// imagine 4 quadrants coming out of the stamp, with the edges diagonally starting directly from each corner
			// if you tap the screen in one of these corners, it shifts the stamp one pixel in that direction
			// if you move between quadrants you can rotate in that direction
			// or flip horizontally / vertically by dragging accross the stamp without touching the side quadrants
			int quadrant, xOffset = (loadSize.X-loadSize.Y)/2;
			Point diff = cursor-(loadPos+loadSize/2);
			if (std::abs(diff.X)-xOffset > std::abs(diff.Y))
				quadrant = (diff.X > 0) ? 3 : 1; // right : left
			else
				quadrant = (diff.Y > 0) ? 2 : 0; // down : up

			matrix2d transform = m2d_identity;
			vector2d translate = v2d_zero;
			// shift (arrow keys)
			if (quadrant == stampQuadrant)
				translate = v2d_new((quadrant-2)%2, (quadrant-1)%2);
			// rotate 90 degrees
			else if (quadrant%2 != stampQuadrant%2)
				transform = m2d_new(0, (quadrant-stampQuadrant+1)%4 == 0 ? -1 : 1, (quadrant-stampQuadrant+1)%4 == 0 ? 1 : -1, 0);
			// flip 180
			else
				transform = m2d_new((quadrant%2)*-2+1, 0, 0, (quadrant%2)*2-1);

			// actual transformation is done here
			void *newData = transform_save(stampData, &stampSize, transform, translate);
			if (!newData)
				return;
			free(stampData);
			stampData = newData;
			free(stampImg);
			stampImg = prerender_save(stampData, stampSize, &loadSize.X, &loadSize.Y);
			return;
		}
		else if (!stampMoving)
			return;
		stampMoving = false;
#endif
		Snapshot::TakeSnapshot(sim);
		//Json::Value tempStampInfo;
		//if (!parse_save(stampData, stampSize, 0, loadPos.X, loadPos.Y, bmap, sim->air->vx, sim->air->vy, sim->air->pv, sim->air->fvx, sim->air->fvy, signs, parts, pmap, &tempStampInfo, !shiftHeld))
		//	MergeStampAuthorInfo(tempStampInfo);
		
		Save *save = new Save();
		save->ParseSave(stampData, stampSize);
		sim->LoadSave(loadPos.X, loadPos.Y, save, 0, !shiftHeld);
		//MergeStampAuthorInfo(save->authors);
		
		ResetStampState();
		return;
	}
	else if (state == SAVE || state == COPY || state == CUT)
	{
		UpdateDrawMode(); // SAVE/COPY/CUT branch always returns early, so run this here
		// already placed initial coordinate. If they haven't ... no idea what happened here
		// mouse could be 4 if strange stuff with zoom window happened so do nothing and reset state in that case too
		if (button != 1)
		{
			ResetStampState();
			return;
		}
		if (!isStampMouseDown)
			return;

		// make sure size isn't negative
		if (saveSize.X < 0)
		{
			savePos.X = savePos.X + saveSize.X - 1;
			saveSize.X = abs(saveSize.X) + 2;
		}
		if (saveSize.Y < 0)
		{
			savePos.Y = savePos.Y + saveSize.Y - 1;
			saveSize.Y = abs(saveSize.Y) + 2;
		}
		if (saveSize.X > 0 && saveSize.Y > 0)
		{
			switch (state)
			{
			case COPY:
			{
				free(clipboardData);
				Json::Value clipboardInfo;
				clipboardInfo["type"] = "clipboard";
				clipboardInfo["username"] = svf_user;
				clipboardInfo["date"] = (Json::Value::UInt64)time(NULL);
				SaveAuthorInfo(&clipboardInfo);
				clipboardData = build_save(&clipboardSize, savePos.X, savePos.Y, saveSize.X, saveSize.Y, bmap, sim->air->vx, sim->air->vy, sim->air->pv, sim->air->fvx, sim->air->fvy, signs, parts, &clipboardInfo, false, !shiftHeld);
				break;
			}
			case CUT:
			{
				free(clipboardData);
				Json::Value clipboardInfo;
				clipboardInfo["type"] = "clipboard";
				clipboardInfo["username"] = svf_user;
				clipboardInfo["date"] = (Json::Value::UInt64)time(NULL);
				SaveAuthorInfo(&clipboardInfo);
				clipboardData = build_save(&clipboardSize, savePos.X, savePos.Y, saveSize.X, saveSize.Y, bmap, sim->air->vx, sim->air->vy, sim->air->pv, sim->air->fvx, sim->air->fvy, signs, parts, &clipboardInfo, false, !shiftHeld);
				if (clipboardData)
					clear_area(savePos.X, savePos.Y, saveSize.X, saveSize.Y);
				break;
			}
			case SAVE:
				// function returns the stamp name which we don't want, so free it
				free(stamp_save(savePos.X, savePos.Y, saveSize.X, saveSize.Y, !shiftHeld));
				break;
			default:
				break;
			}
		}
		ResetStampState();
		return;
	}
	else if (MSIGN != -1)
		MSIGN = -1;
	else if (insideRenderOptions)
	{
		if (this->Subwindows.size() && deletingRenderOptions)
			this->Subwindows[0]->toDelete = true;
	}
	else if (isMouseDown)
	{
		if (drawState == POINTS)
		{
			activeTools[toolIndex]->DrawLine(sim, currentBrush, lastDrawPoint, cursor, true, toolStrength);
			activeTools[toolIndex]->Click(sim, cursor);
		}
		else if (drawState == LINE)
		{
			if (altHeld)
				cursor = LineSnapCoords(initialDrawPoint, cursor);
			Snapshot::TakeSnapshot(sim);
			activeTools[toolIndex]->DrawLine(sim, currentBrush, initialDrawPoint, cursor, false, 1.0f);
		}
		else if (drawState == RECT)
		{
			if (altHeld)
				cursor = RectSnapCoords(initialDrawPoint, cursor);
			Snapshot::TakeSnapshot(sim);
			activeTools[toolIndex]->DrawRect(sim, currentBrush, initialDrawPoint, cursor);
		}
		else if (drawState == FILL)
		{
			activeTools[toolIndex]->FloodFill(sim, currentBrush, cursor);
		}
		isMouseDown = false;
	}
	else
	{
		// ctrl+click moves a sign
		if (ctrlHeld)
		{
			int signID = InsideSign(sim, cursor.X, cursor.Y, true);
			if (signID != -1)
				MSIGN = signID;
		}
		// link signs are clicked from here
		else
		{
			toolIndex = ((button&1) || button == 2) ? 0 : 1;
			bool signTool = ((ToolTool*)activeTools[toolIndex])->GetID() == TOOL_SIGN;
			int signID = InsideSign(sim, cursor.X, cursor.Y, false);
			if (signID != -1)
			{
				// this is a hack so we can edit clickable signs when sign tool is selected (normal signs are handled in activeTool->Click())
				if (signTool)
					openSign = true;
				else if (signs[signID]->GetType() == Sign::Spark)
				{
					Point realPos = signs[signID]->GetRealPos();
					if (pmap[realPos.Y][realPos.X])
						sim->spark_all_attempt(pmap[realPos.Y][realPos.X]>>8, realPos.X, realPos.Y);
				}
				else if (signs[signID]->GetType() == Sign::SaveLink)
				{
					open_ui(vid_buf, (char*)signs[signID]->GetLinkText().c_str(), 0, 0);
				}
				else if (signs[signID]->GetType() == Sign::ThreadLink)
				{
					Platform::OpenLink("http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=" + signs[signID]->GetLinkText());
				}
				else if (signs[signID]->GetType() == Sign::SearchLink)
				{
					strncpy(search_expr, signs[signID]->GetLinkText().c_str(), 255);
					search_own = 0;
					search_ui(vid_buf);
				}
			}
		}
	}
	// update the drawing mode for the next line
	// since ctrl/shift state may have changed since we started drawing
	UpdateDrawMode();
	deletingRenderOptions = false;
}

bool PowderToy::BeforeMouseWheel(int x, int y, int d)
{
#ifdef LUACONSOLE
	int mouseX, mouseY;
	// lua mouse event, cancel mouse action if the function returns false
	if (!luacon_mouseevent(x, y, mouse_get_state(&mouseX, &mouseY), 0, d))
		return false;
#endif
	return true;
}

void PowderToy::OnMouseWheel(int x, int y, int d)
{
	mouseWheel += d;
	if (PlacingZoomWindow())
	{
		zoomSize = std::max(2, std::min(zoomSize+d, 60));
		zoomFactor = 256/zoomSize;
		UpdateZoomCoordinates(zoomMousePosition);
	}
}

bool PowderToy::BeforeKeyPress(int key, unsigned short character, unsigned short modifiers)
{
	heldModifier = modifiers;
	heldKey = key;
	heldAscii = character;
	// do nothing when deco textboxes are selected
	if (deco_disablestuff)
		return true;

#ifdef LUACONSOLE
	if (!luacon_keyevent(key, character, modifiers, LUACON_KDOWN))
	{
		heldKey = 0;
		return false;
	}
#endif

	// lua can disable all key shortcuts
	if (!sys_shortcuts)
		return false;

	return true;
}

void PowderToy::OnKeyPress(int key, unsigned short character, unsigned short modifiers)
{
	switch (key)
	{
	case SDLK_LCTRL:
	case SDLK_RCTRL:
	case SDLK_LMETA:
	case SDLK_RMETA:
		ctrlHeld = true;
		openBrowserButton->SetTooltipText("Open a simulation from your hard drive \bg(ctrl+o)");
		UpdateToolStrength();
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		shiftHeld = true;
		UpdateToolStrength();
		break;
	case SDLK_LALT:
	case SDLK_RALT:
		altHeld = true;
		break;
	}

	if (deco_disablestuff)
		return;

	// loading a stamp, special handling here
	// if stamp was transformed, key presses get ignored
	if (state == LOAD)
	{
		matrix2d transform = m2d_identity;
		vector2d translate = v2d_zero;
		bool doTransform = true;

		switch (key)
		{
		case 'r':
			// vertical invert
			if (ctrlHeld && shiftHeld)
			{
				transform = m2d_new(1, 0, 0, -1);
			}
			// horizontal invert
			else if (shiftHeld)
			{
				transform = m2d_new(-1, 0, 0, 1);
			}
			// rotate anticlockwise 90 degrees
			else
			{
				transform = m2d_new(0, 1, -1, 0);
			}
			break;
		case SDLK_LEFT:
			translate = v2d_new(-1, 0);
			break;
		case SDLK_RIGHT:
			translate = v2d_new(1, 0);
			break;
		case SDLK_UP:
			translate = v2d_new(0, -1);
			break;
		case SDLK_DOWN:
			translate = v2d_new(0, 1);
			break;
		default:
			doTransform = false;
		}

		if (doTransform)
		{
			void *newData = transform_save(stampData, &stampSize, transform, translate);
			if (!newData)
				return;
			free(stampData);
			stampData = newData;
			free(stampImg);
			stampImg = prerender_save(stampData, stampSize, &loadSize.X, &loadSize.Y);
			return;
		}
	}

	// handle normal keypresses
	switch (key)
	{
	case 'q':
	case SDLK_ESCAPE:
	{
		if (this->Subwindows.size() && insideRenderOptions)
		{
			deletingRenderOptions = false;
			this->Subwindows[0]->toDelete = true;
			break;
		}

		class ConfirmQuit : public ConfirmAction
		{
			PowderToy *you_just_lost;
		public:
			ConfirmQuit(PowderToy *the_game) :
				you_just_lost(the_game)
			{

			}
			virtual void Action(bool isConfirmed)
			{
				if (isConfirmed)
				{
					you_just_lost->ignoreQuits = false;
					you_just_lost->toDelete = true;
				}
			}
		};
		ConfirmPrompt *confirm = new ConfirmPrompt(new ConfirmQuit(this), "You are about to quit", "Are you sure you want to exit the game?", "Quit");
		Engine::Ref().ShowWindow(confirm);
		break;
	}
	case 'y':
		// ctrl + y
		if (ctrlHeld)
		{
			Snapshot::RestoreRedoSnapshot(sim);
		}
		break;
	case 'i':
		if (!ctrlHeld)
		{
			for (int nx = 0; nx < XRES/CELL; nx++)
				for (int ny = 0; ny < YRES/CELL; ny++)
				{
					sim->air->pv[ny][nx] = -sim->air->pv[ny][nx];
					sim->air->vx[ny][nx] = -sim->air->vx[ny][nx];
					sim->air->vy[ny][nx] = -sim->air->vy[ny][nx];
				}
		}
		else
		{
			class ConfirmInstall : public ConfirmAction
			{
			public:
				virtual void Action(bool isConfirmed)
				{
					if (isConfirmed)
					{
						if (Platform::RegisterExtension())
						{
							InfoPrompt *info = new InfoPrompt("Install Success", "Powder Toy has been installed!");
							Engine::Ref().ShowWindow(info);
						}
						else
						{
							ErrorPrompt *error = new ErrorPrompt("Install failed - You may not have permission or you may be on a platform that does not support installation");
							Engine::Ref().ShowWindow(error);
						}
					}
				}
			};
			ConfirmPrompt *confirm = new ConfirmPrompt(new ConfirmInstall(), "Install Powder Toy", "You are about to install The Powder Toy", "Install");
			Engine::Ref().ShowWindow(confirm);
		}
		break;
	case 'a':
		if (ctrlHeld && (svf_mod || svf_admin))
		{
			std::string authorString = authors.toStyledString();
			InfoPrompt *info = new InfoPrompt("Save authorship info", authorString);
			Engine::Ref().ShowWindow(info);
		}
		break;
	case 's':
		//if stkm2 is out, you must be holding left ctrl, else not be holding ctrl at all
		if (sim->elementCount[PT_STKM2] > 0 ? ctrlHeld : !ctrlHeld)
		{
			ResetStampState();
			state = SAVE;
		}
		break;
	case 'k':
	case 'l':
		ResetStampState();
		// open stamp interface
		if (key == 'k')
		{
			int reorder = 1;
			int stampID = stamp_ui(vid_buf, &reorder);
			if (stampID >= 0)
				stampData = stamp_load(stampID, &stampSize, reorder);
			else
				stampData = NULL;
		}
		// else, open most recent stamp
		else
			stampData = stamp_load(0, &stampSize, 1);

		// if a stamp was actually loaded
		if (stampData)
		{
			int width, height;
			stampImg = prerender_save(stampData, stampSize, &width, &height);
			if (stampImg)
			{
				state = LOAD;
				loadSize = Point(width, height);
				waitToDraw = true;
				UpdateStampCoordinates(cursor);
			}
			else
			{
				free(stampData);
				stampData = NULL;
			}
		}
		break;
	case 'z':
		// ctrl + z
		if (ctrlHeld)
		{
			if (shiftHeld)
				Snapshot::RestoreRedoSnapshot(sim);
			else
				Snapshot::RestoreSnapshot(sim);
		}
		// zoom window
		else
		{
			if (isStampMouseDown)
				break;
			placingZoom = true;
			isMouseDown = false;
			UpdateZoomCoordinates(mouse);
		}
		break;
	case 'x':
		if (ctrlHeld)
		{
			ResetStampState();
			state = CUT;
		}
		break;
	case 'c':
		if (ctrlHeld)
		{
			ResetStampState();
			state = COPY;
		}
		break;
	case 'v':
		if (ctrlHeld && clipboardData)
		{
			ResetStampState();
			stampData = malloc(clipboardSize);
			if (stampData)
			{
				memcpy(stampData, clipboardData, clipboardSize);
				stampSize = clipboardSize;
				stampImg = prerender_save(stampData, stampSize, &loadSize.X, &loadSize.Y);
				if (stampImg)
				{
					state = LOAD;
					isStampMouseDown = false;
					UpdateStampCoordinates(cursor);
				}
				else
				{
					free(stampData);
					stampData = NULL;
				}
			}
		}
		break;
	case 'b':
		if (sdl_mod & (KMOD_CTRL|KMOD_META))
		{
			decorations_enable = !decorations_enable;
			std::string tooltip;
			if (decorations_enable)
				tooltip = "Decorations layer: On";
			else
				tooltip = "Decorations layer: Off";
			UpdateToolTip(tooltip, Point(XCNTR-VideoBuffer::TextSize(tooltip).X/2, YCNTR-10), INFOTIP, 255);
		}
		else if (active_menu == SC_DECO)
		{
			active_menu = last_active_menu;
			SwapToRegularToolset();
		}
		else
		{
			last_active_menu = active_menu;
			decorations_enable = true;
			SetPause(1);
			active_menu = SC_DECO;

			SwapToDecoToolset();
		}
		break;
	case SDLK_SPACE:
		TogglePause();
		break;
	case SDLK_LEFTBRACKET:
		if (PlacingZoomWindow())
		{
			int temp = std::min(--zoomSize, 60);
			zoomSize = std::max(2, temp);
			zoomFactor = 256/zoomSize;
			UpdateZoomCoordinates(zoomMousePosition);
		}
		break;
	case SDLK_RIGHTBRACKET:
		if (PlacingZoomWindow())
		{
			int temp = std::min(++zoomSize, 60);
			zoomSize = std::max(2, temp);
			zoomFactor = 256/zoomSize;
			UpdateZoomCoordinates(zoomMousePosition);
		}
		break;
	case SDLK_1:
		if (shiftHeld && DEBUG_MODE)
		{
			if (ctrlHeld)
				Renderer::Ref().XORColorMode(COLOR_LIFE);
			else
				LoadRenderPreset(CM_LIFE);
		}
		else if (ctrlHeld)
			Renderer::Ref().ToggleDisplayMode(DISPLAY_AIRV);
		else
			LoadRenderPreset(CM_VEL);
		break;
	case SDLK_2:
		if (ctrlHeld)
			Renderer::Ref().ToggleDisplayMode(DISPLAY_AIRP);
		else
			LoadRenderPreset(CM_PRESS);
		break;
	case SDLK_3:
		if (ctrlHeld)
			Renderer::Ref().ToggleDisplayMode(DISPLAY_PERS);
		else
			LoadRenderPreset(CM_PERS);
		break;
	case SDLK_4:
		if (ctrlHeld)
			Renderer::Ref().ToggleRenderMode(FIREMODE);
		else
			LoadRenderPreset(CM_FIRE);
		break;
	case SDLK_5:
		if (ctrlHeld)
			Renderer::Ref().ToggleRenderMode(PMODE_BLOB);
		else
			LoadRenderPreset(CM_BLOB);
		break;
	case SDLK_6:
		if (ctrlHeld)
			Renderer::Ref().XORColorMode(COLOR_HEAT);
		else
			LoadRenderPreset(CM_HEAT);
		break;
	case SDLK_7:
		if (ctrlHeld)
			Renderer::Ref().ToggleDisplayMode(DISPLAY_WARP);
		else
			LoadRenderPreset(CM_FANCY);
		break;
	case SDLK_8:
		LoadRenderPreset(CM_NOTHING);
		break;
	case SDLK_9:
		if (ctrlHeld)
			Renderer::Ref().XORColorMode(COLOR_GRAD);
		else
			LoadRenderPreset(CM_GRAD);
		break;
	case SDLK_0:
		if (ctrlHeld)
			Renderer::Ref().ToggleDisplayMode(DISPLAY_AIRC);
		else
			LoadRenderPreset(CM_CRACK);
		break;
	}
}

bool PowderToy::BeforeKeyRelease(int key, unsigned short character, unsigned short modifiers)
{
	heldModifier = modifiers;
	releasedKey = key;

	if (deco_disablestuff)
		return true;

#ifdef LUACONSOLE
	if (!luacon_keyevent(key, key < 256 ? key : 0, modifiers, LUACON_KUP))
	{
		releasedKey = 0;
		return false;
	}
#endif
	return true;
}

void PowderToy::OnKeyRelease(int key, unsigned short character, unsigned short modifiers)
{
	// temporary
	if (key == 0)
	{
		ctrlHeld = shiftHeld = altHeld = 0;
	}
	switch (key)
	{
	case SDLK_LCTRL:
	case SDLK_RCTRL:
	case SDLK_LMETA:
	case SDLK_RMETA:
		ctrlHeld = false;
		openBrowserButton->SetTooltipText("Find & open a simulation");
		UpdateToolStrength();
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		shiftHeld = false;
		UpdateToolStrength();
		break;
	case SDLK_LALT:
	case SDLK_RALT:
		altHeld = false;
		break;
	}

	if (deco_disablestuff)
		return;

	switch (key)
	{
	case 'z':
		if (placingZoom)
			HideZoomWindow();
		break;
	}
}

void PowderToy::OnDefocus()
{
#ifdef LUACONSOLE
	if (ctrlHeld || shiftHeld || altHeld)
		luacon_keyevent(0, 0, 0, LUACON_KUP);
#endif

	ctrlHeld = shiftHeld = altHeld = false;
	openBrowserButton->SetTooltipText("Find & open a simulation");
	lastMouseDown = heldKey = heldAscii = releasedKey = mouseWheel = 0; // temporary
	ResetStampState();
	UpdateDrawMode();
	UpdateToolStrength();
}

void PowderToy::OnJoystickMotion(uint8_t joysticknum, uint8_t axis, int16_t value)
{
	orientation[axis] = value;
}
